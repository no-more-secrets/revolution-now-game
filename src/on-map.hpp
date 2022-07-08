/****************************************************************
**on-map.hpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2022-05-18.
*
* Description: Handles actions that need to be take in response
*              to a unit appearing on a map square (after
*              creation or moving).
*
*****************************************************************/
#pragma once

#include "core-config.hpp"

// Revolution Now
#include "unit-id.hpp"
#include "wait.hpp"

// gfx
#include "gfx/coord.hpp"

namespace rn {

struct IMapUpdater;
struct TerrainState;
struct UnitsState;
struct SettingsState;
struct IGui;
struct Player;

// A dummy type to help remind the caller that the unit may have
// disappeared as a result of the call. This works because maybe
// types are [[nodiscard]].
struct UnitDeleted {};

// Whenever a unit is placed on a map square for any reason
// (whether they moved there, were created there, appeared there,
// etc.) this must be called to perform the correct game updates
// (and that includes moving the unit to the target square, which
// this function will do).
//
// WARNING: After this function completes, the unit may no longer
// exist since they might stepped into a lost city rumor and dis-
// appeared! Or new units could have been created, etc.
wait<maybe<UnitDeleted>> unit_to_map_square(
    UnitsState& units_state, TerrainState const& terrain_state,
    Player& player, SettingsState const& settings, IGui& gui,
    IMapUpdater& map_updater, UnitId id, Coord world_square );

// This is the non-coroutine version of the above, only to be
// called from non-coroutines where you know that this action
// won't need to trigger any UI actions.
void unit_to_map_square_non_interactive(
    UnitsState& units_state, IMapUpdater& map_updater, UnitId id,
    Coord world_square );

} // namespace rn
