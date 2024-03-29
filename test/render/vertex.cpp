/****************************************************************
**vertex.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2022-03-07.
*
* Description: Unit tests for the src/render/vertex.* module.
*
*****************************************************************/
#include "test/testing.hpp"

// Under test.
#include "src/render/vertex.hpp"

// refl
#include "refl/to-str.hpp"

// Must be last.
#include "test/catch-common.hpp"

namespace rr {
namespace {

using namespace std;

using ::base::nothing;
using ::gfx::pixel;
using ::gfx::point;
using ::gfx::rect;
using ::gfx::size;

TEST_CASE( "[render/vertex] SpriteVertex" ) {
  SpriteVertex         vert( point{ .x = 1, .y = 2 },
                             point{ .x = 3, .y = 4 },
                             rect{ .origin = point{ .x = 5, .y = 6 },
                                   .size = { .w = 1, .h = 2 } } );
  GenericVertex const& gv = vert.generic();
  REQUIRE( gv.type == 0 );
  REQUIRE( gv.visible == 1 );
  REQUIRE( gv.depixelate == gl::vec4{} );
  REQUIRE( gv.depixelate_stages == gl::vec4{} );
  REQUIRE( gv.position == gl::vec2{ .x = 1, .y = 2 } );
  REQUIRE( gv.atlas_position == gl::vec2{ .x = 3, .y = 4 } );
  REQUIRE( gv.atlas_rect ==
           gl::vec4{ .x = 5, .y = 6, .z = 1, .w = 2 } );
  REQUIRE( gv.atlas_target_offset == gl::vec2{} );
  REQUIRE( gv.fixed_color == gl::color{} );
  REQUIRE( gv.alpha_multiplier == 1.0f );
}

TEST_CASE( "[render/vertex] SolidVertex" ) {
  SolidVertex vert(
      point{ .x = 1, .y = 2 },
      pixel{ .r = 10, .g = 20, .b = 30, .a = 40 } );
  GenericVertex const& gv = vert.generic();
  REQUIRE( gv.type == 1 );
  REQUIRE( gv.visible == 1 );
  REQUIRE( gv.depixelate == gl::vec4{} );
  REQUIRE( gv.depixelate_stages == gl::vec4{} );
  REQUIRE( gv.position == gl::vec2{ .x = 1, .y = 2 } );
  REQUIRE( gv.atlas_position == gl::vec2{} );
  REQUIRE( gv.atlas_rect == gl::vec4{} );
  REQUIRE( gv.atlas_target_offset == gl::vec2{} );
  REQUIRE( gv.fixed_color == gl::color{ .r = 10.0f / 255.0f,
                                        .g = 20.0f / 255.0f,
                                        .b = 30.0f / 255.0f,
                                        .a = 40.0f / 255.0f } );
  REQUIRE( gv.alpha_multiplier == 1.0f );
}

TEST_CASE( "[render/vertex] SilhouetteVertex" ) {
  SilhouetteVertex vert(
      point{ .x = 1, .y = 2 }, point{ .x = 3, .y = 4 },
      rect{ .origin = point{ .x = 5, .y = 6 },
            .size   = { .w = 1, .h = 2 } },
      pixel{ .r = 10, .g = 20, .b = 30, .a = 40 } );
  GenericVertex const& gv = vert.generic();
  REQUIRE( gv.type == 2 );
  REQUIRE( gv.visible == 1 );
  REQUIRE( gv.depixelate == gl::vec4{} );
  REQUIRE( gv.depixelate_stages == gl::vec4{} );
  REQUIRE( gv.position == gl::vec2{ .x = 1, .y = 2 } );
  REQUIRE( gv.atlas_position == gl::vec2{ .x = 3, .y = 4 } );
  REQUIRE( gv.atlas_rect ==
           gl::vec4{ .x = 5, .y = 6, .z = 1, .w = 2 } );
  REQUIRE( gv.atlas_target_offset == gl::vec2{} );
  REQUIRE( gv.fixed_color == gl::color{ .r = 10.0f / 255.0f,
                                        .g = 20.0f / 255.0f,
                                        .b = 30.0f / 255.0f,
                                        .a = 40.0f / 255.0f } );
  REQUIRE( gv.alpha_multiplier == 1.0f );
}

TEST_CASE( "[render/vertex] StencilVertex" ) {
  StencilVertex vert(
      point{ .x = 1, .y = 2 }, point{ .x = 3, .y = 4 },
      rect{ .origin = point{ .x = 5, .y = 6 },
            .size   = { .w = 1, .h = 2 } },
      size{ .w = 2, .h = 3 },
      pixel{ .r = 10, .g = 20, .b = 30, .a = 40 } );
  GenericVertex const& gv = vert.generic();
  REQUIRE( gv.type == 3 );
  REQUIRE( gv.visible == 1 );
  REQUIRE( gv.depixelate == gl::vec4{} );
  REQUIRE( gv.depixelate_stages == gl::vec4{} );
  REQUIRE( gv.position == gl::vec2{ .x = 1, .y = 2 } );
  REQUIRE( gv.atlas_position == gl::vec2{ .x = 3, .y = 4 } );
  REQUIRE( gv.atlas_rect ==
           gl::vec4{ .x = 5, .y = 6, .z = 1, .w = 2 } );
  REQUIRE( gv.atlas_target_offset ==
           gl::vec2{ .x = 2, .y = 3 } );
  REQUIRE( gv.fixed_color == gl::color{ .r = 10.0f / 255.0f,
                                        .g = 20.0f / 255.0f,
                                        .b = 30.0f / 255.0f,
                                        .a = 40.0f / 255.0f } );
  REQUIRE( gv.alpha_multiplier == 1.0f );
}

TEST_CASE( "[render/vertex] depixelation" ) {
  SpriteVertex vert( point{ .x = 1, .y = 2 },
                     point{ .x = 3, .y = 4 },
                     rect{ .origin = point{ .x = 5, .y = 6 },
                           .size   = { .w = 1, .h = 2 } } );
  REQUIRE( vert.depixelation_stage() == 0.0 );
  REQUIRE( vert.depixelation_hash_anchor() == gl::vec2{} );
  REQUIRE( vert.depixelation_gradient() == gl::vec2{} );
  REQUIRE( vert.depixelation_stage_anchor() == gl::vec2{} );
  REQUIRE( vert.generic().atlas_target_offset == gl::vec2{} );
  REQUIRE( vert.generic().depixelate == gl::vec4{} );
  REQUIRE( vert.generic().depixelate_stages == gl::vec4{} );

  vert.set_depixelation_stage( .5 );
  vert.set_depixelation_hash_anchor( { .x = 1, .y = 2 } );
  vert.set_depixelation_gradient( { .w = 4.4, .h = 5.5 } );
  vert.set_depixelation_stage_anchor( { .x = 3.3, .y = 4 } );
  vert.set_depixelation_inversion( false );
  REQUIRE( vert.depixelation_stage() == 0.5 );
  REQUIRE( vert.depixelation_hash_anchor() ==
           gl::vec2{ .x = 1, .y = 2 } );
  REQUIRE( vert.depixelation_gradient() ==
           gl::vec2{ .x = 4.4, .y = 5.5 } );
  REQUIRE( vert.generic().atlas_target_offset == gl::vec2{} );
  REQUIRE( vert.generic().depixelate ==
           gl::vec4{ .x = 1, .y = 2, .z = .5, .w = 0 } );
  REQUIRE( vert.generic().depixelate_stages ==
           gl::vec4{ .x = 3.3, .y = 4, .z = 4.4, .w = 5.5 } );

  vert.set_depixelation_stage( 1.0 );
  vert.set_depixelation_gradient( {} );
  vert.set_depixelation_stage_anchor( {} );
  vert.set_depixelation_inversion( true );
  REQUIRE( vert.depixelation_stage() == 1.0 );
  REQUIRE( vert.depixelation_hash_anchor() ==
           gl::vec2{ .x = 1, .y = 2 } );
  REQUIRE( vert.depixelation_gradient() == gl::vec2{} );
  REQUIRE( vert.depixelation_stage_anchor() == gl::vec2{} );
  REQUIRE( vert.generic().atlas_target_offset == gl::vec2{} );
  REQUIRE( vert.generic().depixelate ==
           gl::vec4{ .x = 1, .y = 2, .z = 1.0, .w = 1 } );
  REQUIRE( vert.generic().depixelate_stages == gl::vec4{} );
}

TEST_CASE( "[render/vertex] visibility" ) {
  SpriteVertex vert( point{ .x = 1, .y = 2 },
                     point{ .x = 3, .y = 4 },
                     rect{ .origin = point{ .x = 5, .y = 6 },
                           .size   = { .w = 1, .h = 2 } } );
  REQUIRE( vert.is_visible() );
  vert.set_visible( true );
  REQUIRE( vert.is_visible() );
  vert.set_visible( false );
  REQUIRE_FALSE( vert.is_visible() );
  vert.set_visible( true );
  REQUIRE( vert.is_visible() );
}

TEST_CASE( "[render/vertex] alpha" ) {
  SpriteVertex vert( point{ .x = 1, .y = 2 },
                     point{ .x = 3, .y = 4 },
                     rect{ .origin = point{ .x = 5, .y = 6 },
                           .size   = { .w = 1, .h = 2 } } );
  vert.reset_alpha();
  REQUIRE( vert.alpha() == 1.0 );
  vert.set_alpha( .5 );
  REQUIRE( vert.alpha() == 0.5 );
  vert.set_alpha( 0.0 );
  REQUIRE( vert.alpha() == 0.0 );
  vert.reset_alpha();
  REQUIRE( vert.alpha() == 1.0 );
}

TEST_CASE( "[render/vertex] scaling" ) {
  SpriteVertex vert( point{ .x = 6, .y = 12 },
                     point{ .x = 3, .y = 4 },
                     rect{ .origin = point{ .x = 5, .y = 6 },
                           .size   = { .w = 1, .h = 2 } } );
  REQUIRE( vert.generic().scaling == 1.0 );
  vert.set_scaling( .5 );
  REQUIRE( vert.generic().scaling == .5f );
  vert.set_scaling( .55 );
  REQUIRE( vert.generic().scaling == .55f );
}

TEST_CASE( "[render/vertex] translation" ) {
  SpriteVertex vert( point{ .x = 6, .y = 12 },
                     point{ .x = 3, .y = 4 },
                     rect{ .origin = point{ .x = 5, .y = 6 },
                           .size   = { .w = 1, .h = 2 } } );
  REQUIRE( vert.generic().translation ==
           gl::vec2{ .x = 0, .y = 0 } );
  vert.set_translation( gfx::dsize{ .w = 2, .h = -4 } );
  REQUIRE( vert.generic().translation ==
           gl::vec2{ .x = 2, .y = -4 } );
}

TEST_CASE( "[render/vertex] color_cycle" ) {
  SpriteVertex vert( point{ .x = 6, .y = 12 },
                     point{ .x = 3, .y = 4 },
                     rect{ .origin = point{ .x = 5, .y = 6 },
                           .size   = { .w = 1, .h = 2 } } );
  REQUIRE( vert.generic().color_cycle == 0 );
  vert.set_color_cycle( true );
  REQUIRE( vert.generic().color_cycle == 1 );
  vert.set_color_cycle( false );
  REQUIRE( vert.generic().color_cycle == 0 );
}

TEST_CASE( "[render/vertex] use_camera" ) {
  SpriteVertex vert( point{ .x = 6, .y = 12 },
                     point{ .x = 3, .y = 4 },
                     rect{ .origin = point{ .x = 5, .y = 6 },
                           .size   = { .w = 1, .h = 2 } } );
  REQUIRE( vert.generic().use_camera == 0 );
  vert.set_use_camera( true );
  REQUIRE( vert.generic().use_camera == 1 );
  vert.set_use_camera( false );
  REQUIRE( vert.generic().use_camera == 0 );
}

} // namespace
} // namespace rr
