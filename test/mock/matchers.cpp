/****************************************************************
**matchers.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2021-11-14.
*
* Description: Unit tests for the src/mock/matchers.* module.
*
*****************************************************************/
#include "test/testing.hpp"

// Under test.
#include "src/mock/matchers.hpp"

// mock
#include "src/mock/mock.hpp"

// Must be last.
#include "test/catch-common.hpp"

#define REQUIRE_UNEXPECTED_ARGS( ... ) \
  REQUIRE_THROWS_WITH(                 \
      __VA_ARGS__,                     \
      Catch::Matches(                  \
          "mock function call with unexpected arguments.*" ) );

namespace mock {
namespace {

using namespace std;
using namespace ::mock::matchers;
using namespace Catch::literals;

struct Foo {
  bool operator==( Foo const& ) const = default;

  int       bar = 6;
  int const baz = 7;

  int        get_bar() const { return bar; }
  int const& get_baz() const { return baz; }
};

/****************************************************************
** IPoint
*****************************************************************/
struct IPoint {
  virtual ~IPoint() = default;

  virtual int get_x() const = 0;

  virtual int get_y() const = 0;

  virtual bool get_xy( int* x_out, int& y_out ) const = 0;

  virtual void set_x( int x ) = 0;

  virtual void set_y( int y ) = 0;

  virtual void set_xy( int x, int y ) = 0;

  virtual void set_xy_pair(
      std::pair<int /*x*/, int /*y*/> p ) = 0;

  virtual void set_x_from_ptr( int* x )                  = 0;
  virtual void set_x_from_const_ptr( int const* x )      = 0;
  virtual void set_x_from_ptr_ptr( int** x )             = 0;
  virtual void set_x_from_const_ptr_ptr( int const** x ) = 0;
  virtual void set_x_from_ptr_const_ptr( int* const* x ) = 0;
  virtual void set_x_from_const_ptr_const_ptr(
      int const* const* x ) = 0;

  virtual void set_x_from_uptr( unique_ptr<int> x ) = 0;
  virtual void set_x_from_uptr_ref(
      unique_ptr<int> const& x ) = 0;
  virtual void set_x_from_const_uptr(
      unique_ptr<int const> x ) = 0;
  virtual void set_x_from_const_uptr_ref(
      unique_ptr<int const> const& x ) = 0;

  virtual double length() const               = 0;
  virtual double double_add( double d ) const = 0;

  virtual int sum_ints( vector<int> const& v ) const = 0;

  virtual int sum_ints_ptr(
      vector<int const*> const& v ) const = 0;

  virtual int sum_ptr_ints_ptr(
      vector<int const*> const* v ) const = 0;

  virtual int sum_ints_nested(
      vector<vector<unsigned int>> const& v ) const = 0;

  virtual string say_hello( string const& to ) const     = 0;
  virtual string say_hello_ptr( string const* to ) const = 0;
  virtual string say_hello_sv( string_view to ) const    = 0;
  virtual string_view say_hello_sv_sv(
      string_view to ) const = 0;

  virtual void set_foo( Foo const& foo ) = 0;

  virtual void take_bool( bool b ) const = 0;
};

/****************************************************************
** MockPoint
*****************************************************************/
struct MockPoint : IPoint {
  virtual ~MockPoint() override = default;
  MockConfig::binder config =
      MockConfig{ .throw_on_unexpected = true };

  MOCK_METHOD( int, get_x, (), ( const ) );
  MOCK_METHOD( int, get_y, (), ( const ) );
  MOCK_METHOD( bool, get_xy, (int*, int&), ( const ) );
  MOCK_METHOD( void, set_x, (int), () );
  MOCK_METHOD( void, set_y, (int), () );
  MOCK_METHOD( void, set_xy, (int, int), () );

  using pair_int_int = std::pair<int, int>;
  MOCK_METHOD( void, set_xy_pair, ( pair_int_int ), () );

  MOCK_METHOD( void, set_x_from_ptr, (int*), () );
  MOCK_METHOD( void, set_x_from_const_ptr, (int const*), () );
  MOCK_METHOD( void, set_x_from_ptr_ptr, (int**), () );
  MOCK_METHOD( void, set_x_from_const_ptr_ptr, (int const**),
               () );
  MOCK_METHOD( void, set_x_from_ptr_const_ptr, (int* const*),
               () );
  MOCK_METHOD( void, set_x_from_const_ptr_const_ptr,
               (int const* const*), () );

  MOCK_METHOD( void, set_x_from_uptr, (unique_ptr<int>), () );
  MOCK_METHOD( void, set_x_from_uptr_ref,
               (unique_ptr<int> const&), () );
  MOCK_METHOD( void, set_x_from_const_uptr,
               (unique_ptr<int const>), () );
  MOCK_METHOD( void, set_x_from_const_uptr_ref,
               (unique_ptr<int const> const&), () );

  MOCK_METHOD( double, length, (), ( const ) );
  MOCK_METHOD( double, double_add, (double), ( const ) );

  MOCK_METHOD( int, sum_ints, (vector<int> const&), ( const ) );
  MOCK_METHOD( int, sum_ints_ptr, (vector<int const*> const&),
               ( const ) );
  MOCK_METHOD( int, sum_ptr_ints_ptr,
               (vector<int const*> const*), ( const ) );
  MOCK_METHOD( int, sum_ints_nested,
               (vector<vector<unsigned int>> const&),
               ( const ) );

  MOCK_METHOD( string, say_hello, (string const&), ( const ) );
  MOCK_METHOD( string, say_hello_ptr, (string const*),
               ( const ) );
  MOCK_METHOD( string, say_hello_sv, ( string_view ),
               ( const ) );
  MOCK_METHOD( string_view, say_hello_sv_sv, ( string_view ),
               ( const ) );

  MOCK_METHOD( void, set_foo, (Foo const&), () );

  MOCK_METHOD( void, take_bool, (bool), ( const ) );
};

/****************************************************************
** PointUser
*****************************************************************/
struct PointUser {
  PointUser( IPoint* p ) : p_( p ) { DCHECK( p_ != nullptr ); }

  void set_xy_pair( std::pair<int /*x*/, int /*y*/> p ) {
    p_->set_xy_pair( p );
  }

  void set_x( int x ) { p_->set_x( x ); }

  void set_x_from_ptr( int* x ) { p_->set_x_from_ptr( x ); }
  void set_x_from_const_ptr( int x ) {
    p_->set_x_from_const_ptr( &x );
  }
  void set_x_from_ptr_ptr( int x ) {
    int* y = &x;
    p_->set_x_from_ptr_ptr( &y );
  }
  void set_x_from_const_ptr_ptr( int x ) {
    int const* y = &x;
    p_->set_x_from_const_ptr_ptr( &y );
  }
  void set_x_from_ptr_const_ptr( int x ) {
    int* y = &x;
    p_->set_x_from_ptr_const_ptr( &y );
  }
  void set_x_from_const_ptr_const_ptr( int x ) {
    int* y = &x;
    p_->set_x_from_const_ptr_const_ptr( &y );
  }

  void set_x_from_uptr( int x ) {
    p_->set_x_from_uptr( make_unique<int>( x ) );
  }
  void set_x_from_uptr_ref( int x ) {
    p_->set_x_from_uptr_ref( make_unique<int>( x ) );
  }
  void set_x_from_const_uptr( int x ) {
    p_->set_x_from_const_uptr( make_unique<int>( x ) );
  }
  void set_x_from_const_uptr_ref( int x ) {
    p_->set_x_from_const_uptr_ref( make_unique<int>( x ) );
  }

  int sum_ints( vector<int> const& v ) const {
    return p_->sum_ints( v );
  }

  int sum_ints_ptr( vector<int const*> const& v ) const {
    return p_->sum_ints_ptr( v );
  }

  int sum_ptr_ints_ptr( vector<int const*> const* v ) const {
    return p_->sum_ptr_ints_ptr( v );
  }

  int sum_ints_nested(
      vector<vector<unsigned int>> const& v ) const {
    return p_->sum_ints_nested( v );
  }

  string say_hello( string const& to ) const {
    return p_->say_hello( to );
  }
  string say_hello_ptr( string const* to ) const {
    return p_->say_hello_ptr( to );
  }
  string say_hello_sv( string_view to ) const {
    return p_->say_hello_sv( to );
  }
  string_view say_hello_sv_sv( string_view to ) const {
    return p_->say_hello_sv_sv( to );
  }

  void set_foo( Foo const& foo ) { p_->set_foo( foo ); }

  void take_bool( bool b ) const { p_->take_bool( b ); }

  double add_two( double d ) const {
    return d + p_->double_add( d + .1 );
  }

  IPoint* p_;
};

/****************************************************************
** Tests
*****************************************************************/
TEST_CASE( "[mock] use stuff" ) {
  // For some reason if we don't explicitly use this then clang
  // warns that operator== is unused, even though it is needed by
  // the mocking framework since Foo is a parameter to a mocked
  // function.
  Foo foo1;
  Foo foo2;
  REQUIRE( foo1 == foo2 );
}

TEST_CASE( "[mock] Pointee" ) {
  MockPoint mp;
  PointUser user( &mp );

  // int*
  EXPECT_CALL( mp, set_x_from_ptr( Pointee( 8 ) ) );
  int n = 8;
  user.set_x_from_ptr( &n );

  // int const*
  EXPECT_CALL( mp, set_x_from_const_ptr( Pointee( 8 ) ) );
  user.set_x_from_const_ptr( 8 );

  // int**
  EXPECT_CALL( mp,
               set_x_from_ptr_ptr( Pointee( Pointee( 8 ) ) ) );
  user.set_x_from_ptr_ptr( 8 );

  // int const**
  EXPECT_CALL(
      mp, set_x_from_const_ptr_ptr( Pointee( Pointee( 8 ) ) ) );
  user.set_x_from_const_ptr_ptr( 8 );

  // int* const*
  EXPECT_CALL(
      mp, set_x_from_ptr_const_ptr( Pointee( Pointee( 8 ) ) ) );
  user.set_x_from_ptr_const_ptr( 8 );

  // int const* const*
  EXPECT_CALL( mp, set_x_from_const_ptr_const_ptr(
                       Pointee( Pointee( 8 ) ) ) );
  user.set_x_from_const_ptr_const_ptr( 8 );

  // unique_ptr<int>
  EXPECT_CALL( mp, set_x_from_uptr( Pointee( 8 ) ) );
  user.set_x_from_uptr( 8 );

  // unique_ptr<int> const&
  EXPECT_CALL( mp, set_x_from_uptr_ref( Pointee( 8 ) ) );
  user.set_x_from_uptr_ref( 8 );

  // unique_ptr<int const>
  EXPECT_CALL( mp, set_x_from_const_uptr( Pointee( 8 ) ) );
  user.set_x_from_const_uptr( 8 );

  // unique_ptr<int const> const&
  EXPECT_CALL( mp, set_x_from_const_uptr_ref( Pointee( 8 ) ) );
  user.set_x_from_const_uptr_ref( 8 );
}

TEST_CASE( "[mock] Pointee arg match failure" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL( mp, set_x_from_const_ptr( Pointee( 9 ) ) );
  // Wrong one.
  REQUIRE_UNEXPECTED_ARGS( user.set_x_from_const_ptr( 8 ) );
  // Right one.
  user.set_x_from_const_ptr( 9 );
}

TEST_CASE( "[mock] IterableElementsAre" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL( mp, sum_ints( IterableElementsAre( 3, 4, 5 ) ) )
      .returns( 12 );
  vector<int> v1{ 3, 4, 5 };
  REQUIRE( user.sum_ints( v1 ) == 12 );

  int n1 = 3, n2 = 4, n3 = 5;

  EXPECT_CALL( mp,
               sum_ints_ptr( IterableElementsAre(
                   Pointee( 3 ), Pointee( 4 ), Pointee( 5 ) ) ) )
      .returns( 12 );
  vector<int const*> v2{ &n1, &n2, &n3 };
  REQUIRE( user.sum_ints_ptr( v2 ) == 12 );

  EXPECT_CALL(
      mp, sum_ptr_ints_ptr( Pointee( IterableElementsAre(
              Pointee( 3 ), Pointee( 4 ), Pointee( 5 ) ) ) ) )
      .returns( 12 );
  REQUIRE( user.sum_ptr_ints_ptr( &v2 ) == 12 );

  EXPECT_CALL( mp, sum_ints_nested( IterableElementsAre(
                       IterableElementsAre( 1, 2 ),
                       IterableElementsAre( 2, 2 ),
                       IterableElementsAre( 2, 3 ) ) ) )
      .returns( 12 );
  vector<vector<unsigned int>> v3{
      { 1, 2 }, { 2, 2 }, { 2, 3 } };
  REQUIRE( user.sum_ints_nested( v3 ) == 12 );
}

TEST_CASE( "[mock] IterableElementsAre arg match failure" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL( mp, sum_ints( IterableElementsAre( 3, 5, 5 ) ) )
      .returns( 12 );
  vector<int> v1{ 3, 4, 5 };
  // Wrong one.
  REQUIRE_UNEXPECTED_ARGS( user.sum_ints( v1 ) );
  // Right one.
  user.sum_ints( { 3, 5, 5 } );
}

TEST_CASE( "[mock] Ge" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL( mp, set_x( Ge( 8 ) ) ).times( 4 );
  user.set_x( 10 );
  user.set_x( 9 );
  user.set_x( 8 );
  REQUIRE_UNEXPECTED_ARGS( user.set_x( 7 ) );
  user.set_x( 8 );
}

TEST_CASE( "[mock] Le" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL( mp, set_x( Le( 8 ) ) ).times( 4 );
  user.set_x( 6 );
  user.set_x( 7 );
  user.set_x( 8 );
  REQUIRE_UNEXPECTED_ARGS( user.set_x( 9 ) );
  user.set_x( 8 );
}

TEST_CASE( "[mock] Gt" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL( mp, set_x( Gt( 8 ) ) ).times( 3 );
  user.set_x( 10 );
  user.set_x( 9 );
  REQUIRE_UNEXPECTED_ARGS( user.set_x( 8 ) );
  user.set_x( 9 );
}

TEST_CASE( "[mock] Lt" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL( mp, set_x( Lt( 8 ) ) ).times( 3 );
  user.set_x( 6 );
  user.set_x( 7 );
  REQUIRE_UNEXPECTED_ARGS( user.set_x( 8 ) );
  user.set_x( 7 );
}

TEST_CASE( "[mock] Ne" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL( mp, set_x( Ne( 8 ) ) ).times( 3 );
  user.set_x( 7 );
  user.set_x( 9 );
  REQUIRE_UNEXPECTED_ARGS( user.set_x( 8 ) );
  user.set_x( 7 );
}

TEST_CASE( "[mock] Eq" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL( mp, set_x( Eq( 8 ) ) );
  REQUIRE_UNEXPECTED_ARGS( user.set_x( 7 ) );
  REQUIRE_UNEXPECTED_ARGS( user.set_x( 9 ) );
  REQUIRE_UNEXPECTED_ARGS( user.set_x( 10 ) );
  user.set_x( 8 );
}

TEST_CASE( "[mock] Not" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL( mp, set_x( Not( Ge( 8 ) ) ) ).times( 4 );
  user.set_x( 5 );
  user.set_x( 6 );
  user.set_x( 7 );
  REQUIRE_UNEXPECTED_ARGS( user.set_x( 8 ) );
  user.set_x( 7 );
}

TEST_CASE( "[mock] string" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL( mp, say_hello( "bob" ) ).returns( "hello bob" );
  REQUIRE( user.say_hello( "bob" ) == "hello bob" );
}

TEST_CASE( "[mock] string_view" ) {
  MockPoint mp;
  PointUser user( &mp );

  SECTION( "takes string_view" ) {
    // Note that if "bob" were a temporary std::string then this
    // would crash due to stored dangling string_view.
    EXPECT_CALL( mp, say_hello_sv( "bob" ) )
        .returns( "hello bob" );
    REQUIRE( user.say_hello_sv( "bob" ) == "hello bob" );
  }

  SECTION( "takes string_view (use string for return)" ) {
    // Note that if "bob" were a temporary std::string then this
    // would crash due to stored dangling string_view.
    EXPECT_CALL( mp, say_hello_sv( "bob" ) )
        // Passing a temporary string here should be OK because
        // say_hello_sv returns a std::string, so nothing will
        // dangle.
        .returns( string( "hello bob" ) );
    REQUIRE( user.say_hello_sv( "bob" ) == "hello bob" );
  }

  SECTION( "takes and returns string_view" ) {
    // Both of these must be non-dangling.
    EXPECT_CALL( mp, say_hello_sv_sv( "bob" ) )
        .returns( "hello bob" );
    REQUIRE( user.say_hello_sv_sv( "bob" ) == "hello bob" );
  }
}

TEST_CASE( "[mock] StartsWith" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL( mp, say_hello( StartsWith( "bob" ) ) )
      .returns( "hello bob" );
  REQUIRE( user.say_hello( "bob bob" ) == "hello bob" );

  EXPECT_CALL( mp,
               say_hello_ptr( Pointee( StartsWith( "bob" ) ) ) )
      .returns( "hello bob" );
  string bobbob = "bob bob";
  REQUIRE( user.say_hello_ptr( &bobbob ) == "hello bob" );
}

TEST_CASE( "[mock] StrContains" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL( mp, say_hello( StrContains( "b b" ) ) )
      .returns( "hello bob" );
  REQUIRE( user.say_hello( "bob bob" ) == "hello bob" );

  EXPECT_CALL( mp, say_hello( StrContains( "ccc" ) ) )
      .returns( "hello bob" );
  REQUIRE_UNEXPECTED_ARGS( user.say_hello( "bob bob" ) );
  REQUIRE( user.say_hello( "bob ccc bob" ) == "hello bob" );
}

TEST_CASE( "[mock] Matches" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL( mp, say_hello( Matches( "bob b.b"s ) ) )
      .returns( "hello bob" );
  REQUIRE( user.say_hello( "bob bob" ) == "hello bob" );

  EXPECT_CALL( mp, say_hello( Matches( "h.*c" ) ) )
      .returns( "hello bob" );
  REQUIRE_UNEXPECTED_ARGS( user.say_hello( "bob bob" ) );
  REQUIRE( user.say_hello( "hob ccc" ) == "hello bob" );
}

TEST_CASE( "[mock] Empty" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL( mp, sum_ints( Not( Empty() ) ) ).returns( 12 );
  vector<int> v{ 3, 4, 5 };
  REQUIRE( user.sum_ints( v ) == 12 );

  EXPECT_CALL( mp, sum_ints( Empty() ) ).returns( 0 );
  v.clear();
  REQUIRE( user.sum_ints( v ) == 0 );
}

TEST_CASE( "[mock] True" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL( mp, take_bool( True() ) );
  REQUIRE_UNEXPECTED_ARGS( user.take_bool( false ) );
  user.take_bool( true );
}

TEST_CASE( "[mock] False" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL( mp, take_bool( False() ) );
  REQUIRE_UNEXPECTED_ARGS( user.take_bool( true ) );
  user.take_bool( false );
}

TEST_CASE( "[mock] Null" ) {
  MockPoint mp;
  PointUser user( &mp );

  int n = 0;
  EXPECT_CALL( mp, set_x_from_ptr( Null() ) );
  REQUIRE_UNEXPECTED_ARGS( user.set_x_from_ptr( &n ) );
  user.set_x_from_ptr( nullptr );
}

TEST_CASE( "[mock] HasSize" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL( mp, sum_ints( HasSize( 3 ) ) ).returns( 12 );
  vector<int> v{ 3, 4, 5 };
  REQUIRE( user.sum_ints( v ) == 12 );

  EXPECT_CALL( mp, sum_ints( HasSize( Ge( 2 ) ) ) )
      .times( 3 )
      .returns( 42 );
  REQUIRE( user.sum_ints( v ) == 42 ); // 3, 4, 5
  v.pop_back();
  REQUIRE( user.sum_ints( v ) == 42 ); // 3, 4
  v.pop_back();
  REQUIRE_UNEXPECTED_ARGS( user.sum_ints( v ) ); // 3
  v.push_back( 1 );
  REQUIRE( user.sum_ints( v ) == 42 ); // 3, 1

  v = { 3, 4, 5 };
  EXPECT_CALL( mp, sum_ints( Not( HasSize( Ge( 2 ) ) ) ) )
      .returns( 42 );
  REQUIRE_UNEXPECTED_ARGS( user.sum_ints( v ) ); // 3, 4, 5
  v.pop_back();
  REQUIRE_UNEXPECTED_ARGS( user.sum_ints( v ) ); // 3, 4
  v.pop_back();
  REQUIRE( user.sum_ints( v ) == 42 ); // 3
}

TEST_CASE( "[mock] Each" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL( mp, sum_ints( Each( 5 ) ) ).returns( 12 );
  vector<int> v{ 5, 5, 5 };
  REQUIRE( user.sum_ints( v ) == 12 );

  EXPECT_CALL( mp, sum_ints( Each( Ge( 6 ) ) ) ).returns( 12 );
  v = { 5, 6, 7 };
  REQUIRE_UNEXPECTED_ARGS( user.sum_ints( v ) );

  v = { 6, 7, 8 };
  REQUIRE( user.sum_ints( v ) == 12 );
}

TEST_CASE( "[mock] AllOf" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL( mp, set_x( AllOf( Ge( 5 ), Not( Ge( 6 ) ) ) ) );
  int n = 5;
  user.set_x( n );

  // Explicit value as matcher.
  EXPECT_CALL( mp, set_x( AllOf( 5 ) ) );
  n = 5;
  user.set_x( n );

  EXPECT_CALL( mp, set_x( AllOf( Ge( 5 ), Not( Ge( 6 ) ) ) ) );
  n = 6;
  REQUIRE_UNEXPECTED_ARGS( user.set_x( n ) );
  n = 4;
  REQUIRE_UNEXPECTED_ARGS( user.set_x( n ) );
  n = 5;
  user.set_x( n );

  // Empty list of matchers should always succeed.
  EXPECT_CALL( mp, set_x( AllOf() ) );
  user.set_x( n );
}

TEST_CASE( "[mock] AnyOf" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL( mp, set_x( AnyOf( Ge( 5 ), Not( Ge( 3 ) ) ) ) )
      .times( 5 );
  user.set_x( 6 );
  user.set_x( 5 );
  REQUIRE_UNEXPECTED_ARGS( user.set_x( 4 ) );
  REQUIRE_UNEXPECTED_ARGS( user.set_x( 3 ) );
  user.set_x( 2 );
  user.set_x( 1 );
  user.set_x( 0 );

  // Empty list of matchers should fail.
  auto& responder = EXPECT_CALL( mp, set_x( AnyOf() ) );
  REQUIRE_UNEXPECTED_ARGS( user.set_x( 1 ) );
  // Since nothing can satisfy the above matcher, we must clear
  // the expectations.
  responder.clear_expectations();
}

TEST_CASE( "[mock] TupleElement" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL(
      mp, set_xy_pair( AllOf( TupleElement<0>( Ge( 5 ) ),
                              TupleElement<1>( Ge( 3 ) ) ) ) );
  REQUIRE_UNEXPECTED_ARGS( user.set_xy_pair( { 5, 2 } ) );
  REQUIRE_UNEXPECTED_ARGS( user.set_xy_pair( { 4, 3 } ) );
  user.set_xy_pair( { 5, 3 } );
}

TEST_CASE( "[mock] Key" ) {
  MockPoint mp;
  PointUser user( &mp );

  EXPECT_CALL(
      mp, set_xy_pair( AllOf( Key( Ge( 5 ) ),
                              TupleElement<1>( Ge( 3 ) ) ) ) );
  REQUIRE_UNEXPECTED_ARGS( user.set_xy_pair( { 5, 2 } ) );
  REQUIRE_UNEXPECTED_ARGS( user.set_xy_pair( { 4, 3 } ) );
  user.set_xy_pair( { 5, 3 } );
}

TEST_CASE( "[mock] Field" ) {
  MockPoint mp;
  PointUser user( &mp );

  auto matcher = AllOf(      //
      Field( &Foo::bar, 5 ), //
      Field( &Foo::baz, 6 )  //
  );
  EXPECT_CALL( mp, set_foo( matcher ) );
  user.set_foo( Foo{ 5, 6 } );

  auto matcher2 = AllOf(           //
      Field( &Foo::bar, Ge( 7 ) ), //
      Field( &Foo::baz, 6 )        //
  );
  EXPECT_CALL( mp, set_foo( matcher2 ) );
  REQUIRE_UNEXPECTED_ARGS( user.set_foo( Foo{ 5, 6 } ) );
  REQUIRE_UNEXPECTED_ARGS( user.set_foo( Foo{ 6, 6 } ) );
  user.set_foo( Foo{ 7, 6 } );
}

TEST_CASE( "[mock] Property" ) {
  MockPoint mp;
  PointUser user( &mp );

  auto matcher = AllOf(             //
      Property( &Foo::get_bar, 5 ), //
      Property( &Foo::get_baz, 6 )  //
  );
  EXPECT_CALL( mp, set_foo( matcher ) );
  user.set_foo( Foo{ 5, 6 } );

  auto matcher2 = AllOf(                  //
      Property( &Foo::get_bar, Ge( 7 ) ), //
      Property( &Foo::get_baz, 6 )        //
  );
  EXPECT_CALL( mp, set_foo( matcher2 ) );
  REQUIRE_UNEXPECTED_ARGS( user.set_foo( Foo{ 5, 6 } ) );
  REQUIRE_UNEXPECTED_ARGS( user.set_foo( Foo{ 6, 6 } ) );
  user.set_foo( Foo{ 7, 6 } );
}

TEST_CASE( "[mock] Approx" ) {
  MockPoint mp;
  PointUser user( &mp );
  EXPECT_CALL( mp, double_add( Approx( .6, .01 ) ) )
      .returns( .7 );
  REQUIRE( user.add_two( .499 ) == 1.199_a );
}

} // namespace
} // namespace mock
