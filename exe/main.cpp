#include "errors.hpp"
#include "fmt-helper.hpp"
#include "frame.hpp"
#include "init.hpp"
#include "linking.hpp"
#include "logging.hpp"
#include "lua.hpp"
#include "screen.hpp"

#include "SDL.h"
#include "SDL_image.h"
#include "SDL_mixer.h"

using namespace rn;
using namespace std;

namespace rn {

void game() {
  frame_loop( /*poll_input=*/true,
              /*finished=*/L0( false ) );
}

} // namespace rn

int main( int /*unused*/, char** /*unused*/ ) {
  try {
    try {
      linker_dont_discard_me();
      run_all_init_routines( e_log_level::debug );
      lua::reload();
      lua::run_startup_main();

      game();

    } catch( exception_exit const& ) {}

    hide_window();

    run_all_cleanup_routines();
  } catch( exception_with_bt const& e ) {
    hide_window();
    lg.error( e.what() );
    string sdl_error = SDL_GetError();
    if( !sdl_error.empty() )
      lg.error( "SDL error (may be a false positive): {}",
                sdl_error );
    print_stack_trace( e.st, 4 );
    run_all_cleanup_routines();
    return 1;
  } catch( exception const& e ) {
    lg.error( e.what() );
    string sdl_error = SDL_GetError();
    if( !sdl_error.empty() )
      lg.error( "SDL error (may be a false positive): {}",
                sdl_error );
    run_all_cleanup_routines();
    return 1;
  }
}
