/****************************************************************
**color.hpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2018-12-17.
*
* Description: All things color.
*
*****************************************************************/
#pragma once

#include "core-config.hpp"

// Revolution Now
#include "aliases.hpp"
#include "geo-types.hpp"

// c++ standard library
#include <cstdint>
#include <string_view>
#include <tuple>
#include <vector>

namespace rn {

// Standard RGBA form.  Can be freely passed by value.
struct Color {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 0;

  Color() {}
  Color( uint8_t r_, uint8_t g_, uint8_t b_ )
    : r( r_ ), g( g_ ), b( b_ ), a( 255 ) {}
  Color( uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ )
    : r( r_ ), g( g_ ), b( b_ ), a( a_ ) {}

  double luminosity() const;

  auto to_tuple() const { return std::tuple( r, g, b, a ); }

  // #NNNNNN with N in [0-9A-F] and optionally #NNNNNNNN
  // if with_alpha = true;
  std::string to_string( bool with_alpha = false ) const;

  // Parses a string of the form 'NNNNNN[NN]' where N is:
  // [0-9a-fA-F]. The optional two digits at the end represent
  // alpha. If these are omitted then alpha will be set to 255.
  static Opt<Color> parse_from_hex( std::string_view hex );

  // A random color.
  static Color random();

  // Abseil hashing API.
  template<typename H>
  friend H AbslHashValue( H h, Color const& c ) {
    return H::combine( std::move( h ), c.to_tuple() );
  }

  bool operator<( Color const& rhs ) const {
    return to_tuple() < rhs.to_tuple();
  }
  bool operator==( Color const& rhs ) const {
    return to_tuple() == rhs.to_tuple();
  }

  static Color red() { return {255, 0, 0, 255}; }
  static Color green() { return {0, 255, 0, 255}; }
  static Color blue() { return {0, 0, 255, 255}; }
  static Color white() { return {255, 255, 255, 255}; }
  static Color black() { return {0, 0, 0, 255}; }
};

// So that we can pass it by value.
static_assert( sizeof( Color ) == 4 );

/****************************************************************
**Non-Game Utilities: Palette Generation/Display
*
* These functions are not run during the game; they are only run
* in a once-off manner to generate/update the game palettes.
*****************************************************************/
// Given an arbitrary color it will return the hierarchical
// location of that color in the bucketing scheme that we use in
// this game. E.g., calling it with #FF0000 might return
// something along the lines of "red.sat2.lum12".
std::string bucket_path( Color c );

// Sorts colors in a quasi-human way.
void hsl_bucketed_sort( Vec<Color>& colors );

// Removes colors with a saturation below a threshold.
void remove_greys( Vec<Color>& colors );

// For holding a list of colors that are bucketed first by hue,
// then by saturation, then by luminosity.
using ColorBuckets = Vec<Vec<Vec<Opt<Color>>>>;

// This will iterate through the colors and place each one into a
// bucket depending on its values of hue, saturation, and
// luminosity (each of which are bucketed). There are only a
// finite number of buckets in each category, so in general the
// resulting colors will be fewer than the input if the input
// contains multiple colors that all fall into the same bucket.
// If there are no colors for a bucket then that bucket will be
// nullopt.
ColorBuckets hsl_bucket( Vec<Color> const& colors );

// Load the image file(s) and scan every pixel and compile a list
// of unique colors. Then, if a target number of colors is
// specified, try to reduce the number of colors to achieve
// approximately the target number. The algorithm will try its
// best to achieve this number, but typically the set of returned
// colors may have a bit more or less. Also, the order of colors
// returned is unspecified.
Vec<Color> extract_palette( fs::path const& glob,
                            Opt<int> target = std::nullopt );

// Will remove colors that are redundant or approximately
// redunant in order to meet the target count. It will always
// return a number of colors that is >= min_count so long as
// there are at least that many to begin with.
Vec<Color> coursen( Vec<Color> const& colors, int min_count );

// Will look in the `where` folder and will load all files
// (assuming they are image files) and will load/scan each one of
// them for their colors and will generate a ~256 color palette
// and will update the schema and ucl palette definition file.
// Note that running this could in general break your build
// because the game might be using colors (e.g.
// config_palette.red.sat0.lum1) that no longer appear after the
// update.
void update_palette( fs::path const& where );

// Will load/parse the ucl config palette file and will
// sort/bucket the colors and display them on the screen. NOTE:
// SDL graphics must have been initialized before calling this
// function.
void show_config_palette();

// Will load the ucl config palette and render it to a png
// file divided into hue/saturation buckets.
void write_palette_png( fs::path const& png_file );

// Generate the config schema and ucl data file with all the
// colors so that the game can reference them with e.g.
// config_palette.red.sat0.lum1. Note that this updates the
// schema file and hence requires recompilation to take effect.
void dump_palette( ColorBuckets const& colors,
                   fs::path const& schema, fs::path const& ucl );

} // namespace rn
