/****************************************************************
**europort.hpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2019-07-08.
*
* Description:
*
*****************************************************************/
#pragma once

#include "core-config.hpp"

// Revolution Now
#include "aliases.hpp"
#include "id.hpp"

namespace rn {

Vec<UnitId> europort_units_on_dock();  // Sorted by arrival.
Vec<UnitId> europort_units_in_port();  // Sorted by arrival.
Vec<UnitId> europort_units_inbound();  // to old world
Vec<UnitId> europort_units_outbound(); // to new world

// These will take a ship and make it old (new) world-bound (must
// be a ship). If it is already in this state then this is a
// no-op. If it is already bound for the opposite world then it
// is "turned around" with position retained.
//
// If a ship in the new world is told to sail to the new world
// then an error will be thrown since this is likely a logic er-
// ror. Likewise, if a ship in the old-world port is told to sail
// to the old-world then a similar thing happens.
void unit_sail_to_old_world( UnitId id );
void unit_sail_to_new_world( UnitId id );

} // namespace rn
