#include "fonts.hpp"
#include "global-constants.hpp"
#include "globals.hpp"
#include "macros.hpp"
#include "ownership.hpp"
#include "sdl-util.hpp"
#include "sound.hpp"
#include "tiles.hpp"
#include "turn.hpp"
#include "unit.hpp"
#include "util.hpp"
#include "viewport.hpp"
#include "window.hpp"
#include "world.hpp"

#include "fmt/format.h"

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_mixer.h>

#include <algorithm>
#include <iostream>

using namespace rn;
using namespace std;

// clang-format off
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
// clang-format on

void stdout_example() {
  // create color multi threaded logger
  auto console = spdlog::stdout_color_mt( "console" );
  console->info( "Welcome to spdlog!" );
  console->error( "Some error message with arg: {}", 1 );

  auto err_logger = spdlog::stderr_color_mt( "stderr" );
  err_logger->error( "Some error message" );

  // Formatting examples
  console->warn( "Easy padding in numbers like {:08d}",
                 12 ); // NOLINT
  console->critical(
      "Support for int: {0:d};  hex: {0:x};  oct: {0:o}; bin: "
      "{0:b}",
      42 ); // NOLINT
  console->info( "Support for floats {:03.2f}",
                 1.23456 ); // NOLINT
  console->info( "Positional args are {1} {0}..", "too",
                 "supported" );
  console->info( "{:<30}", "left aligned" );

  spdlog::get( "console" )
      ->info(
          "loggers can be retrieved from a global registry "
          "using the spdlog::get(logger_name)" );

  // Runtime log levels
  spdlog::set_level(
      spdlog::level::info ); // Set global log level to info
  console->debug( "This message should not be displayed!" );
  console->set_level(
      spdlog::level::trace ); // Set specific logger's log level
  console->debug( "This message should be displayed.." );

  // Customize msg format for all loggers
  spdlog::set_pattern(
      "[%H:%M:%S %z] [%n] [%^---%L---%$] [thread %t] %v" );
  console->info( "This an info message with custom format" );

  // Compile time log levels
  // define SPDLOG_DEBUG_ON or SPDLOG_TRACE_ON
  SPDLOG_TRACE( console,
                "Enabled only #ifdef SPDLOG_TRACE_ON..{} ,{}", 1,
                3.23 );
  SPDLOG_DEBUG( console,
                "Enabled only #ifdef SPDLOG_DEBUG_ON.. {} ,{}",
                1, 3.23 );
}

int main( int /*unused*/, char** /*unused*/ ) try {
  fmt::print( "Hello, {}!\n", "world" );
  auto s = fmt::format( "this {} a {}.\n", "is", "test" );
  fmt::print( s );

  stdout_example();

  init_game();
  load_sprites();
  load_tile_maps();

  // CHECK( play_music_file( "assets/music/bonny-morn.mp3" ) );

  //(void)create_unit_on_map( e_unit_type::free_colonist, Y(2),
  // X(3) );
  (void)create_unit_on_map( e_unit_type::free_colonist, Y( 2 ),
                            X( 4 ) );
  (void)create_unit_on_map( e_unit_type::caravel, Y( 2 ),
                            X( 2 ) );
  //(void)create_unit_on_map( e_unit_type::caravel, Y(2), X(1) );

  // while( turn() != e_turn_result::quit ) {}
  //(void)turn();

  // font_test();
  gui::test_window();

  cleanup();
  return 0;

} catch( exception const& e ) {
  cerr << e.what() << endl;
  cerr << "SDL_GetError: " << SDL_GetError() << endl;
  cleanup();
  return 1;
}
