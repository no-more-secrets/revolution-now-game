/****************************************************************
**runner.hpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2021-07-30.
*
* Description: Runners of parsers.
*
*****************************************************************/
#pragma once

// parz
#include "combinator.hpp"
#include "error.hpp"
#include "ext.hpp"
#include "parser.hpp"
#include "promise.hpp"

// base
#include "base/error.hpp"

// C++ standard library
#include <string>
#include <string_view>

/****************************************************************
** Parser Runners
*****************************************************************/
namespace parz {

struct ErrorPos {
  static ErrorPos from_index( std::string_view in, int idx );
  int             line;
  int             col;
};

// `filename` is the original file name that the string came
// from, in order to improve error messages.
template<typename T>
result_t<T> run_parser( std::string_view filename,
                        std::string_view in, parser<T> p ) {
  p.resume( in );
  DCHECK( p.finished() );
  if( p.is_error() ) {
    // It's always one too far, not sure why.
    ErrorPos ep = ErrorPos::from_index( in, p.farthest() - 1 );
    p.result()  = parz::error(
         fmt::format( "{}:error:{}:{}: {}\n", filename, ep.line,
                      ep.col, p.error() ) );
  }
  return std::move( p.result() );
}

// `filename` is the original file name that the string came
// from, in order to improve error messages.
template<typename Lang, typename T>
result_t<T> parse_from_string( std::string_view filename,
                               std::string_view in ) {
  return run_parser( filename, in,
                     exhaust( parz::parse<Lang, T>() ) );
}

namespace internal {

base::expect<std::string, error> read_file_into_buffer(
    std::string_view filename );

}

template<typename Lang, typename T>
result_t<T> parse_from_file( std::string_view filename ) {
  UNWRAP_RETURN( buffer,
                 internal::read_file_into_buffer( filename ) );
  return parse_from_string<Lang, T>( filename, buffer );
}

} // namespace parz
