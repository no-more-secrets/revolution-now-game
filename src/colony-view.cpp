/****************************************************************
**colony-view.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2020-01-05.
*
* Description: The view that appears when clicking on a colony.
*
*****************************************************************/
#include "colony-view.hpp"

// Revolution Now
#include "anim.hpp"
#include "co-combinator.hpp"
#include "colony.hpp"
#include "colview-entities.hpp"
#include "compositor.hpp"
#include "cstate.hpp"
#include "dragdrop.hpp"
#include "logger.hpp"
#include "map-updater.hpp"
#include "plane.hpp"
#include "text.hpp"

// render
#include "render/renderer.hpp"

// refl
#include "refl/to-str.hpp"

// gfx
#include "gfx/pixel.hpp"

// base
#include "base/lambda.hpp"

using namespace std;

namespace rn {

namespace {

using e_input_handled = Plane::e_input_handled;

/****************************************************************
** Globals
*****************************************************************/
ColonyId                            g_colony_id{};
co::stream<input::event_t>          g_input;
maybe<drag::State<ColViewObject_t>> g_drag_state;

void reset_globals() {
  g_colony_id  = {};
  g_input      = {};
  g_drag_state = nothing;
}

/****************************************************************
** Drawing
*****************************************************************/
void draw_colony_view( rr::Renderer& renderer, ColonyId id ) {
  static gfx::pixel background_color =
      gfx::pixel::parse_from_hex( "f1cf81" ).value();
  renderer.painter().draw_solid_rect(
      gfx::rect{ .origin = {},
                 .size   = renderer.logical_screen_size() },
      background_color );

  UNWRAP_CHECK( canvas, compositor::section(
                            compositor::e_section::normal ) );

  auto& colony = colony_from_id( id );

  Coord pos = canvas.upper_left();

  rr::Typer typer = renderer.typer( pos, gfx::pixel::black() );

  typer.write( "\n\n" );
  typer.write( "id: {}\n\n", colony.id() );
  typer.write( "nation: {}\n\n", colony.nation() );
  typer.write( "location: {}\n\n", colony.location() );

  colview_top_level().view().draw( renderer,
                                   canvas.upper_left() );
}

/****************************************************************
** Drag/Drop
*****************************************************************/
// Must use this to exit the drag_drop_routine function prior to
// the point where the while look starts taking in new events.
// This will happen when the source is not draggable for whatever
// reason; in this case, we need to wait for and discard the re-
// mainder of the drag events before returning, which is what
// this macro does.
//
// FIXME: make this into a template function after clang learns
// to handle function templates that are coroutines.
#define NO_DRAG( ... )                       \
  {                                          \
    lg.debug( "cannot drag: " __VA_ARGS__ ); \
    co_await eat_remaining_drag_events();    \
    co_return;                               \
  }

wait<> eat_remaining_drag_events() {
  while( true ) {
    input::event_t event = co_await g_input.next();
    auto drag = event.get_if<input::mouse_drag_event_t>();
    if( !drag ) continue;
    CHECK( drag->state.phase != input::e_drag_phase::begin )
    if( drag->state.phase != input::e_drag_phase::end ) continue;
    break;
  }
}

// Note that the mouse events received here will be relative to
// the colony-view canvas.
wait<> drag_drop_routine(
    input::mouse_drag_event_t const& event ) {
  CHECK( event.state.phase == input::e_drag_phase::begin );
  CHECK( !g_drag_state.has_value() );
  if( event.button != input::e_mouse_button::l )
    NO_DRAG( "wrong mouse button." );
  Coord const& origin = event.state.origin;

  // First get the view under the mouse at the drag origin.
  ColonySubView&              top_view = colview_top_level();
  maybe<PositionedColSubView> maybe_source_p_view =
      top_view.view_here( origin );
  if( !maybe_source_p_view )
    NO_DRAG( "there is no view to serve an object." );
  ColonySubView& source_view = *maybe_source_p_view->col_view;
  Coord const&   source_upper_left =
      maybe_source_p_view->upper_left;

  // Get entity ID from source view.
  maybe<e_colview_entity> source_entity = source_view.entity();
  if( !source_entity )
    NO_DRAG( "source view has no entity ID." );

  // Next check if there is an object under the cursor.
  maybe<ColViewObjectWithBounds> source_bounded_object =
      source_view.object_here(
          origin.with_new_origin( source_upper_left ) );
  if( !source_bounded_object )
    NO_DRAG( "there is no object under the cursor." );
  ColViewObject_t source_object = source_bounded_object->obj;
  Rect            source_object_bounds =
      source_bounded_object->bounds.as_if_origin_were(
          source_upper_left );

  // Next check if source view allows dragging from that spot.
  maybe<IColViewDragSource&> maybe_drag_source =
      source_view.drag_source();
  if( !maybe_drag_source )
    NO_DRAG(
        "the source view does not allow dragging in general." );
  IColViewDragSource& drag_source = *maybe_drag_source;

  bool can_drag = drag_source.try_drag(
      source_object,
      origin.with_new_origin( source_upper_left ) );
  // This ensures that if the coroutine is interrupted somewhere
  // during the drag (e.g. early return, or cancellation) then
  // the source object will be told about it so that it can go
  // back to normal rendering of the dragged object.
  SCOPE_EXIT( drag_source.cancel_drag() );
  if( !can_drag )
    NO_DRAG(
        "the source view does not allow dragging object {}.",
        source_object );

  // The drag has now started.
  lg.debug( "dragging {} with bounds {}.", source_object,
            source_object_bounds );

  g_drag_state = drag::State<ColViewObject_t>{
      .stream              = {}, // FIXME: unused
      .object              = source_object,
      .indicator           = drag::e_status_indicator::none,
      .user_requests_input = event.mod.shf_down,
      .where               = origin,
      .click_offset = origin - source_object_bounds.upper_left(),
  };
  SCOPE_EXIT( g_drag_state = nothing );

  // The first drag event also contains some motion that we want
  // to process.
  input::event_t latest    = event;
  Coord          mouse_pos = origin;
  goto have_event;

  while( true ) {
    if( auto drag = latest.get_if<input::mouse_drag_event_t>();
        drag.has_value() &&
        drag->state.phase == input::e_drag_phase::end ) {
      // The drag has ended but with no dice, so rubber-band the
      // dragged object back to its source.
      lg.debug( "drag rejected." );
      break;
    }
    latest = co_await g_input.next();
    // Optional sanity check.
    if( auto drag =
            latest.get_if<input::mouse_drag_event_t>() ) {
      CHECK( drag->state.phase != input::e_drag_phase::begin );
    }

    if( auto win_event = latest.get_if<input::win_event_t>();
        win_event &&
        win_event->type == input::e_win_event_type::resized ) {
      // If the window is getting resized then our coordinates
      // might get messed up. It seems safest just ot cancel the
      // drag.
      lg.warn(
          "cancelling drag operation due to window resize." );
      //
      // WARNING: do not force a re-composite here, since there
      // are SCOPE_EXIT's above that are hanging onto pointers
      // into the views, that we don't want to invalidate. FIXME:
      // to fix this, we need to have a way to re-composite
      // without invalidating the pointers to the views.
      //
      // We know that this event is not the last drag event,
      // since it is a window resize event.  So we need to eat
      // the remainder of the drag events.
      co_await eat_remaining_drag_events();
      co_return; // no rubber banding.
    }

  have_event:
    auto drag_event = latest.get_if<input::mouse_drag_event_t>();
    if( drag_event.has_value() ) mouse_pos = drag_event->pos;
    // We allow non-drag events to pass through here so that we
    // can update the rendering of the mouse cursor in response
    // to key events, in particular when the user presses the
    // shift key to ask for user input.

    g_drag_state->where     = mouse_pos;
    g_drag_state->indicator = drag::e_status_indicator::none;
    g_drag_state->user_requests_input = false;

    // Check if there is a view under the current mouse pos.
    maybe<PositionedColSubView> maybe_target_p_view =
        top_view.view_here( mouse_pos );
    if( !maybe_target_p_view ) continue;
    ColonySubView& target_view = *maybe_target_p_view->col_view;
    Coord const&   target_upper_left =
        maybe_target_p_view->upper_left;

    // Check if the target view can accept drags.
    maybe<IColViewDragSink&> maybe_drag_sink =
        target_view.drag_sink();
    if( !maybe_drag_sink ) continue;
    IColViewDragSink& drag_sink = *maybe_drag_sink;
    Coord             sink_coord =
        mouse_pos.with_new_origin( target_upper_left );
    // Assume the drag won't work unless we find out otherwise.
    g_drag_state->indicator = drag::e_status_indicator::bad;

    // Check if the target view can receive the object that is
    // being dragged and can do so at the current mouse position.
    if( !drag_sink.can_receive( source_object, *source_entity,
                                sink_coord ) ) {
      lg.trace( "drag sink cannot accept object {}.",
                source_object );
      continue;
    }
    g_drag_state->indicator = drag::e_status_indicator::good;

    // Check if the user is allowed to request user input. If so,
    // then check if they are holding down shift, which is how
    // they do so.
    maybe<IColViewDragSourceUserInput const&> drag_user_input =
        drag_source.drag_user_input();
    if( drag_user_input ) {
      auto const& base =
          variant_base<input::event_base_t>( latest );
      g_drag_state->user_requests_input = base.mod.shf_down;
    }

    // E.g. if this is a keyboard event, then we're done with it.
    if( !drag_event ) continue;

    // At this point, we have a drag event, so we need to check
    // if this is a drag end point, and if so, check to see if
    // the item can be dropped.
    if( drag_event->state.phase != input::e_drag_phase::end )
      continue;

    // *** After this point, we should only `break` as opposed
    // to `continue`, since we have already received the end drag
    // event.

    // Check if the user wants to input anything.
    if( drag_user_input.has_value() &&
        g_drag_state->user_requests_input ) {
      maybe<ColViewObject_t> new_obj =
          co_await drag_user_input->user_edit_object();
      if( !new_obj ) {
        lg.debug( "drag of object {} cancelled by user.",
                  source_object );
        break;
      }
      source_object = *new_obj;
      lg.debug( "user requests {}.", source_object );
    }

    // Check that the target view can receive this object as it
    // currently is, and/or allow it to adjust it.
    maybe<ColViewObject_t> sink_edited = drag_sink.can_receive(
        source_object, *source_entity, sink_coord );
    if( !sink_edited ) {
      // The sink can't find a way to make it work, drag is can-
      // celled.
      lg.debug( "drag sink cannot receive object {}.",
                source_object );
      break;
    }
    if( *sink_edited != source_object )
      lg.debug( "drag sink responded with object {}.",
                *sink_edited );
    // Ideally here we would check that the new object does not
    // have a larger quantity than the dragged object, if/where
    // that makes sense. In other words, if the user is dragging
    // 50 of a commodity, the drag sink cannot request more,
    // though it can request less. Not sure how to do that in a
    // generic way though.
    source_object = *sink_edited;

    // Since the sink may have edited the object, lets make sure
    // that the source can handle it.
    bool can_drag = drag_source.try_drag(
        source_object,
        origin.with_new_origin( source_upper_left ) );
    // This ensures that if the coroutine is interrupted some-
    // where during the drag (e.g. early return, or cancellation)
    // then the source object will be told about it so that it
    // can go back to normal rendering of the dragged object.
    SCOPE_EXIT( drag_source.cancel_drag() );
    if( !can_drag ) {
      // The source and sink can't negotiate a way to make this
      // drag work, so cancel it.
      lg.debug(
          "drag source and sink cannot negotiate a draggable "
          "object, last attempt was {}.",
          source_object );
      break;
    }

    // The source and sink have agreed on an object that can be
    // transferred, so let's let the sink do a final user confir-
    // mation if it needs to.
    maybe<IColViewDragSinkConfirm const&> drag_confirm =
        drag_sink.drag_confirm();
    if( drag_confirm ) {
      bool proceed = co_await drag_confirm->confirm(
          source_object, sink_coord );
      if( !proceed ) {
        // User has cancelled the drag.
        lg.debug( "drag of object {} cancelled by user.",
                  source_object );
        break;
      }
    }

    // Finally we can do the drag.
    drag_source.disown_dragged_object();
    drag_sink.drop( source_object, sink_coord );
    // Drag happened successfully.
    lg.debug( "drag of object {} successful.", source_object );
    colview_top_level().update();
    co_return;
  }

  // Rubber-band back to starting point.
  g_drag_state->indicator = drag::e_status_indicator::none;
  g_drag_state->user_requests_input = false;

  Coord         start   = g_drag_state->where;
  Coord         end     = origin;
  double        percent = 0.0;
  AnimThrottler throttle( kAlmostStandardFrame );
  while( percent <= 1.0 ) {
    co_await throttle();
    g_drag_state->where =
        start + ( end - start ).multiply_and_round( percent );
    percent += 0.15;
  }
}

/****************************************************************
** Input Handling
*****************************************************************/
// Returns true if the user wants to exit the colony view.
wait<bool> handle_event( input::key_event_t const& event,
                         IMapUpdater& ) {
  if( event.change != input::e_key_change::down )
    co_return false;
  switch( event.keycode ) {
    case ::SDLK_ESCAPE: //
      co_return true;
    default: //
      break;
  }
  co_return false;
}

// Returns true if the user wants to exit the colony view.
wait<bool> handle_event(
    input::mouse_button_event_t const& event, IMapUpdater& ) {
  if( event.buttons != input::e_mouse_button_event::left_up )
    co_return false;
  Coord click_pos = event.pos;
  co_await colview_top_level().perform_click( click_pos );
  co_return false;
}

wait<bool> handle_event( input::win_event_t const& event,
                         IMapUpdater& map_updater ) {
  if( event.type == input::e_win_event_type::resized )
    // Force a re-composite.
    set_colview_colony( g_colony_id, map_updater );
  co_return false;
}

wait<bool> handle_event( input::mouse_drag_event_t const& event,
                         IMapUpdater& ) {
  co_await drag_drop_routine( event );
  co_return false;
}

wait<bool> handle_event( auto const&, IMapUpdater& ) {
  co_return false;
}

// Remove all input events from the queue corresponding to normal
// user input, but save the ones that we always need to process,
// such as window resize events (which are needed to maintain
// proper drawing as the window is resized).
void clear_non_essential_events() {
  vector<input::event_t> saved;
  while( g_input.ready() ) {
    input::event_t e = g_input.next().get();
    switch( e.to_enum() ) {
      case input::e_input_event::win_event:
        saved.push_back( std::move( e ) );
        break;
      default: break;
    }
  }
  CHECK( !g_input.ready() );
  // Re-insert the ones we want to save.
  for( input::event_t& e : saved )
    g_input.send( std::move( e ) );
}

wait<> run_colview( IMapUpdater& map_updater ) {
  while( true ) {
    input::event_t event = co_await g_input.next();
    auto [exit, suspended] =
        co_await co::detect_suspend( std::visit(
            LC( handle_event( _, map_updater ) ), event ) );
    if( suspended ) clear_non_essential_events();
    if( exit ) co_return;
  }
}

/****************************************************************
** Colony Plane
*****************************************************************/
struct ColonyPlane : public Plane {
  ColonyPlane() = default;

  bool covers_screen() const override { return true; }

  // FIXME: find a better way to do this. One idea is that when
  // the compositor changes the layout it will inject a window
  // resize event into the input queue that will then be automat-
  // ically picked up by all of the planes.
  void advance_state() override {
    UNWRAP_CHECK(
        new_canvas,
        compositor::section( compositor::e_section::normal ) );
    if( new_canvas != canvas_ ) {
      canvas_ = new_canvas;
      // This is slightly hacky since this is not a real window
      // resize event, but it'll do for now. Doing it this way
      // will ensure that 1) we wake up the input-processing
      // coroutine which is likely asleep waiting for input
      // events, and 2) we allow the same logic to handle this
      // recompositing that is used for real window resize
      // events, which is good because it has some safeguards
      // built in to it, such as not allowing a recomposite
      // during a drag operation (which would cause dangling
      // pointers; see the comment about that in the dragging
      // coroutine. This should probably be fixed).
      g_input.send( input::win_event_t{
          input::event_base_t{},
          /*type=*/input::e_win_event_type::resized } );
    }
  }

  void draw( rr::Renderer& renderer ) const override {
    draw_colony_view( renderer, g_colony_id );
    if( g_drag_state.has_value() )
      colview_drag_n_drop_draw( renderer, *g_drag_state,
                                canvas_.upper_left() );
  }

  e_input_handled input( input::event_t const& event ) override {
    input::event_t event_translated = move_mouse_origin_by(
        event, canvas_.upper_left().distance_from_origin() );
    g_input.send( event_translated );
    if( event_translated.holds<input::win_event_t>() )
      // Generally we should return no here because this is an
      // event that we want all planes to see. FIXME: need to
      // find a better way to handle this automatically.
      return e_input_handled::no;
    return e_input_handled::yes;
  }

  Plane::e_accept_drag can_drag(
      input::e_mouse_button /*button*/,
      Coord /*origin*/ ) override {
    if( g_drag_state ) return Plane::e_accept_drag::swallow;
    return e_accept_drag::yes_but_raw;
  }

  Rect canvas_;
};

ColonyPlane g_colony_plane;

} // namespace

/****************************************************************
** Public API
*****************************************************************/
Plane* colony_plane() { return &g_colony_plane; }

wait<> show_colony_view( ColonyId     id,
                         IMapUpdater& map_updater ) {
  CHECK( colony_exists( id ) );
  reset_globals();
  g_colony_id = id;
  set_colview_colony( id, map_updater );
  ScopedPlanePush pusher( e_plane_config::colony );
  lg.info( "viewing colony {}.",
           colony_from_id( id ).debug_string() );
  co_await run_colview( map_updater );
  lg.info( "leaving colony view." );
}

} // namespace rn
