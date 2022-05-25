/****************************************************************
**orders-move.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2021-04-16.
*
* Description: Carries out orders wherein a unit is asked to move
*              onto an adjacent square.
*
*****************************************************************/
#include "orders-move.hpp"

// Revolution Now
#include "co-wait.hpp"
#include "colony-mgr.hpp"
#include "colony-view.hpp"
#include "conductor.hpp"
#include "cstate.hpp"
#include "fight.hpp"
#include "game-state.hpp"
#include "gs-events.hpp"
#include "gs-terrain.hpp"
#include "gs-units.hpp"
#include "igui.hpp"
#include "land-view.hpp"
#include "lcr.hpp"
#include "logger.hpp"
#include "map-square.hpp"
#include "mv-calc.hpp"
#include "on-map.hpp"
#include "player.hpp"
#include "ustate.hpp"
#include "utype.hpp"
#include "window.hpp"

// refl
#include "refl/to-str.hpp"

// base
#include "base/lambda.hpp"
#include "base/to-str-ext-std.hpp"

using namespace std;

namespace rn {

namespace {

/****************************************************************
**Unit Movement Behaviors / Capabilities
*****************************************************************/
#define TEMPLATE_BEHAVIOR                                      \
  template<e_surface target, e_unit_relationship relationship, \
           e_entity_category entity>

#define BEHAVIOR_VALUES( surface, relationship, entity ) \
  e_surface::surface, e_unit_relationship::relationship, \
      e_entity_category::entity

#define BEHAVIOR_NS( surface, relationship, entity ) \
  unit_behavior::surface::relationship::entity

#define BEHAVIOR( c, r, e, ... )                              \
  namespace BEHAVIOR_NS( c, r, e ) {                          \
    enum class e_vals { __VA_ARGS__ };                        \
  }                                                           \
  template<>                                                  \
  struct to_behaviors<BEHAVIOR_VALUES( c, r, e )> {           \
    using type = BEHAVIOR_NS( c, r, e )::e_vals;              \
  };                                                          \
  template<>                                                  \
  [[maybe_unused]] to_behaviors_t<BEHAVIOR_VALUES( c, r, e )> \
  behavior<BEHAVIOR_VALUES( c, r, e )>(                       \
      UnitTypeAttributes const& desc )

enum class e_unit_relationship { neutral, friendly, foreign };
enum class e_entity_category { empty, unit, colony, village };

TEMPLATE_BEHAVIOR
struct to_behaviors {
  // This must be void, it is used as a fallback in code that
  // wants to verify that a combination of parameters is NOT
  // implemented.
  using type = void;
};

TEMPLATE_BEHAVIOR
using to_behaviors_t =
    typename to_behaviors<target, relationship, entity>::type;

TEMPLATE_BEHAVIOR
to_behaviors_t<target, relationship, entity> behavior(
    UnitTypeAttributes const& desc );

/****************************************************************/
BEHAVIOR( land, foreign, unit, no_attack, attack, no_bombard,
          bombard );
BEHAVIOR( land, foreign, colony, never, attack, trade );
// BEHAVIOR( land, foreign, village, unused );
BEHAVIOR( land, neutral, empty, never, always, unload );
BEHAVIOR( land, friendly, unit, always, never, unload );
BEHAVIOR( land, friendly, colony, always );
BEHAVIOR( water, foreign, unit, no_attack, attack, no_bombard,
          bombard );
BEHAVIOR( water, neutral, empty, never, always, high_seas );
BEHAVIOR( water, friendly, unit, always, never, move_onto_ship,
          high_seas );
/****************************************************************/

// The macros below are for users of the above functions.

#define CALL_BEHAVIOR( surface_, relationship_, entity_ ) \
  behavior<e_surface::surface_,                           \
           e_unit_relationship::relationship_,            \
           e_entity_category::entity_>( unit.desc() )

// This is a bit of an abuse of the init-statement if the if
// statement. Here we are using it just to automate the calling
// of the behavior function, not to test the result of the
// behavior function.
#define IF_BEHAVIOR( surface_, relationship_, entity_ )     \
  if( surface == e_surface::surface_ &&                     \
      relationship == e_unit_relationship::relationship_ && \
      category == e_entity_category::entity_ )

// This one is used to assert that there is no specialization for
// the given combination of parameters. These can be used for all
// combinations of parameters that are not specialized so that,
// eventually, when they are specialized, the compiler can notify
// us of where we need to insert logic to handle that situation.
#define STATIC_ASSERT_NO_BEHAVIOR( surface, relationship, \
                                   entity )               \
  static_assert(                                          \
      is_same_v<void, decltype( CALL_BEHAVIOR(            \
                          surface, relationship, entity ) )> )

/****************************************************************
** Movement Points Calculator
*****************************************************************/
wait<maybe<MovementPoints>> check_movement_points(
    IGui& gui, Unit const& unit, MapSquare const& src_square,
    MapSquare const& dst_square ) {
  MovementPointsAnalysis analysis =
      expense_movement_points( unit, src_square, dst_square );
  if( analysis.allowed() ) {
    if( analysis.using_start_of_turn_exemption() )
      lg.debug( "move allowed by start-of-turn exemption." );
    if( analysis.using_overdraw_allowance() )
      lg.debug( "move allowed by overdraw allowance." );
    co_return analysis.points_to_subtract();
  }
  // FIXME: add a checkbox to this dialog that allows the user to
  // suppress it; in that case, an attempt to move onto a square
  // with not enough movement points will simply end that unit's
  // turn by forfeighting its movement points, and it appears
  // that will mirror the original game's behavior.
  co_await gui.message_box(
      "Unit requires @[H]{}@[] movement point(s) to enter this "
      "square, but only has @[H]{}@[]+{} = @[H]{}@[] available "
      "to use.",
      analysis.needs, analysis.has,
      MovementPointsAnalysis::kOverdrawAllowance,
      analysis.has +
          MovementPointsAnalysis::kOverdrawAllowance );
  co_return nothing;
}

/****************************************************************
** TravelHandler
*****************************************************************/
struct TravelHandler : public OrdersHandler {
  TravelHandler( UnitId unit_id_, e_direction d,
                 IMapUpdater&  map_updater,
                 TerrainState& terrain_state, IGui& gui )
    : unit_id( unit_id_ ),
      direction( d ),
      map_updater_( map_updater ),
      terrain_state_( terrain_state ),
      gui_( gui ) {}

  enum class e_travel_verdict {
    // Cancelled by user
    cancelled,
    // Non-allowed moves (errors)
    map_edge,
    land_forbidden,
    water_forbidden,
    board_ship_full,
    not_enough_movement_points,
    // Allowed moves
    map_to_map,
    board_ship,
    offboard_ship,
    ship_into_port,
    land_fall,
    sail_high_seas,
  };

  wait<bool> confirm() override {
    verdict = co_await confirm_travel_impl();

    switch( verdict ) {
      // Cancelled by user
      case e_travel_verdict::cancelled: //
        co_return false;

      // Non-allowed moves (errors)
      case e_travel_verdict::map_edge:
      case e_travel_verdict::land_forbidden:
      case e_travel_verdict::water_forbidden: //
      case e_travel_verdict::not_enough_movement_points:
        co_return false;
      case e_travel_verdict::board_ship_full:
        co_await gui_.message_box(
            "None of the ships on this square have enough free "
            "space to hold this unit!" );
        co_return false;

      // Allowed moves
      case e_travel_verdict::map_to_map:
      case e_travel_verdict::board_ship:
      case e_travel_verdict::offboard_ship:
      case e_travel_verdict::ship_into_port:
      case e_travel_verdict::land_fall:      //
      case e_travel_verdict::sail_high_seas: //
        break;
    }

    // Just some sanity checks.
    CHECK( move_src != move_dst );
    CHECK( find( prioritize.begin(), prioritize.end(),
                 unit_id ) == prioritize.end() );
    CHECK( move_src ==
           coord_for_unit_indirect_or_die( unit_id ) );
    CHECK( move_src.is_adjacent_to( move_dst ) );
    CHECK( target_unit != unit_id );

    co_return true;
  }

  wait<> animate() const override {
    // TODO: in the case of board_ship we need to make sure that
    // the ship being borded gets rendered on top because there
    // may be a stack of ships in the square, otherwise it will
    // be deceiving to the player. This is because when a land
    // unit enters a water square it will just automatically pick
    // a ship and board it.
    if( verdict == e_travel_verdict::land_fall ) co_return;

    co_await landview_animate_move( unit_id, direction );
  }

  wait<> perform() override;

  wait<> post() const override {
    // !! Note that the unit theoretically may not exist here if
    // they were destroyed as part of this action, e.g. lost in a
    // lost city rumor.
    co_return;
  }

  vector<UnitId> units_to_prioritize() const override {
    return prioritize;
  }

  wait<e_travel_verdict> confirm_travel_impl();
  wait<e_travel_verdict> analyze_unload() const;

  wait<e_travel_verdict> confirm_sail_high_seas() const;

  // The unit that is moving.
  UnitId      unit_id;
  e_direction direction;

  vector<UnitId> prioritize = {};

  // If this move is allowed and executed, will the unit actually
  // move to the target square as a result? Normally the answer
  // is yes, however there are cases when the answer is no, such
  // as when a ship makes landfall.
  bool unit_would_move = true;

  // The square on which the unit resides.
  Coord move_src{};

  // The square toward which the move is aimed; note that if/when
  // this move is executed the unit will not necessarily move to
  // this square (it depends on the kind of move being made).
  // That said, this field will always contain a valid and mean-
  // ingful value since there must always be a move order in
  // order for this data structure to even be populated.
  Coord move_dst{};

  // Description of what would happen if the move were carried
  // out. This can also serve as a binary indicator of whether
  // the move is possible by checking the type held, as the can_-
  // move() function does as a convenience.
  e_travel_verdict verdict;

  // Unit that is the target of an action, e.g., ship to be
  // boarded, etc. Not relevant in all contexts.
  maybe<UnitId> target_unit = nothing;

  MovementPoints mv_points_to_subtract_ = {};

  IMapUpdater& map_updater_;

  TerrainState& terrain_state_;

  IGui& gui_;
};

wait<TravelHandler::e_travel_verdict>
TravelHandler::analyze_unload() const {
  std::vector<UnitId> to_offload;
  auto&               unit = unit_from_id( unit_id );
  for( auto unit_item :
       unit.cargo().items_of_type<Cargo::unit>() ) {
    auto const& cargo_unit = unit_from_id( unit_item.id );
    if( !cargo_unit.mv_pts_exhausted() )
      to_offload.push_back( unit_item.id );
  }
  if( !to_offload.empty() ) {
    if( colony_from_coord( move_src ) ) {
      co_await gui_.message_box(
          "A ship containing units cannot make landfall while "
          "in port." );
      co_return e_travel_verdict::land_forbidden;
    }
    // We have at least one unit in the cargo that is able to
    // make landfall. So we will indicate that the unit is al-
    // lowed to make this move.
    string msg = "Would you like to make landfall?";
    if( to_offload.size() <
        unit.cargo().items_of_type<Cargo::unit>().size() )
      msg =
          "Some units have already  moved this turn.  Would you "
          "like the remaining units to make landfall anyway?";
    ui::e_confirm answer =
        co_await gui_.yes_no( { .msg       = msg,
                                .yes_label = "Make landfall",
                                .no_label  = "Stay with ships",
                                .no_comes_first = true } );
    co_return( answer == ui::e_confirm::yes )
        ? e_travel_verdict::land_fall
        : e_travel_verdict::cancelled;
  } else {
    co_return e_travel_verdict::land_forbidden;
  }
}

bool is_high_seas( TerrainState const& terrain_state, Coord c ) {
  return terrain_state.square_at( c ).sea_lane;
}

wait<TravelHandler::e_travel_verdict>
TravelHandler::confirm_sail_high_seas() const {
  CHECK( is_high_seas( terrain_state_, move_dst ) );
  // Only ask to sail the high seas if the current square is not
  // a sea lane. This allows ships to sail around in the sea lane
  // without being asked on each turn whether the player wants to
  // sail the high seas.
  if( is_high_seas( terrain_state_, move_src ) )
    co_return e_travel_verdict::map_to_map;
  ui::e_confirm confirmed = co_await gui_.yes_no(
      { .msg       = "Would you like to sail the high seas?",
        .yes_label = "Yes, steady as she goes!",
        .no_label  = "No, let us remain in these waters." } );
  co_return confirmed == ui::e_confirm::yes
      ? TravelHandler::e_travel_verdict::sail_high_seas
      : TravelHandler::e_travel_verdict::map_to_map;
}

wait<TravelHandler::e_travel_verdict>
TravelHandler::confirm_travel_impl() {
  UnitsState const& units_state = GameState::units();

  UnitId id = unit_id;
  move_src  = coord_for_unit_indirect_or_die( id );
  move_dst  = move_src.moved( direction );

  if( !move_dst.is_inside( terrain_state_.world_rect_tiles() ) )
    co_return e_travel_verdict::map_edge;

  auto&       src_square = terrain_state_.square_at( move_src );
  auto&       dst_square = terrain_state_.square_at( move_dst );
  Unit const& unit       = units_state.unit_for( id );
  maybe<MovementPoints> to_subtract =
      co_await check_movement_points( gui_, unit, src_square,
                                      dst_square );
  if( !to_subtract.has_value() )
    co_return e_travel_verdict::not_enough_movement_points;
  mv_points_to_subtract_ = *to_subtract;

  CHECK( !unit.mv_pts_exhausted() );

  auto surface = surface_type( dst_square );

  e_unit_relationship relationship =
      e_unit_relationship::neutral;
  if( auto dst_nation = nation_from_coord( move_dst );
      dst_nation.has_value() ) {
    CHECK( *dst_nation == unit.nation() );
    relationship = e_unit_relationship::friendly;
  }

  auto units_at_dst = units_from_coord( move_dst );

  e_entity_category category = e_entity_category::empty;
  if( !units_at_dst.empty() ) category = e_entity_category::unit;
  // This must override the above for units.
  if( colony_from_coord( move_dst ).has_value() )
    category = e_entity_category::colony;

  // We are entering an empty land square.
  IF_BEHAVIOR( land, neutral, empty ) {
    using bh_t = unit_behavior::land::neutral::empty::e_vals;
    // Possible results: never, always, unload
    bh_t bh = unit.desc().ship ? bh_t::unload : bh_t::always;
    switch( bh ) {
      case bh_t::never:
        co_return e_travel_verdict::land_forbidden;
      case bh_t::always:
        // `holder` will be a valid value if the unit
        // is cargo of an- other unit; the holder's id
        // in that case will be *holder.
        if( auto holder = is_unit_onboard( unit.id() );
            holder ) {
          // We have a unit onboard a ship moving onto
          // land.
          co_return e_travel_verdict::offboard_ship;
        } else {
          co_return e_travel_verdict::map_to_map;
        }
      case bh_t::unload: {
        unit_would_move = false;
        co_return co_await analyze_unload();
      }
    }
  }
  // We are entering a land square containing a friendly unit.
  IF_BEHAVIOR( land, friendly, unit ) {
    using bh_t = unit_behavior::land::friendly::unit::e_vals;
    // Possible results: always, never, unload
    bh_t bh = unit.desc().ship ? bh_t::unload : bh_t::always;
    switch( bh ) {
      case bh_t::never:
        co_return e_travel_verdict::land_forbidden;
      case bh_t::always:
        // `holder` will be a valid value if the unit
        // is cargo of an- other unit; the holder's id
        // in that case will be *holder.
        if( auto holder = is_unit_onboard( unit.id() );
            holder ) {
          // We have a unit onboard a ship moving onto
          // land.
          co_return e_travel_verdict::offboard_ship;
        } else {
          co_return e_travel_verdict::map_to_map;
        }
      case bh_t::unload: {
        unit_would_move = false;
        co_return co_await analyze_unload();
      }
    }
  }
  // We are entering a land square containing a friendly colony.
  IF_BEHAVIOR( land, friendly, colony ) {
    using bh_t = unit_behavior::land::friendly::colony::e_vals;
    // Possible results: always
    bh_t bh = bh_t::always;
    switch( bh ) {
      case bh_t::always:
        // TODO: when a wagon train enters a colony it ends its
        // turn.
        if( unit.desc().ship )
          co_return e_travel_verdict::ship_into_port;
        // `holder` will be a valid value if the unit is cargo of
        // another unit; the holder's id in that case will be
        // *holder.
        if( auto holder = is_unit_onboard( unit.id() );
            holder ) {
          // We have a unit onboard a ship moving onto
          // a land square with a friendly colony.
          co_return e_travel_verdict::offboard_ship;
        } else {
          co_return e_travel_verdict::map_to_map;
        }
    }
  }
  // We are entering an empty water square.
  IF_BEHAVIOR( water, neutral, empty ) {
    using bh_t = unit_behavior::water::neutral::empty::e_vals;
    // Possible results: never, always, high_seas.
    bh_t bh = unit.desc().ship ? bh_t::always : bh_t::never;
    if( unit.desc().ship &&
        is_high_seas( terrain_state_, move_dst ) )
      bh = bh_t::high_seas;
    switch( bh ) {
      case bh_t::never:
        co_return e_travel_verdict::water_forbidden;
      case bh_t::always: co_return e_travel_verdict::map_to_map;
      case bh_t::high_seas:
        co_return co_await confirm_sail_high_seas();
    }
  }
  // We are entering a water square containing a friendly unit.
  IF_BEHAVIOR( water, friendly, unit ) {
    using bh_t = unit_behavior::water::friendly::unit::e_vals;
    bh_t bh;
    // Possible results: always, never, move_onto_ship,
    //                   high_seas.
    if( unit.desc().ship ) {
      if( is_high_seas( terrain_state_, move_dst ) )
        bh = bh_t::high_seas;
      else
        bh = bh_t::always;
    } else {
      bh = unit.desc().cargo_slots_occupies.has_value()
               ? bh_t::move_onto_ship
               : bh_t::never;
    }
    switch( bh ) {
      case bh_t::never:
        co_return e_travel_verdict::water_forbidden;
      case bh_t::always: co_return e_travel_verdict::map_to_map;
      case bh_t::move_onto_ship: {
        auto const& ships = units_at_dst;
        if( ships.empty() )
          co_return e_travel_verdict::water_forbidden;
        // We have at least one ship, so iterate
        // through and find the first one (if any) that
        // the unit can board.
        for( auto ship_id : ships ) {
          auto const& ship_unit = unit_from_id( ship_id );
          CHECK( ship_unit.desc().ship );
          lg.debug( "checking ship cargo: {}",
                    ship_unit.cargo() );
          if( auto const& cargo = ship_unit.cargo();
              cargo.fits_somewhere( Cargo::unit{ id } ) ) {
            prioritize  = { ship_id };
            target_unit = ship_id;
            co_return e_travel_verdict::board_ship;
          }
        }
        co_return e_travel_verdict::board_ship_full;
      }
      case bh_t::high_seas:
        co_return co_await confirm_sail_high_seas();
    }
  }

  // If one of these triggers then that means that:
  //
  //   1) The line should be removed
  //   2) Some logic should be added above to deal
  //      with that particular situation.
  //   3) Don't do #1 without doing #2
  //
  STATIC_ASSERT_NO_BEHAVIOR( land, friendly, empty );
  STATIC_ASSERT_NO_BEHAVIOR( land, friendly, village );
  STATIC_ASSERT_NO_BEHAVIOR( land, neutral, colony );
  STATIC_ASSERT_NO_BEHAVIOR( land, neutral, unit );
  STATIC_ASSERT_NO_BEHAVIOR( land, neutral, village );
  STATIC_ASSERT_NO_BEHAVIOR( water, friendly, colony );
  STATIC_ASSERT_NO_BEHAVIOR( water, friendly, empty );
  STATIC_ASSERT_NO_BEHAVIOR( water, friendly, village );
  STATIC_ASSERT_NO_BEHAVIOR( water, neutral, colony );
  STATIC_ASSERT_NO_BEHAVIOR( water, neutral, unit );
  STATIC_ASSERT_NO_BEHAVIOR( water, neutral, village );

  SHOULD_NOT_BE_HERE;
}

wait<> TravelHandler::perform() {
  EventsState const& events_state = GameState::events();
  auto               id           = unit_id;
  UnitsState&        units_state  = GameState::units();
  auto&              unit         = units_state.unit_for( id );
  Player&            player = player_for_nation( unit.nation() );

  CHECK( !unit.mv_pts_exhausted() );
  CHECK( unit.orders() == e_unit_orders::none );

  // This will throw if the unit has no coords, but I think it
  // should always be ok at this point if we're moving it.
  auto old_coord = coord_for_unit_indirect_or_die( id );

  switch( verdict ) {
    case e_travel_verdict::cancelled:
    case e_travel_verdict::map_edge:
    case e_travel_verdict::land_forbidden:
    case e_travel_verdict::water_forbidden:
    case e_travel_verdict::board_ship_full:            //
    case e_travel_verdict::not_enough_movement_points: //
      SHOULD_NOT_BE_HERE;
    // Allowed moves.
    case e_travel_verdict::map_to_map: {
      // If it's a ship then sentry all its units before it
      // moves.
      if( unit.desc().ship ) {
        for( Cargo::unit u :
             unit.cargo().items_of_type<Cargo::unit>() ) {
          auto& cargo_unit = unit_from_id( u.id );
          cargo_unit.sentry();
        }
      }
      unit_to_map_square( units_state, map_updater_, id,
                          move_dst );
      CHECK_GT( mv_points_to_subtract_, 0 );
      unit.consume_mv_points( mv_points_to_subtract_ );
      break;
    }
    case e_travel_verdict::board_ship: {
      CHECK( target_unit.has_value() );
      GameState::units().change_to_cargo_somewhere( *target_unit,
                                                    id );
      unit.forfeight_mv_points();
      unit.sentry();
      // If the ship is sentried then clear it's orders because
      // the player will likely want to start moving it now that
      // a unit has boarded.
      auto& ship_unit = unit_from_id( *target_unit );
      ship_unit.clear_orders();
      break;
    }
    case e_travel_verdict::offboard_ship:
      unit_to_map_square( units_state, map_updater_, id,
                          move_dst );
      unit.forfeight_mv_points();
      CHECK( unit.orders() == e_unit_orders::none );
      break;
    case e_travel_verdict::ship_into_port: {
      unit_to_map_square( units_state, map_updater_, id,
                          move_dst );
      // When a ship moves into port it forfeights its movement
      // points.
      unit.forfeight_mv_points();
      CHECK( unit.orders() == e_unit_orders::none );
      UNWRAP_CHECK( colony_id, colony_from_coord( move_dst ) );
      // TODO: by default we should not open the colony view when
      // a ship moves into port. But it would be convenient to
      // allow the user to specify that they want to open it by
      // holding SHIFT while moving the unit.
      //
      // TODO: consider prioritizing units that are brought in by
      // the ship.
      co_await show_colony_view( colony_id, map_updater_ );
      break;
    }
    case e_travel_verdict::land_fall:
      // First activate and prioritize all the units on the ship
      // that have not completed their turns, so that they will
      // ask for orders. But for the first unit, in addition, we
      // will also just give it orders to move onto land. This
      // way, when the player makes land fall, the first unit el-
      // igible for moving will move automatically, then the sub-
      // sequent ones will ask for orders immediately after. This
      // reproduces the behavior of the original game and is a
      // good user experience.
      //
      // Note that the ship's movement points are not consumed.
      prioritize.clear();
      for( auto unit_item :
           unit.cargo().items_of_type<Cargo::unit>() ) {
        auto& cargo_unit = unit_from_id( unit_item.id );
        if( !cargo_unit.mv_pts_exhausted() ) {
          cargo_unit.clear_orders();
          prioritize.push_back( unit_item.id );
        }
      }
      // First unit in list should move first.
      reverse( prioritize.begin(), prioritize.end() );
      for( auto unit_item :
           unit.cargo().items_of_type<Cargo::unit>() ) {
        auto& cargo_unit = unit_from_id( unit_item.id );
        if( !cargo_unit.mv_pts_exhausted() ) {
          UNWRAP_CHECK( direction,
                        old_coord.direction_to( move_dst ) );
          orders_t orders = orders::move{ direction };
          push_unit_orders( unit_item.id, orders );
          // Stop after first eligible unit. The rest of the
          // units will just ask for orders on the ship.
          break;
        }
      }
      break;
    case e_travel_verdict::sail_high_seas: {
      UnitOldWorldViewState_t state =
          UnitOldWorldViewState::inbound{ .percent = 0.0 };
      GameState::units().change_to_old_world_view( id, state );
      // Don't process it again this turn.
      unit.forfeight_mv_points();
      break;
    }
  }

  // Now do a sanity check for units that are on the map. The
  // vast majority of the time they are on the map. An example of
  // a case where the unit is no longer on the map at this point
  // would be a ship that was sent to sail the high seas.
  if( is_unit_on_map_indirect( id ) ) {
    auto new_coord = coord_for_unit_indirect_or_die( id );
    CHECK( unit_would_move == ( new_coord == move_dst ) );
  }

  // Check if the unit actually moved and it landed on a Lost
  // City Rumor.
  if( unit_would_move &&
      has_lost_city_rumor( terrain_state_, move_dst ) ) {
    LostCityRumorResult_t lcr_res =
        co_await enter_lost_city_rumor(
            terrain_state_, units_state, events_state, gui_,
            player, map_updater_, unit_id, move_dst );
    // Presumably we don't want to do anything more in this
    // function if the unit that moved has disappeared.
    if( lcr_res.holds<LostCityRumorResult::unit_lost>() )
      co_return;
    if( auto maybe_unit =
            lcr_res.get_if<LostCityRumorResult::unit_created>();
        maybe_unit.has_value() )
      prioritize.push_back( maybe_unit->id );
  }

  // !! Note that the LCR may have removed the unit!
  co_return; //
}

/****************************************************************
** AttackHandler
*****************************************************************/
struct AttackHandler : public OrdersHandler {
  AttackHandler( UnitId unit_id_, e_direction d,
                 IMapUpdater&  map_updater,
                 TerrainState& terrain_state, IGui& gui )
    : unit_id( unit_id_ ),
      direction( d ),
      map_updater_( map_updater ),
      terrain_state_( terrain_state ),
      gui_( gui ) {}

  enum class e_attack_verdict {
    cancelled,
    // Allowed moves
    colony_undefended,
    colony_defended,
    eu_land_unit,
    ship_on_ship,
    // Non-allowed (errors)
    land_unit_attack_ship,
    ship_attack_land_unit,
    unit_cannot_attack,
    attack_from_ship,
  };

  wait<bool> confirm() override {
    verdict = co_await confirm_attack_impl();

    switch( verdict ) {
      case e_attack_verdict::cancelled: co_return false;
      // Non-allowed (errors)
      case e_attack_verdict::land_unit_attack_ship:
        co_await gui_.message_box(
            "Land units cannot attack ships." );
        co_return false;
      case e_attack_verdict::ship_attack_land_unit:
        co_await gui_.message_box(
            "Ships cannot attack land units." );
        co_return false;
      case e_attack_verdict::unit_cannot_attack:
        co_await gui_.message_box( "This unit cannot attack." );
        co_return false;
      case e_attack_verdict::attack_from_ship:
        co_await gui_.message_box(
            "Cannot attack from a ship." );
        co_return false;
      // Allowed moves
      case e_attack_verdict::colony_undefended:
      case e_attack_verdict::colony_defended:
      case e_attack_verdict::eu_land_unit:
      case e_attack_verdict::ship_on_ship: //
        break;
    }

    CHECK( attack_src != attack_dst );
    CHECK( attack_src ==
           coord_for_unit_indirect_or_die( unit_id ) );
    CHECK( attack_src.is_adjacent_to( attack_dst ) );
    CHECK( target_unit != unit_id );
    CHECK( target_unit.has_value() );
    CHECK( fight_stats.has_value() );

    co_return true;
  }

  wait<> animate() const override {
    if( verdict == e_attack_verdict::colony_undefended &&
        fight_stats->attacker_wins ) {
      UNWRAP_CHECK( colony_id, colony_from_coord( attack_dst ) );
      auto attacker_id = unit_id;
      auto defender_id = *target_unit;
      return landview_animate_colony_capture(
          attacker_id, defender_id, colony_id );
    }

    auto attacker = unit_id;
    UNWRAP_CHECK( defender, target_unit );
    UNWRAP_CHECK( stats, fight_stats );
    auto const& defender_unit = unit_from_id( defender );
    auto const& attacker_unit = unit_from_id( attacker );

    e_depixelate_anim dp_anim =
        stats.attacker_wins
            ? ( defender_unit.desc()
                        .on_death
                        .holds<UnitDeathAction::demote>()
                    ? e_depixelate_anim::demote
                    : e_depixelate_anim::death )
            : ( attacker_unit.desc()
                        .on_death
                        .holds<UnitDeathAction::demote>()
                    ? e_depixelate_anim::demote
                    : e_depixelate_anim::death );
    return landview_animate_attack(
        attacker, defender, stats.attacker_wins, dp_anim );
  }

  wait<> perform() override;

  wait<> post() const override {
    // !! Note that the unit theoretically may not exist here if
    // they were destroyed as part of this action, e.g. a ship
    // losing a battle.

    if( verdict == e_attack_verdict::colony_undefended &&
        fight_stats->attacker_wins ) {
      conductor::play_request(
          conductor::e_request::fife_drum_happy,
          conductor::e_request_probability::always );
      UNWRAP_CHECK( colony_id, colony_from_coord( attack_dst ) );
      Colony const&      colony = colony_from_id( colony_id );
      Nationality const& attacker_nation =
          nation_obj( unit_from_id( unit_id ).nation() );
      co_await gui_.message_box(
          "The @[H]{}@[] have captured the colony of @[H]{}@[]!",
          attacker_nation.display_name, colony.name() );
      co_await show_colony_view( colony_id, map_updater_ );
    }
  }

  wait<e_attack_verdict> confirm_attack_impl();

  // The unit doing the attacking.
  UnitId      unit_id;
  e_direction direction;

  // The square on which the unit resides.
  Coord attack_src{};

  // The square toward which the attack is aimed; this is the
  // same as the square of the unit being attacked.
  Coord attack_dst{};

  // Description of what would happen if the move were carried
  // out. This can also serve as a binary indicator of whether
  // the move is possible by checking the type held, as the can_-
  // move() function does as a convenience.
  e_attack_verdict verdict{};

  // Unit being attacked.
  maybe<UnitId> target_unit{};

  // If the fight is allowed then this will hold the numerical
  // breakdown of the statistics contributing to the final proba-
  // bilities.
  maybe<FightStatistics> fight_stats{};

  IMapUpdater& map_updater_;

  TerrainState& terrain_state_;

  IGui& gui_;
};

wait<AttackHandler::e_attack_verdict>
AttackHandler::confirm_attack_impl() {
  auto id    = unit_id;
  attack_src = coord_for_unit_indirect_or_die( id );
  attack_dst = attack_src.moved( direction );

  auto& unit = unit_from_id( id );
  CHECK( !unit.mv_pts_exhausted() );

  if( is_unit_onboard( id ) )
    co_return e_attack_verdict::attack_from_ship;

  if( !can_attack( unit.desc() ) )
    co_return e_attack_verdict::unit_cannot_attack;

  if( unit.movement_points() < 1 ) {
    if( co_await gui_.yes_no(
            { .msg = fmt::format(
                  "This unit only has @[H]{}@[] movement points "
                  "and so will not be fighting at full "
                  "strength.  Continue?",
                  unit.movement_points() ),
              .yes_label =
                  "Yes, let us proceed with full force!",
              .no_label = "No, do not attack." } ) ==
        ui::e_confirm::no )
      co_return e_attack_verdict::cancelled;
  }

  // Make sure there is a foreign entity in the square otherwise
  // there can be no combat.
  auto dst_nation = nation_from_coord( attack_dst );
  CHECK( dst_nation.has_value() &&
         *dst_nation != unit.nation() );
  CHECK( attack_dst.is_inside(
      terrain_state_.world_rect_tiles() ) );

  auto& square = terrain_state_.square_at( attack_dst );

  auto surface      = surface_type( square );
  auto relationship = e_unit_relationship::foreign;
  auto category     = e_entity_category::unit;
  if( colony_from_coord( attack_dst ).has_value() )
    category = e_entity_category::colony;

  auto const& units_at_dst_set = units_from_coord( attack_dst );
  vector<UnitId> units_at_dst( units_at_dst_set.begin(),
                               units_at_dst_set.end() );
  auto           colony_at_dst = colony_from_coord( attack_dst );

  // If we have a colony then we only want to get units that are
  // military units (and not ships), since we want the following
  // behavior: attacking a colony first attacks all military
  // units, then once those are gone, the next attack will attack
  // a colonist working in the colony (and if the attack suc-
  // ceeds, the colony is taken) even if there are free colonists
  // on the colony map square.
  if( colony_at_dst ) {
    erase_if( units_at_dst, L( unit_from_id( _ ).desc().ship ) );
    erase_if(
        units_at_dst,
        L( !is_military_unit( unit_from_id( _ ).desc() ) ) );
  }

  // If military units are exhausted then attack the colony.
  if( colony_at_dst && units_at_dst.empty() ) {
    auto const&    colony = colony_from_id( *colony_at_dst );
    vector<UnitId> units_working_in_colony = colony.units();
    CHECK( units_working_in_colony.size() > 0 );
    // Sort since order is otherwise unspecified.
    sort( units_working_in_colony.begin(),
          units_working_in_colony.end() );
    units_at_dst.push_back( units_working_in_colony[0] );
  }
  CHECK( !units_at_dst.empty() );
  // Now let's find the unit with the highest defense points
  // among the units in the target square.
  vector<UnitId> sorted_by_defense( units_at_dst.begin(),
                                    units_at_dst.end() );
  util::sort_by_key(
      sorted_by_defense,
      L( unit_from_id( _ ).desc().defense_points ) );
  CHECK( !sorted_by_defense.empty() );
  UnitId highest_defense_unit = sorted_by_defense.back();
  lg.info( "unit in target square with highest defense: {}",
           debug_string( highest_defense_unit ) );

  // Deferred evaluation until we know that the attack makes
  // sense.
  auto run_stats = [id, highest_defense_unit] {
    return fight_statistics( id, highest_defense_unit );
  };

  // We are entering a land square containing a foreign unit.
  IF_BEHAVIOR( land, foreign, unit ) {
    using bh_t = unit_behavior::land::foreign::unit::e_vals;
    bh_t bh;
    // Possible results: nothing, attack, bombard, no_bombard
    if( unit.desc().ship )
      bh = bh_t::no_bombard;
    else
      bh = can_attack( unit.desc() ) ? bh_t::attack
                                     : bh_t::no_attack;
    switch( bh ) {
      case bh_t::no_attack:
        co_return e_attack_verdict::unit_cannot_attack;
      case bh_t::attack:
        target_unit = highest_defense_unit;
        fight_stats = run_stats();
        co_return e_attack_verdict::eu_land_unit;
      case bh_t::no_bombard:
        co_return e_attack_verdict::ship_attack_land_unit;
      case bh_t::bombard:
        target_unit = highest_defense_unit;
        fight_stats = run_stats();
        co_return e_attack_verdict::eu_land_unit;
    }
  }
  // We are entering a land square containing a foreign unit.
  IF_BEHAVIOR( land, foreign, colony ) {
    using bh_t = unit_behavior::land::foreign::colony::e_vals;
    bh_t bh;
    // Possible results: never, attack, trade.
    if( unit.desc().ship )
      bh = bh_t::trade;
    else if( is_military_unit( unit.desc() ) )
      bh = bh_t::attack;
    else
      bh = bh_t::never;
    switch( bh ) {
      case bh_t::never:
        co_return e_attack_verdict::unit_cannot_attack;
      case bh_t::attack: {
        e_attack_verdict which =
            is_military_unit(
                unit_from_id( highest_defense_unit ).desc() )
                ? e_attack_verdict::colony_defended
                : e_attack_verdict::colony_undefended;
        target_unit = highest_defense_unit;
        fight_stats = run_stats();
        co_return which;
      }
      case bh_t::trade:
        // FIXME: implement trade.
        co_return e_attack_verdict::unit_cannot_attack;
    }
  }
  // We are entering a water square containing a foreign unit.
  IF_BEHAVIOR( water, foreign, unit ) {
    using bh_t = unit_behavior::water::foreign::unit::e_vals;
    bh_t bh;
    // Possible results: nothing, attack, bombard
    if( !unit.desc().ship )
      bh = bh_t::no_bombard;
    else
      bh = can_attack( unit.desc() ) ? bh_t::attack
                                     : bh_t::no_attack;
    switch( bh ) {
      case bh_t::no_attack:
        co_return e_attack_verdict::unit_cannot_attack;
      case bh_t::attack:
        target_unit = highest_defense_unit;
        fight_stats = run_stats();
        co_return e_attack_verdict::ship_on_ship;
      case bh_t::no_bombard:;
        co_return e_attack_verdict::land_unit_attack_ship;
      case bh_t::bombard:;
        target_unit = highest_defense_unit;
        fight_stats = run_stats();
        co_return e_attack_verdict::ship_on_ship;
    }
  }

  // If one of these triggers then that means that:
  //
  //   1) The line should be removed
  //   2) Some logic should be added above to deal
  //      with that particular situation.
  //   3) Don't do #1 without doing #2
  //
  STATIC_ASSERT_NO_BEHAVIOR( land, foreign, village );

  SHOULD_NOT_BE_HERE;
}

wait<> AttackHandler::perform() {
  auto  id   = unit_id;
  auto& unit = unit_from_id( id );

  UnitsState& units_state = GameState::units();

  CHECK( !unit.mv_pts_exhausted() );
  CHECK( unit.orders() == e_unit_orders::none );
  CHECK( target_unit.has_value() );
  CHECK( fight_stats.has_value() );

  auto& attacker = unit;
  auto& defender = unit_from_id( *target_unit );
  auto& winner =
      fight_stats->attacker_wins ? attacker : defender;
  auto& loser = fight_stats->attacker_wins ? defender : attacker;

  // The original game seems to consume all movement points of a
  // unit when attacking.
  attacker.forfeight_mv_points();

  switch( verdict ) {
    // Non-allowed (errors)
    case e_attack_verdict::cancelled:
    case e_attack_verdict::land_unit_attack_ship:
    case e_attack_verdict::ship_attack_land_unit:
    case e_attack_verdict::unit_cannot_attack:
    case e_attack_verdict::attack_from_ship: //
      SHOULD_NOT_BE_HERE;
    case e_attack_verdict::colony_undefended: {
      if( !fight_stats->attacker_wins )
        // break since in this case the attacker lost, so nothing
        // special happens; we just do what we normally do when
        // an attacker loses a battle.
        break;
      UNWRAP_CHECK( colony_id, colony_from_coord( attack_dst ) );
      // 1. The colony changes ownership, as well as all of the
      // units that are working in it and who are on the map at
      // the colony location.
      change_colony_nation( colony_id, attacker.nation() );
      // 2. The attacker moves into the colony square.
      unit_to_map_square( units_state, map_updater_,
                          attacker.id(), attack_dst );
      // 3. The attacker has all movement points consumed.
      attacker.forfeight_mv_points();
      // TODO: what if there are trade routes that involve this
      // colony?
      lg.info( "the {} have captured the colony of {}.",
               nation_obj( attacker.nation() ).display_name,
               colony_from_id( colony_id ).name() );
      co_return;
    }
    case e_attack_verdict::colony_defended:
    case e_attack_verdict::eu_land_unit:
    case e_attack_verdict::ship_on_ship: //
      break;
  }

  switch( loser.desc().on_death.to_enum() ) {
    using namespace UnitDeathAction;
    case e::destroy: {
      e_unit_type loser_type   = loser.type();
      e_nation    loser_nation = loser.nation();
      GameState::units().destroy_unit( loser.id() );
      if( loser_type == e_unit_type::scout ||
          loser_type == e_unit_type::seasoned_scout )
        co_await gui_.message_box(
            "@[H]{}@[] scout has been lost!",
            nation_obj( loser_nation ).adjective );
      break;
    }
    case e::naval: {
      auto num_units_lost =
          loser.cargo().items_of_type<Cargo::unit>().size();
      lg.info( "ship sunk: {} units onboard lost.",
               num_units_lost );
      string msg = fmt::format(
          "{} @[H]{}@[] sunk by @[H]{}@[] {}",
          loser.nation_desc().adjective, loser.desc().name,
          winner.nation_desc().adjective, winner.desc().name );
      if( num_units_lost == 1 )
        msg += fmt::format(
            ", @[H]1@[] unit onboard has been lost" );
      else if( num_units_lost > 1 )
        msg += fmt::format(
            ", @[H]{}@[] units onboard have been lost",
            num_units_lost );
      msg += '.';
      // Need to destroy unit first before displaying message
      // otherwise the unit will reappear on the map while the
      // message is open.
      GameState::units().destroy_unit( loser.id() );
      co_await gui_.message_box( msg );
      break;
    }
    case e::capture:
      // Capture only happens to defenders.
      if( loser.id() == defender.id() ) {
        loser.change_nation( winner.nation() );
        unit_to_map_square(
            units_state, map_updater_, loser.id(),
            coord_for_unit_indirect_or_die( winner.id() ) );
        // This is so that the captured unit won't ask for orders
        // in the same turn that it is captured.
        loser.forfeight_mv_points();
        loser.clear_orders();
      } else {
        // If the loser is not the defender, then the loser is
        // the attacker, which means the loser must be a military
        // unit, but military units can't really be captured in
        // this game, at least not with the default rules.
        SHOULD_NOT_BE_HERE;
      }
      break;
    case e::demote:
      loser.demote_from_lost_battle();
      // TODO: if a unit loses, should it lose all of its move-
      // ment points? Check the original game.
      break;
  }
  co_return;
}

/****************************************************************
** Dispatch
*****************************************************************/
unique_ptr<OrdersHandler> dispatch( UnitId id, e_direction d,
                                    IMapUpdater&  map_updater,
                                    TerrainState& terrain_state,
                                    IGui&         gui ) {
  Coord dst  = coord_for_unit_indirect_or_die( id ).moved( d );
  auto& unit = unit_from_id( id );

  if( !dst.is_inside( terrain_state.world_rect_tiles() ) )
    // This is an invalid move, but the TravelHandler is the one
    // that knows how to handle it.
    return make_unique<TravelHandler>( id, d, map_updater,
                                       terrain_state, gui );

  auto dst_nation = nation_from_coord( dst );

  if( !dst_nation.has_value() )
    // No units on target sqaure, so it is just a travel.
    return make_unique<TravelHandler>( id, d, map_updater,
                                       terrain_state, gui );

  if( *dst_nation == unit.nation() )
    // Friendly unit on target square, so not an attack.
    return make_unique<TravelHandler>( id, d, map_updater,
                                       terrain_state, gui );

  // Must be an attack.
  return make_unique<AttackHandler>( id, d, map_updater,
                                     terrain_state, gui );
}

} // namespace

/****************************************************************
** Public API
*****************************************************************/
unique_ptr<OrdersHandler> handle_orders(
    UnitId id, orders::move const& mv, IMapUpdater* map_updater,
    IGui& gui ) {
  TerrainState& terrain_state = GameState::terrain();
  return dispatch( id, mv.d, *map_updater, terrain_state, gui );
}

} // namespace rn
