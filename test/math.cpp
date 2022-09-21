/****************************************************************
**math.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2019-09-25.
*
* Description: Unit tests for the math module.
*
*****************************************************************/
#include "testing.hpp"

// Revolution Now
#include "math.hpp"

// Must be last.
#include "catch-common.hpp"

namespace {

using namespace std;
using namespace rn;

TEST_CASE( "[math] cyclic_modulus" ) {
  REQUIRE( rn::cyclic_modulus( 7, 3 ) == 1 );
  REQUIRE( rn::cyclic_modulus( 6, 3 ) == 0 );
  REQUIRE( rn::cyclic_modulus( 5, 3 ) == 2 );
  REQUIRE( rn::cyclic_modulus( 4, 3 ) == 1 );
  REQUIRE( rn::cyclic_modulus( 3, 3 ) == 0 );
  REQUIRE( rn::cyclic_modulus( 2, 3 ) == 2 );
  REQUIRE( rn::cyclic_modulus( 1, 3 ) == 1 );
  REQUIRE( rn::cyclic_modulus( 0, 3 ) == 0 );
  REQUIRE( rn::cyclic_modulus( -1, 3 ) == 2 );
  REQUIRE( rn::cyclic_modulus( -2, 3 ) == 1 );
  REQUIRE( rn::cyclic_modulus( -3, 3 ) == 0 );
  REQUIRE( rn::cyclic_modulus( -4, 3 ) == 2 );
  REQUIRE( rn::cyclic_modulus( -5, 3 ) == 1 );
  REQUIRE( rn::cyclic_modulus( -6, 3 ) == 0 );
  REQUIRE( rn::cyclic_modulus( -7, 3 ) == 2 );

  REQUIRE( rn::cyclic_modulus( 7, -3 ) == 1 );
  REQUIRE( rn::cyclic_modulus( 6, -3 ) == 0 );
  REQUIRE( rn::cyclic_modulus( 5, -3 ) == 2 );
  REQUIRE( rn::cyclic_modulus( 4, -3 ) == 1 );
  REQUIRE( rn::cyclic_modulus( 3, -3 ) == 0 );
  REQUIRE( rn::cyclic_modulus( 2, -3 ) == 2 );
  REQUIRE( rn::cyclic_modulus( 1, -3 ) == 1 );
  REQUIRE( rn::cyclic_modulus( 0, -3 ) == 0 );
  REQUIRE( rn::cyclic_modulus( -1, -3 ) == 2 );
  REQUIRE( rn::cyclic_modulus( -2, -3 ) == 1 );
  REQUIRE( rn::cyclic_modulus( -3, -3 ) == 0 );
  REQUIRE( rn::cyclic_modulus( -4, -3 ) == 2 );
  REQUIRE( rn::cyclic_modulus( -5, -3 ) == 1 );
  REQUIRE( rn::cyclic_modulus( -6, -3 ) == 0 );
  REQUIRE( rn::cyclic_modulus( -7, -3 ) == 2 );
}

TEST_CASE( "[math] round_up_to_nearest_int_multiple" ) {
  auto f = round_up_to_nearest_int_multiple;

  REQUIRE( f( 10.1, 5 ) == 15 );
  REQUIRE( f( 10.0, 5 ) == 10 );
  REQUIRE( f( 9.5, 5 ) == 10 );
  REQUIRE( f( 4.5, 5 ) == 5 );
  REQUIRE( f( 5.5, 5 ) == 10 );
  REQUIRE( f( 5.0, 5 ) == 5 );
  REQUIRE( f( 1.0, 5 ) == 5 );
  REQUIRE( f( 0.0, 5 ) == 0 );
  REQUIRE( f( -0.0, 5 ) == 0 );
  REQUIRE( f( -1.0, 5 ) == 0 );
  REQUIRE( f( -4.5, 5 ) == 0 );
  REQUIRE( f( -5.0, 5 ) == -5 );
  REQUIRE( f( -5.5, 5 ) == -5 );
  REQUIRE( f( -9.5, 5 ) == -5 );
  REQUIRE( f( -10.0, 5 ) == -10 );
  REQUIRE( f( -10.1, 5 ) == -10 );
}

TEST_CASE( "[math] round_down_to_nearest_int_multiple" ) {
  auto f = round_down_to_nearest_int_multiple;

  REQUIRE( f( 10.1, 5 ) == 10 );
  REQUIRE( f( 10.0, 5 ) == 10 );
  REQUIRE( f( 9.5, 5 ) == 5 );
  REQUIRE( f( 4.5, 5 ) == 0 );
  REQUIRE( f( 5.5, 5 ) == 5 );
  REQUIRE( f( 5.0, 5 ) == 5 );
  REQUIRE( f( 1.0, 5 ) == 0 );
  REQUIRE( f( 0.0, 5 ) == 0 );
  REQUIRE( f( -0.0, 5 ) == 0 );
  REQUIRE( f( -1.0, 5 ) == -5 );
  REQUIRE( f( -4.5, 5 ) == -5 );
  REQUIRE( f( -5.0, 5 ) == -5 );
  REQUIRE( f( -5.5, 5 ) == -10 );
  REQUIRE( f( -9.5, 5 ) == -10 );
  REQUIRE( f( -10.0, 5 ) == -10 );
  REQUIRE( f( -10.1, 5 ) == -15 );
}

} // namespace
