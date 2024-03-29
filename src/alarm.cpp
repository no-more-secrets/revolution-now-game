/****************************************************************
**alarm.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2022-11-07.
*
* Description: Things related to alarm level of tribes and
*              dwellings.
*
*****************************************************************/
#include "alarm.hpp"

// Revolution Now
#include "map-square.hpp"
#include "native-owned.hpp"

// config
#include "config/natives.rds.hpp"

// ss
#include "ss/dwelling.rds.hpp"
#include "ss/natives.hpp"
#include "ss/player.rds.hpp"
#include "ss/ref.hpp"
#include "ss/terrain.hpp"
#include "ss/tribe.rds.hpp"

using namespace std;

namespace rn {

namespace {

[[nodiscard]] int clamp_alarm( int alarm ) {
  return clamp( alarm, 0, 99 );
}

[[nodiscard]] int clamp_round_alarm( double alarm ) {
  return clamp_alarm( lround( floor( alarm ) ) );
}

} // namespace

/****************************************************************
** Public API
*****************************************************************/
// TODO: this may not be a relevant quantity in the OG.
int effective_dwelling_alarm( SSConst const&  ss,
                              Dwelling const& dwelling,
                              e_nation        nation ) {
  Tribe const& tribe = ss.natives.tribe_for( dwelling.tribe );
  if( !tribe.relationship[nation].has_value() ) return 0;
  int const tribal_alarm =
      tribe.relationship[nation]->tribal_alarm;
  CHECK_GE( tribal_alarm, 0 );
  CHECK_LT( tribal_alarm, 100 );
  int const dwelling_only_alarm =
      dwelling.relationship[nation].dwelling_only_alarm;
  CHECK_GE( dwelling_only_alarm, 0 );
  CHECK_LT( dwelling_only_alarm, 100 );
  // The alarm A is given by:
  //
  //   A = 1-(1-x)*(1-y) then rescaled to be [0,100).
  //
  // This formula guarantees that:
  //
  //   A >= x  and  A >= y
  //
  // so i.e. the effective alarm is always at least as large as
  // the individual dwelling-only alarm and tribal alarm.
  //
  int const effective_alarm = lround(
      ( 1.0 - ( 1.0 - tribal_alarm / 100.0 ) *
                  ( 1.0 - dwelling_only_alarm / 100.0 ) ) *
      100.0 );
  return clamp_alarm( effective_alarm );
}

e_enter_dwelling_reaction reaction_for_dwelling(
    SSConst const& ss, Player const& player, Tribe const& tribe,
    Dwelling const& dwelling ) {
  if( !tribe.relationship[player.nation].has_value() )
    // Not yet made contact.
    return e_enter_dwelling_reaction::wave_happily;
  UNWRAP_CHECK( relationship,
                tribe.relationship[player.nation] );
  if( relationship.at_war )
    return e_enter_dwelling_reaction::scalps_and_war_drums;
  int const effective_alarm =
      effective_dwelling_alarm( ss, dwelling, player.nation );
  CHECK_GE( effective_alarm, 0 );
  CHECK_LT( effective_alarm, 100 );
  // The below assumes there are five elements in the enum; if
  // that ever changes then the calculation must be redone.
  static_assert( refl::enum_count<e_enter_dwelling_reaction> ==
                 5 );
  // 100/20 == 5
  int const category = effective_alarm / 20;
  return static_cast<e_enter_dwelling_reaction>( category );
}

void increase_tribal_alarm_from_land_grab(
    SSConst const& ss, Player const& player,
    TribeRelationship& relationship, Coord tile ) {
  auto& conf = config_natives.alarm.land_grab;

  // Base.
  double delta = conf.tribal_increase[ss.settings.difficulty];

  // Prime resource penalty.
  MapSquare const& square = ss.terrain.square_at( tile );
  if( effective_resource( square ).has_value() )
    delta *= conf.prime_resource_scale;

  // Distance falloff.
  UNWRAP_CHECK( dwelling_id,
                is_land_native_owned( ss, player, tile ) );
  Dwelling const& dwelling =
      ss.natives.dwelling_for( dwelling_id );
  int const rect_distance = std::max(
      dwelling.location.concentric_square_distance( tile ) - 1,
      0 );
  delta *= pow( conf.distance_factor, rect_distance );

  CHECK_GE( delta, 0.0 );
  relationship.tribal_alarm =
      clamp_round_alarm( relationship.tribal_alarm + delta );
  relationship.tribal_alarm =
      std::max( relationship.tribal_alarm,
                config_natives.alarm
                    .minimum_tribal_alarm[dwelling.tribe] );
}

} // namespace rn
