--[[-------------------------------------------------------------
--                  Creates the editor session.
--]] -------------------------------------------------------------
local M = {}

-----------------------------------------------------------------
-- Imports.
-----------------------------------------------------------------
local contents = require( 'ide.contents' )
local util = require( 'ide.util' )
local module_cpp = require( 'ide.module-cpp' )

-----------------------------------------------------------------
-- Aliases.
-----------------------------------------------------------------
local call = vim.call
local keymap = vim.keymap
local format = string.format
local match = string.match
local exists = util.exists
local map = util.map

-----------------------------------------------------------------
-- Layout Functions.
-----------------------------------------------------------------
local module_types = {
  require( 'ide.module-config' ), --
  require( 'ide.module-main' ), --
  require( 'ide.module-shader' ) --
}

-- This will increase the cmd height each time we log something
-- in order to avoid getting 'Press ENTER to continue...'.
local function cmdlog( fmt, ... )
  vim.o.cmdheight = vim.o.cmdheight + 1
  print( format( fmt, ... ) )
end

local function open_module( stem )
  cmdlog( '  - %s', stem )
  for _, module_type in ipairs( module_types ) do
    if module_type.matches( stem ) then
      return module_type.open( stem )
    end
  end
  -- If the module doesn't exist as any recognized type then it
  -- is likely that we want to create a new C++ module.
  module_cpp.open( stem )
end

local function open_module_with_input()
  open_module( util.input( 'Enter name: ' ) )
end

local function create_tabs()
  cmdlog( 'opening modules...' )
  map( open_module, contents.stems )

  vim.cmd[[tabdo set cmdheight=1]]
  vim.cmd[[tabdo wincmd =]]

  -- When nvim supports it.
  -- vim.cmd[[tabdo set cmdheight=0]]
end

-----------------------------------------------------------------
-- Key maps.
-----------------------------------------------------------------
local function nmap( keys, func )
  -- This will by default have remap=false.
  keymap.set( 'n', keys, func, { silent=true } )
end

local function mappings()
  nmap( '<C-p>', open_module_with_input )
  -- Add more here...
end

-----------------------------------------------------------------
-- Main.
-----------------------------------------------------------------
function M.main()
  vim.cmd[[source .vimrc]]

  -- Create key mappings.
  mappings()

  -- Creates all of the tabs/splits.
  create_tabs()
end

return M