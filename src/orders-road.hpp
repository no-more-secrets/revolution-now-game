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

std::unique_ptr<OrdersHandler> handle_orders(
    UnitId id, orders::road const& road, IMapUpdater*,
    IGui& gui );

} // namespace rn
