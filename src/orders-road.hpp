/****************************************************************
**orders-road.hpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2022-03-25.
*
* Description: Carries out orders to build a road.
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
    UnitId id, orders::road const& road, IMapUpdater*, IGui& gui,
    Player& player, TerrainState const& terrain_state,
    UnitsState& units_state, SettingsState const& settings );

} // namespace rn
