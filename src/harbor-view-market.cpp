/****************************************************************
**harbor-view-market.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2022-09-08.
*
* Description: Market commodities UI element within the harbor
*              view.
*
*****************************************************************/
#include "harbor-view-market.hpp"

// Revolution Now
#include "commodity.hpp"
#include "market.hpp"
#include "tiles.hpp"

using namespace std;

namespace rn {

namespace {} // namespace

/****************************************************************
** HarborMarketCommodities
*****************************************************************/
Delta HarborMarketCommodities::delta() const {
  int x_boxes = 16;
  int y_boxes = 1;
  if( stacked_ ) {
    x_boxes = 8;
    y_boxes = 2;
  }
  // +1 in each dimension for the border.
  return Delta{ .w = 32 * x_boxes + 1, .h = 32 * y_boxes + 1 };
}

maybe<int> HarborMarketCommodities::entity() const {
  return static_cast<int>( e_harbor_view_entity::market );
}

ui::View& HarborMarketCommodities::view() noexcept {
  return *this;
}

ui::View const& HarborMarketCommodities::view() const noexcept {
  return *this;
}

maybe<DraggableObjectWithBounds>
HarborMarketCommodities::object_here(
    Coord const& where ) const {
  maybe<pair<e_commodity, Rect>> res;

  Rect const r     = rect( Coord{} );
  Rect const boxes = r / g_tile_delta;
  UNWRAP_CHECK( comm_idx,
                boxes.rasterize( where / g_tile_delta ) );
  UNWRAP_CHECK( comm_type, commodity_from_index( comm_idx ) );
  Coord const box_origin =
      where.rounded_to_multiple_to_minus_inf( g_tile_delta ) +
      kCommodityInCargoHoldRenderingOffset;
  Rect const box =
      Rect::from( box_origin, Delta{ .w = 1, .h = 1 } *
                                  Delta{ .w = 16, .h = 16 } );
  return DraggableObjectWithBounds{
      .obj =
          HarborDraggableObject::market_commodity{
              .comm = Commodity{ .type     = comm_type,
                                 .quantity = 100 } },
      .bounds = box };
}

void HarborMarketCommodities::draw( rr::Renderer& renderer,
                                    Coord         coord ) const {
  rr::Painter painter = renderer.painter();
  auto        bds     = rect( coord );
  // Our delta for this view has one extra pixel added to the
  // width and height to allow for the border, and so we need to
  // remove that otherwise the to-grid method below will create
  // too many grid boxes.
  --bds.w;
  --bds.h;
  auto grid    = bds.to_grid_noalign( g_tile_delta );
  auto comm_it = refl::enum_values<e_commodity>.begin();
  auto label   = CommodityLabel::buy_sell{};
  for( Rect const rect : grid ) {
    CHECK( comm_it != refl::enum_values<e_commodity>.end() );
    static gfx::pixel const bg_color =
        gfx::pixel{ .r = 0x90, .g = 0x90, .b = 0xc0, .a = 0xff };
    painter.draw_solid_rect( rect, bg_color );
    painter.draw_empty_rect( rect,
                             rr::Painter::e_border_mode::in_out,
                             gfx::pixel::white() );
    CommodityPrice price = market_price( player_, *comm_it );
    label.bid            = price.bid;
    label.ask            = price.ask;
    render_commodity_annotated(
        renderer,
        rect.upper_left() + kCommodityInCargoHoldRenderingOffset,
        *comm_it, label );
    ++comm_it;
  }
  CHECK( comm_it == refl::enum_values<e_commodity>.end() );
}

PositionedHarborSubView<HarborMarketCommodities>
HarborMarketCommodities::create( SS& ss, TS& ts, Player& player,
                                 Rect canvas ) {
  Delta const size = canvas.delta();
  W           comm_block_width =
      size.w / SX{ refl::enum_count<e_commodity> };
  comm_block_width =
      std::clamp( comm_block_width, kCommodityTileSize.w, 32 );
  unique_ptr<HarborMarketCommodities> view;
  HarborSubView*                      harbor_sub_view = nullptr;
  Coord                               pos;
  bool                                stacked = false;
  if( size.w >= HarborMarketCommodities::single_layer_width &&
      size.h >= HarborMarketCommodities::single_layer_height ) {
    stacked = false;
    pos     = Coord{
            .x = canvas.center().x -
             HarborMarketCommodities::single_layer_width / 2,
            .y = canvas.bottom_edge() -
             HarborMarketCommodities::single_layer_height - 1 };
  } else {
    stacked = true;
    pos     = Coord{
            .x = canvas.center().x -
             HarborMarketCommodities::double_layer_width / 2,
            .y = canvas.bottom_edge() -
             HarborMarketCommodities::double_layer_height - 1 };
  };
  view = make_unique<HarborMarketCommodities>( ss, ts, player,
                                               stacked );
  harbor_sub_view                   = view.get();
  HarborMarketCommodities* p_actual = view.get();
  return PositionedHarborSubView<HarborMarketCommodities>{
      .owned  = { .view = std::move( view ), .coord = pos },
      .harbor = harbor_sub_view,
      .actual = p_actual };
}

HarborMarketCommodities::HarborMarketCommodities( SS& ss, TS& ts,
                                                  Player& player,
                                                  bool stacked )
  : HarborSubView( ss, ts, player ), stacked_( stacked ) {}

} // namespace rn