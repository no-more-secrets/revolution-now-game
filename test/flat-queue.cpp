/****************************************************************
**flat-queue.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2019-09-04.
*
* Description: Unit tests for the flat-queue module.
*
*****************************************************************/
#include "testing.hpp"

// Revolution Now
#include "flat-queue.hpp"
#include "src/rand.hpp"
#include "src/ranges.hpp"

// range-v3
#include "range/v3/view/generate_n.hpp"
#include "range/v3/view/indices.hpp"
#include "range/v3/view/zip.hpp"

// Must be last.
#include "catch-common.hpp"

// C++ standard library
#include <queue>

namespace {

using namespace std;
using namespace rn;

TEST_CASE( "[flat-queue] initialization" ) {
  flat_queue<int> q;

  REQUIRE( q.size() == 0 );
  REQUIRE( q.empty() );

  REQUIRE( !q.front().has_value() );
  REQUIRE_THROWS_AS_RN( q.pop() );
}

TEST_CASE( "[flat-queue] push pop small" ) {
  flat_queue<int> q;

  q.push( 5 );
  REQUIRE( q.size() == 1 );
  REQUIRE( !q.empty() );
  REQUIRE( q.front() == 5 );

  q.pop();
  REQUIRE( q.size() == 0 );
  REQUIRE( q.empty() );
  REQUIRE( !q.front().has_value() );
  REQUIRE_THROWS_AS_RN( q.pop() );

  q.push( 5 );
  q.push( 6 );
  REQUIRE( q.size() == 2 );
  REQUIRE( !q.empty() );
  REQUIRE( q.front() == 5 );

  q.pop();
  REQUIRE( q.size() == 1 );
  REQUIRE( !q.empty() );
  REQUIRE( q.front().has_value() );
  REQUIRE( q.front() == 6 );

  q.pop();
  REQUIRE( q.size() == 0 );
  REQUIRE( q.empty() );
  REQUIRE( !q.front().has_value() );
  REQUIRE_THROWS_AS_RN( q.pop() );
}

TEST_CASE( "[flat-queue] min size" ) {
  flat_queue<int> q;

  int const push_size = 10 * 5;
  REQUIRE( push_size > 10 );

  Vec<int> pushed;
  for( int i = 0; i < push_size; ++i ) {
    auto value = i * 2;
    pushed.push_back( value );
    q.push( value );
    REQUIRE( q.size() == i + 1 );
    REQUIRE( !q.empty() );
    REQUIRE( q.front() == 0 );
  }

  for( int i = push_size - 1; i >= 0; --i ) {
    REQUIRE( q.size() == i + 1 );
    REQUIRE( !q.empty() );
    REQUIRE( q.front() == pushed.front() );
    pushed.erase( pushed.begin(), pushed.begin() + 1 );
    q.pop();
  }

  REQUIRE( q.size() == 0 );
  REQUIRE( q.empty() );
  REQUIRE( !q.front().has_value() );
  REQUIRE_THROWS_AS_RN( q.pop() );
}

TEST_CASE( "[flat-queue] reallocation size" ) {
  flat_queue<int> q;

  int const push_size =
      k_flat_queue_reallocation_size_default * 2;
  REQUIRE( push_size > k_flat_queue_reallocation_size_default );

  for( int i = 0; i < push_size; ++i ) q.push( i );

  REQUIRE( q.size() == push_size );

  for( int i = 0; i < k_flat_queue_reallocation_size_default - 1;
       ++i )
    q.pop();

  REQUIRE( q.size() ==
           k_flat_queue_reallocation_size_default + 1 );

  for( int i = 0; i < k_flat_queue_reallocation_size_default + 1;
       ++i )
    q.pop();

  REQUIRE( q.size() == 0 );
}

TEST_CASE( "[flat-queue] non-copyable, non-def-constructible" ) {
  struct A {
    A() = delete;

    A( A const& ) = delete;
    A( A&& )      = default;

    A& operator=( A const& ) = delete;
    A& operator=( A&& ) = default;

    A( int x ) : x_( x ) {}
    int x_;
  };
  flat_queue<A> q;
  q.push_emplace( A{5} );
  REQUIRE( q.size() == 1 );
  REQUIRE( q.front().has_value() );
  REQUIRE( q.front().value().get().x_ == 5 );
  q.pop();
  REQUIRE( q.size() == 0 );
}

// This test really puts the flat_queue to the test. It will for
// three unique configurations of reallocation_size randomly push
// and pop thousands of random elements into the flat_queue and
// an std::queue and compare them at every turn.
TEST_CASE( "[flat-queue] std::queue comparison" ) {
  auto realloc_size = GENERATE( 250, 1000, 10000 );

  flat_queue<int> q( realloc_size );
  std::queue<int> sq;

  // Catch2 takes its seed on the command line; from this seed
  // we generate some random numbers to reseed our own random
  // gener- ator to get predictable results based on Catch2's
  // seed.
  uint32_t sub_seed =
      GENERATE( take( 1, random( 0, 1000000 ) ) );
  INFO( fmt::format( "sub_seed: {}", sub_seed ) );
  rng::reseed( sub_seed );

  SECTION( "roughly equal push and pop" ) {
    vector<bool> bs = rv::generate_n( L0( rng::flip_coin() ),
                                      realloc_size * 3 );
    vector<int>  is = rv::ints( 0, realloc_size * 3 );
    REQUIRE( is.size() == bs.size() );

    for( auto [action, n] : rv::zip( bs, is ) ) {
      if( action ) {
        q.push( n );
        sq.push( n );
      } else if( !q.empty() && !sq.empty() ) {
        q.pop();
        sq.pop();
      }
      REQUIRE( q.size() == int( sq.size() ) );
      if( !q.empty() ) { REQUIRE( q.front() == sq.front() ); }
    }

    while( q.size() > 0 ) {
      REQUIRE( q.front() == sq.front() );
      REQUIRE( q.size() == int( sq.size() ) );
      q.pop();
      sq.pop();
    }

    REQUIRE( !q.front().has_value() );
    REQUIRE( q.size() == sq.size() );
    REQUIRE( q.size() == 0 );
  }

  SECTION( "biased to push" ) {
    vector<bool> bs = rv::generate_n(
        L0( rng::between( 0, 3, rng::e_interval::half_open ) ),
        10000 );
    vector<int> is = rv::ints( 0, 10000 );
    REQUIRE( is.size() == bs.size() );

    for( auto [action, n] : rv::zip( bs, is ) ) {
      REQUIRE( action >= 0 );
      REQUIRE( action <= 2 );
      if( action == 0 || action == 1 ) {
        q.push( n );
        sq.push( n );
      } else if( !q.empty() && !sq.empty() ) {
        q.pop();
        sq.pop();
      }
      REQUIRE( q.size() == sq.size() );
      if( !q.empty() ) { REQUIRE( q.front() == sq.front() ); }
    }

    while( q.size() > 0 ) {
      REQUIRE( q.front() == sq.front() );
      REQUIRE( q.size() == sq.size() );
      q.pop();
      sq.pop();
    }

    REQUIRE( !q.front().has_value() );
    REQUIRE( q.size() == sq.size() );
    REQUIRE( q.size() == 0 );
  }

  SECTION( "biased to pop" ) {
    vector<bool> bs = rv::generate_n(
        L0( rng::between( 0, 3, rng::e_interval::half_open ) ),
        10000 );
    vector<int> is = rv::ints( 0, 10000 );
    REQUIRE( is.size() == bs.size() );

    for( auto [action, n] : rv::zip( bs, is ) ) {
      REQUIRE( action >= 0 );
      REQUIRE( action <= 2 );
      if( action == 0 ) {
        q.push( n );
        sq.push( n );
      } else if( !q.empty() && !sq.empty() ) {
        q.pop();
        sq.pop();
      }
      REQUIRE( q.size() == sq.size() );
      if( !q.empty() ) { REQUIRE( q.front() == sq.front() ); }
    }

    while( q.size() > 0 ) {
      REQUIRE( q.front() == sq.front() );
      REQUIRE( q.size() == sq.size() );
      q.pop();
      sq.pop();
    }

    REQUIRE( !q.front().has_value() );
    REQUIRE( q.size() == sq.size() );
    REQUIRE( q.size() == 0 );
  }
}

} // namespace
