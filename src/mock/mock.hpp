/****************************************************************
**mock.hpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2021-11-11.
*
* Description: Mocking Framework.
*
*****************************************************************/
#pragma once

// mock
#include "matcher.hpp"

// base
#include "base/error.hpp"
#include "base/maybe.hpp"
#include "base/scope-exit.hpp"
#include "base/to-str.hpp"

// base-util
#include "base-util/pp.hpp"

// C++ standard library
#include <queue>

/****************************************************************
** EXPECT* Macros
*****************************************************************/
// This one sets a method that will (and must) be called exactly
// once, at which point it will be removed.
#define EXPECT_CALL( obj, method_with_args ) \
  ( obj ).add__##method_with_args

/****************************************************************
** MOCK_METHOD Macros
*****************************************************************/
#define MAKE_FN_ARG( n, type )      type _##n
#define MAKE_FN_ARG_TUPLE( t )      MAKE_FN_ARG t
#define ADD_MATCHER_WRAPPER( type ) ::mock::MatcherWrapper<type>

#define MAKE_FN_ARG_FWD( n, type ) \
  std::forward<decltype( _##n )>( _##n )
#define MAKE_FN_ARG_FWD_TUPLE( t ) MAKE_FN_ARG_FWD t

#define MAKE_FN_ARGS( fn_args )     \
  PP_MAP_COMMAS( MAKE_FN_ARG_TUPLE, \
                 PP_ENUMERATE( PP_REMOVE_PARENS fn_args ) )
#define MAKE_FN_ARGS_FWD_VARS( fn_args ) \
  PP_MAP_COMMAS( MAKE_FN_ARG_FWD_TUPLE,  \
                 PP_ENUMERATE( PP_REMOVE_PARENS fn_args ) )

#define MOCK_METHOD( ret_type, fn_name, fn_args,      \
                     const_modifier )                 \
  EVAL( MOCK_METHOD_IMPL( ret_type, fn_name, fn_args, \
                          const_modifier ) )

#define MOCK_METHOD_IMPL( ret_type, fn_name, fn_args,          \
                          const_modifier )                     \
  using responder__##fn_name = ::mock::detail::Responder<      \
      ret_type, std::tuple<PP_REMOVE_PARENS fn_args>,          \
      decltype( std::index_sequence_for<                       \
                PP_REMOVE_PARENS fn_args>() )>;                \
                                                               \
  mutable ::mock::detail::ResponderQueue<responder__##fn_name> \
      queue__##fn_name = { #fn_name };                         \
                                                               \
  template<typename... Args>                                   \
  requires std::is_constructible_v<                            \
      std::tuple<PP_MAP_COMMAS( ADD_MATCHER_WRAPPER,           \
                                PP_REMOVE_PARENS fn_args )>,   \
      Args...>                                                 \
  [[maybe_unused]] responder__##fn_name& add__##fn_name(       \
      Args&&... args ) {                                       \
    auto matchers = responder__##fn_name::matchers_t{          \
        std::forward<Args>( args )... };                       \
    return queue__##fn_name.add( std::move( matchers ) );      \
  }                                                            \
                                                               \
  ret_type fn_name( MAKE_FN_ARGS( fn_args ) )                  \
      PP_REMOVE_PARENS const_modifier override {               \
    return queue__##fn_name(                                   \
        MAKE_FN_ARGS_FWD_VARS( fn_args ) );                    \
  }

namespace mock {

/****************************************************************
** Mock Config
*****************************************************************/
struct MockConfig {
  // By default we want to check-fail on unexpected so that we
  // can get the stack trace for debugging. Though when we are
  // unit testing the mocking framework itself we want to throw
  // so that we can test that those errors are thrown.
  bool throw_on_unexpected = false;

  struct binder;
};

struct [[nodiscard]] MockConfig::binder {
  binder( MockConfig config );
  ~binder();
  MockConfig old_config_;
};

/****************************************************************
** Mocking
*****************************************************************/
namespace detail {

template<typename T>
struct RetHolder {
  RetHolder( T& val )
  requires std::is_reference_v<T>
    : o( val ) {}
  RetHolder( T val )
  requires( !std::is_reference_v<T> )
    : o( std::move( val ) ) {}
  decltype( auto ) get() {
    // If our return type is a non-const l-value ref then we need
    // to return it as such (and not as T&&) because the
    // non-const l-value ref that will be waiting for it will not
    // be able to bind to it.
    if constexpr( std::is_lvalue_reference_v<T> &&
                  !std::is_const_v<std::remove_reference_t<T>> )
      return o;
    else
      return std::move( o );
  }
  T o;
};

template<>
struct RetHolder<void> {
  void get() {}
};

[[noreturn]] void throw_unexpected_error( std::string_view msg );

template<typename T>
struct exhaust_checker {
  std::string queue_name_;
  T*          p_;
  exhaust_checker( std::string_view queue_name, T* p )
    : queue_name_( queue_name ), p_( p ) {}
  ~exhaust_checker() {
    // This is so that if there is already an exception in pro-
    // gress, e.g. from an unexpected mock function call, we will
    // continue propagating that exception to the unit test
    // framework so that it can display the appropriate error
    // message (in that case there is no point to checking if all
    // expected calls have been called because 1) we know that
    // they haven't and 2) terminating here would prevent the
    // unit test framework from displaying the real error).
    if( std::uncaught_exceptions() == 0 ) {
      int unfinished = 0;
      while( !p_->empty() ) {
        if( !p_->front().finished() )
          unfinished += p_->front().times_remaining();
        p_->pop();
      }
      BASE_CHECK( !unfinished,
                  "not all expected calls of the function '{}' "
                  "have been called.  It was expected to have "
                  "been called {} more times.",
                  queue_name_, unfinished );
    }
  }
};

template<typename T>
concept SettablePointer = std::is_pointer_v<T> && !
std::is_const_v<std::remove_pointer_t<T>>;

template<typename T>
concept SettableReference = std::is_reference_v<T> && !
std::is_const_v<std::remove_reference_t<T>>;

template<typename T>
concept Settable = SettablePointer<T> || SettableReference<T>;

struct None {};

template<typename...>
struct Responder;

// This is the object that holds the info needed to respond to a
// mock call in the various possible ways that that can be done.
template<typename RetT, typename... Args, size_t... Idx>
struct Responder<RetT, std::tuple<Args...>,
                 std::index_sequence<Idx...>> {
 public:
  using ret_t       = RetT;
  using args_t      = std::tuple<Args...>;
  using args_refs_t = std::tuple<Args const&...>;
  using matchers_t  = std::tuple<MatcherWrapper<Args>...>;
  using setters_t   = std::tuple<
      std::conditional_t<Settable<Args>,
                         base::maybe<std::remove_reference_t<
                             std::remove_pointer_t<Args>>>,
                         None>...>;
  using array_setters_t = std::tuple<
      std::conditional_t<SettablePointer<Args>,
                         std::vector<std::remove_reference_t<
                             std::remove_pointer_t<Args>>>,
                         None>...>;

  Responder( std::string fn_name, matchers_t&& args )
    : fn_name_( std::move( fn_name ) ),
      matchers_( std::move( args ) ),
      times_expected_{ 1 } {}

  RetT operator()( args_refs_t const& args ) {
    auto format_if_possible
        [[maybe_unused]] = []<typename T>( T&& o ) {
          std::string res = "<non-formattable>";
          if constexpr( base::Show<std::remove_cvref_t<T>> )
            res = fmt::to_string( o );
          return res;
        };

    std::string formatted_args;
    ( ( formatted_args +=
        format_if_possible( std::get<Idx>( args ) ) + ", " ),
      ... );
    if( !formatted_args.empty() )
      formatted_args.resize( formatted_args.size() - 2 );

    // 1. Check if the arguments match.
    auto check_argument [[maybe_unused]] =
        [&]<size_t ArgIdx>(
            std::integral_constant<size_t, ArgIdx> ) {
          auto&&      arg = std::get<ArgIdx>( args );
          auto const& matcher_wrapper =
              std::get<ArgIdx>( matchers_ );
          auto const& matcher = matcher_wrapper.matcher();
          if( !matcher.matches( arg ) )
            throw_unexpected_error( fmt::format(
                "mock function call with unexpected arguments: "
                "{}( {} ); Argument #{} (one-based) does not "
                "match expected value {}.",
                fn_name_, formatted_args, ArgIdx + 1,
                matcher.format_expected() ) );
        };
    ( check_argument( std::integral_constant<size_t, Idx>{} ),
      ... );

    // 2. Set any output parameters that need to be set.
    if( setters_.has_value() ) {
      auto setter [[maybe_unused]] = []<typename T, typename U>(
                                         T&& src, U& dst ) {
        if constexpr( !std::is_same_v<std::remove_reference_t<T>,
                                      None> ) {
          // If we're here then the user has requested to set at
          // least one argument on this function call, but poten-
          // tially not all of the ones that are possible (purely
          // based on type) to set.
          if( src.has_value() ) {
            if constexpr( std::is_pointer_v<U> )
              *dst = *src;
            else // reference
              dst = *src;
          }
        }
      };
      ( setter( std::get<Idx>( *setters_ ),
                std::get<Idx>( args ) ),
        ... );
    }

    // 3. Set any output parameter C arrays that need to be set.
    if( array_setters_.has_value() ) {
      auto setter [[maybe_unused]] = []<typename T, typename U>(
                                         T&& src, U& dst ) {
        if constexpr( !std::is_same_v<std::remove_reference_t<T>,
                                      None> ) {
          auto* p = dst;
          for( auto const& src_elem : src ) *p++ = src_elem;
        }
      };
      ( setter( std::get<Idx>( *array_setters_ ),
                std::get<Idx>( args ) ),
        ... );
    }

    // Decrement this after args are checked so that we can de-
    // tect arg failures and recover and still use the responder.
    BASE_CHECK( times_expected_ > 0 );
    --times_expected_;

    // 4. Return what was requested to be returned.
    if constexpr( !std::is_same_v<RetT, void> ) {
      BASE_CHECK( ret_.has_value(),
                  "return value not set for {}.", fn_name_ );
      return ret_->get();
    }
  }

  bool finished() const { return times_expected_ == 0; }
  int  times_remaining() const { return times_expected_; }

  void clear_expectations() { times_expected_ = 0; }

  Responder& times( int n ) {
    BASE_CHECK( n > 0 );
    times_expected_ = n;
    return *this;
  }

  // clang-format off
  template<typename U>
  requires std::is_convertible_v<U, RetT>
  Responder& returns( U&& val = {} ) {
    // clang-format on
    ret_.emplace( static_cast<RetT>( std::forward<U>( val ) ) );
    return *this;
  }

  template<size_t Elem>
  /* clang-format off */
  requires Settable<std::tuple_element_t<Elem, args_t>>
  Responder& sets_arg(
      std::remove_reference_t<std::remove_pointer_t<
          std::tuple_element_t<Elem, args_t>>> const& val ) {
    /* clang-format on */
    if( !setters_.has_value() ) setters_.emplace();
    std::get<Elem>( *setters_ ) = val;
    return *this;
  }

  // For setting pointers to C arrays.
  template<size_t Elem, typename Container>
  /* clang-format off */
  requires
    SettablePointer<std::tuple_element_t<Elem, args_t>> &&
    requires( Container const& c ) {
      std::begin( c );
      std::end( c );
    } &&
    std::is_assignable_v<
      std::add_lvalue_reference_t<
          std::remove_pointer_t<
              std::tuple_element_t<Elem, args_t>>>,
      decltype( *std::begin( std::declval<Container>() ) )
    >
  Responder& sets_arg_array( Container&& container ) {
    /* clang-format on */
    if( !array_setters_.has_value() ) array_setters_.emplace();
    auto& vec = std::get<Elem>( *array_setters_ );
    BASE_CHECK( vec.empty() );
    for( auto&& val : std::forward<Container>( container ) )
      vec.push_back( std::forward<decltype( val )>( val ) );
    return *this;
  }

 private:
  base::maybe<RetHolder<RetT>> ret_ = {};
  // setters_t and array_setters_t are wrapped in a maybe for ef-
  // ficiency purposes; in most cases there will be no parameter
  // setting, and so then they will remain `nothing` and when the
  // mock is called, we will not have to iterate through the
  // tuple members to check if there are any that need to be set.
  base::maybe<setters_t>       setters_       = {};
  base::maybe<array_setters_t> array_setters_ = {};
  std::string                  fn_name_;
  matchers_t                   matchers_;
  int                          times_expected_;
};

template<typename R>
struct ResponderQueue {
  std::string   fn_name_ = {};
  std::queue<R> answers_ = {};

  exhaust_checker<std::queue<R>> checker_ = { fn_name_,
                                              &answers_ };

  // Because checker_ contains a pointer to answers_ that means
  // that these objects are self-referential and should not be
  // moved or copied.
  NO_COPY_NO_MOVE( ResponderQueue );

  ResponderQueue( std::string fn_name )
    : fn_name_( std::move( fn_name ) ) {}

  R& add( typename R::matchers_t args ) {
    answers_.push( R( fn_name_, std::move( args ) ) );
    return answers_.back();
  }

  template<typename... T>
  typename R::ret_t operator()( T&&... args ) {
    // We need to clear any empty responders from the front of
    // the queue just in case the user manually cleared them of
    // their expectations.
    while( !answers_.empty() && answers_.front().finished() )
      answers_.pop();
    if( answers_.empty() )
      throw_unexpected_error( fmt::format(
          "unexpected mock function call: {}", fn_name_ ) );
    R& responder = answers_.front();
    SCOPE_EXIT( if( responder.finished() ) answers_.pop(); );
    return responder( { std::forward<T>( args )... } );
  }
};

} // namespace detail

} // namespace mock
