/****************************************************************
**frame.hpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2019-02-13.
*
* Description: Frames and frame rate.
*
*****************************************************************/
#pragma once

// Revolution Now
#include "cache.hpp"
#include "core-config.hpp"
#include "math.hpp"

// base-util
#include "base-util/non-copyable.hpp"

// Abseil
#include "absl/container/node_hash_map.h"

// C++ standard library
#include <chrono>
#include <functional>

namespace rn {

using Frames = std::chrono::duration<int, std::ratio<1, 60>>;

void frame_loop( bool                  poll_input,
                 std::function<bool()> finished );

double avg_frame_rate();

uint64_t total_frame_count();

using FrameSubscriptionFunc = std::function<void( void )>;

// Subscribe to receive a notification every n ticks.
void subscribe_to_frame_tick( FrameSubscriptionFunc f, int n );
// Subscribe to receive a notification every n milliseconds.
void subscribe_to_frame_tick( FrameSubscriptionFunc,
                              std::chrono::milliseconds n );

// Use node_hash_map because MovingAverage objects are
// non-copyable and non-movable.
using EventCountMap =
    absl::node_hash_map<std::string_view,
                        MovingAverage<3 /*seconds*/>>;

EventCountMap& event_counts();

// This invalidator will report an invalidation the first time it
// is called on each new frame, and not for the rest of the
// frame. It must not be copyable otherwise some frameworks (such
// as range-v3) will copy it and call it multiple times within a
// frame and hence it may end up return `true` more than once per
// frame, thus causing unnecessary calls to the wrapped function.
struct PerFrameInvalidator : public util::movable_only {
  uint64_t curr_frame{0};
  bool     operator()() {
    auto real_frame = total_frame_count();
    if( curr_frame == real_frame ) return false;
    curr_frame = real_frame;
    return true;
  }
};

// Use this to memoize a function in such a way that the wrapped
// function will be called to compute a value at most once per
// frame per argument value (if there is an argument). If the
// wrapped function takes no arguments then it will be called at
// most once per frame.
template<typename Func>
auto per_frame_memoize( Func func ) {
  return memoizer( func, PerFrameInvalidator{} );
}

} // namespace rn
