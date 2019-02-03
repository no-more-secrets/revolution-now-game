/****************************************************************
**menu.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2019-01-27.
*
* Description: Menu Bar
*
*****************************************************************/
#include "menu.hpp"

// Revolution Now
#include "adt.hpp"
#include "aliases.hpp"
#include "config-files.hpp"
#include "errors.hpp"
#include "fonts.hpp"
#include "globals.hpp"
#include "logging.hpp"
#include "plane.hpp"
#include "sdl-util.hpp"
#include "variant.hpp"

// base-util
#include "base-util/algo.hpp"

// Abseil
#include "absl/container/flat_hash_map.h"

// C++ standard library
#include <chrono>

using namespace std;

using absl::flat_hash_map;

ADT( rn, MenuState,                   //
     ( menus_hidden ),                //
     ( menus_closed,                  //
       ( Opt<e_menu>, hover ) ),      //
     ( menu_open,                     //
       ( e_menu, menu ),              //
       ( Opt<e_menu_item>, hover ) ), //
     ( item_click,                    //
       ( e_menu_item, item ),         //
       ( TimeType, start ) )          //
);

namespace rn {

namespace {

/****************************************************************
** Main Data Structures
*****************************************************************/
struct Menu {
  string name;
  bool   right_side;
  char   shortcut;
};

absl::flat_hash_map<e_menu, Menu> g_menus{
    {e_menu::game, {"Game", false, 'G'}},
    {e_menu::view, {"View", false, 'V'}},
    {e_menu::orders, {"Orders", false, 'O'}},
    {e_menu::pedia, {"Revolopedia", true, 'R'}}};

struct MenuDivider {};

struct MenuCallbacks {
  function<void( void )> on_click;
  function<bool( void )> enabled;
};

// menu.cpp
struct MenuClickable {
  e_menu_item   item;
  string        name;
  MenuCallbacks callbacks;
};

using MenuItem = variant<MenuDivider, MenuClickable>;

flat_hash_map<e_menu_item, MenuClickable*> g_menu_items;
flat_hash_map<e_menu_item, e_menu>         g_item_to_menu;
flat_hash_map<e_menu, Vec<e_menu_item>>    g_items_from_menu;

#define ITEM( item, name )      \
  MenuClickable {               \
    e_menu_item::item, name, {} \
  }

#define DIVIDER \
  MenuDivider {}

/****************************************************************
** The Menus
*****************************************************************/
absl::flat_hash_map<e_menu, Vec<MenuItem>> g_menu_def{
    {e_menu::game,
     {
         ITEM( about, "About this Game" ),    //
         /***********/ DIVIDER, /***********/ //
         ITEM( revolution, "REVOLUTION!" ),   //
         /***********/ DIVIDER, /***********/ //
         ITEM( retire, "Retire" ),            //
         ITEM( exit, "Exit to \"DOS\"" )      //
     }},
    {e_menu::view,
     {
         ITEM( zoom_in, "Zoom In" ),          //
         ITEM( zoom_out, "Zoom Out" ),        //
         /***********/ DIVIDER, /***********/ //
         ITEM( restore_zoom, "Zoom Default" ) //
     }},
    {e_menu::orders,
     {
         ITEM( sentry, "Sentry" ),  //
         ITEM( fortify, "Fortify" ) //
     }},
    {e_menu::pedia,
     {
         ITEM( units_help, "Units" ) //
     }}};

/****************************************************************
** Menu State
*****************************************************************/
using Frames = chrono::duration<int, std::ratio<1, 60>>;
namespace click_anim {

// TODO: make an animation framework that can manage the states
// of an animation along with durations in Frames.

// For sustained blinking
// auto constexpr half_period     = Frames{4};
// auto constexpr post_off_time   = Frames{5};
// int constexpr num_half_periods = 4;
// bool constexpr start_on        = false;
// auto constexpr fade_time        = Frames{22};

// For the MacOS single-blink-and-fade style.
auto constexpr half_period      = Frames{6};
auto constexpr post_off_time    = Frames{0};
auto constexpr num_half_periods = int( 2 );
bool constexpr start_on         = false;
auto constexpr fade_time        = Frames{22};

// For example, the click could be like this:
//
//   click_anim_start_on=true, num_half_periods=4
//   ----------------------------------------------------
//   |  on  |  off |  on  |  off | off-time | fade-time |
//   ----------------------------------------------------
//
// where each box is a "half period."

auto period = half_period * 2;
auto total_duration =
    half_period * num_half_periods + post_off_time + fade_time;
} // namespace click_anim

MenuState_t g_menu_state{MenuState::menus_closed{}};

void log_menu_state() {
  logger->debug( "g_menu_state: {}", g_menu_state );
}

/****************************************************************
** Querying State
*****************************************************************/
bool is_menu_open( e_menu menu ) {
  auto matcher = scelta::match(
      []( MenuState::menus_hidden ) { return false; },
      []( MenuState::menus_closed ) { return false; },
      [&]( MenuState::item_click click ) {
        CHECK( g_item_to_menu.contains( click.item ) );
        return g_item_to_menu[click.item] == menu;
      },
      [&]( MenuState::menu_open o ) { return o.menu == menu; } );
  return matcher( g_menu_state );
}

Opt<e_menu> opened_menu() {
  if_v( g_menu_state, MenuState::menu_open, val ) {
    return val->menu;
  }
  return {};
}

/****************************************************************
** Colors
*****************************************************************/
namespace color {
namespace item {
namespace background {
auto const& active   = config_palette.yellow.sat1.lum11;
auto const& inactive = config_palette.orange.sat0.lum3;
} // namespace background
namespace foreground {
auto const& active   = config_palette.orange.sat0.lum2;
auto const& inactive = config_palette.orange.sat1.lum11;
auto const& disabled = config_palette.grey.n68;
} // namespace foreground
} // namespace item
namespace menu {
namespace background {
auto const& active = color::item::background::active;
auto        hover() {
  return color::item::background::inactive.highlighted( 2 );
}
auto const& inactive = color::item::background::inactive;
} // namespace background
namespace foreground {
// auto const& active = color::item::foreground::active;
}
} // namespace menu
} // namespace color

/****************************************************************
** Cached Textures
*****************************************************************/
struct ItemTextures {
  Texture normal;
  Texture highlighted;
  Texture disabled;
  // This is the max width of the above textures, just for the
  // sake of having it precomputed.
  W width{0};
};

flat_hash_map<e_menu_item, ItemTextures> g_menu_item_rendered;

struct MenuTextures {
  // Each menu may have a different width depending on the sizes
  // of the elements inside of it, so we will render different
  // sized dividers for each menu. All dividers in a given menu
  // will be the same size though.
  Texture      divider;
  Texture      item_background_normal;
  Texture      item_background_highlight;
  ItemTextures name;
  Texture      menu_body;
  Texture      menu_background_normal;
  Texture      menu_background_highlight;
  Texture      menu_background_hover;
  W            header_width{0};
};

absl::flat_hash_map<e_menu, MenuTextures> g_menu_rendered;

Texture menu_bar_tx;

// Must be called after all textures are rendered. It will it-
// erate through all the rendered text textures (both menu
// headers and menu items) and will compute the maximum height of
// all of them. This height will then be used as the height for
// all text elements in the menus for simplicity.
//
// This value is cached because a) it requires querying textures
// and b) its value is not expected to ever change.
H const& max_text_height() {
  static H max_height = [] {
    H res{0};
    for( auto menu : values<e_menu> ) {
      CHECK( g_menu_rendered.contains( menu ) );
      auto const& textures = g_menu_rendered[menu];
      CHECK( textures.name.normal );
      res = std::max( res, textures.name.normal.size().h );
      for( auto item : g_items_from_menu[menu] ) {
        CHECK( g_menu_item_rendered.contains( item ) );
        auto const& textures = g_menu_item_rendered[item];
        // These are probably all the same size, but just in case
        // they aren't. The 2_sx is because we have padding on
        // each side of the item.
        res = std::max( res, textures.normal.size().h );
        res = std::max( res, textures.highlighted.size().h );
        res = std::max( res, textures.disabled.size().h );
      }
      return res;
    }
    CHECK( res > 0_h );
    return res;
  }();
  return max_height;
}

/****************************************************************
** Geometry
*****************************************************************/
H menu_bar_height() { return max_text_height(); }

// These cannot be precalculated because menus might be hidden.
X menu_header_x_pos( e_menu target ) {
  X pos{0};
  pos += config_ui.menus.first_menu_start;
  for( auto menu : values<e_menu> ) {
    if( menu == target ) return pos;
    // TODO: is menu visible
    CHECK( g_menu_rendered.contains( menu ) );
    pos += g_menu_rendered[menu].header_width +
           config_ui.menus.spacing;
  }
  SHOULD_NOT_BE_HERE;
}

Delta menu_header_delta( e_menu menu ) {
  CHECK( g_menu_rendered.contains( menu ) );
  return Delta{g_menu_rendered[menu].header_width,
               max_text_height()};
}

// Rectangle around a menu header.
Rect menu_header_rect( e_menu menu ) {
  CHECK( g_menu_rendered.contains( menu ) );
  return Rect::from( Coord{0_y, menu_header_x_pos( menu )},
                     menu_header_delta( menu ) );
}

// The long, thin rectangle around the top menu bar. This extends
// from the left side of the screen to the right, and includes
// the menu headers. but does not include the space that would be
// occupied by open menu bodies.
Rect menu_bar_rect() {
  auto delta_screen = logical_screen_pixel_dimensions();
  auto height       = menu_bar_height();
  return Rect::from( Coord{}, Delta{delta_screen.w, height} );
}

W menu_body_width( e_menu menu ) {
  W res{0};
  for( auto const& item : g_items_from_menu[menu] ) {
    CHECK( g_menu_item_rendered.contains( item ) );
    res = std::max( res, g_menu_item_rendered[item].width );
  }
  // At this point, res holds the width of the largest rendered
  // text texture in this menu.  Now add padding on each side:
  res += config_ui.menus.padding * 2_sx;
  res = clamp( res, config_ui.menus.body_min_width, 1000000_w );
  // Sanity check
  CHECK( res > 0 && res < 2000 );
  return res;
}

H divider_height() { return max_text_height() / 2; }

H menu_body_height( e_menu menu ) {
  H    h{0};
  auto add_height = scelta::match(
      [&]( MenuDivider ) { h += divider_height(); },
      [&]( MenuClickable ) { h += max_text_height(); } );
  CHECK( g_menu_def.contains( menu ) );
  for( auto const& item : g_menu_def[menu] ) add_height( item );
  return h;
}

Delta menu_body_delta( e_menu menu ) {
  return {menu_body_width( menu ), menu_body_height( menu )};
}

Delta menu_item_delta( e_menu menu ) {
  return Delta{menu_body_width( menu ), max_text_height()};
}

Delta divider_delta( e_menu menu ) {
  return Delta{divider_height(), menu_body_width( menu )};
}

Rect menu_body_rect( e_menu menu ) {
  Coord pos{Y{0} + menu_bar_height(), menu_header_x_pos( menu )};
  return Rect::from( pos, menu_body_delta( menu ) );
}

// `h` is the vertical position from the top of the menu body.
Opt<e_menu_item> cursor_to_item( e_menu menu, H h ) {
  CHECK( g_menu_def.contains( menu ) );
  H pos{0};
  for( auto const& item : g_menu_def[menu] ) {
    auto advance = scelta::match(
        [&]( MenuDivider ) { pos += divider_height(); },
        [&]( MenuClickable const& ) {
          pos += max_text_height();
        } );
    advance( item );
    if( pos > h ) {
      return scelta::match(
          [&]( MenuDivider ) { return Opt<e_menu_item>{}; },
          [&]( MenuClickable const& clickable ) {
            return Opt<e_menu_item>( clickable.item );
          } //
          )( item );
    }
  }
  return {};
}

// `cursor` is the screen position of the mouse cursor.
Opt<e_menu_item> cursor_to_item( e_menu menu, Coord cursor ) {
  if( !cursor.is_inside( menu_body_rect( menu ) ) ) return {};
  return cursor_to_item(
      menu, cursor.y - menu_body_rect( menu ).top_edge() );
}

/****************************************************************
** Rendering Implmementation
*****************************************************************/
// For either a menu header or item.
ItemTextures render_menu_element( string const&  s,
                                  optional<char> shortcut ) {
  (void)shortcut; /* TODO */
  auto inactive = render_text_line_shadow(
      fonts::standard, color::item::foreground::inactive, s );
  auto active = render_text_line_shadow(
      fonts::standard, color::item::foreground::active, s );
  auto disabled = render_text_line_shadow(
      fonts::standard, color::item::foreground::disabled, s );
  // Need to do this first before moving.
  auto width = std::max(
      {inactive.size().w, active.size().w, disabled.size().w} );
  auto res =
      ItemTextures{std::move( inactive ), std::move( active ),
                   std::move( disabled ), width};
  // Sanity check
  CHECK( res.width > 0 &&
         res.width < logical_screen_pixel_dimensions().w );
  return res;
}

Texture render_divider( e_menu menu ) {
  Delta   delta = divider_delta( menu );
  Texture res   = create_texture( delta );
  clear_texture_transparent( res );
  // A divider is never highlighted.
  Color color_fore = color::item::foreground::disabled;
  Color color_back = color_fore.shaded( 4 );
  render_line( res, color_fore,
               Coord{} + delta.h / 2 - 1_h + 2_w,
               {delta.w - 5_w, 0_h} );
  render_line( res, color_back,
               Coord{} + delta.h / 2 + 2_w + 1_w - 0_h,
               {delta.w - 5_w, 0_h} );
  return res;
}

Texture render_item_background( e_menu menu, bool active ) {
  return create_texture(
      menu_item_delta( menu ),
      active ? color::item::background::active
             : color::item::background::inactive );
}

Texture render_menu_header_background( e_menu menu, bool active,
                                       bool hover ) {
  return create_texture(
      menu_header_delta( menu ),
      !active ? color::menu::background::inactive
              : hover ? color::menu::background::hover()
                      : color::menu::background::active );
}

Texture create_menu_body_texture( e_menu menu ) {
  return create_texture( menu_body_delta( menu ) );
}

Texture const& render_open_menu( e_menu           menu,
                                 Opt<e_menu_item> subject,
                                 bool             clicking ) {
  CHECK( g_menu_rendered.contains( menu ) );
  if( subject.has_value() ) {
    CHECK( g_item_to_menu[*subject] == menu );
  }
  auto const& textures = g_menu_rendered[menu];
  auto&       dst      = textures.menu_body;
  Coord       pos{};

  auto render = scelta::match(
      [&]( MenuDivider ) {
        copy_texture( textures.item_background_normal, dst,
                      pos );
        copy_texture( textures.divider, dst, pos );
        pos += divider_height();
      },
      [&]( MenuClickable const& clickable ) {
        auto const& desc = g_menu_items[clickable.item];
        auto const& rendered =
            g_menu_item_rendered[clickable.item];
        Texture const* from{nullptr};
        Texture const* background{nullptr};
        if( clicking && clickable.item == subject ) {
          /**********************************************
          ** Click Animation
          ***********************************************/
          using namespace std::chrono;
          using namespace std::literals::chrono_literals;
          auto now = system_clock::now();
          CHECK( util::holds<MenuState::item_click>(
              g_menu_state ) );
          auto start =
              std::get<MenuState::item_click>( g_menu_state )
                  .start;
          auto elapsed = now - start;
          using namespace click_anim;
          if( elapsed >= total_duration - fade_time ) {
            // We're in the fade, in which case we should be
            // highlighted, because it seems to make a better
            // UX when the selected item is highlighted as
            // the fading happens.
            from       = &rendered.highlighted;
            background = &textures.item_background_highlight;
          } else if( elapsed >= total_duration - fade_time -
                                    post_off_time ) {
            // We're in a period between the blinking and
            // fading when the highlight is off, although
            // this period may have zero length.
            from       = &rendered.normal;
            background = &textures.item_background_normal;

          } else if( ( elapsed % period > half_period ) ^
                     start_on ) {
            // Blink on
            from       = &rendered.highlighted;
            background = &textures.item_background_highlight;
          } else {
            // Blink off
            from       = &rendered.normal;
            background = &textures.item_background_normal;
          }
        } else {
          from = !desc->callbacks.enabled()
                     ? &rendered.disabled
                     : ( clickable.item == subject )
                           ? &rendered.highlighted
                           : &rendered.normal;
          background = ( clickable.item == subject )
                           ? &textures.item_background_highlight
                           : &textures.item_background_normal;
        }
        copy_texture( *background, dst, pos );
        copy_texture( *from, dst,
                      pos + config_ui.menus.padding );
        pos += max_text_height();
      } );

  for( auto const& item : g_menu_def[menu] ) render( item );
  return dst;
}

void render_menu_bar() {
  CHECK( menu_bar_tx );
  fill_texture( menu_bar_tx, color::menu::background::inactive );
  for( auto menu : values<e_menu> ) {
    CHECK( g_menu_rendered.contains( menu ) );
    auto const& textures = g_menu_rendered[menu];
    using Txs = Opt<pair<Texture const*, Texture const*>>;
    // Given `menu`, this matcher visits the global menu state
    // and returns a foreground/background texture pair for that
    // menu.
    auto matcher = scelta::match<Txs>(
        []( MenuState::menus_hidden ) { return Txs{}; },
        [&]( MenuState::menus_closed closed ) {
          if( menu == closed.hover )
            return Txs{pair{&textures.name.normal,
                            &textures.menu_background_hover}};
          return Txs{pair{&textures.name.normal,
                          &textures.menu_background_normal}};
        } )( //
        [&]( auto self, MenuState::item_click const& ic ) {
          // Just forward this to the MenuState::menu_open.
          CHECK( g_item_to_menu.contains( ic.item ) );
          return self( MenuState_t{MenuState::menu_open{
              g_item_to_menu[ic.item], /*hover=*/{}}} );
        },
        [&]( auto self, MenuState::menu_open const& o ) {
          if( o.menu == menu ) {
            return Txs{
                pair{&textures.name.highlighted,
                     &textures.menu_background_highlight}};
          } else
            return self(
                MenuState_t{MenuState::menus_closed{}} );
        } );
    if( auto p = matcher( g_menu_state ); p.has_value() ) {
      auto pos = menu_header_x_pos( menu );
      copy_texture( *p->second, menu_bar_tx, {0_y, pos} );
      copy_texture( *p->first, menu_bar_tx,
                    {0_y, pos + config_ui.menus.padding} );
    }
  }
}

void display_menu_bar_tx( Texture const& tx ) {
  render_menu_bar();
  CHECK( tx.size().w == menu_bar_tx.size().w );
  copy_texture( menu_bar_tx, tx, Coord{} );
}

/****************************************************************
** Input Implementation
*****************************************************************/
// TODO: ADT
struct mouse_over_menu_bar {};
bool operator==( mouse_over_menu_bar const&,
                 mouse_over_menu_bar const& ) {
  return true;
}

struct mouse_over_divider {
  e_menu menu;
};
bool operator==( mouse_over_divider const& l,
                 mouse_over_divider const& r ) {
  return l.menu == r.menu;
}

// Must be equality comparable.
using MouseOverMenu =
    Opt<variant<e_menu, e_menu_item, mouse_over_divider,
                mouse_over_menu_bar>>;

MouseOverMenu click_target( Coord screen_coord ) {
  auto matcher = scelta::match<MouseOverMenu>(
      []( MenuState::menus_hidden ) { return MouseOverMenu{}; },
      [&]( MenuState::menus_closed ) {
        for( auto menu : values<e_menu> )
          if( screen_coord.is_inside(
                  menu_header_rect( menu ) ) )
            return MouseOverMenu{menu};
        if( screen_coord.is_inside( menu_bar_rect() ) )
          return MouseOverMenu{mouse_over_menu_bar{}};
        return MouseOverMenu{};
      } )( //
      [&]( auto self, MenuState::item_click const& ic ) {
        // Just forward this to the MenuState::menu_open.
        CHECK( g_item_to_menu.contains( ic.item ) );
        return self( MenuState_t{MenuState::menu_open{
            g_item_to_menu[ic.item], /*hover=*/{}}} );
      },
      [&]( auto self, MenuState::menu_open const& o ) {
        auto closed =
            self( MenuState_t{MenuState::menus_closed{}} );
        if( closed ) return closed;
        if( !screen_coord.is_inside( menu_body_rect( o.menu ) ) )
          return MouseOverMenu{};
        // We over somewhere in the menu body.
        auto maybe_item = cursor_to_item( o.menu, screen_coord );
        if( maybe_item.has_value() )
          return MouseOverMenu{*maybe_item};
        // We are not over an item, so must be a divider.
        return MouseOverMenu{mouse_over_divider{o.menu}};
      } );
  return matcher( g_menu_state );
}

} // namespace

/****************************************************************
** Top-level Render Method
*****************************************************************/
void render_menus( Texture const& tx ) {
  display_menu_bar_tx( tx );
  auto maybe_render_open_menu = scelta::match(
      []( MenuState::menus_hidden ) {},  //
      [&]( MenuState::menus_closed ) {}, //
      [&]( MenuState::item_click const& ic ) {
        // Just forward this to the MenuState::menu_open.
        CHECK( g_item_to_menu.contains( ic.item ) );
        auto        menu = g_item_to_menu[ic.item];
        auto const& open_tx =
            render_open_menu( menu, ic.item, /*clicking=*/true );
        Coord   pos   = menu_body_rect( menu ).upper_left();
        uint8_t alpha = 255;
        auto    now   = chrono::system_clock::now();
        auto    start_fade =
            ic.start + ( click_anim::total_duration -
                         click_anim::fade_time );
        auto end = ic.start + click_anim::total_duration;
        if( now >= start_fade ) {
          if( now < end ) {
            double percent_completion =
                double( chrono::duration_cast<
                            std::chrono::milliseconds>(
                            now - start_fade )
                            .count() ) /
                double( chrono::duration_cast<
                            std::chrono::milliseconds>(
                            click_anim::fade_time )
                            .count() );
            CHECK( percent_completion >= 0.0 &&
                       percent_completion <= 1.0,
                   "percent_completion: {}",
                   percent_completion );
            alpha = 255 - uint8_t( 255.0 * percent_completion );
            copy_texture_alpha( open_tx, tx, pos, alpha );
          } else if( now >= end ) {
            alpha = 0;
            copy_texture_alpha( open_tx, tx, pos, alpha );
          }
        } else
          copy_texture( open_tx, tx, pos );
      },
      [&]( MenuState::menu_open const& o ) {
        auto const& open_tx = render_open_menu(
            o.menu, o.hover, /*clicking=*/false );
        Coord pos = menu_body_rect( o.menu ).upper_left();
        copy_texture( open_tx, tx, pos );
      } );
  maybe_render_open_menu( g_menu_state );
}

/****************************************************************
** Registration
*****************************************************************/
auto& on_click_handlers() {
  static absl::flat_hash_map<e_menu_item,
                             std::function<void( void )>>
      m;
  return m;
}

auto& is_enabled_handlers() {
  static absl::flat_hash_map<e_menu_item,
                             std::function<bool( void )>>
      m;
  return m;
}

void register_menu_item_handler(
    e_menu_item                        item,
    std::function<void( void )> const& on_click,
    std::function<bool( void )> const& is_enabled ) {
  CHECK( on_click );
  CHECK( is_enabled );
  auto& on_clicks   = on_click_handlers();
  auto& is_enableds = is_enabled_handlers();
  CHECK( !on_clicks.contains( item ) );
  CHECK( !is_enableds.contains( item ) );
  on_clicks[item]   = on_click;
  is_enableds[item] = is_enabled;
}

/****************************************************************
** Initialization
*****************************************************************/
void initialize_menus() {
  // Check that all menus have descriptors.
  for( auto menu : values<e_menu> ) {
    CHECK( g_menus.contains( menu ) );
    CHECK( g_menu_def.contains( menu ) );
  }

  // Populate the e_menu_item maps and verify no duplicates.
  for( auto& [menu, items] : g_menu_def ) {
    for( auto& item_desc : items ) {
      if( util::holds<MenuDivider>( item_desc ) ) continue;
      CHECK( util::holds<MenuClickable>( item_desc ) );
      auto& clickable = get<MenuClickable>( item_desc );
      g_items_from_menu[menu].push_back( clickable.item );
      CHECK( !g_menu_items.contains( clickable.item ) );
      g_menu_items[clickable.item] = &clickable;
      CHECK( !g_item_to_menu.contains( clickable.item ) );
      g_item_to_menu[clickable.item] = menu;
    }
  }

  // Check that all menus have at least one item.
  for( auto menu : values<e_menu> ) {
    CHECK( g_items_from_menu[menu].size() > 0 );
  }

  // Check that all e_menu_items are in a menu.
  for( auto item : values<e_menu_item> ) {
    CHECK( g_menu_items.contains( item ) );
    CHECK( g_menu_items[item] != nullptr );
    CHECK( g_item_to_menu.contains( item ) );
  }

  // Populate Callbacks
  for( auto const& [item, func] : on_click_handlers() ) {
    CHECK( !g_menu_items[item]->callbacks.on_click );
    g_menu_items[item]->callbacks.on_click = func;
  }
  for( auto const& [item, func] : is_enabled_handlers() ) {
    CHECK( !g_menu_items[item]->callbacks.enabled );
    g_menu_items[item]->callbacks.enabled = func;
  }

  // Check that all e_menu_items have registered handlers.
  for( auto item : values<e_menu_item> ) {
    auto const& desc = *g_menu_items[item];
    CHECK( item == desc.item );
    CHECK( desc.name.size() > 0 );
    CHECK( desc.callbacks.on_click && desc.callbacks.enabled,
           "the menu item `{}` does not have callback handlers "
           "registered",
           item );
  }

  // Render Menu and Menu-item names. These have to be done first
  // because other things need to be calculated from the sizes of
  // the rendered text.
  for( auto menu_item : values<e_menu_item> )
    g_menu_item_rendered[menu_item] = render_menu_element(
        g_menu_items[menu_item]->name, nullopt );
  for( auto menu : values<e_menu> ) {
    g_menu_rendered[menu]      = {};
    g_menu_rendered[menu].name = render_menu_element(
        g_menus[menu].name, g_menus[menu].shortcut );
  }

  for( auto menu : values<e_menu> ) {
    // The order in which these are done matters, unfortunately,
    // because some of the functions below rely on results from
    // the previous ones.
    g_menu_rendered[menu].header_width =
        g_menu_rendered[menu].name.normal.size().w +
        config_ui.menus.padding * 2_sx;
    g_menu_rendered[menu].item_background_normal =
        render_item_background( menu, /*hightlight=*/false );
    g_menu_rendered[menu].item_background_highlight =
        render_item_background( menu, /*hightlight=*/true );
    g_menu_rendered[menu].menu_body =
        create_menu_body_texture( menu );
    g_menu_rendered[menu].menu_background_normal =
        render_menu_header_background( menu,
                                       /*highlight=*/false,
                                       /*hover=*/false );
    g_menu_rendered[menu].menu_background_highlight =
        render_menu_header_background( menu,
                                       /*highlight=*/true,
                                       /*hover=*/false );
    g_menu_rendered[menu].menu_background_hover =
        render_menu_header_background( menu,
                                       /*highlight=*/true,
                                       /*hover=*/true );

    g_menu_rendered[menu].divider = render_divider( menu );
  }

  menu_bar_tx = create_texture( menu_bar_rect().delta() );
}

void cleanup_menus() {
  // This should free all the textures representing the menu
  // items.
  g_menu_rendered.clear();
  g_menu_item_rendered.clear();
  menu_bar_tx.free();
}

/****************************************************************
** Handlers (temporary)
*****************************************************************/
function<void( void )> empty_handler = [] {};
function<bool( void )> enabled_true  = [] { return true; };
function<bool( void )> enabled_false = [] { return false; };
function<bool( void )> quit_handler  = [] {
  throw exception_exit{};
  return false;
};

MENU_ITEM_HANDLER( about, empty_handler, enabled_true );
MENU_ITEM_HANDLER( revolution, empty_handler, enabled_true );
MENU_ITEM_HANDLER( retire, empty_handler, enabled_true );
MENU_ITEM_HANDLER( exit, quit_handler, enabled_true );
MENU_ITEM_HANDLER( zoom_in, empty_handler, enabled_true );
MENU_ITEM_HANDLER( zoom_out, empty_handler, enabled_true );
MENU_ITEM_HANDLER( restore_zoom, empty_handler, enabled_true );
MENU_ITEM_HANDLER( sentry, empty_handler, enabled_false );
MENU_ITEM_HANDLER( fortify, empty_handler, enabled_true );
MENU_ITEM_HANDLER( units_help, empty_handler, enabled_true );

/****************************************************************
** The Menu Plane
*****************************************************************/
struct MenuPlane : public Plane {
  MenuPlane() = default;
  bool enabled() const override { return true; }
  bool covers_screen() const override { return false; }
  Plane::e_accept_drag can_drag( input::e_mouse_button button,
                                 Coord origin ) override {
    if( !click_target( origin ).has_value() )
      return Plane::e_accept_drag::no;
    return ( button == input::e_mouse_button::l )
               ? Plane::e_accept_drag::yes
               : Plane::e_accept_drag::swallow;
  }
  // We handle dragging but do not really treat it as a drag; in-
  // stead  we  just receive the dragging events and convert them
  // to the appropriate mouse motion/button event and  feed  that
  // event back through the input() method in order  to  get  the
  // behavior that we want but avoid code duplication.
  void on_drag( input::e_mouse_button /*unused*/,
                Coord /*unused*/, Coord prev,
                Coord current ) override {
    // TODO: get the input module to do this.
    // Convert to mouse motion event.
    input::mouse_move_event_t event{{current}, prev};
    this->input( event );
  }
  void on_drag_finished( input::e_mouse_button button,
                         Coord start, Coord end ) override {
    // If the drag started and ended on the same menu element
    // then do nothing, just leave the menu open. This will pre-
    // vent the interface from acting flaky when e.g. the user
    // attempts to click on a menu header but accidentally per-
    // forms a tiny drag and then the menu quickly opens and
    // closes, when it should actually have just stayed open.
    auto click_start = click_target( start );
    if( click_target( start ) == click_target( end ) &&
        click_start.has_value() &&
        util::holds<e_menu>( *click_start ) )
      return;
    if( button == input::e_mouse_button::l ) {
      // TODO: get the input module to do this.
      // Convert to mouse button event.
      auto buttons = input::e_mouse_button_event::left_up;
      input::mouse_button_event_t event{{end}, buttons};
      this->input( event );
    }
  }
  void draw( Texture const& tx ) const override {
    clear_texture_transparent( tx );
    render_menus( tx );
    // TODO: put this code in a dedicated plane callback.
    if_v( g_menu_state, MenuState::item_click, val ) {
      auto item  = val->item;
      auto start = val->start;
      if( chrono::system_clock::now() - start >=
          click_anim::total_duration ) {
        g_menu_items[item]->callbacks.on_click();
        g_menu_state = MenuState::menus_closed{/*hover=*/{}};
      }
    }
  }
  bool input( input::event_t const& event ) override {
    auto matcher = scelta::match(
        []( input::unknown_event_t ) { return false; },
        []( input::quit_event_t ) { return false; },
        [&]( input::key_event_t const& key_event ) {
          auto menu = opened_menu();
          if( !menu ) return false;
          if( key_event.change == input::e_key_change::down ) {
            switch( key_event.keycode ) {
              case ::SDLK_RETURN:
              case ::SDLK_KP_ENTER:
                if_v( g_menu_state, MenuState::menu_open, val ) {
                  if( val->hover.has_value() ) {
                    click_menu_item( val->hover.value() );
                    log_menu_state();
                  }
                }
                return true;
              case ::SDLK_ESCAPE:
                g_menu_state = MenuState::menus_closed{{}};
                log_menu_state();
                return true;
              case ::SDLK_KP_4:
              case ::SDLK_LEFT: {
                menu = util::find_previous_and_cycle(
                    values<e_menu>, *menu );
                CHECK( menu );
                g_menu_state =
                    MenuState::menu_open{*menu, /*hover=*/{}};
                log_menu_state();
                return true;
              }
              case ::SDLK_KP_6:
              case ::SDLK_RIGHT: {
                menu = util::find_subsequent_and_cycle(
                    values<e_menu>, *menu );
                CHECK( menu );
                g_menu_state =
                    MenuState::menu_open{*menu, /*hover=*/{}};
                log_menu_state();
                return true;
              }
              case ::SDLK_KP_2:
              case ::SDLK_DOWN: {
                auto state = std::get<MenuState::menu_open>(
                    g_menu_state );
                if( !state.hover )
                  state.hover = g_items_from_menu[*menu].back();
                state.hover = util::find_subsequent_and_cycle(
                    g_items_from_menu[*menu], *state.hover );
                g_menu_state = state;
                log_menu_state();
                return true;
              }
              case ::SDLK_KP_8:
              case ::SDLK_UP: {
                auto state = std::get<MenuState::menu_open>(
                    g_menu_state );
                if( !state.hover )
                  state.hover = g_items_from_menu[*menu].front();
                state.hover = util::find_previous_and_cycle(
                    g_items_from_menu[*menu], *state.hover );
                g_menu_state = state;
                log_menu_state();
                return true;
              }
              default: break;
            }
          }
          return true;
        },
        []( input::mouse_wheel_event_t ) { return false; },
        []( input::mouse_move_event_t mv_event ) {
          // Remove menu-hover by default and enable it again
          // below if the mouse if over a menu and menus are
          // closed.
          if( util::holds<MenuState::menus_closed>(
                  g_menu_state ) )
            g_menu_state = MenuState::menus_closed{};
          auto over_what = click_target( mv_event.pos );
          if( !over_what.has_value() ) return false;
          auto matcher = scelta::match(
              []( mouse_over_menu_bar ) { return true; },
              []( mouse_over_divider desc ) {
                CHECK( util::holds<MenuState::menu_open>(
                           g_menu_state ) ||
                       util::holds<MenuState::item_click>(
                           g_menu_state ) );
                if( util::holds<MenuState::menu_open>(
                        g_menu_state ) ) {
                  g_menu_state = MenuState::menu_open{
                      desc.menu, /*hover=*/{}};
                }
                return true;
              },
              []( e_menu menu ) {
                if( util::holds<MenuState::menu_open>(
                        g_menu_state ) )
                  g_menu_state =
                      MenuState::menu_open{menu, /*hover=*/{}};
                if( util::holds<MenuState::menus_closed>(
                        g_menu_state ) )
                  g_menu_state =
                      MenuState::menus_closed{/*hover=*/menu};
                return true;
              },
              []( e_menu_item item ) {
                CHECK( util::holds<MenuState::menu_open>(
                           g_menu_state ) ||
                       util::holds<MenuState::item_click>(
                           g_menu_state ) );
                if( util::holds<MenuState::menu_open>(
                        g_menu_state ) ) {
                  auto& o = std::get<MenuState::menu_open>(
                      g_menu_state );
                  CHECK( o.menu == g_item_to_menu[item] );
                  o.hover = {};
                  if( g_menu_items[item]->callbacks.enabled() )
                    o.hover = item;
                }
                return true;
              } );
          return matcher( *over_what );
        },
        [&]( input::mouse_button_event_t b_event ) {
          auto over_what = click_target( b_event.pos );
          if( !over_what.has_value() ) {
            if( util::holds<MenuState::menu_open>(
                    g_menu_state ) ) {
              g_menu_state = MenuState::menus_closed{{}};
              log_menu_state();
              return true; // no click through
            }
            return false;
          }
          if( b_event.buttons ==
              input::e_mouse_button_event::left_down ) {
            auto matcher = scelta::match(
                []( mouse_over_menu_bar ) {
                  g_menu_state = MenuState::menus_closed{{}};
                  log_menu_state();
                  return true;
                },
                []( mouse_over_divider ) { return true; },
                []( e_menu menu ) {
                  if( !is_menu_open( menu ) )
                    g_menu_state =
                        MenuState::menu_open{menu, /*hover=*/{}};
                  else
                    g_menu_state =
                        MenuState::menus_closed{/*hover=*/menu};
                  log_menu_state();
                  return true;
                },
                []( e_menu_item ) { return true; } );
            return matcher( *over_what );
          }
          if( b_event.buttons ==
              input::e_mouse_button_event::left_up ) {
            auto matcher = scelta::match(
                []( mouse_over_menu_bar ) {
                  g_menu_state = MenuState::menus_closed{{}};
                  log_menu_state();
                  return true;
                },
                []( mouse_over_divider ) { return true; },
                []( e_menu ) { return true; },
                [&]( e_menu_item item ) {
                  click_menu_item( item );
                  return true;
                } );
            return matcher( *over_what );
          }
          // `true` here because the mouse is over some menu ui
          // element (and that in turn is because
          // over_what.has_value() == true).
          return true;
        },
        []( input::mouse_drag_event_t ) {
          // The framework does not send us mouse drag events
          // directly; instead it uses the api methods on the
          // Plane class.
          SHOULD_NOT_BE_HERE;
          return false;
        } );
    return matcher( event );
  }

private:
  void click_menu_item( e_menu_item item ) {
    switch_v( g_menu_state ) {
      case_v( MenuState::item_click ) {
        // Already clicking, so do nothing. This can happen if a
        // menu item is clicked after it is already in the click
        // animation.
      }
      case_v( MenuState::menus_closed ) { SHOULD_NOT_BE_HERE; }
      case_v( MenuState::menus_hidden ) { SHOULD_NOT_BE_HERE; }
      case_v( MenuState::menu_open ) {
        if( !g_menu_items[item]->callbacks.enabled() ) return;
        logger->info( "selected menu item `{}`", item );
        g_menu_state = MenuState::item_click{
            item, chrono::system_clock::now()};
        log_menu_state();
      }
      default_v;
    };
  }
};

MenuPlane g_menu_plane;

Plane* menu_plane() { return &g_menu_plane; }

} // namespace rn
