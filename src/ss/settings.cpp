/****************************************************************
**gs-settings.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2022-05-28.
*
* Description: Save-game state for game-wide settings.
*
*****************************************************************/
#include "ss/settings.hpp"

// luapp
#include "luapp/enum.hpp"
#include "luapp/ext-base.hpp"
#include "luapp/register.hpp"
#include "luapp/state.hpp"

// refl
#include "refl/ext.hpp"
#include "refl/to-str.hpp"

using namespace std;

namespace rn {

base::valid_or<string> SettingsState::validate() const {
  return base::valid;
}

/****************************************************************
** Lua Bindings
*****************************************************************/
namespace {

// MarketItem
LUA_STARTUP( lua::state& st ) {
  using U = ::rn::SettingsState;
  auto u  = st.usertype.create<U>();

  u["difficulty"]       = &U::difficulty;
  u["fast_piece_slide"] = &U::fast_piece_slide;
};

} // namespace

} // namespace rn
