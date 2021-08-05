/****************************************************************
**parse.hpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2021-07-30.
*
* Description: Parser for config files.
*
*****************************************************************/
#pragma once

// rcl
#include "model.hpp"

// base
#include "base/expect.hpp"

// C++ standard library
#include <string_view>

namespace rcl {

// This will keep the string the same length but will ovewrite
// all comments (comment delimiters and comment contents) with
// spaces).
void blankify_comments( std::string& text );

base::expect<doc, std::string> parse_file(
    std::string_view filename );

} // namespace rcl