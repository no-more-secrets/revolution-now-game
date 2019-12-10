/****************************************************************
**sync-future.hpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2019-12-08.
*
* Description: Synchronous promise & future.
*
*****************************************************************/
#pragma once

#include "core-config.hpp"

// Revolution Now
#include "aliases.hpp"
#include "errors.hpp"
#include "fmt-helper.hpp"

// base-util
#include "base-util/non-copyable.hpp"

// C++ standard library
#include <memory>

namespace rn {

namespace internal {

template<typename T>
struct sync_shared_state_base {
  virtual ~sync_shared_state_base() = default;
  virtual bool has_value() const    = 0;
  virtual T    get() const          = 0;
};

} // namespace internal

/****************************************************************
** sync_future
*****************************************************************/
// Single-threaded "future" object: represents a value that will
// become available in the future by the same thread.
//
// The sync_future has three states:
//
//   waiting --> ready -->? empty
//
// It starts off in the `waiting` state upon construction (from a
// sync_promise) then transitions to `ready` when the result be-
// comes available. During this time, the value can be retrieved
// any number of times. The sync_future remains in this state
// until `reset` is called (or if `get_and_reset` had been called
// instead of `get`), at which point it transitions to the
// `empty` state. Once in the `empty` state the sync_future is
// "dead" forever.
//
// Example usage:
//
//   sync_promise<int> s_promise;
//   sync_future<int>  s_future1 = s_promise.get_future();
//
//   sync_future<int> s_future2 = s_future1.then(
//       []( int n ){ return n+1; } );
//
//   s_promise.set_value( 3 );
//
//   assert( s_future1.get_and_reset() == 3 );
//   assert( s_future2.get_and_reset() == 4 );
//
//   assert( s_future1.empty() );
//   assert( s_future2.empty() );
//
template<typename T>
class sync_future : public util::movable_only {
  using SharedStatePtr =
      std::shared_ptr<internal::sync_shared_state_base<T>>;

public:
  // This constructor should not be used by client code.
  explicit sync_future( SharedStatePtr shared_state )
    : shared_state_{ shared_state } {
    CHECK( waiting() );
  }

  bool empty() const { return shared_state_ == nullptr; }

  bool waiting() const {
    if( empty() ) return false;
    return !shared_state_->has_value();
  }

  bool ready() const {
    if( empty() ) return false;
    return shared_state_->has_value();
  }

  // Only this method is provided for getting the value out. This
  // is to force the client to only call this method once, since
  // it may have side effects (e.g., if this future was create
  // from a continuation that has side effects).
  T get_and_reset() {
    CHECK( ready(),
           "attempt to get value from sync_future when not in "
           "`ready` state." );
    T res = shared_state_->get();
    shared_state_.reset();
    return res;
  }

  void reset() { shared_state_.reset(); }

  template<typename Func>
  auto then( Func&& func ) {
    CHECK( !empty(),
           "attempting to attach a continuation to an empty "
           "sync_future." );
    using NewResult_t =
        std::decay_t<std::invoke_result_t<Func, T>>;

    struct sync_shared_state_with_continuation
      : public internal::sync_shared_state_base<NewResult_t> {
      ~sync_shared_state_with_continuation() override = default;

      sync_shared_state_with_continuation(
          SharedStatePtr old_shared_state, Func&& continuation )
        : old_shared_state_( old_shared_state ),
          continuation_( std::forward<Func>( continuation ) ) {}

      bool has_value() const override {
        return old_shared_state_->has_value();
      }

      NewResult_t get() const override {
        CHECK( has_value() );
        return continuation_( old_shared_state_->get() );
      }

      SharedStatePtr old_shared_state_;
      Func           continuation_;
    };

    return sync_future<NewResult_t>(
        std::make_shared<sync_shared_state_with_continuation>(
            shared_state_, std::forward<Func>( func ) ) );
  }

  // Returns a future object that, when its value is retreived,
  // will store the value in the given pointer and just return
  // std::monostate{}.
  auto stored( T* destination ) {
    CHECK( !empty(),
           "attempting to attach a continuation to an empty "
           "sync_future." );
    return then( [destination]( T const& value ) {
      *destination = value;
      return std::monostate{};
    } );
  }

private:
  SharedStatePtr shared_state_;
};

/****************************************************************
** sync_promise
*****************************************************************/
// Single-threaded "promise" object: allows creating futures and
// sending values to them in the same thread.
//
// Example usage:
//
//   sync_promise<int> s_promise;
//
//   sync_future<int> s_future = s_promise.get_future();
//   assert( s_future.waiting() );
//
//   s_promise.set_value( 3 );
//
//   assert( s_future.get() == 3 );
//
//   assert( s_future.get_and_reset() == 3 );
//   assert( s_future.empty() );
//
template<typename T>
class sync_promise : public util::movable_only {
  struct sync_shared_state
    : public internal::sync_shared_state_base<T> {
    ~sync_shared_state() override = default;

    sync_shared_state() = default;

    bool has_value() const override {
      return maybe_value.has_value();
    }

    T get() const override {
      CHECK( has_value() );
      return *maybe_value;
    }

    Opt<T> maybe_value;
  };

public:
  sync_promise() : shared_state_( new sync_shared_state ) {}

  bool has_value() const { return shared_state_->has_value(); }

  void set_value( T const& value ) {
    CHECK( !has_value() );
    shared_state_->maybe_value = value;
  }

  void set_value( T&& value ) {
    CHECK( !has_value() );
    shared_state_->maybe_value = std::move( value );
  }

  sync_future<T> get_future() const {
    return sync_future<T>( shared_state_ );
  }

  std::shared_ptr<sync_shared_state> const& shared_state()
      const {
    return shared_state_;
  }

private:
  std::shared_ptr<sync_shared_state> shared_state_;
};

} // namespace rn

/****************************************************************
** {fmt}
*****************************************************************/
namespace fmt {
// {fmt} formatters.

template<typename T>
struct formatter<::rn::sync_future<T>> : formatter_base {
  template<typename FormatContext>
  auto format( ::rn::sync_future<T> const& o,
               FormatContext&              ctx ) {
    std::string res;
    if( o.empty() )
      res = "<empty>";
    else if( o.waiting() )
      res = "<waiting>";
    else if( o.ready() )
      res = fmt::format( "<ready>" );
    return formatter_base::format( res, ctx );
  }
};

template<typename T>
struct formatter<::rn::sync_promise<T>> : formatter_base {
  template<typename FormatContext>
  auto format( ::rn::sync_promise<T> const& o,
               FormatContext&               ctx ) {
    std::string res;
    if( !o.has_value() )
      res = "<empty>";
    else
      res = fmt::format( "<ready>" );
    return formatter_base::format( res, ctx );
  }
};
} // namespace fmt
