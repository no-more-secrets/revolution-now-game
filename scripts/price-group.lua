--[[ ------------------------------------------------------------
|
| price-group.lua
|
| Project: Revolution Now
|
| Created by dsicilia on 2022-08-27.
|
| Description: Simulation of the processed goods price group
|              model from the original game.
|
--]] ------------------------------------------------------------
local GOODS = { 'rum', 'cigars', 'cloth', 'coats' }
local EQ_PRICES = { rum=10, cigars=13, cloth=10, coats=9 }
local INITIAL_GOLD = 0
-- By initializing this to 'e', which means "evolve", the user
-- can just run the program and immediately start hitting <enter>
-- to start evolving the initial prices without buying or selling
-- anything.
local INITIAL_CMD = 'e'

-----------------------------------------------------------------
-- State
-----------------------------------------------------------------
local prices = { rum=0, cigars=0, cloth=0, coats=0 }

local volumes = { rum=0, cigars=0, cloth=0, coats=0 }

local num_turns = 0

local gold = INITIAL_GOLD

local last_cmd = INITIAL_CMD

-----------------------------------------------------------------
-- Model
-----------------------------------------------------------------
local RF = 4 * 100
local VOL = 1
local EQ_SUM_MIN = 43
local EQ_SUM_MAX = 43

local params = {
  -- LuaFormatter off
  rum    = { rise=RF, fall=RF, attrition=-10, volatility=VOL },
  cigars = { rise=RF, fall=RF, attrition=-10, volatility=VOL },
  cloth  = { rise=RF, fall=RF, attrition=-10, volatility=VOL },
  coats  = { rise=RF, fall=RF, attrition=-10, volatility=VOL },
  -- LuaFormatter on
}

local function price_sum()
  local sum = 0
  for good, price in pairs( prices ) do sum = sum + price end
  return sum
end

local function target_price( good )
  assert( good )
  local p = params[good]
  local v = volumes[good]
  local eq = EQ_PRICES[good]
  local res = eq
  if v > 0 then res = res - (v // p.rise) end
  -- if v > 0 then res = res - (v // 100) end
  -- The // integer division operator does weird things when the
  -- numerator is negative, so we'll make it positive.
  if v < 0 then res = res + ((-v) // p.fall) end
  -- if v < 0 then res = res + ((-v) // 100) end
  return res
end

local function update_price( good )
  assert( good )
  local target = target_price( good )
  local current = prices[good]
  local new_price = current
  if target > current then
    new_price = current + 1
  elseif target < current then
    new_price = current - 1
  end
  if new_price < 0 then new_price = 0 end
  if new_price > 19 then new_price = 19 end
  prices[good] = new_price
end

-- One round of price recovery for one good. Need to pass in the
-- current prices sum here because we want to use the one com-
-- puted before any of the goods begin their recovery.
local function recover( sum, good )
  local p = params[good]
  local bought_more_than_sold = (volumes[good] < 0)
  local price_avg_too_high = (sum > EQ_SUM_MAX)
  local sold_more_than_bought = (volumes[good] > 0)
  local price_avg_too_low = (sum < EQ_SUM_MIN)
  if price_avg_too_high and bought_more_than_sold then
    volumes[good] = volumes[good] + p.fall
  elseif price_avg_too_low and sold_more_than_bought then
    volumes[good] = volumes[good] - p.rise
  end
  update_price( good )
end

-- Evolve the goods in the way that is done when starting a new
-- turn. But note that this will not simulate interactions with
-- foreign markets because there are none in this simulation.
local function evolve()
  local sum = price_sum()
  -- Recovery model.
  for _, good in ipairs( GOODS ) do recover( sum, good ) end
end

local function buy( good_to_buy )
  -- TODO
end

local function sell( good_to_sell )
  local p = params[good_to_sell]
  local quantity = 100
  gold = gold + quantity * prices[good_to_sell]
  local sum = price_sum()
  local volume = quantity * (1 << p.volatility)

  volumes[good_to_sell] = volumes[good_to_sell] + volume

  local delta = EQ_PRICES[good_to_sell] - prices[good_to_sell]
  for _, good in ipairs( GOODS ) do
    volumes[good] = volumes[good] -
                        ((p.rise // 100) * delta * quantity) //
                        20
  end

  update_price( good_to_sell )
end

-----------------------------------------------------------------
-- User Interaction
-----------------------------------------------------------------
local prompt = [[
  b1 - buy  rum     s1 - sell rum
  b2 - buy  cigars  s2 - sell cigars
  b3 - buy  cloth   s3 - sell cloth
  b4 - buy  coats   s4 - sell coats
  ba - buy  all     sa - sell all
  e to evolve one turn.
  r to reset to starting state
  <enter> to repeat last command.

> ]]

local function reset()
  for _, good in ipairs( GOODS ) do
    prices[good] = EQ_PRICES[good]
    volumes[good] = 0
  end
  num_turns = 0
  gold = INITIAL_GOLD
  last_cmd = INITIAL_CMD
end

local chart = [[
  turns: %d
  gold:  %d
  ----------------------------------------------------------
  |     #1      |      #2      |     #3      |     #4      |
  ----------------------------------------------------------
  |  %6d     |  %6d      |  %6d     |  %6d     | <- net volume in europe
  ----------------------------------------------------------
  |     Rum     |    Cigars    |    Cloth    |    Coats    |
  |    %2d/%2d    |    %2d/%2d     |    %2d/%2d    |    %2d/%2d    | <- bid prices
  ----------------------------------------------------------
  price sum: %d
  last cmd:  %s
]]

local function clear_screen()
  -- 27 is '\033'.
  io.write( string.char( 27 ) .. '[2J' ) -- clear screen
  io.write( string.char( 27 ) .. '[H' ) -- move cursor to upper left.
end

local function char( str, idx ) return
    string.sub( str, idx, idx ) end

local function loop()
  clear_screen()
  print( string.format( chart, num_turns, gold, volumes.rum,
                        volumes.cigars, volumes.cloth,
                        volumes.coats, prices.rum,
                        prices.rum + 1, prices.cigars,
                        prices.cigars + 1, prices.cloth,
                        prices.cloth + 1, prices.coats,
                        prices.coats + 1, price_sum(),
                        last_cmd or 'none' ) )
  print()
  io.write( prompt )

  local choice = io.read()
  if last_cmd ~= nil and #choice == 0 then choice = last_cmd end
  last_cmd = choice
  if choice == 'e' then
    evolve()
    num_turns = num_turns + 1
    return
  end
  if choice == 'r' then
    reset()
    return
  end
  if #choice ~= 2 then return end
  local what = char( choice, 2 )
  local goods_inputted = {}
  if what == 'a' then
    goods_inputted = GOODS
  else
    local good = GOODS[tonumber( what )]
    if good == nil then return end
    table.insert( goods_inputted, good )
  end
  local buy_sell = char( choice, 1 )
  print()
  for _, good in ipairs( goods_inputted ) do
    if buy_sell == 'b' then
      buy( good )
    elseif buy_sell == 's' then
      sell( good )
    end
  end
  print()
end

reset()
while true do loop() end