/****************************************************************
**cheat.hpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2022-06-16.
*
* Description: Implements cheat mode.
*
*****************************************************************/
#pragma once

#include "core-config.hpp"

// Rds
#include "cheat.rds.hpp"

// Revolution Now
#include "wait.hpp"

// gs
#include "gs/unit-id.hpp"

namespace rn {

struct ColoniesState;
struct Colony;
struct IGui;
struct IMapUpdater;
struct Unit;
struct UnitsState;

/****************************************************************
** In Colony View
*****************************************************************/
// Allows adding and removing buildings individually or in bulk.
wait<> cheat_colony_buildings( Colony& colony, IGui& gui );

// Allows changing a unit's type based on what it's doing, if
// anything. The unit's activity wil be queried, and it will be
// upgraded based on that. Petty criminal always goes to inden-
// tured servant, which goes to free colonist. A free colonist
// will be upgraded to an expert if there is a relevant activity.
// An expert with no activity won't be changed, but an expert
// with a different activity will be switched. If the unit is a
// derived type then it will attempt to promote it without
// changing its occupation. This is to replicate the original
// game's cheat feature where you can select a unit (at least in
// the colony view) and it will be upgraded based on what it is
// currently doing or being.
void cheat_upgrade_unit_expertise(
    UnitsState const&    units_state,
    ColoniesState const& colonies_state, Unit& unit );

void cheat_downgrade_unit_expertise( Unit& unit );

UnitId cheat_create_new_colonist( UnitsState&   units_state,
                                  IMapUpdater&  map_updater,
                                  Colony const& colony );

} // namespace rn