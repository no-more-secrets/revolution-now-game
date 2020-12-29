/****************************************************************
**coro-compat.hpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2020-12-29.
*
* Description: Some fixes and compatibility stuff that will make
*              coroutines work with both clang and gcc and with
*              both libc++ and libstdc++, including the
*              clang+libstdc++ combo.
*
*****************************************************************/
#pragma once

/****************************************************************
** Fix CLI flag inconsistency.
*****************************************************************/
// libstdc++ throws an error if this is not defined, but it is
// only defined when -fcoroutines is specified, which clang does
// not support (it uses -fcoroutines-ts), which makes the
// clang+libstdc++ combo impossible with coroutines. This works
// around that.
#if !defined( __cpp_impl_coroutine )
#  define __cpp_impl_coroutine 1
#endif

/****************************************************************
** Fix header inconsistency.
*****************************************************************/
// FIXME: remove this when libc++ moves the coroutine library out
// of experimental.
#if defined( _LIBCPP_VERSION )
#  include <experimental/coroutine> // libc++
#else
#  include <coroutine> // libstdc++
#endif

/****************************************************************
** Fix namespace inconsistency.
*****************************************************************/
// FIXME: remove this when libc++ moves the coroutine library out
// of experimental.
#if defined( _LIBCPP_VERSION )
namespace coro = ::std::experimental; // libc++
#else
namespace coro = ::std; // libstdc++
#endif

/****************************************************************
** Fix coroutine_handle::from_address not being noexcept.
*****************************************************************/
namespace base {
// Inject this into std::experimental to make clang happy. Also,
// override `coroutine_handle` so that we can make one of its
// members `noexcept` that should be, otherwise clang gets
// scared. FIXME: remove this when libc++ moves the coroutine li-
// brary out of experimental.
template<typename T = void>
class coroutine_handle : public coro::coroutine_handle<T> {
public:
  // Override this function just to make it noexcept; the one in
  // the gcc 10.2.0 libstdc++ is not, and this scares clang.
  constexpr static coroutine_handle from_address(
      void* __a ) noexcept {
    coroutine_handle __self;
    __self._M_fr_ptr = __a;
    return __self;
  }
};
} // namespace base

/****************************************************************
** Fix that clang is looking for these in the experimental ns.
*****************************************************************/
#if defined( __clang__ ) && !defined( _LIBCPP_VERSION )
namespace std::experimental {
// Inject these into std::experimental to make clang happy.
// FIXME: remove these when libc++ moves the coroutine library
// out of experimental.
using base::coroutine_handle;
using coro::coroutine_traits;
} // namespace std::experimental
#endif

/****************************************************************
** Fix that gcc's suspend_never has non-noexcept methods.
*****************************************************************/
namespace base {
// We need to include this because the one that gcc 10.2 comes
// shiped with does not have its methods declared with noexcept,
// which scares clang and prevents us from using coroutines with
// the clang+libstdc++ combo. FIXME: remove this when gcc 10.2
// fixes its version.
struct suspend_never {
  bool await_ready() noexcept { return true; }
  void await_suspend( coro::coroutine_handle<> ) noexcept {}
  void await_resume() noexcept {}
};
} // namespace base
