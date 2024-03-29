/****************************************************************
**natives.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2022-10-30.
*
* Description: Top-level save-game state for native tribes and
*              dwellings.
*
*****************************************************************/
#include "natives.hpp"

// ss
#include "ss/dwelling.hpp"
#include "ss/tribe.hpp"
#include "ss/units.hpp"

// luapp
#include "luapp/enum.hpp"
#include "luapp/ext-base.hpp"
#include "luapp/register.hpp"

// refl
#include "refl/to-str.hpp"

// base
#include "base/keyval.hpp"
#include "base/to-str-ext-std.hpp"

using namespace std;

namespace rn {

namespace {

constexpr int kFirstDwellingId = 1;

using ::base::maybe;
using ::base::nothing;

} // namespace

/****************************************************************
** wrapped::NativesState
*****************************************************************/
base::valid_or<string> wrapped::NativesState::validate() const {
  unordered_set<Coord> used_coords;

  DwellingId max_id{ -1 };

  for( auto const& [id, dwelling] : dwellings ) {
    max_id = std::max( max_id, id );

    // Each dwelling has a unique location.
    Coord where = dwelling.location;
    REFL_VALIDATE( !used_coords.contains( where ),
                   "multiples dwellings on tile {}.", where );
    used_coords.insert( where );
  }

  // next dwelling ID is larger than any used dwelling ID.
  REFL_VALIDATE( DwellingId{ next_dwelling_id } > max_id,
                 "next_dwelling_id ({}) must be larger than any "
                 "dwelling ID in use (max found is {}).",
                 next_dwelling_id, max_id );

  return base::valid;
}

/****************************************************************
** NativesState
*****************************************************************/
base::valid_or<std::string> NativesState::validate() const {
  HAS_VALUE_OR_RET( o_.validate() );

  // Dwelling location matches coord.
  for( auto const& [dwelling_id, dwelling] : o_.dwellings ) {
    Coord const&            coord = dwelling.location;
    base::maybe<DwellingId> actual_dwelling_id =
        base::lookup( dwelling_from_coord_, coord );
    REFL_VALIDATE( actual_dwelling_id == dwelling_id,
                   "Inconsistent dwelling map coordinate ({}) "
                   "for dwelling {}.",
                   coord, dwelling_id );
  }

  return base::valid;
}

void NativesState::validate_or_die() const {
  CHECK_HAS_VALUE( validate() );
}

NativesState::NativesState( wrapped::NativesState&& o )
  : o_( std::move( o ) ) {
  // Populate dwelling_from_coord_.
  for( auto const& [id, dwelling] : o_.dwellings )
    dwelling_from_coord_[dwelling.location] = id;
}

NativesState::NativesState()
  : NativesState( wrapped::NativesState{
        .next_dwelling_id = kFirstDwellingId,
        .dwellings        = {} } ) {
  validate_or_die();
}

bool NativesState::tribe_exists( e_tribe tribe ) const {
  return o_.tribes[tribe].has_value();
}

Tribe& NativesState::tribe_for( e_tribe tribe ) {
  UNWRAP_CHECK_MSG( res, o_.tribes[tribe],
                    "the {} tribe does not exist in this game.",
                    tribe );
  return res;
}

Tribe const& NativesState::tribe_for( e_tribe tribe ) const {
  UNWRAP_CHECK_MSG( res, o_.tribes[tribe],
                    "the {} tribe does not exist in this game.",
                    tribe );
  return res;
}

Tribe& NativesState::create_or_add_tribe( e_tribe tribe ) {
  if( o_.tribes[tribe].has_value() ) return *o_.tribes[tribe];
  Tribe& obj = o_.tribes[tribe].emplace();
  obj.type   = tribe;
  return obj;
}

unordered_map<DwellingId, Dwelling> const&
NativesState::dwellings_all() const {
  return o_.dwellings;
}

Dwelling const& NativesState::dwelling_for(
    DwellingId id ) const {
  UNWRAP_CHECK_MSG( col, base::lookup( o_.dwellings, id ),
                    "dwelling {} does not exist.", id );
  return col;
}

Dwelling& NativesState::dwelling_for( DwellingId id ) {
  UNWRAP_CHECK_MSG( col, base::lookup( o_.dwellings, id ),
                    "dwelling {} does not exist.", id );
  return col;
}

Coord NativesState::coord_for( DwellingId id ) const {
  return dwelling_for( id ).location;
}

vector<DwellingId> NativesState::dwellings_for_tribe(
    e_tribe tribe ) const {
  vector<DwellingId> res;
  res.reserve( o_.dwellings.size() );
  for( auto const& [id, dwelling] : o_.dwellings )
    if( dwelling.tribe == tribe ) res.push_back( id );
  return res;
}

DwellingId NativesState::add_dwelling( Dwelling&& dwelling ) {
  CHECK( dwelling.id == DwellingId{ 0 },
         "dwelling ID must be zero when creating dwelling." );
  DwellingId id = next_dwelling_id();
  dwelling.id   = id;
  CHECK( !dwelling_from_coord_.contains( dwelling.location ) );
  dwelling_from_coord_[dwelling.location] = id;
  // Must be last to avoid use-after-move.
  CHECK( !o_.dwellings.contains( id ) );
  o_.dwellings[id] = std::move( dwelling );
  return id;
}

void NativesState::destroy_dwelling( DwellingId id ) {
  Dwelling& dwelling = dwelling_for( id );
  CHECK( dwelling_from_coord_.contains( dwelling.location ) );
  dwelling_from_coord_.erase( dwelling.location );
  // Should be last so above reference doesn't dangle.
  o_.dwellings.erase( id );
}

DwellingId NativesState::next_dwelling_id() {
  return DwellingId{ o_.next_dwelling_id++ };
}

DwellingId NativesState::last_dwelling_id() const {
  CHECK( o_.next_dwelling_id > 0, "no dwellings yet created." );
  return DwellingId{ o_.next_dwelling_id - 1 };
}

base::maybe<DwellingId> NativesState::maybe_dwelling_from_coord(
    Coord const& coord ) const {
  return base::lookup( dwelling_from_coord_, coord );
}

DwellingId NativesState::dwelling_from_coord(
    Coord const& coord ) const {
  UNWRAP_CHECK( id, maybe_dwelling_from_coord( coord ) );
  return id;
}

bool NativesState::dwelling_exists( DwellingId id ) const {
  return o_.dwellings.contains( id );
}

unordered_map<Coord, DwellingId>&
NativesState::owned_land_without_minuit() {
  return o_.owned_land_without_minuit;
}

unordered_map<Coord, DwellingId> const&
NativesState::owned_land_without_minuit() const {
  return o_.owned_land_without_minuit;
}

void NativesState::mark_land_owned( DwellingId dwelling_id,
                                    Coord      where ) {
  o_.owned_land_without_minuit[where] = dwelling_id;
}

void NativesState::mark_land_unowned( Coord where ) {
  auto it = o_.owned_land_without_minuit.find( where );
  if( it == o_.owned_land_without_minuit.end() ) return;
  o_.owned_land_without_minuit.erase( it );
}

/****************************************************************
** Lua Bindings
*****************************************************************/
namespace {

// NativesState
LUA_STARTUP( lua::state& st ) {
  using U = ::rn::NativesState;
  auto u  = st.usertype.create<U>();

  u["last_dwelling_id"] = &U::last_dwelling_id;
  u["dwelling_exists"]  = &U::dwelling_exists;
  u["dwelling_for_id"]  = [&]( U&         o,
                              DwellingId id ) -> Dwelling& {
    LUA_CHECK( st, o.dwelling_exists( id ),
                "dwelling {} does not exist.", id );
    return o.dwelling_for( id );
  };
  u["has_dwelling_on_square"] =
      []( U& o, Coord where ) -> maybe<Dwelling&> {
    maybe<DwellingId> const dwelling_id =
        o.maybe_dwelling_from_coord( where );
    if( !dwelling_id.has_value() ) return nothing;
    return o.dwelling_for( *dwelling_id );
  };
  // FIXME: temporary; need to move this somewhere else which can
  // create the dwelling properly with all of the fields initial-
  // ized to the correct values.
  u["new_dwelling"] = [&]( U& o, Coord where ) -> Dwelling& {
    Dwelling dwelling;
    dwelling.location = where;
    DwellingId const id =
        o.add_dwelling( std::move( dwelling ) );
    return o.dwelling_for( id );
  };
  u["create_or_add_tribe"] = &U::create_or_add_tribe;
  u["mark_land_owned"]     = &U::mark_land_owned;
};

} // namespace

} // namespace rn
