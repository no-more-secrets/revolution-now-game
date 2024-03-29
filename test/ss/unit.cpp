/****************************************************************
**unit.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2022-03-26.
*
* Description: Unit tests for the src/unit.* module.
*
*****************************************************************/
#include "test/testing.hpp"

// ss
#include "src/ss/player.rds.hpp"
#include "src/ss/unit.hpp"

// Revolution Now
#include "src/ustate.hpp"

// Must be last.
#include "test/catch-common.hpp"

namespace rn {
namespace {

using namespace std;

TEST_CASE( "[test/unit] consume_20_tools pioneer" ) {
  UnitComposition comp =
      UnitComposition::create( e_unit_type::pioneer );
  Player player;
  player.nation = e_nation::english;
  Unit unit     = create_unregistered_unit( player, comp );

  // Initially.
  REQUIRE( unit.type() == e_unit_type::pioneer );
  REQUIRE( unit.composition()[e_unit_inventory::tools] == 100 );
  // Consume.
  unit.consume_20_tools( player );
  REQUIRE( unit.type() == e_unit_type::pioneer );
  REQUIRE( unit.composition()[e_unit_inventory::tools] == 80 );
  // Consume.
  unit.consume_20_tools( player );
  REQUIRE( unit.type() == e_unit_type::pioneer );
  REQUIRE( unit.composition()[e_unit_inventory::tools] == 60 );
  // Consume.
  unit.consume_20_tools( player );
  REQUIRE( unit.type() == e_unit_type::pioneer );
  REQUIRE( unit.composition()[e_unit_inventory::tools] == 40 );
  // Consume.
  unit.consume_20_tools( player );
  REQUIRE( unit.type() == e_unit_type::pioneer );
  REQUIRE( unit.composition()[e_unit_inventory::tools] == 20 );
  // Consume.
  unit.consume_20_tools( player );
  REQUIRE( unit.type() == e_unit_type::free_colonist );
  REQUIRE( unit.composition()[e_unit_inventory::tools] == 0 );
}

TEST_CASE( "[test/unit] consume_20_tools hardy_pioneer" ) {
  UnitComposition comp =
      UnitComposition::create( e_unit_type::hardy_pioneer );
  Player player;
  player.nation = e_nation::english;
  Unit unit     = create_unregistered_unit( player, comp );

  // Initially.
  REQUIRE( unit.type() == e_unit_type::hardy_pioneer );
  REQUIRE( unit.composition()[e_unit_inventory::tools] == 100 );
  // Consume.
  unit.consume_20_tools( player );
  REQUIRE( unit.type() == e_unit_type::hardy_pioneer );
  REQUIRE( unit.composition()[e_unit_inventory::tools] == 80 );
  // Consume.
  unit.consume_20_tools( player );
  REQUIRE( unit.type() == e_unit_type::hardy_pioneer );
  REQUIRE( unit.composition()[e_unit_inventory::tools] == 60 );
  // Consume.
  unit.consume_20_tools( player );
  REQUIRE( unit.type() == e_unit_type::hardy_pioneer );
  REQUIRE( unit.composition()[e_unit_inventory::tools] == 40 );
  // Consume.
  unit.consume_20_tools( player );
  REQUIRE( unit.type() == e_unit_type::hardy_pioneer );
  REQUIRE( unit.composition()[e_unit_inventory::tools] == 20 );
  // Consume.
  unit.consume_20_tools( player );
  REQUIRE( unit.type() == e_unit_type::hardy_colonist );
  REQUIRE( unit.composition()[e_unit_inventory::tools] == 0 );
}

} // namespace
} // namespace rn
