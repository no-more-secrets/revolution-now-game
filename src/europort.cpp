/****************************************************************
**europort.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2019-07-08.
*
* Description:
*
*****************************************************************/
#include "europort.hpp"

// Revolution Now
#include "error.hpp"
#include "logging.hpp"
#include "lua.hpp"
#include "ustate.hpp"
#include "variant.hpp"

// base
#include "base/lambda.hpp"

// base-util
#include "base-util/algo.hpp"

using namespace std;

namespace rn {

namespace {

template<typename Func>
vector<UnitId> units_in_europort_filtered( Func&& func ) {
  vector<UnitId> res = units_in_euro_port_view();
  erase_if( res, not_fn( std::forward<Func>( func ) ) );
  return res;
}

} // namespace

/****************************************************************
** Public API
*****************************************************************/
bool is_unit_on_dock( UnitId id ) {
  auto europort_status = unit_euro_port_view_info( id );
  return europort_status.has_value() &&
         !unit_from_id( id ).desc().ship &&
         holds<UnitEuroPortViewState::in_port>(
             *europort_status );
}

bool is_unit_inbound( UnitId id ) {
  auto europort_status = unit_euro_port_view_info( id );
  auto is_inbound =
      europort_status.has_value() &&
      holds<UnitEuroPortViewState::inbound>( *europort_status );
  if( is_inbound ) { CHECK( unit_from_id( id ).desc().ship ); }
  return is_inbound;
}

bool is_unit_outbound( UnitId id ) {
  auto europort_status = unit_euro_port_view_info( id );
  auto is_outbound =
      europort_status.has_value() &&
      holds<UnitEuroPortViewState::outbound>( *europort_status );
  if( is_outbound ) { CHECK( unit_from_id( id ).desc().ship ); }
  return is_outbound;
}

bool is_unit_in_port( UnitId id ) {
  auto europort_status = unit_euro_port_view_info( id );
  return europort_status.has_value() &&
         unit_from_id( id ).desc().ship &&
         holds<UnitEuroPortViewState::in_port>(
             *europort_status );
}

vector<UnitId> europort_units_on_dock() {
  vector<UnitId> res =
      units_in_europort_filtered( is_unit_on_dock );
  // Now we must order the units by their arrival time in port
  // (or on dock).
  sort( res.begin(), res.end() );
  return res;
}

vector<UnitId> europort_units_in_port() {
  vector<UnitId> res =
      units_in_europort_filtered( is_unit_in_port );
  // Now we must order the units by their arrival time in port
  // (or on dock).
  sort( res.begin(), res.end() );
  return res;
}

// To old world.
vector<UnitId> europort_units_inbound() {
  return units_in_europort_filtered( is_unit_inbound );
}

// To new world.
vector<UnitId> europort_units_outbound() {
  return units_in_europort_filtered( is_unit_outbound );
}

void unit_sail_to_old_world( UnitId id ) {
  // FIXME: do other checks here, e.g., make sure that the
  //        ship is not damaged.
  CHECK( unit_from_id( id ).desc().ship );
  // This is the state to which we will set the unit, at least by
  // default (though it might get modified below based on the
  // current state of the unit).
  UnitEuroPortViewState_t target_state =
      UnitEuroPortViewState::inbound{ /*progress=*/0.0 };
  auto maybe_state = unit_euro_port_view_info( id );
  if( maybe_state ) {
    switch( auto& v = *maybe_state; v.to_enum() ) {
      case UnitEuroPortViewState::e::inbound: {
        auto& val = v.get<UnitEuroPortViewState::inbound>();
        // no-op, i.e., keep state the same.
        target_state = val;
        break;
      }
      case UnitEuroPortViewState::e::outbound: {
        auto& [percent] =
            v.get<UnitEuroPortViewState::outbound>();
        if( percent > 0.0 ) {
          // Unit must "turn around" and go the other way.
          target_state = UnitEuroPortViewState::inbound{
              /*progress=*/( 1.0 - percent ) };
        } else {
          // Unit has not yet made any progress, so we can imme-
          // diately move it to in_port.
          target_state = UnitEuroPortViewState::in_port{};
        }
        break;
      }
      case UnitEuroPortViewState::e::in_port: {
        auto& val = v.get<UnitEuroPortViewState::in_port>();
        // no-op, unit is already in port.
        target_state = val;
        break;
      }
    }
  }
  if( target_state == maybe_state ) //
    return;
  lg.info( "setting {} to state {}", debug_string( id ),
           target_state );
  // Note: unit may already be in a europort state here.
  ustate_change_to_euro_port_view( id, target_state );
}

void unit_sail_to_new_world( UnitId id ) {
  // FIXME: do other checks here, e.g., make sure that the
  //        ship is not damaged.
  CHECK( unit_from_id( id ).desc().ship );
  // This is the state to which we will set the unit, at least by
  // default (though it might get modified below based on the
  // current state of the unit).
  UnitEuroPortViewState_t target_state =
      UnitEuroPortViewState::outbound{ /*progress=*/0.0 };
  auto maybe_state = unit_euro_port_view_info( id );
  CHECK( maybe_state );
  switch( auto& v = *maybe_state; v.to_enum() ) {
    case UnitEuroPortViewState::e::outbound: {
      auto& val = v.get<UnitEuroPortViewState::outbound>();
      // no-op, i.e., keep state the same.
      target_state = val;
      break;
    }
    case UnitEuroPortViewState::e::inbound: {
      auto& [percent] = v.get<UnitEuroPortViewState::inbound>();
      if( percent > 0.0 ) {
        // Unit must "turn around" and go the other way.
        target_state = UnitEuroPortViewState::outbound{
            /*progress=*/( 1.0 - percent ) };
      } else {
        NOT_IMPLEMENTED; // find a place on the map to move to.
      }
      break;
    }
    case UnitEuroPortViewState::e::in_port: {
      // keep default target.
      break;
    }
  }
  if( target_state == maybe_state ) //
    return;
  lg.info( "setting {} to state {}", debug_string( id ),
           target_state );
  // Note: unit may already be in a europort state here.
  ustate_change_to_euro_port_view( id, target_state );
}

void unit_move_to_europort_dock( UnitId id ) {
  if( is_unit_on_dock( id ) ) return;
  auto holder = is_unit_onboard( id );
  CHECK( holder && is_unit_in_port( *holder ),
         "cannot move unit to dock unless it is in the cargo of "
         "a ship that is in port." );
  ustate_change_to_euro_port_view(
      id, UnitEuroPortViewState::in_port{} );
  DCHECK( is_unit_on_dock( id ) );
  DCHECK( !is_unit_onboard( id ) );
}

void advance_unit_on_high_seas( UnitId id ) {
  UNWRAP_CHECK( info, unit_euro_port_view_info( id ) );
  constexpr double const advance = 0.2;
  if_get( info, UnitEuroPortViewState::outbound, outbound ) {
    outbound.percent += advance;
    if( outbound.percent >= 1.0 ) {
      NOT_IMPLEMENTED; // find a place on the map to move to.
    }
    return;
  }
  if_get( info, UnitEuroPortViewState::inbound, inbound ) {
    inbound.percent += advance;
    if( inbound.percent >= 1.0 )
      ustate_change_to_euro_port_view(
          id, UnitEuroPortViewState::in_port{} );
    return;
  }
  FATAL( "{} is not on the high seas.", debug_string( id ) );
}

/****************************************************************
** Lua Bindings
*****************************************************************/
namespace {

LUA_FN( create_unit_in_port, UnitId, e_nation nation,
        e_unit_type type ) {
  auto id = create_unit( nation, type );
  ustate_change_to_euro_port_view(
      id, UnitEuroPortViewState::in_port{} );
  lg.info( "created a {} in {} port/dock.",
           unit_desc( type ).name,
           nation_obj( nation ).adjective );
  return id;
}

LUA_FN( unit_sail_to_new_world, void, UnitId id ) {
  unit_sail_to_new_world( id );
}

LUA_FN( advance_unit_on_high_seas, void, UnitId id ) {
  advance_unit_on_high_seas( id );
}

} // namespace

} // namespace rn
