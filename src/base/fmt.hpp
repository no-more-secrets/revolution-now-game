/****************************************************************
**fmt.hpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2021-06-09.
*
* Description: Just includes fmt.  Probably don't need this.
*
*****************************************************************/
#pragma once

#include "config.hpp"

// {fmt}
// We should only include this file from this header so that we
// can control which warnings are supressed.
#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Weverything"
#endif
#include "fmt/format.h"
#ifdef __clang__
#  pragma clang diagnostic pop
#endif
