/****************************************************************
* Main config file
*****************************************************************/
#ifndef RN_INL
#define RN_INL

#include "src/nation.hpp"

namespace rn {

CFG( rn,
  FLD( e_nation, player_nation )
  FLD( int, target_frame_rate )
  FLD( int, depixelate_pixels_per_frame )
  OBJ( main_window,
    FLD( Str, title )
  )
  OBJ( viewport,
    FLD( double, pan_speed )
    FLD( double, zoom_min )
    FLD( double, zoom_speed )
    FLD( double, zoom_accel_coeff )
    FLD( double, zoom_accel_drag_coeff )
    FLD( double, pan_accel_init_coeff )
    FLD( double, pan_accel_drag_init_coeff )
  )
  OBJ( controls,
    FLD( int, drag_buffer )
  )
)

}

#endif
