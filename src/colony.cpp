/****************************************************************
**colony.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2019-12-15.
*
* Description: Data structure representing a Colony.
*
*****************************************************************/
#include "colony.hpp"

// Revolution Now
#include "errors.hpp"
#include "lua.hpp"

using namespace std;

namespace rn {

namespace {} // namespace

/****************************************************************
** Public API
*****************************************************************/

int Colony::population() const { return units_.size(); }

string Colony::to_string() const {
  return fmt::format(
      "Colony{{name=\"{}\",id={},nation={},coord={},population={"
      "}}}",
      name(), id(), nation(), location(), population() );
}

void Colony::add_building( e_colony_building building ) {
  CHECK( !buildings_.contains( building ),
         "Colony already has a {}.", building );
  buildings_.insert( building );
}

void Colony::add_unit( UnitId id, ColonyJob_t const& job ) {
  CHECK( !has_unit( id ), "Unit {} already in colony.", id );
  units_[id] = job;
}

void Colony::remove_unit( UnitId id ) {
  CHECK( has_unit( id ), "Unit {} is not in colony.", id );
  units_.erase( id );
}

bool Colony::has_unit( UnitId id ) const {
  return units_.contains( id );
}

} // namespace rn

/****************************************************************
** Lua Bindings
*****************************************************************/
namespace rn {
namespace {

LUA_MODULE()

LUA_STARTUP( sol::state& st ) {
  sol::usertype<Colony> colony =
      st.new_usertype<Colony>( "Colony", sol::no_constructor );

  // Getters.
  colony["id"]        = &Colony::id;
  colony["nation"]    = &Colony::nation;
  colony["name"]      = &Colony::name;
  colony["location"]  = &Colony::location;
  colony["sentiment"] = &Colony::sentiment;

  // Modifiers.
  colony["add_building"] = &Colony::add_building;
};

} // namespace
} // namespace rn
