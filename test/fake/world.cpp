/****************************************************************
**world.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2022-06-03.
*
* Description: Helper for building fake worlds for unit tests.
*
*****************************************************************/
#include "world.hpp"

// Testing
#include "test/mocks/igui.hpp"
#include "test/mocks/irand.hpp"

// Revolution Now
#include "src/colony-mgr.hpp"
#include "src/harbor-units.hpp"
#include "src/lua.hpp"
#include "src/map-updater-lua.hpp"
#include "src/map-updater.hpp"
#include "src/market.hpp"
#include "src/plane-stack.hpp"
#include "src/ts.hpp"
#include "src/ustate.hpp"

// config
#include "config/colony.rds.hpp"
#include "config/market.rds.hpp"
#include "config/natives.rds.hpp"
#include "config/production.rds.hpp"
#include "config/unit-type.rds.hpp"

// ss
#include "src/ss/root.hpp"

// luapp
#include "luapp/state.hpp"

// refl
#include "refl/to-str.hpp"

// base
#include "src/base/keyval.hpp"
#include "src/base/to-str-ext-std.hpp"

using namespace std;

namespace rn::testing {

FormatVersion& World::version() { return root().version; }
EventsState&   World::events() { return root().events; }
SettingsState& World::settings() { return root().settings; }
UnitsState&    World::units() { return root().units; }
PlayersState&  World::players() { return root().players; }
TurnState&     World::turn() { return root().turn; }
ColoniesState& World::colonies() { return root().colonies; }
NativesState&  World::natives() { return root().natives; }
LandViewState& World::land_view() { return root().land_view; }
TerrainState&  World::terrain() { return root().zzz_terrain; }
RootState&     World::root() { return ss_->root; }

FormatVersion const& World::version() const {
  return root().version;
}
EventsState const& World::events() const {
  return root().events;
}
SettingsState const& World::settings() const {
  return root().settings;
}
UnitsState const&   World::units() const { return root().units; }
PlayersState const& World::players() const {
  return root().players;
}
TurnState const&     World::turn() const { return root().turn; }
ColoniesState const& World::colonies() const {
  return root().colonies;
}
NativesState const& World::natives() const {
  return root().natives;
}
LandViewState const& World::land_view() const {
  return root().land_view;
}
TerrainState const& World::terrain() const {
  return root().zzz_terrain;
}
RootState const& World::root() const { return ss_->root; }

SS&            World::ss() { return *ss_; }
SSConst const& World::ss() const { return *ss_const_; }
SS&            World::ss_saved() { return *ss_saved_; }

Planes& World::planes() {
  if( uninitialized_planes_ == nullptr )
    uninitialized_planes_ = make_unique<Planes>();
  return *uninitialized_planes_;
}

lua::state& World::lua() {
  if( uninitialized_lua_ == nullptr ) {
    uninitialized_lua_ = make_unique<lua::state>();
    lua::state& st     = *uninitialized_lua_;
    // FIXME: need to dedupe this logic.
    st["ROOT"] = root();
    st["SS"]   = ss();
  }
  return *uninitialized_lua_;
}

MockIGui& World::gui() {
  if( uninitialized_gui_ == nullptr )
    uninitialized_gui_ = make_unique<MockIGui>();
  return *uninitialized_gui_;
}

MockIRand& World::rand() {
  if( uninitialized_rand_ == nullptr )
    uninitialized_rand_ = make_unique<MockIRand>();
  return *uninitialized_rand_;
}

namespace {

// We need this because we can't (yet?) do aggregate initializa-
// tion for an object on the heap. We have to use new and return
// a pointer because the TS objects are not relocatable, since
// they currently store a reference to themselves in the lua
// state (please FIXME).
TS* make_ts( World& world ) {
  return new TS( world.map_updater(), world.lua(), world.gui(),
                 world.rand(), world.ss_saved().root );
}

}

TS& World::ts() {
  if( uninitialized_ts_ == nullptr )
    uninitialized_ts_ = unique_ptr<TS>( make_ts( *this ) );
  return *uninitialized_ts_;
}

MapSquare World::make_ocean() {
  return map_square_for_terrain( e_terrain::ocean );
}

MapSquare World::make_sea_lane() {
  MapSquare S = map_square_for_terrain( e_terrain::ocean );
  S.sea_lane  = true;
  return S;
}

MapSquare World::make_grassland() {
  return map_square_for_terrain( e_terrain::grassland );
}

MapSquare World::make_terrain( e_terrain terrain ) {
  return map_square_for_terrain( terrain );
}

void World::build_map( vector<MapSquare> tiles, W width ) {
  map_updater().modify_entire_map( [&]( Matrix<MapSquare>& m ) {
    m = Matrix<MapSquare>( std::move( tiles ), width );
  } );
  init_player_maps();
  ss().land_view.viewport.set_world_size_tiles(
      ss().terrain.world_size_tiles() );
}

void World::init_player_maps() {
  for( e_nation nation : refl::enum_values<e_nation> ) {
    if( !ss().players.players[nation].has_value() ) continue;
    ss().mutable_terrain_use_with_care.initialize_player_terrain(
        nation, /*visible=*/false );
  }
}

MapSquare& World::square( gfx::point p ) {
  return terrain().mutable_square_at( Coord::from_gfx( p ) );
}

maybe<FogSquare>& World::player_square(
    gfx::point p, maybe<e_nation> nation ) {
  return ss()
      .mutable_terrain_use_with_care
      .mutable_player_terrain(
          nation.value_or( default_nation() ) )
      .map[Coord::from_gfx( p )];
}

void World::add_forest( gfx::point p ) {
  map_updater_->modify_map_square(
      Coord::from_gfx( p ), []( MapSquare& square ) {
        square.overlay = e_land_overlay::forest;
      } );
}

void World::add_mountains( gfx::point p ) {
  map_updater_->modify_map_square(
      Coord::from_gfx( p ), []( MapSquare& square ) {
        square.overlay = e_land_overlay::mountains;
      } );
}

void World::add_hills( gfx::point p ) {
  map_updater_->modify_map_square(
      Coord::from_gfx( p ), []( MapSquare& square ) {
        square.overlay = e_land_overlay::hills;
      } );
}

void World::add_road( gfx::point p ) {
  map_updater_->modify_map_square(
      Coord::from_gfx( p ),
      []( MapSquare& square ) { square.road = true; } );
}

void World::add_plow( gfx::point p ) {
  map_updater_->modify_map_square(
      Coord::from_gfx( p ),
      []( MapSquare& square ) { square.irrigation = true; } );
}

void World::add_minor_river( gfx::point p ) {
  map_updater_->modify_map_square(
      Coord::from_gfx( p ), []( MapSquare& square ) {
        square.river = e_river::minor;
      } );
}

void World::add_major_river( gfx::point p ) {
  map_updater_->modify_map_square(
      Coord::from_gfx( p ), []( MapSquare& square ) {
        square.river = e_river::major;
      } );
}

Unit& World::add_unit_in_port( e_unit_type     type,
                               maybe<e_nation> nation ) {
  return units().unit_for( create_unit_in_harbor(
      units(), player( nation.value_or( default_nation_ ) ),
      type ) );
}

Unit& World::add_unit_on_map( e_unit_type type, Coord where,
                              maybe<e_nation> nation ) {
  return add_unit_on_map( UnitType::create( type ), where,
                          nation );
}

NativeUnit& World::add_unit_on_map( e_native_unit_type type,
                                    Coord              where,
                                    DwellingId dwelling_id ) {
  return units().unit_for( create_unit_on_map_non_interactive(
      ss(), type, where, dwelling_id ) );
}

Unit& World::add_unit_on_map( UnitType type, Coord where,
                              maybe<e_nation> nation ) {
  if( !nation ) nation = default_nation_;
  return units().unit_for( create_unit_on_map_non_interactive(
      ss(), ts(), player( *nation ),
      UnitComposition::create( type ), where ) );
}

Unit& World::add_unit_on_map( UnitComposition const& comp,
                              Coord                  where,
                              maybe<e_nation>        nation ) {
  if( !nation ) nation = default_nation_;
  UnitId const unit_id = create_unit_on_map_non_interactive(
      ss(), ts(), player( *nation ), comp, where );
  return units().unit_for( unit_id );
}

Unit& World::add_unit_in_cargo( e_unit_type type, UnitId holder,
                                maybe<e_nation> nation ) {
  if( !nation ) nation = default_nation_;
  UnitId held =
      create_free_unit( units(), player( *nation ),
                        UnitComposition::create( type ) );
  units().change_to_cargo_somewhere( holder, held );
  return units().unit_for( held );
}

Unit& World::add_free_unit( e_unit_type     type,
                            maybe<e_nation> nation ) {
  if( !nation ) nation = default_nation_;
  UnitId const id =
      create_free_unit( units(), player( *nation ),
                        UnitComposition::create( type ) );
  return units().unit_for( id );
}

Unit& World::add_unit_indoors( ColonyId     colony_id,
                               e_indoor_job indoor_job,
                               e_unit_type  type ) {
  Colony& colony = colonies().colony_for( colony_id );
  Coord   loc    = colonies().coord_for( colony_id );
  Unit&   unit   = add_unit_on_map( type, loc, colony.nation );
  ColonyJob::indoor job{ .job = indoor_job };
  move_unit_to_colony( units(), player( colony.nation ), colony,
                       unit.id(), job );
  return unit;
}

Unit& World::add_expert_unit_indoors( ColonyId     colony_id,
                                      e_indoor_job indoor_job ) {
  return add_unit_indoors( colony_id, indoor_job,
                           config_production.indoor_production
                               .expert_for[indoor_job] );
}

void World::ship_to_outbound( UnitId id ) {
  CHECK( units().unit_for( id ).desc().ship );
  UnitHarborViewState new_state{
      .port_status = PortStatus::outbound{ .turns = 0 } };
  units().change_to_harbor_view( id, new_state );
}

Unit& World::add_unit_outdoors( ColonyId      colony_id,
                                e_direction   d,
                                e_outdoor_job outdoor_job,
                                e_unit_type   type ) {
  Colony& colony = colonies().colony_for( colony_id );
  Coord   loc    = colonies().coord_for( colony_id );
  Unit&   unit   = add_unit_on_map( type, loc, colony.nation );
  ColonyJob::outdoor job{ .direction = d, .job = outdoor_job };
  move_unit_to_colony( units(), player( colony.nation ), colony,
                       unit.id(), job );
  return unit;
}

Unit& World::add_expert_unit_outdoors(
    ColonyId colony_id, e_direction d,
    e_outdoor_job outdoor_job ) {
  return add_unit_outdoors( colony_id, d, outdoor_job,
                            config_production.outdoor_production
                                .expert_for[outdoor_job] );
}

void World::add_player( e_nation nation ) {
  root().players.players[nation] = Player{};
  // This is the minimal amount that we need to set for a player.
  root().players.players[nation]->nation = nation;
  root().zzz_terrain.initialize_player_terrain(
      nation, /*visible=*/false );
}

void World::add_default_player() {
  add_player( default_nation() );
}

Colony& World::add_colony( UnitId founder ) {
  string name =
      fmt::to_string( colonies().last_colony_id() + 1 );
  ColonyId id = found_colony(
      ss(), ts(),
      player( ss().units.unit_for( founder ).nation() ), founder,
      name );
  return colonies().colony_for( id );
}

Colony& World::add_colony( Coord           where,
                           maybe<e_nation> nation ) {
  string name =
      fmt::to_string( colonies().last_colony_id() + 1 );
  Colony& colony   = colonies().colony_for( create_empty_colony(
      colonies(), nation.value_or( default_nation_ ), where,
      name ) );
  colony.buildings = config_colony.initial_colony_buildings;
  return colony;
}

Colony& World::add_colony_with_new_unit(
    Coord where, maybe<e_nation> nation ) {
  if( !nation ) nation = default_nation_;
  Unit& founder = add_unit_on_map( e_unit_type::free_colonist,
                                   where, *nation );
  return add_colony( founder.id() );
}

// --------------------------------------------------------------
// Creating Native Dwellings.
// --------------------------------------------------------------
Dwelling& World::add_dwelling( Coord where, e_tribe tribe ) {
  natives().create_or_add_tribe( tribe );
  DwellingId const id = natives().add_dwelling( Dwelling{
      .tribe      = tribe,
      .location   = where,
      .population = config_natives.tribes[tribe].max_population,
  } );
  return natives().dwelling_for( id );
}

Tribe& World::add_tribe( e_tribe tribe ) {
  return natives().create_or_add_tribe( tribe );
}

// --------------------------------------------------------------
// Colony setup.
// --------------------------------------------------------------
void World::give_all_buildings( Colony& colony ) {
  for( e_colony_building b :
       refl::enum_values<e_colony_building> )
    colony.buildings[b] = true;
}

// ------------------------------------------------------------
// Market Prices.
// ------------------------------------------------------------
void World::set_current_bid_price( e_commodity type,
                                   int price_in_hundreds ) {
  Player& player    = default_player();
  auto& comm_config = player.old_world.market.commodities[type];
  comm_config.bid_price = price_in_hundreds;
}

void World::set_stable_bid_price( e_commodity type,
                                  int price_in_hundreds ) {
  CHECK( !is_in_processed_goods_price_group( type ),
         "cannot set the equilibrium price for goods in a price "
         "group." );
  Player& player    = default_player();
  auto& comm_config = player.old_world.market.commodities[type];
  comm_config.bid_price        = price_in_hundreds;
  comm_config.intrinsic_volume = 0;
}

void World::init_prices_to_average() {
  for( e_commodity type : refl::enum_values<e_commodity> ) {
    auto const& limits =
        config_market.price_behavior[type].price_limits;
    int const price =
        ( limits.bid_price_min + limits.bid_price_max ) / 2;
    set_current_bid_price( type, price );
  }
}

void World::set_tax_rate( int rate ) {
  default_player().old_world.taxes.tax_rate = rate;
}

// --------------------------------------------------------------
// Players.
// --------------------------------------------------------------
Player& World::dutch() {
  UNWRAP_CHECK( player,
                root().players.players[e_nation::dutch] );
  return player;
}

Player& World::english() {
  UNWRAP_CHECK( player,
                root().players.players[e_nation::english] );
  return player;
}

Player& World::spanish() {
  UNWRAP_CHECK( player,
                root().players.players[e_nation::spanish] );
  return player;
}

Player& World::french() {
  UNWRAP_CHECK( player,
                root().players.players[e_nation::french] );
  return player;
}

Player const& World::dutch() const {
  UNWRAP_CHECK( player,
                root().players.players[e_nation::dutch] );
  return player;
}

Player const& World::english() const {
  UNWRAP_CHECK( player,
                root().players.players[e_nation::english] );
  return player;
}

Player const& World::spanish() const {
  UNWRAP_CHECK( player,
                root().players.players[e_nation::spanish] );
  return player;
}

Player const& World::french() const {
  UNWRAP_CHECK( player,
                root().players.players[e_nation::french] );
  return player;
}

Player& World::default_player() {
  return player( default_nation_ );
}

Player const& World::default_player() const {
  return player( default_nation_ );
}

Player& World::player( maybe<e_nation> nation ) {
  switch( nation.value_or( default_nation_ ) ) {
    case e_nation::dutch: return dutch();
    case e_nation::english: return english();
    case e_nation::french: return french();
    case e_nation::spanish: return spanish();
  }
}

Player const& World::player( maybe<e_nation> nation ) const {
  switch( nation.value_or( default_nation_ ) ) {
    case e_nation::dutch: return dutch();
    case e_nation::english: return english();
    case e_nation::french: return french();
    case e_nation::spanish: return spanish();
  }
}

base::valid_or<string> World::validate_colonies() const {
  for( auto const& [colony_id, colony] : colonies().all() ) {
    valid_or<string> res = colony.validate();
    if( !res.valid() )
      return fmt::format( "colony {} failed validation: {}",
                          colony_id, res.error() );
  }
  return base::valid;
}

void World::expensive_run_lua_init() {
  lua::state& st = lua();
  lua_init( st );
}

void World::initialize_ts() { ts(); }

World::World()
  : ss_( new SS ),
    ss_const_( new SSConst( *ss_ ) ),
    ss_saved_( new SS ),
    map_updater_( new NonRenderingMapUpdater( *ss_ ) ),
    // These are left uninitialized until they are needed.
    uninitialized_planes_(),
    uninitialized_lua_(),
    uninitialized_gui_(),
    uninitialized_ts_() {}

World::~World() noexcept = default;

} // namespace rn::testing
