/****************************************************************
**state.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2021-05-29.
*
* Description: Unit tests for the src/luapp/state.* module.
*
*****************************************************************/
#include "test/testing.hpp"

// Under test.
#include "src/luapp/state.hpp"

// Testing
#include "test/monitoring-types.hpp"

// luapp
#include "src/luapp/c-api.hpp"

// Lua
#include "lauxlib.h"
#include "lua.h"

// Must be last.
#include "test/catch-common.hpp"

FMT_TO_CATCH( ::luapp::e_lua_type );

namespace luapp {
namespace {

using namespace std;

using ::base::valid;
using ::testing::Tracker;

TEST_CASE( "[state] creation/destruction" ) { state st; }

TEST_CASE( "[state] tables" ) {
  state  st;
  c_api& C = st.api();
  REQUIRE( C.getglobal( "t1" ) == e_lua_type::nil );
  C.pop();
  REQUIRE( C.stack_size() == 0 );

  SECTION( "empty" ) { st.tables( { "" } ); }

  SECTION( "single" ) {
    st.tables( { "t1" } );
    REQUIRE( C.getglobal( "t1" ) == e_lua_type::table );
    REQUIRE( C.stack_size() == 1 );
    C.pop();
  }

  SECTION( "double" ) {
    st.tables( { "t1", "t2" } );
    REQUIRE( C.getglobal( "t1" ) == e_lua_type::table );
    REQUIRE( C.getfield( -1, "t2" ) == e_lua_type::table );
    REQUIRE( C.stack_size() == 2 );
    C.pop( 2 );
  }

  SECTION( "triple" ) {
    st.tables( { "t1", "t2", "t3" } );
    REQUIRE( C.getglobal( "t1" ) == e_lua_type::table );
    REQUIRE( C.getfield( -1, "t2" ) == e_lua_type::table );
    REQUIRE( C.getfield( -1, "t3" ) == e_lua_type::table );
    REQUIRE( C.stack_size() == 3 );
    C.pop( 3 );
  }

  SECTION( "tables already present" ) {
    st.tables( { "t1" } );
    REQUIRE( C.getglobal( "t1" ) == e_lua_type::table );
    REQUIRE( C.getfield( -1, "t2" ) == e_lua_type::nil );
    REQUIRE( C.stack_size() == 2 );
    C.pop( 2 );
    st.tables( { "t1", "t2" } );
    REQUIRE( C.getglobal( "t1" ) == e_lua_type::table );
    REQUIRE( C.getfield( -1, "t2" ) == e_lua_type::table );
    REQUIRE( C.getfield( -1, "t3" ) == e_lua_type::nil );
    REQUIRE( C.stack_size() == 3 );
    C.pop( 3 );
    st.tables( { "t1", "t2", "t3" } );
    REQUIRE( C.getglobal( "t1" ) == e_lua_type::table );
    REQUIRE( C.getfield( -1, "t2" ) == e_lua_type::table );
    REQUIRE( C.getfield( -1, "t3" ) == e_lua_type::table );
    REQUIRE( C.stack_size() == 3 );
    C.pop( 3 );
    st.tables( { "t1", "t2", "t3" } );
    REQUIRE( C.getglobal( "t1" ) == e_lua_type::table );
    REQUIRE( C.getfield( -1, "t2" ) == e_lua_type::table );
    REQUIRE( C.getfield( -1, "t3" ) == e_lua_type::table );
    REQUIRE( C.stack_size() == 3 );
    C.pop( 3 );
  }

  SECTION( "triple 2" ) {
    st.tables( { "hello_world", "yes123x", "_" } );
    REQUIRE( C.getglobal( "hello_world" ) == e_lua_type::table );
    REQUIRE( C.getfield( -1, "yes123x" ) == e_lua_type::table );
    REQUIRE( C.getfield( -1, "_" ) == e_lua_type::table );
    REQUIRE( C.stack_size() == 3 );
    C.pop( 3 );
  }

  SECTION( "spaces" ) {
    st.tables( { " t1", " t2" } );
    REQUIRE( C.getglobal( " t1" ) == e_lua_type::table );
    REQUIRE( C.getfield( -1, " t2" ) == e_lua_type::table );
    REQUIRE( C.stack_size() == 2 );
    C.pop( 2 );
  }

  SECTION( "with reserved" ) {
    st.tables( { "t1", "if", "t3" } );
    REQUIRE( C.getglobal( "t1" ) == e_lua_type::table );
    REQUIRE( C.getfield( -1, "if" ) == e_lua_type::table );
    REQUIRE( C.getfield( -1, "t3" ) == e_lua_type::table );
    REQUIRE( C.stack_size() == 3 );
    C.pop( 3 );
  }

  SECTION( "bad identifier" ) {
    st.tables( { "t1", "x-z", "t3" } );
    REQUIRE( C.getglobal( "t1" ) == e_lua_type::table );
    REQUIRE( C.getfield( -1, "x-z" ) == e_lua_type::table );
    REQUIRE( C.getfield( -1, "t3" ) == e_lua_type::table );
    REQUIRE( C.stack_size() == 3 );
    C.pop( 3 );
  }

  REQUIRE( C.stack_size() == 0 );
}

TEST_CASE( "[state] push_closure, stateless function" ) {
  state st;
  st.openlibs();
  c_api& C = st.api();

  st.push_function( []( lua_State* L ) -> int {
    c_api C( L, /*own=*/false );
    int   n = luaL_checkinteger( L, 1 );
    C.push( n + 1 );
    return 1;
  } );
  C.setglobal( "add_one" );
  REQUIRE( C.dostring( "assert( add_one( 6 ) == 7 )" ) ==
           valid );

  // Make sure that it has no upvalues.
  C.getglobal( "add_one" );
  REQUIRE( C.type_of( -1 ) == e_lua_type::function );
  REQUIRE_FALSE( C.getupvalue( -1, 1 ) );
  C.pop();
  REQUIRE( C.stack_size() == 0 );
}

TEST_CASE( "[state] push_function, closure" ) {
  Tracker::reset();

  SECTION( "__gc metamethod is called, twice" ) {
    state st;
    st.openlibs();
    c_api& C = st.api();

    bool created = st.push_function(
        [tracker = Tracker{}]( lua_State* L ) -> int {
          c_api C( L, /*own=*/false );
          int   n = luaL_checkinteger( L, 1 );
          C.push( n + 1 );
          return 1;
        } );
    REQUIRE( created );
    C.setglobal( "add_one" );
    REQUIRE( Tracker::constructed == 1 );
    REQUIRE( Tracker::destructed == 2 );
    REQUIRE( Tracker::copied == 0 );
    REQUIRE( Tracker::move_constructed == 2 );
    REQUIRE( Tracker::move_assigned == 0 );
    Tracker::reset();

    REQUIRE( C.dostring( "assert( add_one( 6 ) == 7 )" ) ==
             valid );
    REQUIRE( C.dostring( "assert( add_one( 7 ) == 8 )" ) ==
             valid );
    REQUIRE( Tracker::constructed == 0 );
    REQUIRE( Tracker::destructed == 0 );
    REQUIRE( Tracker::copied == 0 );
    REQUIRE( Tracker::move_constructed == 0 );
    REQUIRE( Tracker::move_assigned == 0 );
    Tracker::reset();

    // Test that the function has an up value and that the upval-
    // ue's metatable has the right name.
    C.getglobal( "add_one" );
    REQUIRE( C.type_of( -1 ) == e_lua_type::function );
    REQUIRE_FALSE( C.getupvalue( -1, 2 ) );
    REQUIRE( C.getupvalue( -1, 1 ) == true );
    REQUIRE( C.type_of( -1 ) == e_lua_type::userdata );
    REQUIRE( C.stack_size() == 2 );
    REQUIRE( C.getmetatable( -1 ) );
    REQUIRE( C.stack_size() == 3 );
    REQUIRE( C.getfield( -1, "__name" ) == e_lua_type::string );
    REQUIRE( C.stack_size() == 4 );
    REQUIRE( C.get<string>( -1 ) ==
             "base::unique_func<int (lua_State*) const>" );
    C.pop( 4 );
    REQUIRE( C.stack_size() == 0 );

    // Now set a second closure and ensure that the metatable
    // gets reused.
    created = st.push_function(
        [tracker = Tracker{}]( lua_State* L ) -> int {
          c_api C( L, /*own=*/false );
          int   n = luaL_checkinteger( L, 1 );
          C.push( n + 2 );
          return 1;
        } );
    REQUIRE_FALSE( created );
    C.setglobal( "add_two" );
    REQUIRE( Tracker::constructed == 1 );
    REQUIRE( Tracker::destructed == 2 );
    REQUIRE( Tracker::copied == 0 );
    REQUIRE( Tracker::move_constructed == 2 );
    REQUIRE( Tracker::move_assigned == 0 );
    Tracker::reset();

    REQUIRE( C.dostring( "assert( add_two( 6 ) == 8 )" ) ==
             valid );
    REQUIRE( C.dostring( "assert( add_two( 7 ) == 9 )" ) ==
             valid );
    REQUIRE( Tracker::constructed == 0 );
    REQUIRE( Tracker::destructed == 0 );
    REQUIRE( Tracker::copied == 0 );
    REQUIRE( Tracker::move_constructed == 0 );
    REQUIRE( Tracker::move_assigned == 0 );
    Tracker::reset();
  }

  // Ensure that precisely two closures get destroyed (will
  // happen when `st` goes out of scope and Lua calls the final-
  // izers on the userdatas for the two closures that we created
  // above).
  REQUIRE( Tracker::constructed == 0 );
  REQUIRE( Tracker::destructed == 2 );
  REQUIRE( Tracker::copied == 0 );
  REQUIRE( Tracker::move_constructed == 0 );
  REQUIRE( Tracker::move_assigned == 0 );
}

} // namespace
} // namespace luapp
