/****************************************************************
**config-files.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2018-11-29.
*
* Description: Handles config file data.
*
*****************************************************************/
#include "config-files.hpp"

#include "errors.hpp"

// Only include this in this cpp module.
#include "ucl++.h"

#include <string>
#include <unordered_map>

#undef CFG
#undef OBJ
#undef FLD

// This is just for debugging the registration of the the config
// data structure members.  Normally should not be enabled even
// for debug builds.
#if 0
#define CONFIG_LOG_STREAM std::cout
#else
#define CONFIG_LOG_STREAM util::cnull
#endif

#define FLD( __type, __name )                             \
  static void __populate_##__name() {                     \
    auto* dest = &( dest_ptr()->__name );                 \
    populate_config_field( dest, TO_STRING( __name ),     \
                           this_level() + 1, this_name(), \
                           this_path(), this_file() );    \
  }                                                       \
  static inline bool const __register_##__name = [] {     \
    CONFIG_LOG_STREAM << "register: " << this_name()      \
                      << "." TO_STRING( __name ) "\n";    \
    config_registration_functions().push_back(            \
        {this_level() + 1, __populate_##__name} );        \
    return true;                                          \
  }();

#define OBJ( __name, __body )                                    \
  static auto*      __name##_parent_ptr() { return dest_ptr(); } \
  static ConfigPath __name##_parent_path() {                     \
    return this_path();                                          \
  }                                                              \
  static int __name##_parent_level() { return this_level(); }    \
                                                                 \
  struct __name##_object;                                        \
  __name##_object* __##__name;                                   \
                                                                 \
  struct __name##_object {                                       \
    using this_type = __name##_object;                           \
    static auto* dest_ptr() {                                    \
      return &( __name##_parent_ptr()->__name );                 \
    }                                                            \
    static ConfigPath this_path() {                              \
      auto path = __name##_parent_path();                        \
      path.push_back( TO_STRING( __name ) );                     \
      return path;                                               \
    }                                                            \
    static int this_level() {                                    \
      return __name##_parent_level() + 1;                        \
    }                                                            \
                                                                 \
    __body                                                       \
  };

#define CFG( __name, __body )                               \
  config_##__name##_object config_##__name;                 \
                                                            \
  struct shadow_config_##__name##_object {                  \
    using this_type = config_##__name##_object;             \
    static config_##__name##_object* dest_ptr() {           \
      return &( config_##__name );                          \
    }                                                       \
    static ConfigPath  this_path() { return {}; }           \
    static int         this_level() { return 0; }           \
    static std::string this_name() {                        \
      return TO_STRING( __name );                           \
    }                                                       \
    static std::string this_file() {                        \
      return config_file_for_name( this_name() );           \
    }                                                       \
                                                            \
    __body                                                  \
  };                                                        \
                                                            \
  struct startup_##__name {                                 \
    static void __load_##__name() {                         \
      config_files().push_back(                             \
          {shadow_config_##__name##_object::this_name(),    \
           shadow_config_##__name##_object::this_file()} ); \
    }                                                       \
    static inline int const __register_##__name = [] {      \
      CONFIG_LOG_STREAM << "register load: "                \
                        << TO_STRING( __name ) "\n";        \
      load_registration_functions().push_back(              \
          __load_##__name );                                \
      return 0;                                             \
    }();                                                    \
  };

namespace rn {

namespace {

std::unordered_map<std::string, ucl::Ucl> ucl_configs;

using RankedFunction =
    std::pair<int, std::function<void( void )>>;

using ConfigPath = std::vector<std::string>;

#define POPULATE_FIELD_DECL( __type )                   \
  void populate_config_field(                           \
      __type* dest, std::string const& name, int level, \
      std::string const& config_name,                   \
      ConfigPath const& parent_path, std::string const& file );

POPULATE_FIELD_DECL( bool )
POPULATE_FIELD_DECL( int )
POPULATE_FIELD_DECL( double )
POPULATE_FIELD_DECL( std::string )

ucl::Ucl ucl_from_path( std::string const& name,
                        ConfigPath const&  components ) {
  // This must be by value since we reassign it.
  ucl::Ucl obj = ucl_configs[name];
  for( auto const& s : components ) {
    obj = obj[s];
    if( !obj ) break;
  }
  return obj; // return it whether true or false
}

// TODO: use fs::path here
using ConfigLoadPairs =
    std::vector<std::pair<std::string, std::string>>;

ConfigLoadPairs& config_files() {
  static ConfigLoadPairs pairs;
  return pairs;
}

std::vector<RankedFunction>& config_registration_functions() {
  static std::vector<RankedFunction>
      config_registration_functions;
  return config_registration_functions;
}

std::vector<std::function<void( void )>>&
load_registration_functions() {
  static std::vector<std::function<void( void )>>
      load_registration_functions;
  return load_registration_functions;
}

std::string config_file_for_name( std::string const& name ) {
  return "config/" + name + ".ucl";
}

#define POPULATE_FIELD_IMPL( __type, __ucl_type, __ucl_getter ) \
  void populate_config_field(                                   \
      __type* dest, std::string const& name, int level,         \
      std::string const& config_name,                           \
      ConfigPath const&  parent_path,                           \
      std::string const& file ) {                               \
    auto path = parent_path;                                    \
    path.push_back( name );                                     \
    auto obj = ucl_from_path( config_name, path );              \
    CHECK_( obj.type() != ::UCL_NULL,                           \
            "UCL Config field `" << util::join( path, "." )     \
                                 << "` was not found in file "  \
                                 << file << "." );              \
    CHECK_( obj.type() == __ucl_type,                           \
            "expected `"                                        \
                << config_name << "."                           \
                << util::join( path, "." )                      \
                << "` to be of type " TO_STRING( __type ) );    \
    *dest = obj.__ucl_getter();                                 \
    CONFIG_LOG_STREAM << level << " populate: " << config_name  \
                      << "." << util::join( path, "." )         \
                      << " = " << util::to_string( *dest )      \
                      << "\n";                                  \
  }

// clang-format off
/****************************************************************
* Mapping From C++ Types to UCL Types
*
*                    C++ type      UCL Enum      Ucl::???
*                    -------------------------------------------*/
POPULATE_FIELD_IMPL( int,          UCL_INT,      int_value      )
POPULATE_FIELD_IMPL( bool,         UCL_BOOLEAN,  bool_value     )
POPULATE_FIELD_IMPL( double,       UCL_FLOAT,    number_value   )
POPULATE_FIELD_IMPL( std::string,  UCL_STRING,   string_value   )
/****************************************************************/
// clang-format on

} // namespace

#include "../config/config-vars.schema"

void load_configs() {
  for( auto const& f : load_registration_functions() ) f();
  for( auto [ucl_name, file] : config_files() ) {
    // std::cout << "Loading file " << file << "\n";
    auto&       ucl_obj = ucl_configs[ucl_name];
    std::string errors;
    ucl_obj = ucl::Ucl::parse_from_file( file, errors );
    CHECK_( ucl_obj,
            "failed to load " << file << ": " << errors );
  }
  sort( config_registration_functions().begin(),
        config_registration_functions().end(),
        []( RankedFunction const& left,
            RankedFunction const& right ) {
          return left.first < right.first;
        } );
  for( auto const& p : config_registration_functions() )
    p.second();
}

} // namespace rn
