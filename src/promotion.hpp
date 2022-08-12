/****************************************************************
**promotion.hpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2022-08-10.
*
* Description: All things related to unit type promotion
*              & demotion.
*
*****************************************************************/
#pragma once

#include "core-config.hpp"

// Rds
#include "promotion.rds.hpp"

// ss
#include "ss/unit-composer.hpp"
#include "ss/unit-type.hpp"

// C++ standard library
#include <vector>

namespace rn {

struct Colony;
struct SSConst;
struct TS;
struct Unit;

// This is the function that top-level game logic should call
// whenever it is determined that a unit is up for promotion (the
// it is ok if the unit cannot be promoted further). This will
// find the unit's current activity and promote it along those
// lines if a promotion target exists. It will return true if a
// promotion was possible (and was thus made). If the unit has no
// activity the no promotion will be made. Technically, in that
// case, we could still theoretically promote e.g. an indentured
// servant to a free colonist, but the game will never do that
// (outside of cheat mode) when the unit has no activity, so we
// don't do that. Also, note that if the unit is an expert at
// something other than the given activity then no promotion will
// happen.
//
// TODO: promote veterans to continentals after independence.
//
bool try_promote_unit_for_current_activity( SSConst const& ss,
                                            Unit& unit );

// This function will promote a unit given an activity. It will
// first promote the unit type given the activity, then preserve
// the inventory. If the promotion is possible then either the
// base type or derived type (or both) may change. The `activity`
// parameter may or may not be used depending on the unit type.
// Note that if the unit type is already an expert at something
// other than the activity then this will not promote them, since
// that does not happen in the game normally (there may be some
// cheat/debug features that allow doing that, but that logic is
// kept separate from this). On the other hand, if the unit is
// already an expert at the given activity, then no promotion
// will happen and an error will be returned.
//
// This may be a bit expensive to call; it should not be called
// on every frame or on every unit in a given turn. It should
// only be called when we know that we want to try to promote a
// unit, which shouldn't happen so often. It's ok to call it on
// the order of once per battle, although that probably won't
// happen since the probability of promotion in a battle is low.
expect<UnitComposition> promoted_from_activity(
    UnitComposition const& comp, e_unit_activity activity );

// Will attempt to clear the expertise (if any) of the base type
// while holding any modifiers constant. Though if the derived
// type specifies a cleared_expertise target then that will be
// respected without regard for the base type: that target will
// be created with its default base type and returned.
maybe<UnitType> cleared_expertise( UnitType ut );

// This will return nothing if the unit does not have an
// on_death.demoted property, otherwise it will return the new
// UnitType representing the demoted unit, which is guaranteed by
// the game rules (and validation performed during deserializa-
// tion of the unit descriptor configs) to exist regardless of
// base type.
maybe<UnitType> on_death_demoted_type( UnitType ut );

// For units that get demoted upon capture (e.g.
// veteran_colonist) this will return that demoted type.
maybe<e_unit_type> on_capture_demoted_type( UnitType ut );

// This function should be called once on each colony each turn
// to decide which of its workers should be promoted be promoted
// for on-the-job training. This happens rarely; in the original
// game, a free colonist only has a 1/100 chance while a petty
// criminal has a 1/300. But, any colonist that gets promoted
// will go straight to being an expert. Note that this function
// won't actually make the promotions, it will just report on
// what they should be. Also, its results are non-deterministic
// since it selects the colonists randomly. Each colonist is con-
// sidered independently of the others, so in principle, multiple
// colonists in a colony could get promoted in a single turn; but
// in practice, it is safe to say that the vast majorityof time
// the result of this function will be empty, and very occasion-
// ally it will be one, and even more rarely will it be > 1.
std::vector<OnTheJobPromotionResult>
workers_to_promote_for_on_the_job_training(
    SSConst const& ss, TS& ts, Colony const& colony );

} // namespace rn
