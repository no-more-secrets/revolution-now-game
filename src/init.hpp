/****************************************************************
**init.hpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2019-02-19.
*
* Description: Handles initialization of all subsystems.
*
*****************************************************************/
#pragma once

#include "core-config.hpp"

// Revolution Now
#include "aliases.hpp"
#include "enum.hpp"

// base-util
#include "base-util/macros.hpp"

// Abseil
#include "absl/types/span.h"

// C++ standard library
#include <functional>
#include <vector>

namespace rn {

// Ordering of these enum values does not matter.
enum class e_( init_routine,
               /************/
               compositor,    //
               conductor,     //
               configs,       //
               europort_view, //
               lua,           //
               images,        //
               menus,         //
               midiplayer,    //
               midiseq,       //
               oggplayer,     //
               planes,        //
               plane_config,  //
               renderer,      //
               rng,           //
               screen,        //
               sdl,           //
               sound,         //
               sprites,       //
               terrain,       //
               text,          //
               ttf,           //
               tunes          //
);

using InitFunction = std::function<void( void )>;

void register_init_routine( e_init_routine      routine,
                            InitFunction const& init_func,
                            InitFunction const& cleanup_func );

#define REGISTER_INIT_ROUTINE( name )                         \
  STARTUP() {                                                 \
    register_init_routine( e_init_routine::name, init_##name, \
                           cleanup_##name );                  \
  }

// Will run initialization routines in order of dependencies. If
// `only` is not nullopt then just it and its dependencies will
// be run. Otherwise all routines will be run in order of depen-
// dencies.
void run_all_init_routines(
    Opt<e_log_level>                 level,
    absl::Span<e_init_routine const> top_level = {} );
// This will run the corresponding cleanup routine for each
// initialization routine that was successfully run, and will do
// so in the opposite order.
void run_all_cleanup_routines();

// Returns false during initialization.
bool has_init_finished();

#define SHOULD_BE_HERE_ONLY_DURING_INITIALIZATION      \
  CHECK( !has_init_finished(),                         \
         "This code path should only be taken during " \
         "initialization" )

} // namespace rn
