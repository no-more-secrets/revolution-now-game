/****************************************************************
**ttf.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2019-07-27.
*
* Description: Render TTF fonts.
*
*****************************************************************/
#include "ttf.hpp"

// Revolution Now
#include "config-files.hpp"
#include "error.hpp"
#include "gfx.hpp"
#include "init.hpp"
#include "macros.hpp"
#include "screen.hpp"
#include "sdl-util.hpp"

// Revolution Now (config)
#include "../config/rcl/font.inl"
#include "../config/rcl/palette.inl"

// refl
#include "refl/query-enum.hpp"
#include "refl/to-str.hpp"

// base
#include "base/keyval.hpp"

// SDL
#include "SDL_ttf.h"

using namespace std;

namespace rn {

namespace {

unordered_map<e_font, FontTTFInfo> g_font_ttf_info;

struct FontDesc {
  fs::path    file_name;
  int         pt_size;
  Y           vert_offset;
  ::TTF_Font* ttf_font;
};
NOTHROW_MOVE( FontDesc );

unordered_map<e_font, FontDesc>& loaded_fonts() {
  static unordered_map<e_font, FontDesc> m = [] {
    unordered_map<e_font, FontDesc> res;
    for( auto font : refl::enum_values<e_font> ) {
      UNWRAP_CHECK( path,
                    base::lookup( config_font.paths, font ) );
      UNWRAP_CHECK( size,
                    base::lookup( config_font.sizes, font ) );
      UNWRAP_CHECK( vert_offset,
                    base::lookup( config_font.offsets, font ) );
      res[font] = FontDesc{ path, size, vert_offset, nullptr };
    }
    return res;
  }();
  return m;
}

Surface render_line_standard_surface( ::TTF_Font* font,
                                      ::SDL_Color fg,
                                      string_view line ) {
  auto max_size = max_texture_size();
  while( line.size() >= 1 ) {
    auto* sdl_surface = ::TTF_RenderText_Solid(
        font, string( line ).c_str(), fg );
    CHECK( sdl_surface != nullptr );
    auto surface = Surface( sdl_surface );
    if( surface.size().w <= max_size.w &&
        surface.size().h <= max_size.h )
      return surface;
    line.remove_suffix( line.size() / 2 );
  }
  FATAL( "failed to render string within max texture size: {}",
         max_size );
}

Texture render_line_standard_impl( ::TTF_Font* font,
                                   ::SDL_Color fg,
                                   string_view line,
                                   Y           vert_offset ) {
  // SDL can't render empty lines for some reason.
  if( line.empty() ) {
    return Texture::create(
        Delta{ W{ 1 }, // maybe safer than zero?
               H{ TTF_FontHeight( font ) } } );
  }
  auto surface = render_line_standard_surface( font, fg, line );
  auto texture = Texture::from_surface( surface );
  if( vert_offset != 0_y ) {
    auto new_texture = create_texture( texture.size() );
    clear_texture_transparent( new_texture );
    copy_texture( texture, new_texture, { 0_x, vert_offset } );
    texture = std::move( new_texture );
  }
  // Not sure why this doesn't happen automatically.
  texture.set_alpha_mod( fg.a );
  return texture;
}

void init_ttf() {
  CHECK( !TTF_Init() );
  ::SDL_version        compiled_version;
  ::SDL_version const* link_version = ::TTF_Linked_Version();
  SDL_TTF_VERSION( &compiled_version );
  check_SDL_compile_link_version( "TTF", *link_version,
                                  compiled_version );

  for( auto& font : loaded_fonts() ) {
    auto& font_desc = font.second;
    int   pt_size   = font_desc.pt_size;
    auto  font_file = font_desc.file_name.string();
    auto ttf_font = ::TTF_OpenFont( font_file.c_str(), pt_size );
    CHECK( ttf_font );
    // Check style first before setting this.
    ::TTF_SetFontStyle( ttf_font, TTF_STYLE_NORMAL );
    int outline = 0;
    ::TTF_SetFontOutline( ttf_font, outline );
    font_desc.ttf_font = ttf_font;
  }
}

// Texture render_text_line_shadow( e_font font, gfx::pixel fg,
//                                 string_view line ) {
//  gfx::pixel bg        = fg.shaded( 6 );
//  bg.a            = 80;
//  auto texture_fg = ttf_render_text_line_uncached( font, fg,
//  line ); auto texture_bg = ttf_render_text_line_uncached(
//  font, bg, line ); auto delta      = texture_delta( texture_fg
//  ); auto result_texture =
//      create_texture( delta.w + 1, delta.h + 1 );
//  clear_texture_transparent( result_texture );
//  copy_texture( texture_bg, result_texture, {1_y, 1_x} );
//  copy_texture( texture_fg, result_texture, {0_y, 0_x} );
//  return result_texture;
//}

void cleanup_ttf() {
  for( auto& font : loaded_fonts() ) {
    auto& font_desc = font.second;
    ::TTF_CloseFont( font_desc.ttf_font );
  }
  TTF_Quit();
}

REGISTER_INIT_ROUTINE( ttf );

} // namespace

/****************************************************************
** Public API
*****************************************************************/
Texture ttf_render_text_line_uncached( e_font      font,
                                       gfx::pixel  fg,
                                       string_view line ) {
  auto* ttf_font    = loaded_fonts()[font].ttf_font;
  auto  vert_offset = loaded_fonts()[font].vert_offset;
  return render_line_standard_impl( ttf_font, to_SDL( fg ), line,
                                    vert_offset );
}

FontTTFInfo const& ttf_get_font_info( e_font font ) {
  if( !g_font_ttf_info.contains( font ) ) {
    auto* ttf_font        = loaded_fonts()[font].ttf_font;
    auto  height          = ::TTF_FontHeight( ttf_font );
    g_font_ttf_info[font] = FontTTFInfo{
        /*height=*/H{ height } //
    };
  }
  return g_font_ttf_info[font];
}

void font_size_spectrum( char const* msg,
                         char const* font_file ) {
  // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
  int  y     = 30;
  auto sizes = {
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
      3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
      // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
      14, 15, 16, 17, 18, 19, 20, 21, 22, 23 };
  for( auto ptsize : sizes ) {
    auto font = ::TTF_OpenFont( font_file, ptsize );
    CHECK( font );
    std::string num_msg = std::to_string( ptsize ) + ": " + msg;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
    ::SDL_Color fg{ 255, 255, 255, 255 };
    auto        texture =
        render_line_standard_impl( font, fg, num_msg, 0_y );
    copy_texture( texture, Texture::screen(), { Y( y ), 0_x } );
    y += ::TTF_FontLineSkip( font );
    TTF_CloseFont( font );
  }
}

void font_test() {
  auto font = e_font::_7_12_serif_16pt;

  // H skip( ::TTF_FontLineSkip( loaded_fonts()[font].ttf_font )
  // );

  string msg =
      "Ask not what your country can do for you, but instead "
      "ask what you can do for your country!";

  auto render_line = [font]( string const& text ) {
    gfx::pixel fg = config_palette.orange.sat1.lum11;
    return ttf_render_text_line_uncached( font, fg, text );
  };
  auto texture = render_line( msg );

  copy_texture( texture, Texture::screen(), { 100_y, 100_x } );
  // font_size_spectrum( msg, font_file );

  //::SDL_RenderPresent( g_renderer );

  // input::wait_for_q();
}

} // namespace rn
