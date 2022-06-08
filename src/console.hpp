/****************************************************************
**console.hpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2019-02-03.
*
* Description: The developer/mod console.
*
*****************************************************************/
#pragma once

#include "core-config.hpp"

// C++ standard library
#include <memory>

namespace rn {

struct Planes;
struct MenuPlane;

struct ConsolePlane {
  ConsolePlane( Planes& planes, MenuPlane& menu_plane );
  ~ConsolePlane() noexcept;

 private:
  Planes& planes_;

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace rn
