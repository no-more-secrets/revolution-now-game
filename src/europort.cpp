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
#include "aliases.hpp"
#include "errors.hpp"
#include "logging.hpp"
#include "ownership.hpp"

// base-util
#include "base-util/algo.hpp"
#include "base-util/variant.hpp"

// Range-v3
#include "range/v3/action/sort.hpp"
#include "range/v3/view/filter.hpp"

namespace rn {

namespace {

bool is_unit_on_dock( UnitId id ) {
  auto europort_status = unit_euro_port_view_info( id );
  return europort_status.has_value() &&
         !unit_from_id( id ).desc().boat &&
         util::holds<UnitEuroPortViewState::in_port>(
             europort_status->get() );
}

bool is_unit_inbound( UnitId id ) {
  auto europort_status = unit_euro_port_view_info( id );
  auto is_inbound      = europort_status.has_value() &&
                    util::holds<UnitEuroPortViewState::inbound>(
                        europort_status->get() );
  if( is_inbound ) { CHECK( unit_from_id( id ).desc().boat ); }
  return is_inbound;
}

bool is_unit_outbound( UnitId id ) {
  auto europort_status = unit_euro_port_view_info( id );
  auto is_outbound =
      europort_status.has_value() &&
      util::holds<UnitEuroPortViewState::outbound>(
          europort_status->get() );
  if( is_outbound ) { CHECK( unit_from_id( id ).desc().boat ); }
  return is_outbound;
}

bool is_unit_in_port( UnitId id ) {
  auto europort_status = unit_euro_port_view_info( id );
  return europort_status.has_value() &&
         unit_from_id( id ).desc().boat &&
         util::holds<UnitEuroPortViewState::in_port>(
             europort_status->get() );
}

int unit_arrival_id_throw( UnitId id ) {
  ASSIGN_CHECK_OPT( info, unit_euro_port_view_info( id ) );
  GET_CHECK_VARIANT( in_port, info.get(),
                     UnitEuroPortViewState::in_port );
  return in_port.global_arrival_id;
}

} // namespace

/****************************************************************
** Public API
*****************************************************************/
Vec<UnitId> europort_units_on_dock() {
  auto        in_euroview = units_in_euro_port_view();
  Vec<UnitId> res = in_euroview | rv::filter( is_unit_on_dock );
  // Now we must order the units by their arrival time in port
  // (or on dock).
  res |= rg::action::sort( std::less{}, unit_arrival_id_throw );
  return res;
}

Vec<UnitId> europort_units_in_port() {
  auto        in_euroview = units_in_euro_port_view();
  Vec<UnitId> res = in_euroview | rv::filter( is_unit_in_port );
  // Now we must order the units by their arrival time in port
  // (or on dock).
  res |= rg::action::sort( std::less{}, unit_arrival_id_throw );
  return res;
}

// To old world.
Vec<UnitId> europort_units_inbound() {
  auto in_euroview = units_in_euro_port_view();
  return in_euroview | rv::filter( is_unit_inbound );
}

// To new world.
Vec<UnitId> europort_units_outbound() {
  auto in_euroview = units_in_euro_port_view();
  return in_euroview | rv::filter( is_unit_outbound );
}

void unit_sail_to_old_world( UnitId id ) {
  // FIXME: do other checks here, e.g., make sure that the
  //        ship is not damaged.
  CHECK( unit_from_id( id ).desc().boat );
  // This is the state to which we will set the unit, at least by
  // default (though it might get modified below based on the
  // current state of the unit).
  auto target_state =
      UnitEuroPortViewState::inbound{/*progress=*/0.0};
  auto maybe_state = unit_euro_port_view_info( id );
  if( maybe_state ) {
    switch_( maybe_state->get() ) {
      case_( UnitEuroPortViewState::inbound ) {
        // no-op, i.e., keep state the same.
        target_state = val;
      }
      case_( UnitEuroPortViewState::outbound, percent ) {
        // Unit must "turn around" and go the other way.
        target_state = UnitEuroPortViewState::inbound{
            /*progress=*/( 1.0 - percent )};
      }
      case_( UnitEuroPortViewState::in_port ) {
        FATAL(
            "unit {} cannot sail to the old world because it is "
            "already in-port in the old world.",
            debug_string( id ) );
      }
      switch_exhaustive;
    }
  }
  lg.info( "setting unit {} to state {}", debug_string( id ),
           target_state );
  // Note: unit may already be in a europort state here.
  ownership_change_to_euro_port_view( id, target_state );
}

void unit_sail_to_new_world( UnitId id ) {
  // FIXME: do other checks here, e.g., make sure that the
  //        ship is not damaged.
  CHECK( unit_from_id( id ).desc().boat );
  // This is the state to which we will set the unit, at least by
  // default (though it might get modified below based on the
  // current state of the unit).
  auto target_state =
      UnitEuroPortViewState::outbound{/*progress=*/0.0};
  auto maybe_state = unit_euro_port_view_info( id );
  CHECK( maybe_state,
         "unit {} cannot sail to the new world because it is "
         "already in the new world.",
         debug_string( id ) );
  switch_( maybe_state->get() ) {
    case_( UnitEuroPortViewState::outbound ) {
      // no-op, i.e., keep state the same.
      target_state = val;
    }
    case_( UnitEuroPortViewState::inbound, percent ) {
      // Unit must "turn around" and go the other way.
      target_state = UnitEuroPortViewState::outbound{
          /*progress=*/( 1.0 - percent )};
    }
    case_( UnitEuroPortViewState::in_port ) {
      // keep default target.
    }
    switch_exhaustive;
  }
  lg.info( "setting unit {} to state {}", debug_string( id ),
           target_state );
  // Note: unit may already be in a europort state here.
  ownership_change_to_euro_port_view( id, target_state );
}

} // namespace rn
