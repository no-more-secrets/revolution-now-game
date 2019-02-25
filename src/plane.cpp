/****************************************************************
**plane.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2018-12-30.
*
* Description: Rendering planes.
*
*****************************************************************/
#include "plane.hpp"

// Revolution Now
#include "aliases.hpp"
#include "console.hpp"
#include "frame.hpp"
#include "image.hpp"
#include "init.hpp"
#include "logging.hpp"
#include "menu.hpp"
#include "ranges.hpp"
#include "render.hpp"
#include "window.hpp"

// base-util
#include "base-util/algo.hpp"
#include "base-util/misc.hpp"
#include "base-util/variant.hpp"

// C++ standard library
#include <algorithm>
#include <array>

using namespace std;

namespace rn {

namespace {

constexpr auto num_planes =
    static_cast<size_t>( e_plane::_size() );

// Planes are rendered from 0 --> count.
array<ObserverPtr<Plane>, num_planes> planes;
array<Texture, num_planes>            textures;

ObserverPtr<Plane>& plane( e_plane plane ) {
  auto idx = static_cast<size_t>( plane._value );
  CHECK( idx < planes.size() );
  return planes[idx];
}

Texture& plane_tx( e_plane plane ) {
  auto idx = static_cast<size_t>( plane._value );
  CHECK( idx < planes.size() );
  return textures[idx];
}

struct InactivePlane : public Plane {
  InactivePlane() {}
  bool enabled() const override { return false; }
  bool covers_screen() const override { return false; }
  void draw( Texture const& /*unused*/ ) const override {}
};

InactivePlane dummy;

// This is the plan that is currently receiving mouse dragging
// events (nullopt if there is not dragging event happening).
Opt<e_plane> g_drag_plane{};

auto relevant_planes() {
  auto not_covers_screen = L( !_.second->covers_screen() );
  auto enabled           = L( _.second->enabled() );
  return rv::zip( values<e_plane>, planes ) //
         | rv::filter( enabled )            //
         | rv::reverse                      //
         | take_while_inclusive( not_covers_screen );
}

auto planes_to_draw() { return relevant_planes() | rv::reverse; }

void init_planes() {
  // By default, all planes are dummies, unless we provide an
  // object below.
  planes.fill( ObserverPtr<Plane>( &dummy ) );

  plane( e_plane::viewport ).reset( viewport_plane() );
  plane( e_plane::panel ).reset( panel_plane() );
  plane( e_plane::image ).reset( image_plane() );
  // plane( e_plane::colony ).reset( colony_plane() );
  // plane( e_plane::europe ).reset( europe_plane() );
  plane( e_plane::effects ).reset( effects_plane() );
  plane( e_plane::window ).reset( window_plane() );
  plane( e_plane::menu ).reset( menu_plane() );
  plane( e_plane::console ).reset( console_plane() );

  // No plane must be null, they must all point to a valid Plane
  // object even if it is the dummy above.
  for( auto p : planes ) { CHECK( p ); }

  // Initialize the textures. These are intended to cover the
  // entire screen and are measured in logical coordinates (which
  // means, typically, that they will be smaller than the full
  // screen resolution).
  for( auto& tx : textures ) {
    tx = create_screen_sized_texture();
    clear_texture_transparent( tx );
  }
}

void cleanup_planes() {
  // This actually just destroys the textures, since the planes
  // will be held by value as global variables elsewhere.
  for( auto& tx : textures ) tx = {};
}

REGISTER_INIT_ROUTINE( planes, init_planes, cleanup_planes );

} // namespace

Plane& Plane::get( e_plane p ) { return *plane( p ); }

bool Plane::input( input::event_t const& /*unused*/ ) {
  return false;
}

Plane::e_accept_drag Plane::can_drag(
    input::e_mouse_button /*unused*/, Coord /*unused*/ ) {
  return e_accept_drag::no;
}

void Plane::on_drag( input::e_mouse_button /*unused*/,
                     Coord /*unused*/, Coord /*unused*/,
                     Coord /*unused*/ ) {}

void Plane::on_drag_finished( input::e_mouse_button /*unused*/,
                              Coord /*unused*/,
                              Coord /*unused*/ ) {}

OptRef<Plane::MenuClickHandler> Plane::menu_click_handler(
    e_menu_item /*unused*/ ) const {
  return nullopt;
}

void draw_all_planes( Texture const& tx ) {
  clear_texture_black( tx );

  // This will find the last plane that will render (opaquely)
  // over every pixel. If one is found then we will not render
  // any planes before it. This is technically not necessary, but
  // saves rendering work by avoiding to render things that would
  // go unseen anyway.
  for( auto [e, ptr] : planes_to_draw() ) {
    set_render_target( plane_tx( e ) );
    ptr->draw( plane_tx( e ) );
    copy_texture( plane_tx( e ), tx );
  }
}

bool send_input_to_planes( input::event_t const& event ) {
  using namespace input;
  if_v( event, input::mouse_drag_event_t, drag_event ) {
    if( g_drag_plane ) {
      auto& plane = Plane::get( *g_drag_plane );
      CHECK( plane.enabled() );
      // Drag should already be in progress.
      CHECK( drag_event->state.phase != +e_drag_phase::begin );
      // There is already a drag in progress, so send this one to
      // the same plane that accepted the initial drag event.
      plane.on_drag( drag_event->button,
                     drag_event->state.origin, drag_event->prev,
                     drag_event->pos );
      // If the drag is finished then sound out that event.
      if( drag_event->state.phase == +e_drag_phase::end ) {
        logger->debug( "finished `{}` drag event",
                       *g_drag_plane );
        plane.on_drag_finished( drag_event->button,
                                drag_event->state.origin,
                                drag_event->pos );
        g_drag_plane = nullopt;
      }
      // Here it is assumed/required that the plane handle it
      // because the plane has already accepted this drag.
      return true;
    }
    // No drag plane registered to accept the event, so lets
    // send out the event but only if it's a `begin` event.
    if( drag_event->state.phase == +e_drag_phase::begin ) {
      for( auto [e, plane] : relevant_planes() ) {
        auto drag_result = plane->can_drag( drag_event->button,
                                            drag_event->pos );
        switch( drag_result ) {
          // If the plane doesn't want to handle it then move
          // on to ask the next one.
          case Plane::e_accept_drag::no: continue;
          // In this case the plane says that it doesn't want
          // to handle it AND it doesn't want anyone else to
          // handle it.
          case Plane::e_accept_drag::swallow:
            return true;
            // Wants to handle it.
          case Plane::e_accept_drag::yes:
            g_drag_plane = e;
            logger->debug( "plane `{}` can drag", e );
            // Now we must send it an on_drag because this
            // mouse event that we're dealing with serves both
            // to tell us about a new drag even but also may
            // have a mouse delta in it that needs to be
            // processed.
            plane->on_drag( drag_event->button,
                            drag_event->state.origin,
                            drag_event->prev, drag_event->pos );
            return true;
        }
      }
    }
    // If no one handled it then that's it.
    return false;
  }

  // Just a normal event, so send it out using the usual proto-
  // col.
  for( auto p : relevant_planes() )
    if( p.second->input( event ) ) return true;

  return false;
}

namespace {

bool is_menu_item_enabled( e_menu_item item ) {
  for( auto p : relevant_planes() ) {
    event_counts()["imie inner"].tick();
    if( p.second->menu_click_handler( item ).has_value() )
      return true;
  }
  return false;
}

void on_menu_item_clicked( e_menu_item item ) {
  for( auto p : relevant_planes() ) {
    if( auto maybe_handler =
            p.second->menu_click_handler( item );
        maybe_handler.has_value() ) {
      maybe_handler.value().get()();
      return;
    }
  }
  SHOULD_NOT_BE_HERE;
}

#define MENU_ITEM_HANDLER_PLANE( item )                   \
  MENU_ITEM_HANDLER(                                      \
      e_menu_item::item,                                  \
      [] { on_menu_item_clicked( e_menu_item::item ); },  \
      [] {                                                \
        return is_menu_item_enabled( e_menu_item::item ); \
      } )

MENU_ITEM_HANDLER_PLANE( about );
MENU_ITEM_HANDLER_PLANE( economics_advisor );
MENU_ITEM_HANDLER_PLANE( european_advisor );
MENU_ITEM_HANDLER_PLANE( fortify );
MENU_ITEM_HANDLER_PLANE( founding_father_help );
MENU_ITEM_HANDLER_PLANE( military_advisor );
MENU_ITEM_HANDLER_PLANE( restore_zoom );
MENU_ITEM_HANDLER_PLANE( retire );
MENU_ITEM_HANDLER_PLANE( revolution );
MENU_ITEM_HANDLER_PLANE( sentry );
MENU_ITEM_HANDLER_PLANE( terrain_help );
MENU_ITEM_HANDLER_PLANE( units_help );
MENU_ITEM_HANDLER_PLANE( zoom_in );
MENU_ITEM_HANDLER_PLANE( zoom_out );

} // namespace

} // namespace rn
