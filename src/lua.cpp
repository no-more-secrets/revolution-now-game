/****************************************************************
**lua.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2019-09-13.
*
* Description: Interface to lua.
*
*****************************************************************/
#include "lua.hpp"

// Revolution Now
#include "aliases.hpp"
#include "errors.hpp"
#include "fmt-helper.hpp"
#include "init.hpp"
#include "logging.hpp"
#include "util.hpp"

// base-util
#include "base-util/io.hpp"
#include "base-util/string.hpp"
#include "base-util/type-map.hpp"

// Abseil
#include "absl/strings/str_replace.h"

using namespace std;

namespace rn::lua {

namespace {

sol::state g_lua;

auto& registration_functions() {
  static vector<RegistrationFn_t> fns;
  return fns;
}

expect<monostate> lua_script_( string_view script ) {
  auto result =
      g_lua.safe_script( script, sol::script_pass_on_error );
  if( !result.valid() ) {
    sol::error err = result;
    return UNEXPECTED( err.what() );
  }
  return monostate{};
}

using IntermediateCppTypeMap = TypeMap< //
    KV<int, double>                     //
    >;

template<typename Ret>
expect<Ret> sol_obj_convert( sol::object const& o ) {
  using intermediate_t = Get<IntermediateCppTypeMap, Ret, Ret>;
  if( !o.is<intermediate_t>() ) {
    string via =
        is_same_v<Ret, intermediate_t>
            ? ""
            : ( " (via `"s +
                demangled_typename<intermediate_t>() + "`)" );
    return UNEXPECTED( fmt::format(
        "expected type `{}`{} but got `{}`.",
        demangled_typename<Ret>(), via,
        sol::type_name( g_lua.lua_state(), o.get_type() ) ) );
  }
  auto intermediate_res = o.as<intermediate_t>();
  return static_cast<Ret>( intermediate_res );
}

template<typename Ret>
expect<Ret> lua_script( string_view script ) {
  auto result =
      g_lua.safe_script( script, sol::script_pass_on_error );
  if( !result.valid() ) {
    sol::error err = result;
    return UNEXPECTED( err.what() );
  }
  return sol_obj_convert<Ret>( result.get<sol::object>() );
}

bool is_valid_lua_identifier( string_view name ) {
  // Good enough for now.
  return !util::contains( name, " " ) &&
         !util::contains( name, "." );
}

expect<monostate> load_module( string const& name ) {
  lg.info( "loading lua module \"{}\".", name );
  CHECK( is_valid_lua_identifier( name ),
         "module name `{}` is not a valid lua identifier.",
         name );
  CHECK( g_lua[name] == sol::lua_nil,
         "cannot import package `{}` because that symbol "
         "already exists.",
         name );
  fs::path file_name = "src/lua/" + name + ".lua";
  CHECK( fs::exists( file_name ), "file {} does not exist.",
         file_name );
  g_lua["package_exports"] = sol::lua_nil;
  auto pf_result           = g_lua.safe_script_file(
      "src/lua/" + name + ".lua",
      []( auto*, auto pfr ) { return std::move( pfr ); } );
  if( !pf_result.valid() ) {
    sol::error err = pf_result;
    return UNEXPECTED( err.what() );
  }
  CHECK( g_lua["package_exports"] != sol::lua_nil,
         "module `{}` does not have package exports.", name );
  g_lua[name] = g_lua["package_exports"];

  g_lua["package_exports"] = sol::lua_nil;
  return monostate{};
}

void init_lua() {
  g_lua.open_libraries( sol::lib::base );
  CHECK( g_lua["log"] == sol::lua_nil );
  g_lua["log"].get_or_create<sol::table>();
  g_lua["log"]["info"] = []( string const& msg ) {
    lg.info( "{}", msg );
  };
  g_lua["log"]["debug"] = []( string const& msg ) {
    lg.debug( "{}", msg );
  };
  g_lua["log"]["warn"] = []( string const& msg ) {
    lg.warn( "{}", msg );
  };
  g_lua["log"]["error"] = []( string const& msg ) {
    lg.error( "{}", msg );
  };
  g_lua["log"]["critical"] = []( string const& msg ) {
    lg.critical( "{}", msg );
  };
  g_lua["print"] = []( sol::object o ) {
    if( o == sol::lua_nil ) return;
    if( auto maybe_string = o.as<Opt<string>>();
        maybe_string.has_value() )
      lg.info( "{}", *maybe_string );
    else if( auto maybe_bool = o.as<Opt<bool>>();
             maybe_bool.has_value() )
      lg.info( "{}", *maybe_bool );
    else if( auto maybe_double = o.as<Opt<double>>();
             maybe_double.has_value() )
      lg.info( "{}", *maybe_double );
    else
      lg.info( "(print: object cannot be converted to string)" );
  };

  lg.info( "registering Lua functions." );
  // Now run all the registration functions.
  for( auto const& fn : registration_functions() ) fn( g_lua );
}

void cleanup_lua() { g_lua = sol::state{}; }

REGISTER_INIT_ROUTINE( lua );

} // namespace

/****************************************************************
** Public API
*****************************************************************/
sol::state& state() { return g_lua; }

template<>
expect<monostate> run<void>( string const& script ) {
  return lua_script_( script );
}

template<>
expect<string> run<string>( string const& script ) {
  return lua_script<string>( script );
}

void load_modules() {
  for( auto const& path : util::wildcard( "src/lua/*.lua" ) )
    CHECK_XP( load_module( path.stem() ) );
}

void run_startup() {
  CHECK_XP( lua::run<void>( "startup.run()" ) );
}

Vec<Str> format_lua_error_msg( Str const& msg ) {
  Vec<Str> res;
  for( auto const& line : util::split_on_any( msg, "\n\r" ) )
    if( !line.empty() ) //
      res.push_back(
          absl::StrReplaceAll( line, {{"\t", "  "}} ) );
  return res;
}

void register_fn( RegistrationFn_t fn ) {
  registration_functions().push_back( std::move( fn ) );
}

/****************************************************************
** Testing
*****************************************************************/
void test_lua() {
  sol::state lua;
  // int        x = 0;
  // lua.open_libraries( sol::lib::base );
  // lua.set_function( "beep", [&x]() { ++x; } );
  auto result = lua_script<int>( "return 56.4" );
  lg.info( "result: {}", result );
}

} // namespace rn::lua
