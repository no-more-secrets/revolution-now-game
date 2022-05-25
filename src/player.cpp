/****************************************************************
**player.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2019-08-29.
*
* Description: Data structure representing a human or AI player.
*
*****************************************************************/
#include "player.hpp"

// Revolution Now
#include "game-state.hpp"
#include "gs-players.hpp"
#include "logger.hpp"
#include "lua.hpp"
#include "util.hpp"

// Rds
#include "gs-players.rds.hpp"

// luapp
#include "luapp/as.hpp"
#include "luapp/iter.hpp"
#include "luapp/state.hpp"

// refl
#include "refl/to-str.hpp"

// base
#include "base/to-str-tags.hpp"

using namespace std;

namespace rn {

/****************************************************************
** Player
*****************************************************************/
int Player::add_money( int amount ) {
  o_.money += amount;
  return o_.money;
}

/****************************************************************
** Public API
*****************************************************************/
Player& player_for_nation( e_nation nation ) {
  auto& players = GameState::players().players;
  CHECK( players.contains( nation ) );
  return players[nation];
}

Player& player_for_nation( PlayersState& players_state,
                           e_nation      nation ) {
  auto it = players_state.players.find( nation );
  CHECK( it != players_state.players.end(),
         "player for nation {} does not exist.", nation );
  return it->second;
}

Player const& player_for_nation(
    PlayersState const& players_state, e_nation nation ) {
  auto it = players_state.players.find( nation );
  CHECK( it != players_state.players.end(),
         "player for nation {} does not exist.", nation );
  return it->second;
}

void set_players( PlayersState&           players_state,
                  vector<e_nation> const& nations ) {
  auto& players = players_state.players;
  players.clear();
  for( auto nation : nations ) {
    players.emplace( nation, Player( wrapped::Player{
                                 .nation = nation,
                                 .human  = true,
                                 .money  = 0,
                             } ) );
  }
}

void linker_dont_discard_module_player() {}

/****************************************************************
** Lua Bindings
*****************************************************************/
namespace {

LUA_FN( set_players, void, lua::table nations ) {
  auto&            players_state = GameState::players();
  vector<e_nation> vec;
  for( auto p : nations )
    vec.push_back( lua::as<e_nation>( p.second ) );
  lg.info( "enabling nations: {}",
           base::FmtJsonStyleList{ vec } );
  set_players( players_state, vec );
}

} // namespace

} // namespace rn
