/****************************************************************
**units.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2022-12-21.
*
* Description: Unit tests for the src/ss/units.* module.
*
*****************************************************************/
#include "test/testing.hpp"

// Under test.
#include "src/ss/units.hpp"

// Testing
#include "test/fake/world.hpp"

// ss
#include "src/ss/dwelling.rds.hpp"

// Must be last.
#include "test/catch-common.hpp"

namespace rn {
namespace {

using namespace std;

/****************************************************************
** Fake World Setup
*****************************************************************/
struct World : testing::World {
  using Base = testing::World;
  World() : Base() {
    add_default_player();
    create_default_map();
  }

  void create_default_map() {
    MapSquare const   _ = make_ocean();
    MapSquare const   L = make_grassland();
    vector<MapSquare> tiles{
        _, L, _, //
        L, L, L, //
        _, L, L, //
    };
    build_map( std::move( tiles ), 3 );
  }
};

/****************************************************************
** Test Cases
*****************************************************************/
TEST_CASE( "[units] dwelling_for/from_dwelling" ) {
  World W;

  Dwelling const& dwelling1 =
      W.add_dwelling( { .x = 1, .y = 1 }, e_tribe::apache );
  Dwelling const& dwelling2 =
      W.add_dwelling( { .x = 1, .y = 2 }, e_tribe::inca );
  NativeUnit const& unit_id1 =
      W.add_unit_on_map( e_native_unit_type::armed_brave,
                         { .x = 0, .y = 0 }, dwelling1.id );
  NativeUnit const& unit_id2 =
      W.add_unit_on_map( e_native_unit_type::brave,
                         { .x = 0, .y = 1 }, dwelling2.id );

  REQUIRE( W.units().dwelling_for( unit_id1.id ) ==
           dwelling1.id );
  REQUIRE( W.units().dwelling_for( unit_id2.id ) ==
           dwelling2.id );

  REQUIRE( W.units().from_dwelling( dwelling1.id ) ==
           unit_id1.id );
  REQUIRE( W.units().from_dwelling( dwelling2.id ) ==
           unit_id2.id );
  REQUIRE( W.units().from_dwelling( DwellingId{ 3 } ) ==
           nothing );
}

} // namespace
} // namespace rn
