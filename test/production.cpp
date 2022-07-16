/****************************************************************
**production.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2022-06-06.
*
* Description: Unit tests for the src/production.* module.
*
*****************************************************************/
#include "test/testing.hpp"

// Under test.
#include "src/production.hpp"

// Testing.
#include "test/fake/world.hpp"

// Revolution Now
#include "colony-mgr.hpp"
#include "map-square.hpp"

// ss
#include "src/ss/player.rds.hpp"
#include "src/ss/ref.hpp"
#include "src/ss/settings.hpp"

// refl
#include "refl/to-str.hpp"

// base
#include "base/to-str-ext-std.hpp"

// Must be last.
#include "test/catch-common.hpp"

namespace rn {
namespace {

using namespace std;

/****************************************************************
** Fake World Setup
*****************************************************************/
struct World : testing::World {
  using Base = testing::World;
  World() : Base() { add_player( e_nation::dutch ); }

  static inline Coord kGrasslandTile{ .x = 1, .y = 1 };
  static inline Coord kPrairieTile{ .x = 4, .y = 1 };
  static inline Coord kConiferTile{ .x = 3, .y = 1 };

  void create_default_map() {
    MapSquare const _ = make_ocean();
    MapSquare const G = make_grassland();
    MapSquare const C = make_terrain( e_terrain::conifer );
    MapSquare const P = make_terrain( e_terrain::prairie );
    // clang-format off
    vector<MapSquare> tiles{
      _, G, _, _, P, _,
      G, G, G, C, P, P,
      _, G, G, P, P, _,
    };
    // clang-format on
    build_map( std::move( tiles ), 6 );

    CHECK( effective_terrain( square( kGrasslandTile ) ) ==
           e_terrain::grassland );
    CHECK( effective_terrain( square( kPrairieTile ) ) ==
           e_terrain::prairie );
    CHECK( effective_terrain( square( kConiferTile ) ) ==
           e_terrain::conifer );
  }
};

/****************************************************************
** Test Cases
*****************************************************************/
TEST_CASE( "[production] production_for_slot" ) {
  ColonyProduction pr;

  pr.ore_products.muskets_produced_theoretical   = 1;
  pr.ore_products.tools_produced_theoretical     = 2;
  pr.sugar_rum.product_produced_theoretical      = 3;
  pr.cotton_cloth.product_produced_theoretical   = 4;
  pr.fur_coats.product_produced_theoretical      = 5;
  pr.tobacco_cigars.product_produced_theoretical = 6;
  pr.lumber_hammers.product_produced_theoretical = 7;
  pr.bells                                       = 8;
  pr.food.horses_produced_theoretical            = 12;
  pr.crosses                                     = 15;

  REQUIRE( production_for_slot(
               pr, e_colony_building_slot::muskets ) == 1 );
  REQUIRE( production_for_slot(
               pr, e_colony_building_slot::tools ) == 2 );
  REQUIRE( production_for_slot(
               pr, e_colony_building_slot::rum ) == 3 );
  REQUIRE( production_for_slot(
               pr, e_colony_building_slot::cloth ) == 4 );
  REQUIRE( production_for_slot(
               pr, e_colony_building_slot::coats ) == 5 );
  REQUIRE( production_for_slot(
               pr, e_colony_building_slot::cigars ) == 6 );
  REQUIRE( production_for_slot(
               pr, e_colony_building_slot::hammers ) == 7 );
  REQUIRE( production_for_slot(
               pr, e_colony_building_slot::town_hall ) == 8 );
  REQUIRE( production_for_slot(
               pr, e_colony_building_slot::newspapers ) ==
           nothing );
  REQUIRE( production_for_slot(
               pr, e_colony_building_slot::schools ) ==
           nothing );
  REQUIRE( production_for_slot(
               pr, e_colony_building_slot::offshore ) ==
           nothing );
  REQUIRE( production_for_slot(
               pr, e_colony_building_slot::horses ) == 12 );
  REQUIRE( production_for_slot(
               pr, e_colony_building_slot::wall ) == nothing );
  REQUIRE( production_for_slot(
               pr, e_colony_building_slot::warehouses ) ==
           nothing );
  REQUIRE( production_for_slot(
               pr, e_colony_building_slot::crosses ) == 15 );
  REQUIRE( production_for_slot(
               pr, e_colony_building_slot::custom_house ) ==
           nothing );
}

TEST_CASE( "[production] crosses" ) {
  World W;
  W.create_default_map();
  Colony& colony = W.add_colony( Coord{ .x = 1, .y = 1 } );
  Player& player = W.dutch();

  auto crosses = [&] {
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    return pr.crosses;
  };

  // Baseline.
  REQUIRE( crosses() == 1 );

  // With church.
  colony.buildings[e_colony_building::church] = true;
  REQUIRE( crosses() == 2 );

  // With free colonist working in church.
  W.add_unit_indoors( colony.id, e_indoor_job::crosses );
  REQUIRE( crosses() == 2 + 3 );

  // With free colonist and firebrand preacher working in church.
  W.add_unit_indoors( colony.id, e_indoor_job::crosses,
                      e_unit_type::firebrand_preacher );
  REQUIRE( crosses() == 2 + 3 + 6 );

  // With free colonist and firebrand preacher working in cathe-
  // dral.
  colony.buildings[e_colony_building::cathedral] = true;
  REQUIRE( crosses() == 3 + ( 3 + 6 ) * 2 );

  // Taking away the church should have no effect because the
  // cathedral should override it.
  colony.buildings[e_colony_building::church] = false;
  REQUIRE( crosses() == 3 + ( 3 + 6 ) * 2 );

  // With free colonist and firebrand preacher working in cathe-
  // dral with William Penn.
  player.fathers.has[e_founding_father::william_penn] = true;
  // Should be a 50% increase (rounded up) applied only to the
  // units' production and not the base production.
  REQUIRE( crosses() ==
           ( 3 + ( 3 + 6 ) * 2 ) + ( ( 3 + 6 ) * 2 ) / 2 );

  // All colonies constructed in this test should be valid.
  REQUIRE( W.validate_colonies() == base::valid );
}

TEST_CASE( "[production] lumber/hammers" ) {
  World W;
  W.create_default_map();
  Colony&    colony = W.add_colony( Coord{ .x = 1, .y = 1 } );
  gfx::point P{ .x = 0, .y = 1 };

  auto lum = [&] {
    return production_for_colony( W.ss(), colony )
        .lumber_hammers;
  };

  // TODO
  // Need to add hammers.

  SECTION( "petty_criminal" ) {
    W.add_unit_outdoors( colony.id, e_direction::w,
                         e_outdoor_job::lumber,
                         e_unit_type::petty_criminal );
    SECTION( "grassland" ) {
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With road.
      W.add_road( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With minor river.
      W.add_minor_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With major river.
      W.add_major_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );
    }
    SECTION( "conifer forest" ) {
      W.add_forest( P );
      REQUIRE( lum() == RawMaterialAndProduct{
                            .raw_produced          = 6,
                            .raw_delta_theoretical = 6,
                            .raw_delta_final       = 6,
                        } );

      // With road.
      W.add_road( P );
      REQUIRE( lum() == RawMaterialAndProduct{
                            .raw_produced          = 8,
                            .raw_delta_theoretical = 8,
                            .raw_delta_final       = 8,
                        } );

      // With minor river.
      W.add_minor_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{
                            .raw_produced          = 10,
                            .raw_delta_theoretical = 10,
                            .raw_delta_final       = 10,
                        } );

      // With major river.
      W.add_major_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{
                            .raw_produced          = 12,
                            .raw_delta_theoretical = 12,
                            .raw_delta_final       = 12,
                        } );
    }
    SECTION( "mountains" ) {
      W.add_mountains( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With road.
      W.add_road( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With minor river.
      W.add_minor_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With major river.
      W.add_major_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );
    }
  }

  SECTION( "native_convert" ) {
    W.add_unit_outdoors( colony.id, e_direction::w,
                         e_outdoor_job::lumber,
                         e_unit_type::native_convert );
    SECTION( "grassland" ) {
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With road.
      W.add_road( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With minor river.
      W.add_minor_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With major river.
      W.add_major_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );
    }
    SECTION( "conifer forest" ) {
      W.add_forest( P );
      REQUIRE( lum() == RawMaterialAndProduct{
                            .raw_produced          = 6,
                            .raw_delta_theoretical = 6,
                            .raw_delta_final       = 6,
                        } );

      // With road.
      W.add_road( P );
      REQUIRE( lum() == RawMaterialAndProduct{
                            .raw_produced          = 8,
                            .raw_delta_theoretical = 8,
                            .raw_delta_final       = 8,
                        } );

      // With minor river.
      W.add_minor_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{
                            .raw_produced          = 10,
                            .raw_delta_theoretical = 10,
                            .raw_delta_final       = 10,
                        } );

      // With major river.
      W.add_major_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{
                            .raw_produced          = 12,
                            .raw_delta_theoretical = 12,
                            .raw_delta_final       = 12,
                        } );
    }
    SECTION( "mountains" ) {
      W.add_mountains( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With road.
      W.add_road( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With minor river.
      W.add_minor_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With major river.
      W.add_major_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );
    }
  }

  SECTION( "free_colonist" ) {
    W.add_unit_outdoors( colony.id, e_direction::w,
                         e_outdoor_job::lumber,
                         e_unit_type::free_colonist );
    SECTION( "grassland" ) {
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With road.
      W.add_road( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With minor river.
      W.add_minor_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With major river.
      W.add_major_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );
    }
    SECTION( "conifer forest" ) {
      W.add_forest( P );
      REQUIRE( lum() == RawMaterialAndProduct{
                            .raw_produced          = 6,
                            .raw_delta_theoretical = 6,
                            .raw_delta_final       = 6,
                        } );

      // With road.
      W.add_road( P );
      REQUIRE( lum() == RawMaterialAndProduct{
                            .raw_produced          = 8,
                            .raw_delta_theoretical = 8,
                            .raw_delta_final       = 8,
                        } );

      // With minor river.
      W.add_minor_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{
                            .raw_produced          = 10,
                            .raw_delta_theoretical = 10,
                            .raw_delta_final       = 10,
                        } );

      // With major river.
      W.add_major_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{
                            .raw_produced          = 12,
                            .raw_delta_theoretical = 12,
                            .raw_delta_final       = 12,
                        } );
    }
    SECTION( "mountains" ) {
      W.add_mountains( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With road.
      W.add_road( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With minor river.
      W.add_minor_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With major river.
      W.add_major_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );
    }
  }

  SECTION( "expert_lumberjack" ) {
    W.add_unit_outdoors( colony.id, e_direction::w,
                         e_outdoor_job::lumber,
                         e_unit_type::expert_lumberjack );
    SECTION( "grassland" ) {
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With road.
      W.add_road( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With minor river.
      W.add_minor_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With major river.
      W.add_major_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );
    }
    SECTION( "conifer forest" ) {
      W.add_forest( P );
      REQUIRE( lum() == RawMaterialAndProduct{
                            .raw_produced          = 12,
                            .raw_delta_theoretical = 12,
                            .raw_delta_final       = 12,
                        } );

      // With road.
      W.add_road( P );
      REQUIRE( lum() == RawMaterialAndProduct{
                            .raw_produced          = 16,
                            .raw_delta_theoretical = 16,
                            .raw_delta_final       = 16,
                        } );

      // With minor river.
      W.add_minor_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{
                            .raw_produced          = 20,
                            .raw_delta_theoretical = 20,
                            .raw_delta_final       = 20,
                        } );

      // With major river.
      W.add_major_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{
                            .raw_produced          = 24,
                            .raw_delta_theoretical = 24,
                            .raw_delta_final       = 24,
                        } );
    }
    SECTION( "mountains" ) {
      W.add_mountains( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With road.
      W.add_road( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With minor river.
      W.add_minor_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );

      // With major river.
      W.add_major_river( P );
      REQUIRE( lum() == RawMaterialAndProduct{} );
    }
  }
}

TEST_CASE( "[production] silver [discoverer]" ) {
  World W;
  W.create_default_map();
  MapSquare& has_silver      = W.square( World::kGrasslandTile -
                                         Delta{ .w = 1, .h = 0 } );
  has_silver.overlay         = e_land_overlay::mountains;
  has_silver.ground_resource = e_natural_resource::silver;

  using SP  = SquareProduction;
  using LP  = refl::enum_map<e_direction, SP>;
  using RMP = RawMaterialAndProduct;

  W.settings().difficulty = e_difficulty::discoverer;

  SECTION( "silver miner, warehouse empty" ) {
    Colony& colony = W.add_colony( W.kGrasslandTile );
    W.add_unit_outdoors( colony.id, e_direction::w,
                         e_outdoor_job::silver,
                         e_unit_type::expert_silver_miner );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE(
        pr.land_production ==
        LP{ { e_direction::w, SP{ .what = e_outdoor_job::silver,
                                  .quantity = 6 } } } );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.silver == 6 );
  }

  SECTION( "silver miner, warehouse partial" ) {
    Colony& colony = W.add_colony( W.kGrasslandTile );
    colony.commodities[e_commodity::silver] = 50;
    W.add_unit_outdoors( colony.id, e_direction::w,
                         e_outdoor_job::silver,
                         e_unit_type::expert_silver_miner );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE(
        pr.land_production ==
        LP{ { e_direction::w, SP{ .what = e_outdoor_job::silver,
                                  .quantity = 6 } } } );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.silver == 6 );
  }

  SECTION( "silver miner, warehouse almost full" ) {
    Colony& colony = W.add_colony( W.kGrasslandTile );
    colony.commodities[e_commodity::silver] = 98;
    W.add_unit_outdoors( colony.id, e_direction::w,
                         e_outdoor_job::silver,
                         e_unit_type::expert_silver_miner );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE(
        pr.land_production ==
        LP{ { e_direction::w, SP{ .what = e_outdoor_job::silver,
                                  .quantity = 6 } } } );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.silver == 2 );
  }

  SECTION( "silver miner, warehouse full" ) {
    Colony& colony = W.add_colony( W.kGrasslandTile );
    colony.commodities[e_commodity::silver] = 100;
    W.add_unit_outdoors( colony.id, e_direction::w,
                         e_outdoor_job::silver,
                         e_unit_type::expert_silver_miner );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE(
        pr.land_production ==
        LP{ { e_direction::w, SP{ .what = e_outdoor_job::silver,
                                  .quantity = 6 } } } );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.silver == 0 );
  }

  SECTION( "silver miner, warehouse over" ) {
    Colony& colony = W.add_colony( W.kGrasslandTile );
    colony.commodities[e_commodity::silver] = 150;
    W.add_unit_outdoors( colony.id, e_direction::w,
                         e_outdoor_job::silver,
                         e_unit_type::expert_silver_miner );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE(
        pr.land_production ==
        LP{ { e_direction::w, SP{ .what = e_outdoor_job::silver,
                                  .quantity = 6 } } } );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.silver == 0 );
  }
}

TEST_CASE( "[production] tobacco/cigar [discoverer]" ) {
  World W;
  W.create_default_map();

  using SP  = SquareProduction;
  using LP  = refl::enum_map<e_direction, SP>;
  using RMP = RawMaterialAndProduct;

  W.settings().difficulty = e_difficulty::discoverer;

  SECTION( "center square tobacco only" ) {
    Colony&          colony = W.add_colony( W.kGrasslandTile );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE( pr.land_production == LP{} );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 4,
                 .raw_consumed_theoretical     = 0,
                 .raw_delta_theoretical        = 4,
                 .raw_consumed_actual          = 0,
                 .raw_delta_final              = 4,
                 .product_produced_theoretical = 0,
                 .product_produced_actual      = 0,
                 .product_delta_final          = 0,
             } );
  }

  SECTION(
      "center square tobacco only, almost full warehouse" ) {
    Colony& colony = W.add_colony( W.kGrasslandTile );
    colony.commodities[e_commodity::tobacco] = 98;
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE( pr.land_production == LP{} );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 4,
                 .raw_consumed_theoretical     = 0,
                 .raw_delta_theoretical        = 4,
                 .raw_consumed_actual          = 0,
                 .raw_delta_final              = 2,
                 .product_produced_theoretical = 0,
                 .product_produced_actual      = 0,
                 .product_delta_final          = 0,
             } );
  }

  SECTION( "center square tobacco only, full warehouse" ) {
    Colony& colony = W.add_colony( W.kGrasslandTile );
    colony.commodities[e_commodity::tobacco] = 100;
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE( pr.land_production == LP{} );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 4,
                 .raw_consumed_theoretical     = 0,
                 .raw_delta_theoretical        = 4,
                 .raw_consumed_actual          = 0,
                 .raw_delta_final              = 0,
                 .product_produced_theoretical = 0,
                 .product_produced_actual      = 0,
                 .product_delta_final          = 0,
             } );
  }

  SECTION( "center square tobacco only, over warehouse" ) {
    Colony& colony = W.add_colony( W.kGrasslandTile );
    colony.commodities[e_commodity::tobacco] = 150;
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE( pr.land_production == LP{} );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 4,
                 .raw_consumed_theoretical     = 0,
                 .raw_delta_theoretical        = 4,
                 .raw_consumed_actual          = 0,
                 .raw_delta_final              = 0,
                 .product_produced_theoretical = 0,
                 .product_produced_actual      = 0,
                 .product_delta_final          = 0,
             } );
  }

  SECTION( "one tobacco planter" ) {
    Colony& colony = W.add_colony( W.kGrasslandTile );
    W.add_unit_outdoors( colony.id, e_direction::w,
                         e_outdoor_job::tobacco );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE(
        pr.land_production ==
        LP{ { e_direction::w, SP{ .what = e_outdoor_job::tobacco,
                                  .quantity = 3 } } } );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 7,
                 .raw_consumed_theoretical     = 0,
                 .raw_delta_theoretical        = 7,
                 .raw_consumed_actual          = 0,
                 .raw_delta_final              = 7,
                 .product_produced_theoretical = 0,
                 .product_produced_actual      = 0,
                 .product_delta_final          = 0,
             } );
  }

  SECTION( "no tobacco center square" ) {
    Colony&          colony = W.add_colony( W.kPrairieTile );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE( pr.land_production == LP{} );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::cotton, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 0,
                 .raw_consumed_theoretical     = 0,
                 .raw_delta_theoretical        = 0,
                 .raw_consumed_actual          = 0,
                 .raw_delta_final              = 0,
                 .product_produced_theoretical = 0,
                 .product_produced_actual      = 0,
                 .product_delta_final          = 0,
             } );
  }

  SECTION( "no tobacco center square/with rum distiller" ) {
    Colony& colony = W.add_colony( W.kPrairieTile );
    W.add_unit_indoors( colony.id, e_indoor_job::rum,
                        e_unit_type::master_rum_distiller );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE( pr.land_production == LP{} );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::cotton, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 0,
                 .raw_consumed_theoretical     = 0,
                 .raw_delta_theoretical        = 0,
                 .raw_consumed_actual          = 0,
                 .raw_delta_final              = 0,
                 .product_produced_theoretical = 0,
                 .product_produced_actual      = 0,
                 .product_delta_final          = 0,
             } );
  }

  SECTION(
      "no tobacco center square/with tobacconist, tobacco in "
      "store" ) {
    Colony& colony = W.add_colony( W.kPrairieTile );
    colony.commodities[e_commodity::tobacco] = 50;
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::free_colonist );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE( pr.land_production == LP{} );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::cotton, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 0,
                 .raw_consumed_theoretical     = 3,
                 .raw_delta_theoretical        = -3,
                 .raw_consumed_actual          = 3,
                 .raw_delta_final              = -3,
                 .product_produced_theoretical = 3,
                 .product_produced_actual      = 3,
                 .product_delta_final          = 3,
             } );
  }

  SECTION(
      "no tobacco center square/with master tobacconist, some "
      "tobacco in store" ) {
    Colony& colony = W.add_colony( W.kPrairieTile );
    colony.commodities[e_commodity::tobacco] = 3;
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::master_tobacconist );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE( pr.land_production == LP{} );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::cotton, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 0,
                 .raw_consumed_theoretical     = 6,
                 .raw_delta_theoretical        = -6,
                 .raw_consumed_actual          = 3,
                 .raw_delta_final              = -3,
                 .product_produced_theoretical = 6,
                 .product_produced_actual      = 3,
                 .product_delta_final          = 3,
             } );
  }

  SECTION(
      "no tobacco center square/with tobacconist, some tobacco "
      "in store" ) {
    Colony& colony = W.add_colony( W.kPrairieTile );
    colony.commodities[e_commodity::tobacco] = 3;
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::free_colonist );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE( pr.land_production == LP{} );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::cotton, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 0,
                 .raw_consumed_theoretical     = 3,
                 .raw_delta_theoretical        = -3,
                 .raw_consumed_actual          = 3,
                 .raw_delta_final              = -3,
                 .product_produced_theoretical = 3,
                 .product_produced_actual      = 3,
                 .product_delta_final          = 3,
             } );
  }

  SECTION(
      "no tobacco center square/with master tobacconist, "
      "tobacco in store" ) {
    Colony& colony = W.add_colony( W.kPrairieTile );
    colony.commodities[e_commodity::tobacco] = 50;
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::master_tobacconist );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE( pr.land_production == LP{} );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::cotton, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 0,
                 .raw_consumed_theoretical     = 6,
                 .raw_delta_theoretical        = -6,
                 .raw_consumed_actual          = 6,
                 .raw_delta_final              = -6,
                 .product_produced_theoretical = 6,
                 .product_produced_actual      = 6,
                 .product_delta_final          = 6,
             } );
  }

  SECTION(
      "no tobacco center square/with master tobacconist, "
      "tobacco in store, almost full warehouse for cigars" ) {
    Colony& colony = W.add_colony( W.kPrairieTile );
    colony.commodities[e_commodity::tobacco] = 50;
    colony.commodities[e_commodity::cigars]  = 98;
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::master_tobacconist );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE( pr.land_production == LP{} );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::cotton, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced             = 0,
                 .raw_consumed_theoretical = 6,
                 .raw_delta_theoretical    = -6,
                 // No backpressure.
                 .raw_consumed_actual          = 6,
                 .raw_delta_final              = -6,
                 .product_produced_theoretical = 6,
                 .product_produced_actual      = 6,
                 .product_delta_final          = 2,
             } );
  }

  SECTION(
      "no tobacco center square/with master tobacconist, "
      "tobacco in store, full warehouse for cigars" ) {
    Colony& colony = W.add_colony( W.kPrairieTile );
    colony.commodities[e_commodity::tobacco] = 50;
    colony.commodities[e_commodity::cigars]  = 100;
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::master_tobacconist );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE( pr.land_production == LP{} );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::cotton, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced             = 0,
                 .raw_consumed_theoretical = 6,
                 .raw_delta_theoretical    = -6,
                 // No backpressure.
                 .raw_consumed_actual          = 6,
                 .raw_delta_final              = -6,
                 .product_produced_theoretical = 6,
                 .product_produced_actual      = 6,
                 .product_delta_final          = 0,
             } );
  }

  SECTION(
      "no tobacco center square/with master tobacconist, "
      "tobacco in store, over warehouse for cigars" ) {
    Colony& colony = W.add_colony( W.kPrairieTile );
    colony.commodities[e_commodity::tobacco] = 50;
    colony.commodities[e_commodity::cigars]  = 150;
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::master_tobacconist );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE( pr.land_production == LP{} );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::cotton, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced             = 0,
                 .raw_consumed_theoretical = 6,
                 .raw_delta_theoretical    = -6,
                 // No backpressure.
                 .raw_consumed_actual          = 6,
                 .raw_delta_final              = -6,
                 .product_produced_theoretical = 6,
                 .product_produced_actual      = 6,
                 .product_delta_final          = 0,
             } );
  }

  SECTION(
      "tobacco center square/with master tobacconist, "
      "no tobacco in store, no cigars in store" ) {
    Colony& colony = W.add_colony( W.kGrasslandTile );
    colony.commodities[e_commodity::tobacco] = 0;
    colony.commodities[e_commodity::cigars]  = 0;
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::master_tobacconist );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE( pr.land_production == LP{} );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 4,
                 .raw_consumed_theoretical     = 6,
                 .raw_delta_theoretical        = -2,
                 .raw_consumed_actual          = 4,
                 .raw_delta_final              = 0,
                 .product_produced_theoretical = 6,
                 .product_produced_actual      = 4,
                 .product_delta_final          = 4,
             } );
  }

  SECTION(
      "tobacco center square/with master tobacconist, "
      "no tobacco in store, full cigar warehouse" ) {
    Colony& colony = W.add_colony( W.kGrasslandTile );
    colony.commodities[e_commodity::tobacco] = 0;
    colony.commodities[e_commodity::cigars]  = 100;
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::master_tobacconist );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE( pr.land_production == LP{} );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 4,
                 .raw_consumed_theoretical     = 6,
                 .raw_delta_theoretical        = -2,
                 .raw_consumed_actual          = 4,
                 .raw_delta_final              = 0,
                 .product_produced_theoretical = 6,
                 .product_produced_actual      = 4,
                 .product_delta_final          = 0,
             } );
  }

  SECTION(
      "tobacco center square, one tobacco planter, with master "
      "tobacconist, no tobacco in store, no cigars in store, "
      "cigar shop" ) {
    Colony& colony = W.add_colony( W.kGrasslandTile );
    colony.buildings[e_colony_building::tobacconists_shop] =
        true;
    colony.commodities[e_commodity::tobacco] = 0;
    colony.commodities[e_commodity::cigars]  = 0;
    W.add_unit_outdoors( colony.id, e_direction::w,
                         e_outdoor_job::tobacco,
                         e_unit_type::free_colonist );
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::master_tobacconist );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE(
        pr.land_production ==
        LP{ { e_direction::w, SP{ .what = e_outdoor_job::tobacco,
                                  .quantity = 3 } } } );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 7,
                 .raw_consumed_theoretical     = 12,
                 .raw_delta_theoretical        = -5,
                 .raw_consumed_actual          = 7,
                 .raw_delta_final              = 0,
                 .product_produced_theoretical = 12,
                 .product_produced_actual      = 7,
                 .product_delta_final          = 7,
             } );
  }

  SECTION(
      "tobacco center square, one tobacco planter, with master "
      "tobacconist, some tobacco in store, no cigars in store, "
      "cigar shop" ) {
    Colony& colony = W.add_colony( W.kGrasslandTile );
    colony.buildings[e_colony_building::tobacconists_shop] =
        true;
    colony.commodities[e_commodity::tobacco] = 50;
    colony.commodities[e_commodity::cigars]  = 0;
    W.add_unit_outdoors( colony.id, e_direction::w,
                         e_outdoor_job::tobacco,
                         e_unit_type::free_colonist );
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::master_tobacconist );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE(
        pr.land_production ==
        LP{ { e_direction::w, SP{ .what = e_outdoor_job::tobacco,
                                  .quantity = 3 } } } );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 7,
                 .raw_consumed_theoretical     = 12,
                 .raw_delta_theoretical        = -5,
                 .raw_consumed_actual          = 12,
                 .raw_delta_final              = -5,
                 .product_produced_theoretical = 12,
                 .product_produced_actual      = 12,
                 .product_delta_final          = 12,
             } );
  }

  SECTION(
      "tobacco center square, one tobacco planter, with master "
      "tobacconist, no tobacco in store, no cigars in store, "
      "cigar factory" ) {
    Colony& colony = W.add_colony( W.kGrasslandTile );
    colony.buildings[e_colony_building::cigar_factory] = true;
    colony.commodities[e_commodity::tobacco]           = 0;
    colony.commodities[e_commodity::cigars]            = 0;
    W.add_unit_outdoors( colony.id, e_direction::w,
                         e_outdoor_job::tobacco,
                         e_unit_type::free_colonist );
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::master_tobacconist );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE(
        pr.land_production ==
        LP{ { e_direction::w, SP{ .what = e_outdoor_job::tobacco,
                                  .quantity = 3 } } } );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 7,
                 .raw_consumed_theoretical     = 12,
                 .raw_delta_theoretical        = -5,
                 .raw_consumed_actual          = 7,
                 .raw_delta_final              = 0,
                 .product_produced_theoretical = 18,
                 .product_produced_actual      = 11,
                 .product_delta_final          = 11,
             } );
  }

  SECTION(
      "tobacco center square, one tobacco planter, with master "
      "tobacconist, some tobacco in store, no cigars in store, "
      "cigar factory" ) {
    Colony& colony = W.add_colony( W.kGrasslandTile );
    colony.buildings[e_colony_building::cigar_factory] = true;
    colony.commodities[e_commodity::tobacco]           = 2;
    colony.commodities[e_commodity::cigars]            = 0;
    W.add_unit_outdoors( colony.id, e_direction::w,
                         e_outdoor_job::tobacco,
                         e_unit_type::free_colonist );
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::master_tobacconist );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE(
        pr.land_production ==
        LP{ { e_direction::w, SP{ .what = e_outdoor_job::tobacco,
                                  .quantity = 3 } } } );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 7,
                 .raw_consumed_theoretical     = 12,
                 .raw_delta_theoretical        = -5,
                 .raw_consumed_actual          = 9,
                 .raw_delta_final              = -2,
                 .product_produced_theoretical = 18,
                 .product_produced_actual      = 14,
                 .product_delta_final          = 14,
             } );
  }

  SECTION(
      "tobacco center square, one expert tobacco planter, with "
      "master tobacconist, no tobacco in store, no cigars in "
      "store, cigar factory" ) {
    Colony& colony = W.add_colony( W.kGrasslandTile );
    colony.buildings[e_colony_building::cigar_factory] = true;
    colony.commodities[e_commodity::tobacco]           = 0;
    colony.commodities[e_commodity::cigars]            = 0;
    W.add_unit_outdoors( colony.id, e_direction::w,
                         e_outdoor_job::tobacco,
                         e_unit_type::expert_tobacco_planter );
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::master_tobacconist );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE(
        pr.land_production ==
        LP{ { e_direction::w, SP{ .what = e_outdoor_job::tobacco,
                                  .quantity = 6 } } } );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 10,
                 .raw_consumed_theoretical     = 12,
                 .raw_delta_theoretical        = -2,
                 .raw_consumed_actual          = 10,
                 .raw_delta_final              = 0,
                 .product_produced_theoretical = 18,
                 .product_produced_actual      = 15,
                 .product_delta_final          = 15,
             } );
  }

  SECTION(
      "tobacco center square, one expert tobacco planter, one "
      "tobacco planter, with master tobacconist, no tobacco in "
      "store, no cigars in store, cigar factory" ) {
    Colony& colony = W.add_colony( W.kGrasslandTile );
    colony.buildings[e_colony_building::cigar_factory] = true;
    colony.commodities[e_commodity::tobacco]           = 0;
    colony.commodities[e_commodity::cigars]            = 0;
    W.add_unit_outdoors( colony.id, e_direction::w,
                         e_outdoor_job::tobacco,
                         e_unit_type::expert_tobacco_planter );
    W.add_unit_outdoors( colony.id, e_direction::e,
                         e_outdoor_job::tobacco,
                         e_unit_type::petty_criminal );
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::master_tobacconist );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE(
        pr.land_production ==
        LP{ { e_direction::w, SP{ .what = e_outdoor_job::tobacco,
                                  .quantity = 6 } },
            { e_direction::e, SP{ .what = e_outdoor_job::tobacco,
                                  .quantity = 3 } } } );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 13,
                 .raw_consumed_theoretical     = 12,
                 .raw_delta_theoretical        = 1,
                 .raw_consumed_actual          = 12,
                 .raw_delta_final              = 1,
                 .product_produced_theoretical = 18,
                 .product_produced_actual      = 18,
                 .product_delta_final          = 18,
             } );
  }

  SECTION(
      "tobacco center square, one expert tobacco planter, one "
      "tobacco planter, with master tobacconist, full tobacco "
      "in store, no cigars in store, cigar factory" ) {
    Colony& colony = W.add_colony( W.kGrasslandTile );
    colony.buildings[e_colony_building::cigar_factory] = true;
    colony.commodities[e_commodity::tobacco]           = 100;
    colony.commodities[e_commodity::cigars]            = 0;
    W.add_unit_outdoors( colony.id, e_direction::w,
                         e_outdoor_job::tobacco,
                         e_unit_type::expert_tobacco_planter );
    W.add_unit_outdoors( colony.id, e_direction::e,
                         e_outdoor_job::tobacco,
                         e_unit_type::petty_criminal );
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::master_tobacconist );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE(
        pr.land_production ==
        LP{ { e_direction::w, SP{ .what = e_outdoor_job::tobacco,
                                  .quantity = 6 } },
            { e_direction::e, SP{ .what = e_outdoor_job::tobacco,
                                  .quantity = 3 } } } );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 13,
                 .raw_consumed_theoretical     = 12,
                 .raw_delta_theoretical        = 1,
                 .raw_consumed_actual          = 12,
                 .raw_delta_final              = 0,
                 .product_produced_theoretical = 18,
                 .product_produced_actual      = 18,
                 .product_delta_final          = 18,
             } );
  }

  SECTION(
      "tobacco center square, one expert tobacco planter, one "
      "tobacco planter, two master tobacconists, full tobacco "
      "in store, no cigars in store, cigar factory" ) {
    Colony& colony = W.add_colony( W.kGrasslandTile );
    colony.buildings[e_colony_building::cigar_factory] = true;
    colony.commodities[e_commodity::tobacco]           = 100;
    colony.commodities[e_commodity::cigars]            = 0;
    W.add_unit_outdoors( colony.id, e_direction::w,
                         e_outdoor_job::tobacco,
                         e_unit_type::expert_tobacco_planter );
    W.add_unit_outdoors( colony.id, e_direction::e,
                         e_outdoor_job::tobacco,
                         e_unit_type::petty_criminal );
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::master_tobacconist );
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::master_tobacconist );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE(
        pr.land_production ==
        LP{ { e_direction::w, SP{ .what = e_outdoor_job::tobacco,
                                  .quantity = 6 } },
            { e_direction::e, SP{ .what = e_outdoor_job::tobacco,
                                  .quantity = 3 } } } );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 13,
                 .raw_consumed_theoretical     = 24,
                 .raw_delta_theoretical        = -11,
                 .raw_consumed_actual          = 24,
                 .raw_delta_final              = -11,
                 .product_produced_theoretical = 36,
                 .product_produced_actual      = 36,
                 .product_delta_final          = 36,
             } );
  }

  SECTION(
      "tobacco center square, one expert tobacco planter, one "
      "tobacco planter, two master tobacconists, some tobacco "
      "in store, no cigars in store, cigar factory" ) {
    Colony& colony = W.add_colony( W.kGrasslandTile );
    colony.buildings[e_colony_building::cigar_factory] = true;
    colony.commodities[e_commodity::tobacco]           = 2;
    colony.commodities[e_commodity::cigars]            = 0;
    W.add_unit_outdoors( colony.id, e_direction::w,
                         e_outdoor_job::tobacco,
                         e_unit_type::expert_tobacco_planter );
    W.add_unit_outdoors( colony.id, e_direction::e,
                         e_outdoor_job::tobacco,
                         e_unit_type::petty_criminal );
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::master_tobacconist );
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::master_tobacconist );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE(
        pr.land_production ==
        LP{ { e_direction::w, SP{ .what = e_outdoor_job::tobacco,
                                  .quantity = 6 } },
            { e_direction::e, SP{ .what = e_outdoor_job::tobacco,
                                  .quantity = 3 } } } );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 13,
                 .raw_consumed_theoretical     = 24,
                 .raw_delta_theoretical        = -11,
                 .raw_consumed_actual          = 15,
                 .raw_delta_final              = -2,
                 .product_produced_theoretical = 36,
                 .product_produced_actual      = 23,
                 .product_delta_final          = 23,
             } );
  }

  SECTION(
      "tobacco center square, one expert tobacco planter, one "
      "tobacco planter, two master tobacconists, some tobacco "
      "in store, cigars almost full, cigar factory" ) {
    Colony& colony = W.add_colony( W.kGrasslandTile );
    colony.buildings[e_colony_building::cigar_factory] = true;
    colony.commodities[e_commodity::tobacco]           = 2;
    colony.commodities[e_commodity::cigars]            = 90;
    W.add_unit_outdoors( colony.id, e_direction::w,
                         e_outdoor_job::tobacco,
                         e_unit_type::expert_tobacco_planter );
    W.add_unit_outdoors( colony.id, e_direction::e,
                         e_outdoor_job::tobacco,
                         e_unit_type::petty_criminal );
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::master_tobacconist );
    W.add_unit_indoors( colony.id, e_indoor_job::cigars,
                        e_unit_type::master_tobacconist );
    ColonyProduction pr =
        production_for_colony( W.ss(), colony );
    REQUIRE(
        pr.land_production ==
        LP{ { e_direction::w, SP{ .what = e_outdoor_job::tobacco,
                                  .quantity = 6 } },
            { e_direction::e, SP{ .what = e_outdoor_job::tobacco,
                                  .quantity = 3 } } } );
    REQUIRE(
        pr.center_extra_production ==
        SP{ .what = e_outdoor_job::tobacco, .quantity = 4 } );
    REQUIRE( pr.tobacco_cigars ==
             RMP{
                 .raw_produced                 = 13,
                 .raw_consumed_theoretical     = 24,
                 .raw_delta_theoretical        = -11,
                 .raw_consumed_actual          = 15,
                 .raw_delta_final              = -2,
                 .product_produced_theoretical = 36,
                 .product_produced_actual      = 23,
                 .product_delta_final          = 10,
             } );
  }
}

TEST_CASE( "[production] food/horses" ) {
  World W;
  W.create_default_map();
  Colony& colony = W.add_colony( Coord{ .x = 1, .y = 1 } );
  REQUIRE( colony_population( colony ) == 0 );

  SECTION( "no units no horses/arctic" ) {
    // TODO: implement center production first.
  }

  SECTION( "no units no horses" ) {
    // TODO: implement center production first.
  }

  SECTION( "one farmer no horses" ) {
    // TODO: implement center production first.
  }

  SECTION( "one fisherman no horses" ) {
    // TODO: implement center production first.
  }

  SECTION( "one farmer breaking even" ) {
    // TODO: implement center production first.
  }

  SECTION( "one farmer breaking even, horses=2" ) {
    // TODO: implement center production first.
  }

  SECTION( "one farmer surplus=1, horses=1" ) {
    // TODO: implement center production first.
  }

  SECTION( "one farmer surplus=1, horses=2" ) {
    // TODO: implement center production first.
  }

  SECTION( "one fisherman surplus=3, horses=0" ) {
    // TODO: implement center production first.
  }

  SECTION( "one farmer surplus=3, horses=25" ) {
    // TODO: implement center production first.
  }

  SECTION( "one farmer surplus=3, horses=max" ) {
    // TODO: implement center production first.
  }

  SECTION( "one farmer surplus=3, horses=25, +stable" ) {
    // TODO: implement center production first.
  }

  SECTION( "deficit no starve, horses=0" ) {
    // TODO: implement center production first.
  }

  SECTION( "deficit no starve, horses=50" ) {
    // TODO: implement center production first.
  }

  SECTION( "deficit and starve" ) {
    // TODO: implement center production first.
  }

  SECTION( "deficit and starve, horses=50" ) {
    // TODO: implement center production first.
  }

  SECTION( "two farmers, two fisherman, surplus" ) {
    // TODO: implement center production first.
  }

  SECTION(
      "one farmer, surplus=2, warehouse expansion, horses=100, "
      "+stable" ) {
    // TODO: implement center production first.
  }
}

} // namespace
} // namespace rn
