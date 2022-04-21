/****************************************************************
**map-gen.hpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2022-04-21.
*
* Description: Game map generator.
*
*****************************************************************/
#pragma once

#include "core-config.hpp"

// Revolution Now
#include "game-state.hpp"

namespace rn {

void generate_terrain( TerrainState& terrain_state );

void linker_dont_discard_module_map_gen();

} // namespace rn
