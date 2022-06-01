/****************************************************************
**colony-mfg.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2019-12-18.
*
* Description: All things related to the jobs colonists can have
*              by working in the buildings in the colony.
*
*****************************************************************/
#include "colony-mfg.hpp"

// Revolution Now
#include "lua.hpp"

// luapp
#include "luapp/rtable.hpp"
#include "luapp/state.hpp"
#include "luapp/types.hpp"

using namespace std;

namespace rn {

namespace {} // namespace

/****************************************************************
** Public API
*****************************************************************/
// ...

void linker_dont_discard_module_colony_mfg() {}

/****************************************************************
** Lua Bindings
*****************************************************************/
LUA_ENUM( colony_building );

} // namespace rn
