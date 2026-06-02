------------------------------------------------------------------------------
-- DynASM BitOp compatibility module.
--
-- Copyright (C) 2005-2026 Mike Pall, pkhead. All rights reserved.
-- See dynasm.lua for full copyright notice.
------------------------------------------------------------------------------

local function bitop_test(bit)
  -- Test cases for bit operations library.
  local vb = {
    0, 1, -1, 2, -2, 0x12345678, 0x87654321,
    0x33333333, 0x77777777, 0x55aa55aa, 0xaa55aa55,
    0x7fffffff, 0x80000000, 0xffffffff
  }

  local function cksum(name, s, r)
    local z = 0
    for i=1,#s do z = (z + string.byte(s, i)*i) % 2147483629 end
    if z ~= r then
      error("bit."..name.." test failed (got "..z..", expected "..r..")", 0)
    end
  end

  local function check_unop(name, r)
    local f = bit[name]
    local s = ""
    if pcall(f) or pcall(f, "z") or pcall(f, true) then
      error("bit."..name.." fails to detect argument errors", 0)
    end
    for _,x in ipairs(vb) do s = s..","..tostring(f(x)) end
    cksum(name, s, r)
  end

  local function check_binop(name, r)
    local f = bit[name]
    local s = ""
    if pcall(f) or pcall(f, "z") or pcall(f, true) then
      error("bit."..name.." fails to detect argument errors", 0)
    end
    for _,x in ipairs(vb) do
      for _,y in ipairs(vb) do s = s..","..tostring(f(x, y)) end
    end
    cksum(name, s, r)
  end

  local function check_binop_range(name, r, yb, ye)
    local f = bit[name]
    local s = ""
    if pcall(f) or pcall(f, "z") or pcall(f, true) or pcall(f, 1, true) then
      error("bit."..name.." fails to detect argument errors", 0)
    end
    for _,x in ipairs(vb) do
      for y=yb,ye do s = s..","..tostring(f(x, y)) end
    end
    cksum(name, s, r)
  end

  local function check_shift(name, r)
    check_binop_range(name, r, 0, 31)
  end

  -- Minimal sanity checks.
  assert(0x7fffffff == 2147483647, "broken hex literals")
  assert(0xffffffff == -1 or 0xffffffff == 2^32-1, "broken hex literals")
  assert(tostring(-1) == "-1", "broken tostring()")
  assert(tostring(0xffffffff) == "-1" or tostring(0xffffffff) == "4294967295", "broken tostring()")

  -- Basic argument processing.
  assert(bit.tobit(1) == 1)
  assert(bit.band(1) == 1)
  assert(bit.bxor(1,2) == 3)
  assert(bit.bor(1,2,4,8,16,32,64,128) == 255)

  -- Apply operations to test vectors and compare checksums.
  check_unop("tobit", 277312)
  check_unop("bnot", 287870)
  check_unop("bswap", 307611)

  check_binop("band", 41206764)
  check_binop("bor", 51253663)
  check_binop("bxor", 79322427)

  check_shift("lshift", 325260344)
  check_shift("rshift", 139061800)
  check_shift("arshift", 111364720)
  check_shift("rol", 302401155)
  check_shift("ror", 302316761)

  check_binop_range("tohex", 47880306, -8, 8)

  return bit
end

if bit then return bitop_test(bit) end

if require then
	local s, v
	s, v = pcall(require, "bit")
	if s then return bitop_test(v) end

  local b32
  s, b32 = pcall(require, "bit32")
  if s then    
    local band = b32.band
    local bor = b32.bor
    local bnot = b32.bnot
    local bxor = b32.bxor
    local lshift = b32.lshift
    local rshift = b32.rshift
    local arshift = b32.arshift
    local lrotate = b32.lrotate
    local rrotate = b32.rrotate
    
    local function asi32(x)
      return band(math.floor(x + 0.5), 0xffffffff)
    end

    local function tobit(x)
      -- sign-extend integer
      local m = 0x80000000
      return bxor(band(x, 0xffffffff), m) - m
    end

    return bitop_test({
      tobit = tobit,
      tohex = function(x, n)
        if n == 0 then return "" end
        if n == nil then n = 8 end
        
        local t
        if n > 0 then
          t = "x"
        else
          t = "X"
          n = -n
        end
        
        return string.sub(string.format("%."..n..t, asi32(x)), -n)
      end,
      bnot = function(n) return tobit(bnot(n)) end,
      band = function(...)
        if (...) == nil then
          error("invalid number input to bitop", 2)  
        end
        for i=1, select("#", ...) do
          if type(select(i, ...)) ~= "number" then
            error("invalid number input to bitop", 2)
          end
        end

        return tobit(band(...))
      end,
      bor = function(...)
        if (...) == nil then
          error("invalid number input to bitop", 2)  
        end
        for i=1, select("#", ...) do
          if type(select(i, ...)) ~= "number" then
            error("invalid number input to bitop", 2)
          end
        end

        return tobit(bor(...))
      end,
      bxor = function(...)
        if (...) == nil then
          error("invalid number input to bitop", 2)  
        end
        for i=1, select("#", ...) do
          if type(select(i, ...)) ~= "number" then
            error("invalid number input to bitop", 2)
          end
        end

        return tobit(bxor(...))
      end,
      lshift = function(x, n)
        return tobit(lshift(x, band(n, 31)))
      end,
      rshift = function(x, n)
        return tobit(rshift(asi32(x), band(n, 31)))
      end,
      arshift = function(x, n)
        return tobit(arshift(asi32(x), band(n)))
      end,
      rol = function(x, n)
        return tobit(lrotate(asi32(x), band(n)))
      end,
      ror = function(x, n)
        return tobit(rrotate(asi32(x), band(n)))
      end,
      bswap = function(x)
        x = asi32(x)
        local b1, b2, b3, b4 =
          band(x, 255),
          band(rshift(x, 8), 255),
          band(rshift(x, 16), 255),
          band(rshift(x, 24), 255)
          
        return tobit(bor(lshift(b1, 24), lshift(b2, 16), lshift(b3, 8), b4))
      end
    })
  end
end

-- Neither bit or bit32 exists. Could either be an old version of Lua, or 5.4+,
-- which removes the bit32 module in favor of its built-in bitwise operators.
-- Assume the latter. Also load and run it as a dynamically loaded string
-- so that Lua versions older than 5.3 don't generate a syntax error when
-- loading this file.
return bitop_test((loadstring or load)([[
local U32_MAX = 0xffffffff

local function asi32(x)
  if math.type(x) ~= "integer" then
    return math.tointeger(math.floor(x + 0.5)) & U32_MAX
  else
    return x & U32_MAX
  end
end

local function tobit(x)
  if math.type(x) ~= "integer" then
    x = math.tointeger(math.floor(x + 0.5))
  end

  -- sign-extend integer
  local m = 0x80000000
  return ((x & U32_MAX) ~ m) - m
end

return {
	tobit = tobit,
	tohex = function(x, n)
    if n == 0 then return "" end
    if n == nil then n = 8 end
    
    local t
    if n > 0 then
      t = "x"
    else
      t = "X"
      n = -n
    end
    
    return string.sub(string.format("%."..n..t, asi32(x)), -n)
	end,
	bnot = function(n) return ~tobit(n) end,
	band = function(...)
    local v = ...
    for i=2, select("#", ...) do
      v = v & (select(i, ...))
		end
		return tobit(v)
	end,
	bor = function(...)
		local v = ...
		for i=2, select("#", ...) do
			v = v | (select(i, ...))
		end
		return tobit(v)
	end,
	bxor = function(...)
		local v = ...
		for i=2, select("#", ...) do
			v = v ~ (select(i, ...))
		end
		return tobit(v)
	end,
	lshift = function(x, n)
		return tobit(x << (n & 31))
	end,
	rshift = function(x, n)
		return tobit(asi32(x) >> (n & 31))
	end,
	arshift = function(x, n)
		n = n & 31
    x = asi32(x)
    local m = (x & 0x80000000) >> n
    return tobit( ((x >> n) ~ m) - m )
	end,
	rol = function(x, n)
		n = n & 31
    x = asi32(x)
		return tobit((x << n) | (x >> (32 - n)))
	end,
	ror = function(x, n)
		n = n & 31
    x = asi32(x)
		return tobit((x << (32 - n)) | (x >> n))
	end,
	bswap = function(x)
    x = asi32(x)
		local b1, b2, b3, b4 =
			x & 255,
			(x >> 8) & 255,
			(x >> 16) & 255,
			(x >> 24) & 255
    	
		return tobit((b1 << 24) | (b2 << 16) | (b3 << 8) | b4)
	end
}
]])())