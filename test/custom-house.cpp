/****************************************************************
**custom-house.cpp
*
* Project: Revolution Now
*
* Created by dsicilia on 2022-10-22.
*
* Description: Unit tests for the src/custom-house.* module.
*
*****************************************************************/
#include "test/testing.hpp"

// Under test.
#include "src/custom-house.hpp"

// Testing
#include "test/fake/world.hpp"
#include "test/mocks/igui.hpp"

// Revolution Now
#include "src/market.hpp"

// config
#include "config/market.rds.hpp"

// ss
#include "ss/player.rds.hpp"
#include "ss/players.rds.hpp"
#include "ss/ref.hpp"
#include "ss/settings.rds.hpp"

// refl
#include "refl/to-str.hpp"

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
  World() : Base() {
    create_default_map();
    add_player( e_nation::dutch );
    add_player( e_nation::french );
    set_default_player( e_nation::dutch );
  }

  void create_default_map() {
    MapSquare const   L = make_grassland();
    vector<MapSquare> tiles{ L };
    build_map( std::move( tiles ), 1 );
  }
};

/****************************************************************
** Test Cases
*****************************************************************/
TEST_CASE( "[custom-house] set_default_custom_house_state" ) {
  World   W;
  Colony& colony = W.add_colony_with_new_unit( Coord{} );

  // Sanity check.
  for( auto& [comm, on] : colony.custom_house ) {
    INFO( fmt::format( "commodity: {}", comm ) );
    REQUIRE( !on );
  }

  set_default_custom_house_state( colony );

  REQUIRE_FALSE( colony.custom_house[e_commodity::food] );
  REQUIRE( colony.custom_house[e_commodity::sugar] );
  REQUIRE( colony.custom_house[e_commodity::tobacco] );
  REQUIRE( colony.custom_house[e_commodity::cotton] );
  REQUIRE( colony.custom_house[e_commodity::fur] );
  REQUIRE_FALSE( colony.custom_house[e_commodity::lumber] );
  REQUIRE( colony.custom_house[e_commodity::ore] );
  REQUIRE( colony.custom_house[e_commodity::silver] );
  REQUIRE_FALSE( colony.custom_house[e_commodity::horses] );
  REQUIRE( colony.custom_house[e_commodity::rum] );
  REQUIRE( colony.custom_house[e_commodity::cigars] );
  REQUIRE( colony.custom_house[e_commodity::cloth] );
  REQUIRE( colony.custom_house[e_commodity::coats] );
  REQUIRE_FALSE( colony.custom_house[e_commodity::trade_goods] );
  REQUIRE_FALSE( colony.custom_house[e_commodity::tools] );
  REQUIRE_FALSE( colony.custom_house[e_commodity::muskets] );
}

TEST_CASE( "[custom-house] apply_custom_house_sales" ) {
  World   W;
  Player& player = W.default_player();
  Colony& colony = W.add_colony_with_new_unit( Coord{} );
  vector<CustomHouseSale> sales;

  auto f = [&] {
    apply_custom_house_sales( W.ss(), player, colony, sales );
  };

  player.money = 1000;

  for( e_commodity comm : refl::enum_values<e_commodity> ) {
    colony.commodities[comm]                            = 150;
    player.old_world.market.commodities[comm].bid_price = 10;
  }

  W.player( e_nation::dutch )
      .old_world.market.commodities[e_commodity::sugar]
      .boycott = true;
  W.player( e_nation::french )
      .old_world.market.commodities[e_commodity::sugar]
      .boycott = true;

  sales = {
      CustomHouseSale{
          .invoice =
              Invoice{
                  .what = Commodity{ .type = e_commodity::sugar,
                                     .quantity = 100 },
                  .money_delta_before_taxes = 100,
                  .tax_rate                 = 7,
                  .tax_amount               = 7,
                  .money_delta_final        = 93,
                  .player_volume_delta      = 100,
                  .intrinsic_volume_delta =
                      { { e_nation::dutch, 1 },
                        { e_nation::french, 1 } },
                  .global_intrinsic_volume_deltas =
                      { { e_commodity::sugar, 2 } },
                  .price_change = create_price_change(
                      W.default_player(), e_commodity::sugar,
                      /*price_change=*/0 ) } },
      CustomHouseSale{
          .invoice = Invoice{
              .what = Commodity{ .type     = e_commodity::rum,
                                 .quantity = 200 },
              .money_delta_before_taxes = 200,
              .tax_rate                 = 14,
              .tax_amount               = 14,
              .money_delta_final        = 186,
              .player_volume_delta      = 200,
              .intrinsic_volume_delta = { { e_nation::dutch, 2 },
                                          { e_nation::french,
                                            2 } },
              .global_intrinsic_volume_deltas =
                  { { e_commodity::rum, 4 } },
              .price_change = create_price_change(
                  W.default_player(), e_commodity::sugar,
                  /*price_change=*/0 ) } } };
  f();
  REQUIRE( player.money == 1000 + 186 + 93 );
  for( e_commodity comm : refl::enum_values<e_commodity> ) {
    INFO( fmt::format( "comm: {}", comm ) );
    if( comm == e_commodity::sugar ) {
      REQUIRE( colony.commodities[comm] == 50 );
      REQUIRE( W.player( e_nation::french )
                   .old_world.market.commodities[comm]
                   .intrinsic_volume == 1 );
      REQUIRE( W.player( e_nation::dutch )
                   .old_world.market.commodities[comm]
                   .intrinsic_volume == 1 );
      REQUIRE( W.players()
                   .global_market_state.commodities[comm]
                   .intrinsic_volume == 2 );
      REQUIRE(
          player.old_world.market.commodities[comm].bid_price ==
          10 );
    } else if( comm == e_commodity::rum ) {
      REQUIRE( colony.commodities[comm] == 50 );
      REQUIRE( W.player( e_nation::french )
                   .old_world.market.commodities[comm]
                   .intrinsic_volume == 2 );
      REQUIRE( W.player( e_nation::dutch )
                   .old_world.market.commodities[comm]
                   .intrinsic_volume == 2 );
      REQUIRE( W.players()
                   .global_market_state.commodities[comm]
                   .intrinsic_volume == 4 );
      REQUIRE(
          player.old_world.market.commodities[comm].bid_price ==
          10 );
    } else {
      REQUIRE( colony.commodities[comm] == 150 );
      REQUIRE( W.player( e_nation::french )
                   .old_world.market.commodities[comm]
                   .intrinsic_volume == 0 );
      REQUIRE( W.player( e_nation::dutch )
                   .old_world.market.commodities[comm]
                   .intrinsic_volume == 0 );
      REQUIRE( W.players()
                   .global_market_state.commodities[comm]
                   .intrinsic_volume == 0 );
      REQUIRE(
          player.old_world.market.commodities[comm].bid_price ==
          10 );
    }
  }
}

TEST_CASE( "[custom-house] compute_custom_house_sales" ) {
  World W;
  W.settings().difficulty = e_difficulty::conquistador;
  Player& dutch           = W.dutch();
  Player& french          = W.french();
  Colony& colony = W.add_colony_with_new_unit( Coord{} );
  vector<CustomHouseSale> expected;
  colony.buildings[e_colony_building::custom_house] = true;
  for( e_commodity comm : refl::enum_values<e_commodity> ) {
    colony.commodities[comm] = 200;
    dutch.old_world.market.commodities[comm].bid_price =
        config_market.price_behavior[comm]
            .price_limits.bid_price_max;
  }
  colony.commodities[e_commodity::cloth]   = 100;
  colony.commodities[e_commodity::muskets] = 99;
  dutch.old_world.taxes.tax_rate           = 10;

  dutch.human  = true;
  french.human = true;

  W.players()
      .global_market_state.commodities[e_commodity::cloth]
      .intrinsic_volume = 1000;

  SECTION( "independence not declared" ) {
    colony.custom_house[e_commodity::fur]     = true;
    colony.custom_house[e_commodity::ore]     = true;
    colony.custom_house[e_commodity::cloth]   = true;
    colony.custom_house[e_commodity::muskets] = true;
    dutch.revolution_status = e_revolution_status::not_declared;
    vector<CustomHouseSale> res =
        compute_custom_house_sales( W.ss(), dutch, colony );
    expected =
        {
            CustomHouseSale{
                .invoice =
                    Invoice{
                        .what =
                            Commodity{ .type = e_commodity::fur,
                                       .quantity = 150 },
                        .money_delta_before_taxes = 150 * 5,
                        .tax_rate                 = 10,
                        .tax_amount               = 75,
                        .money_delta_final        = 675,
                        .player_volume_delta      = 150,
                        .intrinsic_volume_delta =
                            { { e_nation::dutch, 199 },
                              { e_nation::french, 300 } },
                        .global_intrinsic_volume_deltas = {
                            /*only processed goods*/ },
                        .price_change = create_price_change(
                            dutch, e_commodity::fur,
                            /*price_change=*/0 ) },
            },
            CustomHouseSale{
                .invoice =
                    Invoice{
                        .what =
                            Commodity{ .type = e_commodity::ore,
                                       .quantity = 150 },
                        .money_delta_before_taxes = 150 * 5,
                        .tax_rate                 = 10,
                        .tax_amount               = 75,
                        .money_delta_final        = 675,
                        .player_volume_delta      = 150,
                        .intrinsic_volume_delta =
                            { { e_nation::dutch, 99 },
                              { e_nation::french, 150 } },
                        .global_intrinsic_volume_deltas = {
                            /*only processed goods*/ },
                        .price_change = create_price_change(
                            dutch, e_commodity::ore,
                            /*price_change=*/0 ) },
            },
            CustomHouseSale{
                .invoice =
                    Invoice{ .what =
                                 Commodity{
                                     .type = e_commodity::cloth,
                                     .quantity = 50 },
                             .money_delta_before_taxes = 50 * 19,
                             .tax_rate                 = 10,
                             .tax_amount               = 95,
                             .money_delta_final   = 50 * 19 - 95,
                             .player_volume_delta = 50,
                             .intrinsic_volume_delta = {
                                 /*non-processed goods only*/ },
                             .global_intrinsic_volume_deltas = { /*these don't evolve when the dutch make the trade.*/ },
                             .price_change = create_price_change(
                                 dutch, e_commodity::cloth,
                                 /*price_change=*/0 ) },
            },
        };
    REQUIRE( res == expected );
  }

  SECTION( "declared independence" ) {
    // Charge (in place of tax rate) is 50%.
    dutch.revolution_status = e_revolution_status::declared;
    // One should be enough here.
    colony.custom_house[e_commodity::fur] = true;
    vector<CustomHouseSale> res =
        compute_custom_house_sales( W.ss(), dutch, colony );
    expected = {
        CustomHouseSale{
            .invoice =
                Invoice{
                    .what = Commodity{ .type = e_commodity::fur,
                                       .quantity = 150 },
                    .money_delta_before_taxes = 150 * 5,
                    .tax_rate                 = 50,
                    .tax_amount               = 375,
                    .money_delta_final        = 150 * 5 - 375,
                    .player_volume_delta      = 150,
                    .intrinsic_volume_delta =
                        { { e_nation::dutch, 199 },
                          { e_nation::french, 300 } },
                    .global_intrinsic_volume_deltas = {
                        /*only processed goods*/ },
                    .price_change = create_price_change(
                        dutch, e_commodity::fur,
                        /*price_change=*/0 ) },
        },
    };
    REQUIRE( res == expected );
  }
}

} // namespace
} // namespace rn
