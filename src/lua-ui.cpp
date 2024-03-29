/****************************************************************
**lua-ui.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2021-07-19.
*
* Description: Exposes some UI functions to Lua.
*
*****************************************************************/
#include "lua-ui.hpp"

// Revolution Now
#include "co-lua.hpp"
#include "co-wait.hpp"
#include "logger.hpp"
#include "lua-wait.hpp"

// luapp
#include "luapp/ext-base.hpp"
#include "luapp/ext-monostate.hpp"
#include "luapp/register.hpp"
#include "luapp/rtable.hpp"
#include "luapp/state.hpp"

// refl
#include "refl/to-str.hpp"

using namespace std;

namespace rn {

void linker_dont_discard_module_lua_ui();
void linker_dont_discard_module_lua_ui() {}

namespace {

// LUA_AUTO_FN( "message_box", ui::message_box_basic );
// LUA_AUTO_FN( "str_input_box", ui::str_input_box );

// LUA_FN( ok_cancel, wait<string>, string_view msg ) {
//   ui::e_ok_cancel res = co_await ui::ok_cancel( msg );
//   co_return fmt::to_string( res );
// }

} // namespace

wait<> lua_ui_test() {
  NOT_IMPLEMENTED;
  // ScopedPlanePush pusher( e_plane_config::black );
  // lua::state&     st = lua_global_state();
  //
  // auto n = co_await lua_wait<maybe<int>>(
  //     st["test"]["some_ui_routine"], 42 );
  //
  // lg.info( "received {} from some_ui_routine.", n );
}

} // namespace rn
