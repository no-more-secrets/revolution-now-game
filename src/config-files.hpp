/****************************************************************
**config-files.hpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2018-11-29.
*
* Description: Handles config file data.
*
*  FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
*  The config-vars.inl needs to be split because each time it
*  changes (i.e., we add a new config module) it causes all files
*  that include this one to be rebuilt (i.e., most).
*  FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
*
*****************************************************************/
#pragma once

#include "core-config.hpp"

#include "aliases.hpp"
#include "color.hpp"
#include "conductor.hpp"
#include "coord.hpp"
#include "mv-points.hpp"
#include "nation.hpp"
#include "tune.hpp"
#include "typed-int.hpp"
#include "utype.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#define STRUCT( name, body ) \
  struct name {              \
    body                     \
  };

// It is found that when a config object contains only fields
// and subobjects which can be constructed at compile time
// (constexpr constructable?) then the compiler may opt to put
// the structure into read-only memory since it is declared
// const.  However this then prevents us from populating it
// initially (leads to a segfault). We want it const, but we want
// to be able to const_cast it during initialization to give
// values to it.  So for thatreason we inject a std::string
// variable into the struct to prevent this.
#define CFG( name, body )                 \
  struct config_##name##_t {              \
    body std::string __prevent_read_only; \
  };                                      \
  extern config_##name##_t const config_##name;

#define OBJ( name, body )  \
  STRUCT( name##_t, body ) \
  name##_t name;

#define FLD( type, name ) type name;

#define LNK( from, to ) \
  decltype( ::rn::config_##to ) const* from;

namespace rn {

#include "../config/config-vars.inl"

// The main global palette sorted using bucketed hue-sat-lum.
// Colors should normally be accessed using the config_palette
// global config structure, e.g. config_palette.red.sat0.lum1.
// This function should only be used during development e.g. to
// aid in color selection or troubleshooting and/or if one needs
// to iterate over all the colors in the palette. NOTE: this
// will skip over the grey scale colors.
Vec<Color> const& g_palette();

} // namespace rn
