/****************************************************************
**mv-points.hpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2018-09-02.
*
* Description: A type for representing movement points that will
*              ensure correct handling of fractional points.
*
*****************************************************************/
#pragma once

#include "core-config.hpp"

// Revolution Now
#include "fb.hpp"
#include "fmt-helper.hpp"

// Flatbuffers
#include "fb/mv-points_generated.h"

namespace rn {

// Cannot convert to int.
class ND MovementPoints {
public:
  MovementPoints() = default;
  explicit MovementPoints( int p )
    : points_atoms( p * factor ) {}

  MovementPoints& operator=( int p ) {
    points_atoms = p * factor;
    return *this;
  }

  MovementPoints( MovementPoints const& other ) = default;
  MovementPoints( MovementPoints&& other )      = default;

  MovementPoints& operator=( MovementPoints const& other ) =
      default;
  MovementPoints& operator=( MovementPoints&& other ) = default;

  static MovementPoints _1_3() {
    return MovementPoints( 0, 1 );
  };

  static MovementPoints _2_3() {
    return MovementPoints( 0, 2 );
  };

  bool operator==( MovementPoints const& rhs ) const {
    return points_atoms == rhs.points_atoms;
  }
  bool operator==( int rhs ) const {
    return points_atoms == rhs * factor;
  }

  bool operator!=( MovementPoints const& rhs ) const {
    return points_atoms != rhs.points_atoms;
  }
  bool operator!=( int rhs ) const {
    return points_atoms != rhs * factor;
  }

  bool operator>( MovementPoints const& rhs ) const {
    return points_atoms > rhs.points_atoms;
  }
  bool operator>( int rhs ) const {
    return points_atoms > rhs * factor;
  }

  bool operator<( MovementPoints const& rhs ) const {
    return points_atoms < rhs.points_atoms;
  }
  bool operator<( int rhs ) const {
    return points_atoms < rhs * factor;
  }

  bool operator>=( MovementPoints const& rhs ) const {
    return points_atoms >= rhs.points_atoms;
  }
  bool operator>=( int rhs ) const {
    return points_atoms >= rhs * factor;
  }

  bool operator<=( MovementPoints const& rhs ) const {
    return points_atoms <= rhs.points_atoms;
  }
  bool operator<=( int rhs ) const {
    return points_atoms <= rhs * factor;
  }

  MovementPoints operator+( MovementPoints const& rhs ) const {
    return MovementPoints( 0, points_atoms + rhs.points_atoms );
  }
  MovementPoints operator+( int rhs ) const {
    return MovementPoints( 0, points_atoms + ( rhs * factor ) );
  }

  MovementPoints operator-( MovementPoints const& rhs ) const {
    return MovementPoints( 0, points_atoms - rhs.points_atoms );
  }
  MovementPoints operator-( int rhs ) const {
    return MovementPoints( 0, points_atoms - ( rhs * factor ) );
  }

  void operator+=( MovementPoints const& rhs ) {
    points_atoms += rhs.points_atoms;
  }
  void operator+=( int rhs ) { points_atoms += rhs * factor; }

  void operator-=( MovementPoints const& rhs ) {
    points_atoms -= rhs.points_atoms;
  }
  void operator-=( int rhs ) { points_atoms -= rhs * factor; }

  std::string to_string() const;

  valid_deserial_t check_invariants_safe() const;

private:
  // atoms can be > 2
  MovementPoints( int integral, int atoms );
  static constexpr int factor = 3;

  // clang-format off
  SERIALIZABLE_STRUCT_MEMBERS( MovementPoints,
  // 2 points would be represented by 2*factor.
  ( int, points_atoms ));
  // clang-format on
};
NOTHROW_MOVE( MovementPoints );

using MvPoints = MovementPoints;

} // namespace rn

DEFINE_FORMAT( rn::MvPoints, "{}", o.to_string() )
