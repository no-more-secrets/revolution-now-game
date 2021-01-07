/****************************************************************
**europort-view.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2019-06-14.
*
* Description: Implements the Europe port view.
*
*****************************************************************/
#include "europort-view.hpp"

// Revolution Now
#include "cargo.hpp"
#include "commodity.hpp"
#include "coord.hpp"
#include "dragdrop.hpp"
#include "europort.hpp"
#include "fb.hpp"
#include "fmt-helper.hpp"
#include "fsm.hpp"
#include "gfx.hpp"
#include "image.hpp"
#include "init.hpp"
#include "input.hpp"
#include "logging.hpp"
#include "macros.hpp"
#include "plane-ctrl.hpp"
#include "plane.hpp"
#include "render.hpp"
#include "screen.hpp"
#include "sg-macros.hpp"
#include "sync-future.hpp"
#include "text.hpp"
#include "tiles.hpp"
#include "ustate.hpp"
#include "variant.hpp"
#include "window.hpp"

// base
#include "base/range-lite.hpp"
#include "base/scope-exit.hpp"

// Rnl
#include "rnl/europort-view.hpp"

// Flatbuffers
#include "fb/sg-europort-view_generated.h"

using namespace std;

namespace rn {

namespace rl = ::base::rl;

DECLARE_SAVEGAME_SERIALIZERS( EuroportView );

namespace {

constexpr Delta const k_rendered_commodity_offset{ 8_w, 3_h };

// When we drag a commodity from the market this is the default
// amount that we take.
constexpr int const k_default_market_quantity = 100;

/****************************************************************
** Save-Game State
*****************************************************************/
struct SAVEGAME_STRUCT( EuroportView ) {
  // Fields that are actually serialized.

  // clang-format off
  SAVEGAME_MEMBERS( EuroportView,
  ( maybe<UnitId>, selected_unit ));
  // clang-format on

public:
  // Fields that are derived from the serialized fields.

private:
  SAVEGAME_FRIENDS( EuroportView );
  SAVEGAME_SYNC() {
    // Sync all fields that are derived from serialized fields
    // and then validate (check invariants).

    return valid;
  }
  // Called after all modules are deserialized.
  SAVEGAME_VALIDATE() { return valid; }
};
SAVEGAME_IMPL( EuroportView );

/****************************************************************
** Draggable Object
*****************************************************************/
static_assert( std::is_copy_constructible_v<DraggableObject_t> );

// Global State.
maybe<DraggableObject_t> g_dragging_object;

maybe<DraggableObject_t> cargo_slot_to_draggable(
    CargoSlotIndex slot_idx, CargoSlot_t const& slot ) {
  switch( slot.to_enum() ) {
    case CargoSlot::e::empty: {
      return nothing;
    }
    case CargoSlot::e::overflow: {
      return nothing;
    }
    case CargoSlot::e::cargo: {
      auto& cargo = slot.get<CargoSlot::cargo>();
      return overload_visit(
          cargo.contents,
          []( UnitId id ) -> DraggableObject_t {
            return DraggableObject::unit{ /*id=*/id };
          },
          [&]( Commodity const& c ) -> DraggableObject_t {
            return DraggableObject::cargo_commodity{
                /*comm=*/c,
                /*slot=*/slot_idx };
          } );
    }
  }
}

maybe<Cargo> draggable_to_cargo_object(
    DraggableObject_t const& draggable ) {
  switch( draggable.to_enum() ) {
    case DraggableObject::e::unit: {
      auto& val = draggable.get<DraggableObject::unit>();
      return val.id;
    }
    case DraggableObject::e::market_commodity: return nothing;
    case DraggableObject::e::cargo_commodity: {
      auto& val =
          draggable.get<DraggableObject::cargo_commodity>();
      return val.comm;
    }
  }
}

maybe<DraggableObject_t> draggable_in_cargo_slot(
    CargoSlotIndex slot ) {
  return SG()
      .selected_unit.fmap( unit_from_id )
      .bind( LC( _.cargo().at( slot ) ) )
      .bind( LC( cargo_slot_to_draggable( slot, _ ) ) );
}

maybe<DraggableObject_t> draggable_in_cargo_slot( int slot ) {
  return draggable_in_cargo_slot( CargoSlotIndex{ slot } );
}

Texture draw_draggable_object(
    DraggableObject_t const& object ) {
  switch( object.to_enum() ) {
    case DraggableObject::e::unit: {
      auto& [id] = object.get<DraggableObject::unit>();
      auto tx    = create_texture_transparent(
          lookup_sprite( unit_from_id( id ).desc().tile )
              .size() );
      render_unit( tx, id, Coord{}, /*with_icon=*/false );
      return tx;
    }
    case DraggableObject::e::market_commodity: {
      auto& [type] =
          object.get<DraggableObject::market_commodity>();
      return render_commodity_create( type );
    }
    case DraggableObject::e::cargo_commodity: {
      auto& val = object.get<DraggableObject::cargo_commodity>();
      return render_commodity_create( val.comm.type );
    }
  }
  UNREACHABLE_LOCATION;
}

/****************************************************************
** Helpers
*****************************************************************/
// Both rl::all and the lambda will take rect_proxy by reference
// so we therefore must have this function take a reference to a
// rect_proxy that outlives the use of the returned range. And of
// course the Rect referred to by the rect_proxy must outlive
// everything.
auto range_of_rects(
    RectGridProxyIteratorHelper const& rect_proxy ) {
  return rl::all( rect_proxy )
      .map( [&rect_proxy]( Coord coord ) {
        return Rect::from(
            coord, Delta{ 1_w, 1_h } * rect_proxy.scale() );
      } );
}

auto range_of_rects( RectGridProxyIteratorHelper&& ) = delete;

/****************************************************************
** Europe View Entities
*****************************************************************/
namespace entity {

// Each entity is defined by a struct that holds its state and
// that has the following methods:
//
//  void draw( Texture& tx, Delta offset ) const;
//  Rect bounds() const;
//  static maybe<EntityClass> create( ... );
//  maybe<pair<T,Rect>> obj_under_cursor( Coord const& );

// This object represents the array of cargo items available for
// trade in europe and which is show at the bottom of the screen.
class MarketCommodities {
  static constexpr W single_layer_blocks_width  = 16_w;
  static constexpr W double_layer_blocks_width  = 8_w;
  static constexpr H single_layer_blocks_height = 1_h;
  static constexpr H double_layer_blocks_height = 2_h;

  // Commodities will be 24x24 + 8 pixels for text.
  static constexpr auto sprite_scale = Scale{ 32 };

  static constexpr W single_layer_width =
      single_layer_blocks_width * sprite_scale.sx;
  static constexpr W double_layer_width =
      double_layer_blocks_width * sprite_scale.sx;
  static constexpr H single_layer_height =
      single_layer_blocks_height * sprite_scale.sy;
  static constexpr H double_layer_height =
      double_layer_blocks_height * sprite_scale.sy;

public:
  Rect bounds() const {
    return Rect::from( origin_,
                       Delta{ doubled_ ? double_layer_width
                                       : single_layer_width,
                              doubled_ ? double_layer_height
                                       : single_layer_height } );
  }

  void draw( Texture& tx, Delta offset ) const {
    auto bds  = bounds();
    auto grid = bds.to_grid_noalign( sprite_scale );
    auto comm_it =
        magic_enum::enum_values<e_commodity>().begin();
    auto label = CommodityLabel::buy_sell{ 100, 200 };
    for( auto rect : range_of_rects( grid ) ) {
      render_rect( tx, Color::white(),
                   rect.shifted_by( offset ) );
      render_commodity_annotated(
          tx, *comm_it++,
          rect.shifted_by( offset ).upper_left() +
              k_rendered_commodity_offset,
          label );
      label.buy += 120;
      label.sell += 120;
    }
  }

  MarketCommodities( MarketCommodities&& ) = default;
  MarketCommodities& operator=( MarketCommodities&& ) = default;

  static maybe<MarketCommodities> create( Delta const& size ) {
    maybe<MarketCommodities> res;
    auto                     rect = Rect::from( Coord{}, size );
    if( size.w >= single_layer_width &&
        size.h >= single_layer_height ) {
      res = MarketCommodities{
          /*doubled_=*/false,
          /*origin_=*/Coord{
              /*x=*/rect.center().x - single_layer_width / 2_sx,
              /*y=*/rect.bottom_edge() - single_layer_height } };
    } else if( rect.w >= double_layer_width &&
               rect.h >= double_layer_height ) {
      res = MarketCommodities{
          /*doubled_=*/true,
          /*origin_=*/Coord{
              /*x=*/rect.center().x - double_layer_width / 2_sx,
              /*y=*/rect.bottom_edge() - double_layer_height } };

    } else {
      // cannot draw.
    }
    return res;
  }

  maybe<pair<e_commodity, Rect>> obj_under_cursor(
      Coord const& coord ) const {
    maybe<pair<e_commodity, Rect>> res;
    if( coord.is_inside( bounds() ) ) {
      auto boxes =
          bounds().with_new_upper_left( Coord{} ) / sprite_scale;
      auto maybe_type =
          boxes
              .rasterize( coord.with_new_origin(
                              bounds().upper_left() ) /
                          sprite_scale )
              .bind( L( commodity_from_index( _ ) ) );
      if( maybe_type ) {
        auto box_origin =
            coord.with_new_origin( bounds().upper_left() )
                .rounded_to_multiple_to_minus_inf( sprite_scale )
                .as_if_origin_were( bounds().upper_left() ) +
            k_rendered_commodity_offset;
        auto box = Rect::from( box_origin,
                               Delta{ 1_w, 1_h } * Scale{ 16 } );

        res = pair{ *maybe_type, box };
      }
    }
    return res;
  }

  bool  doubled_{};
  Coord origin_{};

private:
  MarketCommodities() = default;
  MarketCommodities( bool doubled, Coord origin )
    : doubled_( doubled ), origin_( origin ) {}
};
NOTHROW_MOVE( MarketCommodities );

class ActiveCargoBox {
  static constexpr Delta size_blocks{ 6_w, 1_h };

public:
  // Commodities will be 24x24.
  static constexpr auto  box_scale   = Scale{ 32 };
  static constexpr Delta size_pixels = size_blocks * box_scale;

  Rect bounds() const {
    return Rect::from( origin_, size_pixels );
  }

  void draw( Texture& tx, Delta offset ) const {
    auto bds  = bounds();
    auto grid = bds.to_grid_noalign( box_scale );
    for( auto rect : range_of_rects( grid ) )
      render_rect( tx, Color::white(),
                   rect.shifted_by( offset ) );
  }

  ActiveCargoBox( ActiveCargoBox&& ) = default;
  ActiveCargoBox& operator=( ActiveCargoBox&& ) = default;

  static maybe<ActiveCargoBox> create(
      Delta const& size, maybe<MarketCommodities> const&
                             maybe_market_commodities ) {
    maybe<ActiveCargoBox> res;
    auto                  rect = Rect::from( Coord{}, size );
    if( size.w < size_pixels.w || size.h < size_pixels.h )
      return res;
    if( maybe_market_commodities.has_value() ) {
      auto const& market_commodities = *maybe_market_commodities;
      if( market_commodities.origin_.y < 0_y + size_pixels.h )
        return res;
      if( market_commodities.doubled_ ) {
        res = ActiveCargoBox(
            /*origin_=*/Coord{
                market_commodities.origin_.y - size_pixels.h +
                    1_h,
                rect.center().x - size_pixels.w / 2_sx } );
      } else {
        // Possibly just for now do this.
        res = ActiveCargoBox(
            /*origin_=*/Coord{
                market_commodities.origin_.y - size_pixels.h +
                    1_h,
                rect.center().x - size_pixels.w / 2_sx } );
      }
    }
    return res;
  }

private:
  ActiveCargoBox() = default;
  ActiveCargoBox( Coord origin ) : origin_( origin ) {}
  Coord origin_{};
};
NOTHROW_MOVE( ActiveCargoBox );

class DockAnchor {
  static constexpr Delta cross_leg_size{ 5_w, 5_h };
  static constexpr H     above_active_cargo_box{ 32_h };

public:
  Rect bounds() const {
    // Just a point.
    return Rect::from( location_, Delta{} );
  }

  void draw( Texture& tx, Delta offset ) const {
    // This mess just draws an X.
    render_line(
        tx, Color::white(), location_ - cross_leg_size + offset,
        cross_leg_size * Scale{ 2 } + Delta{ 1_w, 1_h } );
    render_line(
        tx, Color::white(),
        location_ - cross_leg_size.mirrored_vertically() +
            offset,
        cross_leg_size.mirrored_vertically() * Scale{ 2 } +
            Delta{ 1_w, -1_h } );
  }

  DockAnchor( DockAnchor&& ) = default;
  DockAnchor& operator=( DockAnchor&& ) = default;

  static maybe<DockAnchor> create(
      Delta const&                 size,
      maybe<ActiveCargoBox> const& maybe_active_cargo_box,
      maybe<MarketCommodities> const&
          maybe_market_commodities ) {
    maybe<DockAnchor> res;
    if( maybe_active_cargo_box && maybe_market_commodities ) {
      auto active_cargo_box_top =
          maybe_active_cargo_box->bounds().top_edge();
      auto location_y =
          active_cargo_box_top - above_active_cargo_box;
      auto location_x =
          maybe_market_commodities->bounds().right_edge() - 32_w;
      auto x_upper_bound = 0_x + size.w - 60_w;
      auto x_lower_bound =
          maybe_active_cargo_box->bounds().right_edge();
      if( x_upper_bound < x_lower_bound ) return res;
      location_x =
          std::clamp( location_x, x_lower_bound, x_upper_bound );
      if( location_y < 0_y ) return res;
      res              = DockAnchor{};
      res->location_.x = location_x;
      res->location_.y = location_y;
    }
    return res;
  }

private:
  DockAnchor() = default;
  DockAnchor( Coord location ) : location_( location ) {}
  Coord location_{};
};
NOTHROW_MOVE( DockAnchor );

class Backdrop {
  static constexpr Delta image_distance_from_anchor{ 950_w,
                                                     544_h };

public:
  Rect bounds() const { return Rect::from( Coord{}, size_ ); }

  void draw( Texture& tx, Delta offset ) const {
    copy_texture(
        image( e_image::europe ), tx,
        Rect::from( upper_left_of_render_rect_, size_ ),
        Coord{} + offset );
  }

  Backdrop( Backdrop&& ) = default;
  Backdrop& operator=( Backdrop&& ) = default;

  static maybe<Backdrop> create(
      Delta const&             size,
      maybe<DockAnchor> const& maybe_dock_anchor ) {
    maybe<Backdrop> res;
    if( maybe_dock_anchor ) {
      res =
          Backdrop{ -( maybe_dock_anchor->bounds().upper_left() -
                       image_distance_from_anchor ),
                    size };
    }
    return res;
  }

private:
  Backdrop() = default;
  Backdrop( Coord upper_left, Delta size )
    : upper_left_of_render_rect_( upper_left ), size_( size ) {}
  Coord upper_left_of_render_rect_{};
  Delta size_{};
};
NOTHROW_MOVE( Backdrop );

class InPortBox {
public:
  static constexpr Delta block_size{ 32_w, 32_h };
  static constexpr SY    height_blocks{ 3 };
  static constexpr SX    width_wide{ 3 };
  static constexpr SX    width_narrow{ 2 };

  Rect bounds() const {
    return Rect::from( origin_, block_size * size_in_blocks_ +
                                    Delta{ 1_w, 1_h } );
  }

  void draw( Texture& tx, Delta offset ) const {
    render_rect( tx, Color::white(),
                 bounds().shifted_by( offset ) );
    auto const& label_tx =
        render_text( "In Port", Color::white() );
    copy_texture(
        label_tx, tx,
        bounds().upper_left() + Delta{ 2_w, 2_h } + offset );
  }

  InPortBox( InPortBox&& ) = default;
  InPortBox& operator=( InPortBox&& ) = default;

  static maybe<InPortBox> create(
      Delta const&                 size,
      maybe<ActiveCargoBox> const& maybe_active_cargo_box,
      maybe<MarketCommodities> const&
          maybe_market_commodities ) {
    maybe<InPortBox> res;
    if( maybe_active_cargo_box && maybe_market_commodities ) {
      bool  is_wide = !maybe_market_commodities->doubled_;
      Scale size_in_blocks;
      size_in_blocks.sy = height_blocks;
      size_in_blocks.sx = is_wide ? width_wide : width_narrow;
      auto origin =
          maybe_active_cargo_box->bounds().upper_left() -
          block_size.h * size_in_blocks.sy;
      if( origin.y < 0_y || origin.x < 0_x ) return res;

      res = InPortBox{ origin,         //
                       size_in_blocks, //
                       is_wide };

      auto lr_delta = res->bounds().lower_right() - Coord{};
      if( lr_delta.w > size.w || lr_delta.h > size.h )
        res = nothing;
    }
    return res;
  }

  Coord origin_{};
  Scale size_in_blocks_{};
  bool  is_wide_{};

private:
  InPortBox() = default;
  InPortBox( Coord origin, Scale size_in_blocks, bool is_wide )
    : origin_( origin ),
      size_in_blocks_( size_in_blocks ),
      is_wide_( is_wide ) {}
};
NOTHROW_MOVE( InPortBox );

class InboundBox {
public:
  Rect bounds() const {
    return Rect::from( origin_,
                       InPortBox::block_size * size_in_blocks_ +
                           Delta{ 1_w, 1_h } );
  }

  void draw( Texture& tx, Delta offset ) const {
    render_rect( tx, Color::white(),
                 bounds().shifted_by( offset ) );
    auto const& label_tx =
        render_text( "Inbound", Color::white() );
    copy_texture(
        label_tx, tx,
        bounds().upper_left() + Delta{ 2_w, 2_h } + offset );
  }

  InboundBox( InboundBox&& ) = default;
  InboundBox& operator=( InboundBox&& ) = default;

  static maybe<InboundBox> create(
      Delta const&            size,
      maybe<InPortBox> const& maybe_in_port_box ) {
    maybe<InboundBox> res;
    if( maybe_in_port_box ) {
      bool  is_wide = maybe_in_port_box->is_wide_;
      Scale size_in_blocks;
      size_in_blocks.sy = InPortBox::height_blocks;
      size_in_blocks.sx = is_wide ? InPortBox::width_wide
                                  : InPortBox::width_narrow;
      auto origin = maybe_in_port_box->bounds().upper_left() -
                    InPortBox::block_size.w * size_in_blocks.sx;
      if( origin.x < 0_x ) {
        // Screen is too narrow horizontally to fit this box, so
        // we need to try to put it on top of the InPortBox.
        origin = maybe_in_port_box->bounds().upper_left() -
                 InPortBox::block_size.h * size_in_blocks.sy;
      }
      res = InboundBox{
          origin,         //
          size_in_blocks, //
          is_wide         //
      };
      auto lr_delta = res->bounds().lower_right() - Coord{};
      if( lr_delta.w > size.w || lr_delta.h > size.h )
        res = nothing;
      if( res->bounds().y < 0_y ) res = nothing;
      if( res->bounds().x < 0_x ) res = nothing;
    }
    return res;
  }

  bool is_wide() const { return is_wide_; }

private:
  InboundBox() = default;
  InboundBox( Coord origin, Scale size_in_blocks, bool is_wide )
    : origin_( origin ),
      size_in_blocks_( size_in_blocks ),
      is_wide_( is_wide ) {}
  Coord origin_{};
  Scale size_in_blocks_{};
  bool  is_wide_{};
};
NOTHROW_MOVE( InboundBox );

class OutboundBox {
public:
  Rect bounds() const {
    return Rect::from( origin_,
                       InPortBox::block_size * size_in_blocks_ +
                           Delta{ 1_w, 1_h } );
  }

  void draw( Texture& tx, Delta offset ) const {
    render_rect( tx, Color::white(),
                 bounds().shifted_by( offset ) );
    auto const& label_tx =
        render_text( "Outbound", Color::white() );
    copy_texture(
        label_tx, tx,
        bounds().upper_left() + Delta{ 2_w, 2_h } + offset );
  }

  OutboundBox( OutboundBox&& ) = default;
  OutboundBox& operator=( OutboundBox&& ) = default;

  static maybe<OutboundBox> create(
      Delta const&             size,
      maybe<InboundBox> const& maybe_inbound_box ) {
    maybe<OutboundBox> res;
    if( maybe_inbound_box ) {
      bool  is_wide = maybe_inbound_box->is_wide();
      Scale size_in_blocks;
      size_in_blocks.sy = InPortBox::height_blocks;
      size_in_blocks.sx = is_wide ? InPortBox::width_wide
                                  : InPortBox::width_narrow;
      auto origin = maybe_inbound_box->bounds().upper_left() -
                    InPortBox::block_size.w * size_in_blocks.sx;
      if( origin.x < 0_x ) {
        // Screen is too narrow horizontally to fit this box, so
        // we need to try to put it on top of the InboundBox.
        origin = maybe_inbound_box->bounds().upper_left() -
                 InPortBox::block_size.h * size_in_blocks.sy;
      }
      res = OutboundBox{
          origin,         //
          size_in_blocks, //
          is_wide         //
      };
      auto lr_delta = res->bounds().lower_right() - Coord{};
      if( lr_delta.w > size.w || lr_delta.h > size.h )
        res = nothing;
      if( res->bounds().y < 0_y ) res = nothing;
      if( res->bounds().x < 0_x ) res = nothing;
    }
    return res;
  }

  bool is_wide() const { return is_wide_; }

private:
  OutboundBox() = default;
  OutboundBox( Coord origin, Scale size_in_blocks, bool is_wide )
    : origin_( origin ),
      size_in_blocks_( size_in_blocks ),
      is_wide_( is_wide ) {}
  Coord origin_{};
  Scale size_in_blocks_{};
  bool  is_wide_{};
};
NOTHROW_MOVE( OutboundBox );

class Exit {
  static constexpr Delta exit_block_pixels{ 24_w, 24_h };

public:
  Rect bounds() const {
    return Rect::from( origin_, exit_block_pixels ) +
           Delta{ 2_w, 2_h };
  }

  void draw( Texture& tx, Delta offset ) const {
    auto bds = bounds().with_inc_size();
    bds      = bds.shifted_by( Delta{ -2_w, -2_h } );
    auto const& exit_tx =
        render_text( font::standard(), Color::red(), "Exit" );
    auto drawing_origin = centered( exit_tx.size(), bds );
    copy_texture( exit_tx, tx, drawing_origin + offset );
    render_rect( tx, Color::white(), bds.shifted_by( offset ) );
  }

  Exit( Exit&& ) = default;
  Exit& operator=( Exit&& ) = default;

  static maybe<Exit> create( Delta const& size,
                             maybe<MarketCommodities> const&
                                 maybe_market_commodities ) {
    maybe<Exit> res;
    if( maybe_market_commodities ) {
      auto origin =
          maybe_market_commodities->bounds().lower_right() -
          Delta{ 1_w, 1_h } - exit_block_pixels.h;
      auto lr_delta = origin + exit_block_pixels - Coord{};
      if( lr_delta.w > size.w || lr_delta.h > size.h ) {
        origin =
            maybe_market_commodities->bounds().upper_right() -
            1_w - exit_block_pixels;
      }
      res = Exit{ origin };
      lr_delta =
          ( res->bounds().lower_right() - Delta{ 1_w, 1_h } ) -
          Coord{};
      if( lr_delta.w > size.w || lr_delta.h > size.h )
        res = nothing;
      if( res->bounds().y < 0_y ) res = nothing;
      if( res->bounds().x < 0_x ) res = nothing;
    }
    return res;
  }

private:
  Exit() = default;
  Exit( Coord origin ) : origin_( origin ) {}
  Coord origin_{};
};
NOTHROW_MOVE( Exit );

class Dock {
  static constexpr Scale dock_block_pixels{ 24 };

public:
  Rect bounds() const {
    return Rect::from(
        origin_, Delta{ length_in_blocks_ * dock_block_pixels.sx,
                        1_h * dock_block_pixels.sy } );
  }

  void draw( Texture& tx, Delta offset ) const {
    auto bds  = bounds();
    auto grid = bds.to_grid_noalign( dock_block_pixels );
    for( auto rect : range_of_rects( grid ) )
      render_rect( tx, Color::white(),
                   rect.shifted_by( offset ) );
  }

  Dock( Dock&& ) = default;
  Dock& operator=( Dock&& ) = default;

  static maybe<Dock> create(
      Delta const&             size,
      maybe<DockAnchor> const& maybe_dock_anchor,
      maybe<InPortBox> const&  maybe_in_port_box ) {
    maybe<Dock> res;
    if( maybe_dock_anchor && maybe_in_port_box ) {
      auto available = maybe_dock_anchor->bounds().left_edge() -
                       maybe_in_port_box->bounds().right_edge();
      available /= dock_block_pixels.sx;
      auto origin = maybe_dock_anchor->bounds().upper_left() -
                    ( available * dock_block_pixels.sx );
      origin -= 1_h * dock_block_pixels.sy / 2;
      res = Dock{ /*origin_=*/origin,
                  /*length_in_blocks_=*/available };
      auto lr_delta =
          ( res->bounds().lower_right() - Delta{ 1_w, 1_h } ) -
          Coord{};
      if( lr_delta.w > size.w || lr_delta.h > size.h )
        res = nothing;
      if( res->bounds().y < 0_y ) res = nothing;
      if( res->bounds().x < 0_x ) res = nothing;
    }
    return res;
  }

private:
  Dock() = default;
  Dock( Coord origin, W length_in_blocks )
    : origin_( origin ), length_in_blocks_( length_in_blocks ) {}
  Coord origin_{};
  W     length_in_blocks_{};
};
NOTHROW_MOVE( Dock );

// Base class for other entities that just consist of a collec-
// tion of units.
class UnitCollection {
public:
  Rect bounds() const {
    auto Union = L2( _1.uni0n( _2 ) );
    auto to_rect =
        L( Rect::from( _.pixel_coord, g_tile_delta ) );
    auto maybe_rect =
        rl::all( units_ ).map( to_rect ).accumulate_monoid(
            Union );
    return maybe_rect.value_or( bounds_when_no_units_ );
  }

  void draw( Texture& tx, Delta offset ) const {
    // auto bds = bounds();
    // render_rect( tx, Color::white(), bds.shifted_by( offset )
    // );
    for( auto const& unit_with_pos : units_ )
      if( g_dragging_object !=
          DraggableObject_t{
              DraggableObject::unit{ unit_with_pos.id } } )
        render_unit( tx, unit_with_pos.id,
                     unit_with_pos.pixel_coord + offset,
                     /*with_icon=*/false );
    if( SG().selected_unit ) {
      for( auto [id, coord] : units_ ) {
        if( id == *SG().selected_unit ) {
          render_rect( tx, Color::green(),
                       Rect::from( coord, g_tile_delta )
                           .shifted_by( offset ) );
          break;
        }
      }
    }
  }

  maybe<pair<UnitId, Rect>> obj_under_cursor(
      Coord const& pos ) const {
    maybe<pair<UnitId, Rect>> res;
    for( auto [id, coord] : units_ ) {
      auto rect = Rect::from( coord, g_tile_delta );
      if( pos.is_inside( rect ) ) {
        res = pair{ id, rect };
        // !! don't break in case units are overlapping.
      }
    }
    return res;
  }

  UnitCollection( UnitCollection&& ) = default;
  UnitCollection& operator=( UnitCollection&& ) = default;

protected:
  struct UnitWithPosition {
    UnitId id;
    Coord  pixel_coord;
  };

  UnitCollection( Rect bounds_when_no_units,
                  vector<UnitWithPosition>&& units )
    : bounds_when_no_units_( bounds_when_no_units ),
      units_( std::move( units ) ) {}
  // Returned as rect when no units. This is actaully a point.
  Rect                     bounds_when_no_units_;
  vector<UnitWithPosition> units_;
};
NOTHROW_MOVE( UnitCollection );

class UnitsOnDock : public UnitCollection {
public:
  UnitsOnDock( UnitsOnDock&& ) = default;
  UnitsOnDock& operator=( UnitsOnDock&& ) = default;

  static maybe<UnitsOnDock> create(
      Delta const&             size,
      maybe<DockAnchor> const& maybe_dock_anchor,
      maybe<Dock> const&       maybe_dock ) {
    maybe<UnitsOnDock> res;
    if( maybe_dock_anchor && maybe_dock ) {
      vector<UnitWithPosition> units;
      Coord                    coord =
          maybe_dock->bounds().upper_right() - g_tile_delta;
      for( auto id : europort_units_on_dock() ) {
        units.push_back( { id, coord } );
        coord -= g_tile_delta.w;
        if( coord.x < maybe_dock->bounds().left_edge() )
          coord = Coord{ ( maybe_dock->bounds().upper_right() -
                           g_tile_delta )
                             .x,
                         coord.y - g_tile_delta.h };
      }
      // populate units...
      res = UnitsOnDock{
          /*bounds_when_no_units_=*/maybe_dock_anchor->bounds(),
          /*units_=*/std::move( units ) };
      auto bds = res->bounds();
      auto lr_delta =
          ( bds.lower_right() - Delta{ 1_w, 1_h } ) - Coord{};
      if( lr_delta.w > size.w || lr_delta.h > size.h )
        res = nothing;
      if( bds.y < 0_y ) res = nothing;
      if( bds.x < 0_x ) res = nothing;
    }
    return res;
  }

private:
  UnitsOnDock( Rect                       dock_anchor,
               vector<UnitWithPosition>&& units )
    : UnitCollection( dock_anchor, std::move( units ) ) {}
};
NOTHROW_MOVE( UnitsOnDock );

class ShipsInPort : public UnitCollection {
public:
  ShipsInPort( ShipsInPort&& ) = default;
  ShipsInPort& operator=( ShipsInPort&& ) = default;

  static maybe<ShipsInPort> create(
      Delta const&            size,
      maybe<InPortBox> const& maybe_in_port_box ) {
    maybe<ShipsInPort> res;
    if( maybe_in_port_box ) {
      vector<UnitWithPosition> units;
      auto  in_port_bds = maybe_in_port_box->bounds();
      Coord coord = in_port_bds.lower_right() - g_tile_delta;
      for( auto id : europort_units_in_port() ) {
        units.push_back( { id, coord } );
        coord -= g_tile_delta.w;
        if( coord.x < in_port_bds.left_edge() )
          coord = Coord{
              ( in_port_bds.upper_right() - g_tile_delta ).x,
              coord.y - g_tile_delta.h };
      }
      // populate units...
      res =
          ShipsInPort{ /*bounds_when_no_units_=*/Rect::from(
                           in_port_bds.lower_right(), Delta{} ),
                       /*units_=*/std::move( units ) };
      auto bds = res->bounds();
      auto lr_delta =
          ( bds.lower_right() - Delta{ 1_w, 1_h } ) - Coord{};
      if( lr_delta.w > size.w || lr_delta.h > size.h )
        res = nothing;
      if( bds.y < 0_y ) res = nothing;
      if( bds.x < 0_x ) res = nothing;
    }
    return res;
  }

private:
  ShipsInPort( Rect                       dock_anchor,
               vector<UnitWithPosition>&& units )
    : UnitCollection( dock_anchor, std::move( units ) ) {}
};
NOTHROW_MOVE( ShipsInPort );

class ShipsInbound : public UnitCollection {
public:
  ShipsInbound( ShipsInbound&& ) = default;
  ShipsInbound& operator=( ShipsInbound&& ) = default;

  static maybe<ShipsInbound> create(
      Delta const&             size,
      maybe<InboundBox> const& maybe_inbound_box ) {
    maybe<ShipsInbound> res;
    if( maybe_inbound_box ) {
      vector<UnitWithPosition> units;
      auto  frame_bds = maybe_inbound_box->bounds();
      Coord coord     = frame_bds.lower_right() - g_tile_delta;
      for( auto id : europort_units_inbound() ) {
        units.push_back( { id, coord } );
        coord -= g_tile_delta.w;
        if( coord.x < frame_bds.left_edge() )
          coord = Coord{
              ( frame_bds.upper_right() - g_tile_delta ).x,
              coord.y - g_tile_delta.h };
      }
      // populate units...
      res = ShipsInbound{ /*bounds_when_no_units_=*/Rect::from(
                              frame_bds.lower_right(), Delta{} ),
                          /*units_=*/std::move( units ) };
      auto bds = res->bounds();
      auto lr_delta =
          ( bds.lower_right() - Delta{ 1_w, 1_h } ) - Coord{};
      if( lr_delta.w > size.w || lr_delta.h > size.h )
        res = nothing;
      if( bds.y < 0_y ) res = nothing;
      if( bds.x < 0_x ) res = nothing;
    }
    return res;
  }

private:
  ShipsInbound( Rect                       dock_anchor,
                vector<UnitWithPosition>&& units )
    : UnitCollection( dock_anchor, std::move( units ) ) {}
};
NOTHROW_MOVE( ShipsInbound );

class ShipsOutbound : public UnitCollection {
public:
  ShipsOutbound( ShipsOutbound&& ) = default;
  ShipsOutbound& operator=( ShipsOutbound&& ) = default;

  static maybe<ShipsOutbound> create(
      Delta const&              size,
      maybe<OutboundBox> const& maybe_outbound_box ) {
    maybe<ShipsOutbound> res;
    if( maybe_outbound_box ) {
      vector<UnitWithPosition> units;
      auto  frame_bds = maybe_outbound_box->bounds();
      Coord coord     = frame_bds.lower_right() - g_tile_delta;
      for( auto id : europort_units_outbound() ) {
        units.push_back( { id, coord } );
        coord -= g_tile_delta.w;
        if( coord.x < frame_bds.left_edge() )
          coord = Coord{
              ( frame_bds.upper_right() - g_tile_delta ).x,
              coord.y - g_tile_delta.h };
      }
      // populate units...
      res =
          ShipsOutbound{ /*bounds_when_no_units_=*/Rect::from(
                             frame_bds.lower_right(), Delta{} ),
                         /*units_=*/std::move( units ) };
      auto bds = res->bounds();
      auto lr_delta =
          ( bds.lower_right() - Delta{ 1_w, 1_h } ) - Coord{};
      if( lr_delta.w > size.w || lr_delta.h > size.h )
        res = nothing;
      if( bds.y < 0_y ) res = nothing;
      if( bds.x < 0_x ) res = nothing;
    }
    return res;
  }

private:
  ShipsOutbound( Rect                       dock_anchor,
                 vector<UnitWithPosition>&& units )
    : UnitCollection( dock_anchor, std::move( units ) ) {}
};
NOTHROW_MOVE( ShipsOutbound );

class ActiveCargo {
public:
  Rect bounds() const { return bounds_; }

  void draw( Texture& tx, Delta offset ) const {
    auto bds  = bounds();
    auto grid = bds.to_grid_noalign( ActiveCargoBox::box_scale );
    if( maybe_active_unit_ ) {
      auto&       unit = unit_from_id( *maybe_active_unit_ );
      auto const& cargo_slots = unit.cargo().slots();
      for( auto const& [idx, cargo_slot, rect] :
           rl::zip( rl::ints(), cargo_slots,
                    range_of_rects( grid ) ) ) {
        if( g_dragging_object.has_value() ) {
          if_get( *g_dragging_object,
                  DraggableObject::cargo_commodity, cc ) {
            if( cc.slot._ == idx ) continue;
          }
        }
        auto dst_coord       = rect.upper_left() + offset;
        auto cargo_slot_copy = cargo_slot;
        switch( auto& v = cargo_slot_copy; v.to_enum() ) {
          case CargoSlot::e::empty: {
            break;
          }
          case CargoSlot::e::overflow: {
            break;
          }
          case CargoSlot::e::cargo: {
            auto& cargo = v.get<CargoSlot::cargo>();
            overload_visit(
                cargo.contents,
                [&]( UnitId id ) {
                  if( g_dragging_object !=
                      DraggableObject_t{
                          DraggableObject::unit{ id } } )
                    render_unit( tx, id, dst_coord,
                                 /*with_icon=*/false );
                },
                [&]( Commodity const& c ) {
                  render_commodity_annotated(
                      tx, c,
                      dst_coord + k_rendered_commodity_offset );
                } );
            break;
          }
        }
      }
      for( auto [idx, rect] :
           rl::zip( rl::ints(), range_of_rects( grid ) ) ) {
        if( idx >= unit.cargo().slots_total() )
          render_fill_rect( tx, Color::white(),
                            rect.shifted_by( offset ) );
      }
    } else {
      for( auto rect : range_of_rects( grid ) )
        render_fill_rect( tx, Color::white(),
                          rect.shifted_by( offset ) );
    }
  }

  ActiveCargo( ActiveCargo&& ) = default;
  ActiveCargo& operator=( ActiveCargo&& ) = default;

  static maybe<ActiveCargo> create(
      Delta const&                 size,
      maybe<ActiveCargoBox> const& maybe_active_cargo_box,
      maybe<ShipsInPort> const&    maybe_ships_in_port ) {
    maybe<ActiveCargo> res;
    if( maybe_active_cargo_box && maybe_ships_in_port ) {
      res = ActiveCargo{
          /*maybe_active_unit_=*/SG().selected_unit,
          /*bounds_=*/maybe_active_cargo_box->bounds() };
      // FIXME: if we are inside the active cargo box, and the
      // active cargo box exists, do we need the following
      // checks?
      auto lr_delta =
          ( res->bounds().lower_right() - Delta{ 1_w, 1_h } ) -
          Coord{};
      if( lr_delta.w > size.w || lr_delta.h > size.h )
        res = nothing;
      if( res->bounds().y < 0_y ) res = nothing;
      if( res->bounds().x < 0_x ) res = nothing;
    }
    return res;
  }

  maybe<pair<CargoSlotIndex, Rect>> obj_under_cursor(
      Coord const& coord ) const {
    maybe<pair<CargoSlotIndex, Rect>> res;
    if( maybe_active_unit_ ) {
      if( coord.is_inside( bounds_ ) ) {
        auto boxes = bounds_.with_new_upper_left( Coord{} ) /
                     ActiveCargoBox::box_scale;
        auto maybe_slot = boxes.rasterize(
            coord.with_new_origin( bounds_.upper_left() ) /
            ActiveCargoBox::box_scale );
        if( maybe_slot ) {
          auto box_origin =
              coord.with_new_origin( bounds().upper_left() )
                  .rounded_to_multiple_to_minus_inf(
                      ActiveCargoBox::box_scale )
                  .as_if_origin_were( bounds().upper_left() );
          auto scale = ActiveCargoBox::box_scale;

          using DraggableObject::cargo_commodity;
          if( draggable_in_cargo_slot( *maybe_slot )
                  .bind( L( holds<cargo_commodity>( _ ) ) ) ) {
            box_origin += k_rendered_commodity_offset;
            scale = Scale{ 16 };
          }
          auto box = Rect::from( box_origin,
                                 Delta{ 1_w, 1_h } * scale );

          res = pair{ *maybe_slot, box };
        }
      }
      auto& unit = unit_from_id( *maybe_active_unit_ );
      if( res && res->first >= unit.cargo().slots_total() )
        res = nothing;
    }
    return res;
  }

  // maybe<CargoSlot_t const&>> cargo_slot_from_coord(
  //    Coord coord ) const {
  //  // Lambda will only be called if a valid index is returned,
  //  // in which case there is guaranteed to be an active unit.
  //  return slot_idx_from_coord( coord ) //
  //         .fmap( LC( unit_from_id( *maybe_active_unit_ )
  //                         .cargo()[_] ) );
  //}

  maybe<UnitId> active_unit() const {
    return maybe_active_unit_;
  }

private:
  ActiveCargo() = default;
  ActiveCargo( maybe<UnitId> maybe_active_unit, Rect bounds )
    : maybe_active_unit_( maybe_active_unit ),
      bounds_( bounds ) {}
  maybe<UnitId> maybe_active_unit_;
  Rect          bounds_;
};
NOTHROW_MOVE( ActiveCargo );

} // namespace entity

//- Buttons
//- Message box
//- Stats area (money, tax rate, etc.)

struct Entities {
  maybe<entity::MarketCommodities> market_commodities;
  maybe<entity::ActiveCargoBox>    active_cargo_box;
  maybe<entity::DockAnchor>        dock_anchor;
  maybe<entity::Backdrop>          backdrop;
  maybe<entity::InPortBox>         in_port_box;
  maybe<entity::InboundBox>        inbound_box;
  maybe<entity::OutboundBox>       outbound_box;
  maybe<entity::Exit>              exit_label;
  maybe<entity::Dock>              dock;
  maybe<entity::UnitsOnDock>       units_on_dock;
  maybe<entity::ShipsInPort>       ships_in_port;
  maybe<entity::ShipsInbound>      ships_inbound;
  maybe<entity::ShipsOutbound>     ships_outbound;
  maybe<entity::ActiveCargo>       active_cargo;
};
NOTHROW_MOVE( Entities );

void create_entities( Entities* entities ) {
  using namespace entity;
  Delta clip                   = main_window_logical_size();
  entities->market_commodities = //
      MarketCommodities::create( clip );
  entities->active_cargo_box =      //
      ActiveCargoBox::create( clip, //
                              entities->market_commodities );
  entities->dock_anchor =                             //
      DockAnchor::create( clip,                       //
                          entities->active_cargo_box, //
                          entities->market_commodities );
  entities->backdrop =        //
      Backdrop::create( clip, //
                        entities->dock_anchor );
  entities->in_port_box =                            //
      InPortBox::create( clip,                       //
                         entities->active_cargo_box, //
                         entities->market_commodities );
  entities->inbound_box =       //
      InboundBox::create( clip, //
                          entities->in_port_box );
  entities->outbound_box =       //
      OutboundBox::create( clip, //
                           entities->inbound_box );
  entities->exit_label =  //
      Exit::create( clip, //
                    entities->market_commodities );
  entities->dock =                         //
      Dock::create( clip,                  //
                    entities->dock_anchor, //
                    entities->in_port_box );
  entities->units_on_dock =                       //
      UnitsOnDock::create( clip,                  //
                           entities->dock_anchor, //
                           entities->dock );
  entities->ships_in_port =      //
      ShipsInPort::create( clip, //
                           entities->in_port_box );
  entities->ships_inbound =       //
      ShipsInbound::create( clip, //
                            entities->inbound_box );
  entities->ships_outbound =       //
      ShipsOutbound::create( clip, //
                             entities->outbound_box );
  entities->active_cargo =                             //
      ActiveCargo::create( clip,                       //
                           entities->active_cargo_box, //
                           entities->ships_in_port );
}

void draw_entities( Texture& tx, Entities const& entities ) {
  auto offset = Delta{};
  if( entities.backdrop.has_value() )
    entities.backdrop->draw( tx, offset );
  if( entities.market_commodities.has_value() )
    entities.market_commodities->draw( tx, offset );
  if( entities.active_cargo_box.has_value() )
    entities.active_cargo_box->draw( tx, offset );
  if( entities.dock_anchor.has_value() )
    entities.dock_anchor->draw( tx, offset );
  if( entities.in_port_box.has_value() )
    entities.in_port_box->draw( tx, offset );
  if( entities.inbound_box.has_value() )
    entities.inbound_box->draw( tx, offset );
  if( entities.outbound_box.has_value() )
    entities.outbound_box->draw( tx, offset );
  if( entities.exit_label.has_value() )
    entities.exit_label->draw( tx, offset );
  if( entities.dock.has_value() )
    entities.dock->draw( tx, offset );
  if( entities.units_on_dock.has_value() )
    entities.units_on_dock->draw( tx, offset );
  if( entities.ships_in_port.has_value() )
    entities.ships_in_port->draw( tx, offset );
  if( entities.ships_inbound.has_value() )
    entities.ships_inbound->draw( tx, offset );
  if( entities.ships_outbound.has_value() )
    entities.ships_outbound->draw( tx, offset );
  if( entities.active_cargo.has_value() )
    entities.active_cargo->draw( tx, offset );
}

/****************************************************************
** Drag & Drop
*****************************************************************/
class EuroViewDragAndDrop
  : public DragAndDrop<EuroViewDragAndDrop, DraggableObject_t,
                       DragSrc_t, DragDst_t, DragArc_t> {
public:
  EuroViewDragAndDrop(
      Entities const* entities,
      function<void( e_commodity, std::string const& )>
          ask_for_quantity )
    : entities_( entities ),
      stored_arc_{},
      ask_for_quantity_( std::move( ask_for_quantity ) ) {
    CHECK( entities );
  }

  DraggableObject_t draggable_from_src(
      DragSrc_t const& drag_src ) const {
    switch( drag_src.to_enum() ) {
      case DragSrc::e::dock: {
        auto& [id] = drag_src.get<DragSrc::dock>();
        return DraggableObject::unit{ id };
      }
      case DragSrc::e::cargo: {
        auto& val = drag_src.get<DragSrc::cargo>();
        // Not all cargo slots must have an item in them, but in
        // this case the slot should otherwise the DragSrc object
        // should never have been created.
        UNWRAP_CHECK( object,
                      draggable_in_cargo_slot( val.slot ) );
        return object;
      }
      case DragSrc::e::outbound: {
        auto& [id] = drag_src.get<DragSrc::outbound>();
        return DraggableObject::unit{ id };
      }
      case DragSrc::e::inbound: {
        auto& [id] = drag_src.get<DragSrc::inbound>();
        return DraggableObject::unit{ id };
      }
      case DragSrc::e::inport: {
        auto& [id] = drag_src.get<DragSrc::inport>();
        return DraggableObject::unit{ id };
      }
      case DragSrc::e::market: {
        auto& val = drag_src.get<DragSrc::market>();
        return DraggableObject::market_commodity{ val.type };
      }
    }
    UNREACHABLE_LOCATION;
  }

  constexpr static auto const draw_dragged_item =
      L( draw_draggable_object( _ ) );

  maybe<DragSrcInfo> drag_src( Coord const& coord ) const {
    using namespace entity;
    maybe<DragSrcInfo> res;
    if( entities_->units_on_dock.has_value() ) {
      if( auto maybe_pair =
              entities_->units_on_dock->obj_under_cursor(
                  coord );
          maybe_pair ) {
        auto const& [id, rect] = *maybe_pair;
        res = DragSrcInfo{ /*src=*/DragSrc::dock{ /*id=*/id },
                           /*rect=*/rect };
      }
    }
    if( entities_->active_cargo.has_value() ) {
      auto const& active_cargo = *entities_->active_cargo;
      auto        in_port      = active_cargo.active_unit()
                         .fmap( is_unit_in_port )
                         .is_value_truish();
      auto maybe_pair = base::just( coord ).bind(
          LC( active_cargo.obj_under_cursor( _ ) ) );
      if( in_port &&
          maybe_pair.fmap( L( _.first ) )
              .bind( L( draggable_in_cargo_slot( _ ) ) ) ) {
        auto const& [slot, rect] = *maybe_pair;

        res = DragSrcInfo{
            /*src=*/DragSrc::cargo{ /*slot=*/slot,
                                    /*quantity=*/nothing },
            /*rect=*/rect };
      }
    }
    if( entities_->ships_outbound.has_value() ) {
      if( auto maybe_pair =
              entities_->ships_outbound->obj_under_cursor(
                  coord );
          maybe_pair ) {
        auto const& [id, rect] = *maybe_pair;

        res =
            DragSrcInfo{ /*src=*/DragSrc::outbound{ /*id=*/id },
                         /*rect=*/rect };
      }
    }
    if( entities_->ships_inbound.has_value() ) {
      if( auto maybe_pair =
              entities_->ships_inbound->obj_under_cursor(
                  coord );
          maybe_pair ) {
        auto const& [id, rect] = *maybe_pair;

        res = DragSrcInfo{ /*src=*/DragSrc::inbound{ /*id=*/id },
                           /*rect=*/rect };
      }
    }
    if( entities_->ships_in_port.has_value() ) {
      if( auto maybe_pair =
              entities_->ships_in_port->obj_under_cursor(
                  coord );
          maybe_pair ) {
        auto const& [id, rect] = *maybe_pair;

        res = DragSrcInfo{ /*src=*/DragSrc::inport{ /*id=*/id },
                           /*rect=*/rect };
      }
    }
    if( entities_->market_commodities.has_value() ) {
      if( auto maybe_pair =
              entities_->market_commodities->obj_under_cursor(
                  coord );
          maybe_pair ) {
        auto const& [type, rect] = *maybe_pair;

        res = DragSrcInfo{
            /*src=*/DragSrc::market{ /*type=*/type,
                                     /*quantity=*/nothing },
            /*rect=*/rect };
      }
    }

    return res;
  }

  // Important: in this function we should not return early; we
  // should check all the entities (in order) to allow later ones
  // to override earlier ones.
  maybe<DragDst_t> drag_dst( Coord const& coord ) const {
    using namespace entity;
    maybe<DragDst_t> res;
    if( entities_->active_cargo.has_value() ) {
      if( auto maybe_pair =
              entities_->active_cargo->obj_under_cursor( coord );
          maybe_pair ) {
        auto const& slot = maybe_pair->first;

        res = DragDst::cargo{
            /*slot=*/slot //
        };
      }
    }
    if( entities_->dock.has_value() ) {
      if( coord.is_inside( entities_->dock->bounds() ) )
        res = DragDst::dock{};
    }
    if( entities_->units_on_dock.has_value() ) {
      if( coord.is_inside( entities_->units_on_dock->bounds() ) )
        res = DragDst::dock{};
    }
    if( entities_->outbound_box.has_value() ) {
      if( coord.is_inside( entities_->outbound_box->bounds() ) )
        res = DragDst::outbound{};
    }
    if( entities_->inbound_box.has_value() ) {
      if( coord.is_inside( entities_->inbound_box->bounds() ) )
        res = DragDst::inbound{};
    }
    if( entities_->in_port_box.has_value() ) {
      if( coord.is_inside( entities_->in_port_box->bounds() ) )
        res = DragDst::inport{};
    }
    if( entities_->ships_in_port.has_value() ) {
      if( auto maybe_pair =
              entities_->ships_in_port->obj_under_cursor(
                  coord );
          maybe_pair ) {
        auto const& ship = maybe_pair->first;

        res = DragDst::inport_ship{
            /*id=*/ship, //
        };
      }
    }
    if( entities_->market_commodities.has_value() ) {
      if( coord.is_inside(
              entities_->market_commodities->bounds() ) )
        return DragDst::market{};
    }
    return res;
  }

  maybe<UnitId> active_cargo_ship() const {
    return entities_->active_cargo.bind(
        &entity::ActiveCargo::active_unit );
  }

  bool can_perform_drag( DragArc_t const& drag_arc ) const {
    switch( auto& v = drag_arc; drag_arc.to_enum() ) {
      case DragArc::e::dock_to_cargo: {
        auto& [src, dst] = v.get<DragArc::dock_to_cargo>();
        UNWRAP_CHECK( ship, active_cargo_ship() );
        if( !is_unit_in_port( ship ) ) return false;
        return unit_from_id( ship ).cargo().fits_somewhere(
            src.id, dst.slot._ );
      }
      case DragArc::e::cargo_to_dock: {
        auto& val = v.get<DragArc::cargo_to_dock>();
        return holds<DraggableObject::unit>(
                   draggable_from_src( val.src ) )
            .has_value();
      }
      case DragArc::e::cargo_to_cargo: {
        auto& c_to_c     = v.get<DragArc::cargo_to_cargo>();
        auto& [src, dst] = c_to_c;
        UNWRAP_CHECK( ship, active_cargo_ship() );
        if( !is_unit_in_port( ship ) ) return false;
        if( src.slot == dst.slot ) return true;
        UNWRAP_CHECK( cargo_object,
                      draggable_to_cargo_object(
                          draggable_from_src( src ) ) );
        return overload_visit(
            cargo_object,
            [&]( UnitId ) {
              return unit_from_id( ship )
                  .cargo()
                  .fits_with_item_removed(
                      /*cargo=*/cargo_object,          //
                      /*remove_slot=*/c_to_c.src.slot, //
                      /*insert_slot=*/c_to_c.dst.slot  //
                  );
            },
            [&]( Commodity const& c ) {
              // If at least one quantity of the commodity can be
              // moved then we will allow (at least a partial
              // transfer) to proceed.
              auto size_one     = c;
              size_one.quantity = 1;
              return unit_from_id( ship ).cargo().fits(
                  /*cargo=*/size_one,
                  /*slot=*/c_to_c.dst.slot );
            } );
      }
      case DragArc::e::outbound_to_inbound: return true;
      case DragArc::e::outbound_to_inport: {
        auto& val = v.get<DragArc::outbound_to_inport>();
        UNWRAP_CHECK( info,
                      unit_euro_port_view_info( val.src.id ) );
        ASSIGN_CHECK_V( outbound, info,
                        UnitEuroPortViewState::outbound );
        return outbound.percent == 0.0;
      }
      case DragArc::e::inbound_to_outbound: return true;
      case DragArc::e::inport_to_outbound: return true;
      case DragArc::e::dock_to_inport_ship: {
        auto& [src, dst] = v.get<DragArc::dock_to_inport_ship>();
        return unit_from_id( dst.id ).cargo().fits_somewhere(
            src.id );
      }
      case DragArc::e::cargo_to_inport_ship: {
        auto& [src, dst] =
            v.get<DragArc::cargo_to_inport_ship>();
        auto dst_ship = dst.id;
        UNWRAP_CHECK( cargo_object,
                      draggable_to_cargo_object(
                          draggable_from_src( src ) ) );
        return overload_visit(
            cargo_object,
            [&]( UnitId id ) {
              if( is_unit_onboard( id ) == dst_ship )
                return false;
              return unit_from_id( dst_ship )
                  .cargo()
                  .fits_somewhere( id );
            },
            [&]( Commodity const& c ) {
              // If even 1 quantity can fit then we can proceed
              // with (at least) a partial transfer.
              auto size_one     = c;
              size_one.quantity = 1;
              return unit_from_id( dst_ship )
                  .cargo()
                  .fits_somewhere( size_one );
            } );
      }
      case DragArc::e::market_to_cargo: {
        auto& [src, dst] = v.get<DragArc::market_to_cargo>();
        UNWRAP_CHECK( ship, active_cargo_ship() );
        if( !is_unit_in_port( ship ) ) return false;
        auto comm = Commodity{
            /*type=*/src.type, //
            // If the commodity can fit even with just one quan-
            // tity then it is allowed, since we will just insert
            // as much as possible if we can't insert 100.
            /*quantity=*/1 //
        };
        return unit_from_id( ship ).cargo().fits_somewhere(
            comm, dst.slot._ );
      }
      case DragArc::e::market_to_inport_ship: {
        auto& [src, dst] =
            v.get<DragArc::market_to_inport_ship>();
        auto comm = Commodity{
            /*type=*/src.type, //
            // If the commodity can fit even with just one quan-
            // tity then it is allowed, since we will just insert
            // as much as possible if we can't insert 100.
            /*quantity=*/1 //
        };
        return unit_from_id( dst.id ).cargo().fits_somewhere(
            comm, /*starting_slot=*/0 );
      }
      case DragArc::e::cargo_to_market: {
        auto& val = v.get<DragArc::cargo_to_market>();
        UNWRAP_CHECK( ship, active_cargo_ship() );
        if( !is_unit_in_port( ship ) ) return false;
        return unit_from_id( ship )
            .cargo()
            .template slot_holds_cargo_type<Commodity>(
                val.src.slot._ )
            .has_value();
      }
    }
    UNREACHABLE_LOCATION;
  }

  void finalize_drag( input::mod_keys const& mod,
                      DragArc_t const&       drag_arc ) {
    CHECK( !stored_arc_.has_value() );
    if( !mod.shf_down ) {
      accept_finalized_drag( drag_arc );
      return;
    }
    // Shift is down.
    switch( auto& v = drag_arc; v.to_enum() ) {
      case DragArc::e::market_to_cargo: {
        auto& val = v.get<DragArc::market_to_cargo>();
        ask_for_quantity_( val.src.type, "buy" );
        stored_arc_ = drag_arc;
        break;
      }
      case DragArc::e::market_to_inport_ship: {
        auto& val = v.get<DragArc::market_to_inport_ship>();
        ask_for_quantity_( val.src.type, "buy" );
        stored_arc_ = drag_arc;
        break;
      }
      case DragArc::e::cargo_to_market: {
        auto& val = v.get<DragArc::cargo_to_market>();
        UNWRAP_CHECK( ship, active_cargo_ship() );
        CHECK( is_unit_in_port( ship ) );
        UNWRAP_CHECK(
            commodity_ref,
            unit_from_id( ship )
                .cargo()
                .template slot_holds_cargo_type<Commodity>(
                    val.src.slot._ ) );
        ask_for_quantity_( commodity_ref.type, "sell" );
        stored_arc_ = drag_arc;
        break;
      }
      case DragArc::e::cargo_to_inport_ship: {
        auto& val = v.get<DragArc::cargo_to_inport_ship>();
        UNWRAP_CHECK( ship, active_cargo_ship() );
        CHECK( is_unit_in_port( ship ) );
        auto maybe_commodity_ref =
            unit_from_id( ship )
                .cargo()
                .template slot_holds_cargo_type<Commodity>(
                    val.src.slot._ );
        if( !maybe_commodity_ref.has_value() ) {
          // It's a unit.
          accept_finalized_drag( drag_arc );
        } else {
          ask_for_quantity_( maybe_commodity_ref->type, "move" );
          stored_arc_ = drag_arc;
        }
        break;
      }
      default:
        accept_finalized_drag( drag_arc ); //
        break;
    }
  }

  void receive_quantity( int quantity ) {
    CHECK( stored_arc_.has_value() );
    SCOPE_EXIT( stored_arc_ = nothing );
    if( quantity == 0 ) {
      // The drag has been cancelled.
      accept_finalized_drag( nothing );
      return;
    }
    auto set_it = [this, quantity]( auto& val ) {
      auto new_val         = val;
      new_val.src.quantity = quantity;
      DragArc_t new_arc    = DragArc_t{ new_val };
      accept_finalized_drag( new_arc );
    };
    switch( auto& v = *stored_arc_; v.to_enum() ) {
      case DragArc::e::market_to_cargo: {
        auto& val = v.get<DragArc::market_to_cargo>();
        set_it( val );
        break;
      }
      case DragArc::e::market_to_inport_ship: {
        auto& val = v.get<DragArc::market_to_inport_ship>();
        set_it( val );
        break;
      }
      case DragArc::e::cargo_to_market: {
        auto& val = v.get<DragArc::cargo_to_market>();
        set_it( val );
        break;
      }
      case DragArc::e::cargo_to_inport_ship: {
        auto& val = v.get<DragArc::cargo_to_inport_ship>();
        set_it( val );
        break;
      }
      default:
        FATAL( "need to receive quantity for drag arc type {}",
               *stored_arc_ );
        break;
    }
  }

  void perform_drag( DragArc_t const& drag_arc ) const {
    if( !can_perform_drag( drag_arc ) ) {
      lg.error(
          "Drag was marked as correct, but can_perform_drag "
          "returned false for arc: {}",
          drag_arc );
      DCHECK( false );
      return;
    }
    lg.debug( "performing drag: {}", drag_arc );
    // Beyond this point it is assumed that this drag is compat-
    // ible with game rules.
    switch( auto& v = drag_arc; v.to_enum() ) {
      case DragArc::e::dock_to_cargo: {
        auto& [src, dst] = v.get<DragArc::dock_to_cargo>();
        UNWRAP_CHECK( ship, active_cargo_ship() );
        // First try to respect the destination slot chosen by
        // the player,
        if( unit_from_id( ship ).cargo().fits( src.id,
                                               dst.slot._ ) )
          ustate_change_to_cargo( ship, src.id, dst.slot._ );
        else
          ustate_change_to_cargo( ship, src.id );
        break;
      }
      case DragArc::e::cargo_to_dock: {
        auto& val = v.get<DragArc::cargo_to_dock>();
        ASSIGN_CHECK_V( unit, draggable_from_src( val.src ),
                        DraggableObject::unit );
        unit_move_to_europort_dock( unit.id );
        break;
      }
      case DragArc::e::cargo_to_cargo: {
        auto& c_to_c = v.get<DragArc::cargo_to_cargo>();
        UNWRAP_CHECK( ship, active_cargo_ship() );
        UNWRAP_CHECK( cargo_object,
                      draggable_to_cargo_object(
                          draggable_from_src( c_to_c.src ) ) );
        overload_visit(
            cargo_object,
            [&]( UnitId id ) {
              // Will first "disown" unit which will remove it
              // from the cargo.
              ustate_change_to_cargo( ship, id,
                                      c_to_c.dst.slot._ );
            },
            [&]( Commodity const& ) {
              move_commodity_as_much_as_possible(
                  ship, c_to_c.src.slot._, ship,
                  c_to_c.dst.slot._,
                  /*max_quantity=*/nothing,
                  /*try_other_dst_slots=*/false );
            } );
        break;
      }
      case DragArc::e::outbound_to_inbound: {
        auto& val = v.get<DragArc::outbound_to_inbound>();
        unit_sail_to_old_world( val.src.id );
        break;
      }
      case DragArc::e::outbound_to_inport: {
        auto& val = v.get<DragArc::outbound_to_inport>();
        unit_sail_to_old_world( val.src.id );
        break;
      }
      case DragArc::e::inbound_to_outbound: {
        auto& val = v.get<DragArc::inbound_to_outbound>();
        unit_sail_to_new_world( val.src.id );
        break;
      }
      case DragArc::e::inport_to_outbound: {
        auto& val = v.get<DragArc::inport_to_outbound>();
        unit_sail_to_new_world( val.src.id );
        break;
      }
      case DragArc::e::dock_to_inport_ship: {
        auto& [src, dst] = v.get<DragArc::dock_to_inport_ship>();
        ustate_change_to_cargo( dst.id, src.id );
        break;
      }
      case DragArc::e::cargo_to_inport_ship: {
        auto& c_to_i_s = v.get<DragArc::cargo_to_inport_ship>();
        UNWRAP_CHECK( cargo_object,
                      draggable_to_cargo_object(
                          draggable_from_src( c_to_i_s.src ) ) );
        overload_visit(
            cargo_object,
            [&]( UnitId id ) {
              CHECK( !c_to_i_s.src.quantity.has_value() );
              // Will first "disown" unit which will remove it
              // from the cargo.
              ustate_change_to_cargo( c_to_i_s.dst.id, id );
            },
            [&]( Commodity const& ) {
              UNWRAP_CHECK( src_ship, active_cargo_ship() );
              move_commodity_as_much_as_possible(
                  src_ship, c_to_i_s.src.slot._,
                  /*dst_ship=*/c_to_i_s.dst.id,
                  /*dst_slot=*/0,
                  /*max_quantity=*/c_to_i_s.src.quantity,
                  /*try_other_dst_slots=*/true );
            } );
        break;
      }
      case DragArc::e::market_to_cargo: {
        auto& [src, dst] = v.get<DragArc::market_to_cargo>();
        UNWRAP_CHECK( ship, active_cargo_ship() );
        auto comm = Commodity{
            /*type=*/src.type, //
            /*quantity=*/src.quantity.value_or(
                k_default_market_quantity ) //
        };
        comm.quantity = std::min(
            comm.quantity,
            unit_from_id( ship )
                .cargo()
                .max_commodity_quantity_that_fits( src.type ) );
        // Cap it.
        comm.quantity =
            std::min( comm.quantity, k_default_market_quantity );
        CHECK( comm.quantity > 0 );
        add_commodity_to_cargo( comm, ship,
                                /*slot=*/dst.slot._,
                                /*try_other_slots=*/true );
        break;
      }
      case DragArc::e::market_to_inport_ship: {
        auto& [src, dst] =
            v.get<DragArc::market_to_inport_ship>();
        auto comm = Commodity{
            /*type=*/src.type, //
            /*quantity=*/src.quantity.value_or(
                k_default_market_quantity ) //
        };
        comm.quantity = std::min(
            comm.quantity,
            unit_from_id( dst.id )
                .cargo()
                .max_commodity_quantity_that_fits( src.type ) );
        // Cap it.
        comm.quantity =
            std::min( comm.quantity, k_default_market_quantity );
        CHECK( comm.quantity > 0 );
        add_commodity_to_cargo( comm, dst.id, /*slot=*/0,
                                /*try_other_slots=*/true );
        break;
      }
      case DragArc::e::cargo_to_market: {
        auto& val = v.get<DragArc::cargo_to_market>();
        UNWRAP_CHECK( ship, active_cargo_ship() );
        UNWRAP_CHECK(
            commodity_ref,
            unit_from_id( ship )
                .cargo()
                .template slot_holds_cargo_type<Commodity>(
                    val.src.slot._ ) );
        auto quantity_wants_to_sell =
            val.src.quantity.value_or( commodity_ref.quantity );
        int amount_to_sell = std::min( quantity_wants_to_sell,
                                       commodity_ref.quantity );
        Commodity new_comm = commodity_ref;
        new_comm.quantity -= amount_to_sell;
        rm_commodity_from_cargo( ship, val.src.slot._ );
        if( new_comm.quantity > 0 )
          add_commodity_to_cargo( new_comm, ship,
                                  /*slot=*/val.src.slot._,
                                  /*try_other_slots=*/false );
        break;
      }
    }
  }

  // This class cannot change the entities, but note that the en-
  // tities will be changed on each frame.
  Entities const*  entities_;
  maybe<DragArc_t> stored_arc_;
  function<void( e_commodity, std::string const& )>
      ask_for_quantity_;
};
NOTHROW_MOVE( EuroViewDragAndDrop );

/****************************************************************
** The Europe View State Machine
*****************************************************************/
// clang-format off
fsm_transitions( Euroview,
  ((normal, none ),  ->  ,normal),
);
// clang-format on

fsm_class( Euroview ) {
  fsm_init( EuroviewState::normal{} ); //
};

FSM_DEFINE_FORMAT_RN_( Euroview );

// Will be called repeatedly until no more events added to fsm.
void advance_euroview_state( EuroviewFsm& fsm ) {
  switch( auto& v = fsm.mutable_state(); v.to_enum() ) {
    case EuroviewState::e::normal: //
      break;
    case EuroviewState::e::future: {
      auto& [s_future] = v.get<EuroviewState::future>();
      advance_fsm_ui_state( &fsm, &s_future );
      break;
    }
  }
}

/****************************************************************
** The Europe Plane
*****************************************************************/
struct EuropePlane : public Plane {
  EuropePlane() = default;
  bool covers_screen() const override { return true; }

  void advance_state() override {
    fsm_auto_advance( fsm_, "euroview",
                      { advance_euroview_state } );
    drag_n_drop_.advance_state();
    g_dragging_object = drag_n_drop_.obj_being_dragged();
    // Should be last.
    create_entities( &entities_ );
  }

  void draw( Texture& tx ) const override {
    clear_texture_transparent( tx );
    draw_entities( tx, entities_ );
    // Should be last.
    drag_n_drop_.handle_draw( tx );
  }

  e_input_handled input( input::event_t const& event ) override {
    if( drag_n_drop_.handle_input( event ) )
      return e_input_handled::yes;
    switch( event.to_enum() ) {
      case input::e_input_event::unknown_event:
        return e_input_handled::no;
      case input::e_input_event::quit_event:
        return e_input_handled::no;
      case input::e_input_event::win_event:
        // Note: we don't have to handle the window-resize event
        // here because currently the europort-plane completely
        // re-composites and re-draws itself every frame ac-
        // cording to the current window size.
        //
        // Generally we should return no here because this is an
        // event that we want all planes to see.
        return e_input_handled::no;
      case input::e_input_event::key_event:
        return e_input_handled::no;
      case input::e_input_event::mouse_wheel_event:
        return e_input_handled::no;
      case input::e_input_event::mouse_move_event:
        return e_input_handled::yes;
      case input::e_input_event::mouse_button_event: {
        auto& val = event.get<input::mouse_button_event_t>();
        if( val.buttons !=
            input::e_mouse_button_event::left_down )
          return e_input_handled::yes;

        // Exit button.
        if( entities_.exit_label.has_value() ) {
          if( val.pos.is_inside(
                  entities_.exit_label->bounds() ) ) {
            pop_plane_config();
            return e_input_handled::yes;
          }
        }

        // Unit selection.
        auto handled         = e_input_handled::no;
        auto try_select_unit = [&]( auto const& maybe_entity ) {
          if( maybe_entity ) {
            if( auto maybe_pair =
                    maybe_entity->obj_under_cursor( val.pos );
                maybe_pair ) {
              SG().selected_unit = maybe_pair->first;
              handled            = e_input_handled::yes;
            }
          }
        };
        try_select_unit( entities_.ships_in_port );
        try_select_unit( entities_.ships_inbound );
        try_select_unit( entities_.ships_outbound );
        return handled;
      }
      case input::e_input_event::mouse_drag_event:
        return e_input_handled::no;
    }
    UNREACHABLE_LOCATION;
  }

  Plane::DragInfo can_drag( input::e_mouse_button button,
                            Coord origin ) override {
    // Should be last.
    if( button == input::e_mouse_button::l )
      return drag_n_drop_.handle_can_drag( origin );
    return e_accept_drag::no;
  }

  void on_drag( input::mod_keys const& mod,
                input::e_mouse_button /*button*/,
                Coord /*origin*/, Coord /*prev*/,
                Coord current ) override {
    CHECK( fsm_.holds<EuroviewState::normal>() );
    if( drag_n_drop_.is_drag_in_progress() )
      drag_n_drop_.handle_on_drag( mod, current );
  }

  void on_drag_finished( input::mod_keys const& mod,
                         input::e_mouse_button /*button*/,
                         Coord origin, Coord end ) override {
    CHECK( fsm_.holds<EuroviewState::normal>() );
    if( drag_n_drop_.handle_on_drag_finished( mod, origin,
                                              end ) )
      return;
  }

  // ------------------------------------------------------------
  // Callbacks
  // ------------------------------------------------------------
  void ask_for_quantity( e_commodity type, string const& verb ) {
    CHECK( fsm_.holds<EuroviewState::normal>() );
    auto text = fmt::format(
        "What quantity of @[H]{}@[] would you like to "
        "{}? (0-100):",
        commodity_display_name( type ), verb );

    sync_future<> s_future =
        ui::int_input_box(
            /*title=*/"Choose Quantity",
            /*msg=*/text,
            /*min=*/0,
            /*max=*/100 )
            .consume( [this]( maybe<int> result ) {
              lg.debug( "received quantity: {}", result );
              this->drag_n_drop_.receive_quantity(
                  result.value_or( 0 ) );
            } );

    fsm_.push( EuroviewState::future{ s_future } );
  }

  // ------------------------------------------------------------
  // Members
  // ------------------------------------------------------------
  Entities            entities_{};
  EuroviewFsm         fsm_;
  EuroViewDragAndDrop drag_n_drop_{
      &entities_,                            //
      [this] λ( ask_for_quantity( _1, _2 ) ) //
  };
};

EuropePlane g_europe_plane;

/****************************************************************
** Initialization / Cleanup
*****************************************************************/
void init_europort_view() {}

void cleanup_europort_view() {}

REGISTER_INIT_ROUTINE( europort_view );

} // namespace

/****************************************************************
** Public API
*****************************************************************/
Plane* europe_plane() { return &g_europe_plane; }

/****************************************************************
** Menu Handlers
*****************************************************************/

MENU_ITEM_HANDLER(
    europort_view,
    [] { push_plane_config( e_plane_config::europe ); },
    [] { return !is_plane_enabled( e_plane::europe ); } )

MENU_ITEM_HANDLER(
    europort_close, [] { pop_plane_config(); },
    [] { return is_plane_enabled( e_plane::europe ); } )

} // namespace rn
