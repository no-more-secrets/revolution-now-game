/****************************************************************
**turn.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2018-08-31.
*
* Description: Main loop that processes a turn.
*
*****************************************************************/
#include "turn.hpp"

// Revolution Now
#include "logging.hpp"
#include "loops.hpp"
#include "movement.hpp"
#include "ownership.hpp"
#include "render.hpp"
#include "unit.hpp"
#include "viewport.hpp"

// base-util
#include "base-util/variant.hpp"

// C++ standard library
#include <algorithm>
#include <deque>

namespace rn {

namespace {} // namespace

e_turn_result turn() {
  // start of turn:

  // Mark all units as not having moved.
  reset_moves();

  //  clang-format off
  //  Iterate through the colonies, for each:
  //  TODO

  //    * advance state of the colony

  //    * display messages to user any/or show animations where
  //    necessary

  //    * allow them to enter colony when events happens; in that
  //    case
  //      go to the colony screen game loop.  When the user exits
  //      the colony screen then this colony iteration
  //      immediately proceeds; i.e., user cannot enter any other
  //      colonies.  This prevents the user from making
  //      last-minute changes to colonies that have not yet been
  //      advanced in this turn (otherwise that might allow
  //      cheating in some way).

  //    * during this time, the user is not free to scroll
  //      map (menus?) or make any changes to units.  They are
  //      also not allowed to enter colonies apart from the one
  //      that has just been processed.

  //  Advance the state of the old world, possibly displaying
  //  messages to the user where necessary.
  //  clang-format on

  // If no units need to take orders this turn then we need to
  // pause at the end of the turn to allow the user to take
  // control or make changes.  In that case, the flag will remain
  // true.  On the other hand, if at least one unit takes orders
  // then that means that the user will at least have that
  // opportunity to have control and so then we don't need to
  // pause at the end of the turn.  This flag controls that.
  // TODO: consider using a flag type from type_safe
  auto need_eot_loop{true};

  ViewportRenderOptions viewport_options;

  RenderStacker push_renderer( [&viewport_options] {
    render_world_viewport( viewport_options );
    render_copy_viewport_texture();
  } );

  // We keep looping until all units that need moving have moved.
  // We don't know this list a priori because some units may de-
  // cide to require orders during the course of this process,
  // and this could happen for various reasons. Perhaps even
  // units could be created during this process (?).
  while( true ) {
    auto units    = units_all( player_nationality() );
    auto finished = []( UnitId id ) {
      return unit_from_id( id ).finished_turn();
    };
    if( all_of( units.begin(), units.end(), finished ) ) break;

    std::deque<UnitId> q;
    for( auto id : units ) q.push_back( id );

    //  Iterate through all units, for each:
    while( !q.empty() ) {
      auto& unit = unit_from_id( q.front() );
      if( unit.finished_turn() ) {
        q.pop_front();
        continue;
      }
      auto id = unit.id();
      logger->debug( "processing turn for unit {}",
                     debug_string( id ) );
      auto coords = coords_for_unit( id );
      viewport().ensure_tile_surroundings_visible( coords );

      //    clang-format off
      //
      //    * if it is it in `goto` mode focus on it and advance
      //      it
      //
      //    * if it is a ship on the high seas then advance it
      //        if it has arrived in the old world then jump to
      //        the old world screen (maybe ask user whether they
      //        want to ignore), which has its own game loop (see
      //        old-world loop).
      //
      //    * if it is in the old world then ignore it, or
      //      possibly remind the user it is there.
      //
      //    * if it is performing an action, such as building a
      //      road, advance the state.  If it finishes then mark
      //      it as active so that it will wait for orders in the
      //      next step.
      //
      //    * if it is in an indian village then advance it, and
      //      mark it active if it is finished.

      //    * if unit is waiting for orders then focus on it, and
      //      enter a realtime game loop where the user can
      //      interact with the map and GUI in general.  See
      //      `unit orders` game loop.
      //
      //    clang-format on
      while( q.front() == id &&
             unit.orders_mean_input_required() &&
             !unit.moved_this_turn() ) {
        need_eot_loop = false;

        orders_loop_result res;

        /***************************************************/
        viewport_options.reset();
        viewport_options.unit_to_blink = id;
        frame_throttler( [&res] {
          res = loop_orders();
          return ( res.type !=
                   orders_loop_result::e_type::none );
        } );
        /***************************************************/

        switch( res.type ) {
          case orders_loop_result::e_type::none:
            SHOULD_NOT_BE_HERE;
            break;
          case orders_loop_result::e_type::quit_game:
            return e_turn_result::quit;
          case orders_loop_result::e_type::orders_received:
            // Handle this below.
            break;
        }
        if( util::holds<orders::wait_t>( res.orders ) ) {
          q.push_back( q.front() );
          q.pop_front();
          break;
        }
        auto analysis =
            analyze_proposed_orders( id, res.orders );
        if( confirm_explain_orders( analysis ) ) {
          // Check if the unit is physically moving; usually at
          // this point it will be unless it is e.g. a ship
          // offloading units.
          GET_IF( analysis.result, ProposedMoveAnalysisResult,
                  mv_res ) {
            /***************************************************/
            viewport().ensure_tile_surroundings_visible(
                coords );
            viewport_options.reset();
            viewport_options.units_to_skip.insert( id );
            double         percent               = 0;
            constexpr auto min_velocity          = 0;
            constexpr auto max_velocity          = .1;
            constexpr auto initial_velocity      = .1;
            constexpr auto mag_acceleration      = 1;
            constexpr auto mag_drag_acceleration = .004;

            DissipativeVelocity percent_vel(
                /*min_velocity=*/min_velocity,
                /*max_velocity=*/max_velocity,
                /*initial_velocity=*/initial_velocity,
                /*mag_acceleration=*/
                mag_acceleration, // not relevant
                                  /*mag_drag_acceleration=*/
                mag_drag_acceleration );

            RenderStacker push_renderer(
                [id, &mv_res, &percent] {
                  render_mv_unit( id, mv_res->coords, percent );
                  render_copy_viewport_texture();
                } );
            frame_throttler( [&percent, &percent_vel] {
              return loop_mv_unit( percent, percent_vel );
            } );
            /***************************************************/
          }
          apply_orders( id, analysis );
          // Note that it shouldn't hurt if the unit is already
          // in the queue, since this turn code will never move a
          // unit after it has already completed its turn, no
          // matter how many times it appears in the queue.
          for( auto prioritize : analysis.units_to_prioritize() )
            q.push_front( prioritize );
        }
      }

      // Now we must decide if the unit has finished its turn.
      // TODO: try to put thsi in the unit class.
      if( unit.moved_this_turn() ||
          !unit.orders_mean_input_required() )
        unit.finish_turn();
    }
  }

  //    clang-format off
  //    * Make AI moves
  //        Make European moves
  //        Make Native moves
  //        Make expeditionary force moves
  //      TODO

  //    * if no player units needed orders then show a message
  //    somewhere
  //      that says "end of turn" and let the user interact with
  //      the map and GUI.
  //    clang-format on
  if( need_eot_loop ) {
    e_eot_loop_result res;
    /***************************************************/
    viewport_options.reset();
    frame_throttler( [&res] {
      res = loop_eot();
      return ( res != e_eot_loop_result::none );
    } );
    /***************************************************/
    switch( res ) {
      case e_eot_loop_result::quit_game:
        return e_turn_result::quit;
      case e_eot_loop_result::none: break;
    };
  }
  return e_turn_result::cont;
} // namespace rn

} // namespace rn
