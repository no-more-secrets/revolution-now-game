/****************************************************************
**generic.vert
*
* Project: Revolution Now
*
* Created by dsicilia on 2022-03-08.
*
* Description: Generic vertex shader for 2D rendering engine.
*
*****************************************************************/
#version 330 core

layout (location = 0) in int   in_type;
layout (location = 1) in int   in_visible;
layout (location = 2) in vec3  in_depixelate;
layout (location = 3) in vec2  in_position;
layout (location = 4) in vec2  in_atlas_position;
layout (location = 5) in vec2  in_atlas_target_offset;
layout (location = 6) in vec4  in_fixed_color;
layout (location = 7) in float in_alpha_multiplier;
layout (location = 8) in float in_scaling;
layout (location = 9) in vec2  in_translation;

flat out int   frag_type;
flat out vec3  frag_depixelate;
     out vec2  frag_position;
     out vec2  frag_atlas_position;
flat out vec2  frag_atlas_target_offset;
     out vec4  frag_fixed_color;
     out float frag_alpha_multiplier;
flat out float frag_scaling;

// Screen dimensions in the game's logical pixel units.
uniform vec2 u_screen_size;

// Any input that refers to screen (game) coordinates needs to be
// adjusted by this function.
vec2 translate_scale( in vec2 position ) {
  vec2 adjusted_position = position;
  adjusted_position *= in_scaling;
  adjusted_position += in_translation;
  return adjusted_position;
}

// Forward things to the fragment shader that need to be for-
// warded as they are.  But note any input that refers to the
// screen position of something must be scaled/translated.
void forwarding() {
  frag_type                = in_type;
  frag_depixelate.z        = in_depixelate.z;
  frag_depixelate.xy       = translate_scale( in_depixelate.xy );
  frag_position            = translate_scale( in_position );
  frag_atlas_position      = in_atlas_position;
  frag_atlas_target_offset = in_atlas_target_offset;
  frag_fixed_color         = in_fixed_color;
  frag_alpha_multiplier    = in_alpha_multiplier;
  frag_scaling             = in_scaling;
}

// Convert a coordinate in game coordinates (meaning that 0,0 is
// at the upper left and each subsequent integer corresponds to a
// logical pixel) to normalized device coordinates (-1, 1) with
// (-1,-1) at the bottom right and (0,0) at the center.
vec2 to_ndc( in vec2 game_pos ) {
  vec2 ndc_pos = game_pos.xy / u_screen_size;
  ndc_pos = ndc_pos*2.0 - vec2( 1.0 );
  ndc_pos.y = -ndc_pos.y;
  return ndc_pos;
}

// This will discard a vertex by simply moving it outside of clip
// space, and so this will only really work for cases when all
// vertices in a triangle get discarded in the same way.
void discard_vertex() { gl_Position = vec4( 2, 2, 2, 0 ); }

void main() {
  forwarding();

  if( in_visible == 0 ) {
    discard_vertex();
    return;
  }

  vec2 adjusted_position = translate_scale( in_position );

  gl_Position = vec4( to_ndc( adjusted_position ), 0.0, 1.0 );
}
