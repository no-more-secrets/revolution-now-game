/****************************************************************
**orders-disband.hpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2021-04-16.
*
* Description: Carries out orders to disband a unit.
*
*****************************************************************/
#pragma once

#include "core-config.hpp"

// Revolution Now
#include "orders.hpp"

namespace rn {

struct IMapUpdater;
struct SettingsState;
struct UnitsState;
struct TerrainState;
struct Player;

std::unique_ptr<OrdersHandler> handle_orders(
    UnitId id, orders::disband const& disband,
    IMapUpdater* map_updater, IGui& gui, Player& player,
    TerrainState const& terrain_state, UnitsState& units_state,
    SettingsState const& settings );

} // namespace rn
