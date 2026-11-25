-- Name:        ddp_check.lua
-- Description: DoDonPachi on the real MAME, played as the port's checks play it, for tests/mame_compare: the
--              picture, the work RAM and the slowdown of MAME's own ddonpach, against the port's
-- Version:     0.3.0 (0.2.0: the inputs on the port's VBLANK, not one later; 0.3.0: waits.txt)
-- Created:     25-11-2026
--
-- license:BSD-3-Clause
--
-- Run from the project's folder with a MAME of 0.254 or later (its Lua API), where ddonpach.zip is in MAME's ROM
-- path, with empty folders for MAME's NVRAM and settings (the ROM set's own EEPROM, as the port's checks start with):
--
--     mkdir build\mame_check\nvram, build\mame_check\cfg
--     mame ddonpach -rompath rom -autoboot_script tools\mame\ddp_check.lua -nothrottle -sound none -window
--          -skip_gameinfo -nvram_directory build\mame_check\nvram -cfg_directory build\mame_check\cfg -noplugin
--          -nocheat
--
-- (mame: the full path of mame.exe, if it isn't on the PATH.) Then: tests/mame_compare build/mame_check
--
-- (-sound none: MAME still runs the sound chip, its interrupt and all, only nothing is played.)
-- It writes into build\mame_check (DDP_OUT to change it):
--     log.txt        a line a VBLANK: "vblank inputs arrived signature" (the inputs given for the next one; arrived 1
--                    when the game came to its wait for a frame since the last VBLANK, 0 when it didn't: slowdown;
--                    the signature a sum of both ships' records and the scores)
--     ram_N.bin      the work RAM ($100000-$10FFFF, 64 KB) at VBLANK N, every DDP_EVERY VBLANKs (5,000)
--     screen_N.bin   MAME's picture then: its screen's pixels as MAME keeps them (32-bit, little endian), with
--                    screen_size.txt
--     waits.txt      a line a frame of the game: "game_frame cycle", the 68000's cycle from the power on (MAME's time
--                    there, at the CPU's own instruction, times its clock) as the game makes its wait ready: when each
--                    frame's work ended, to set beside the port's (tests/slowdown_check's CYCLELOG, mame_compare)
-- and quits at DDP_FRAMES VBLANKs (300,000: both loops, 2-7 and the ending, about 87 minutes of the game; about 10
-- to 20 minutes with -nothrottle). Zip the folder and send it; tests/mame_compare plays the port the same way.
--
-- The player is tests/player.h's, by the VBLANK alone (a coin and Start every 600, firing and sweeping), with player
-- 1 kept from harm from VBLANK 2,500 ($102CCA held at $80, as tools/cref/cref_timed's INVINCIBLE=1 and
-- tests/slowdown_check's): through both loops. MAME's VBLANKs are counted from its first frame; mame_compare finds
-- how they line up with the port's from the frames before the first coin, where nothing is pressed.

local out = os.getenv("DDP_OUT") or "build/mame_check"
local frames = tonumber(os.getenv("DDP_FRAMES") or "300000")
local every = tonumber(os.getenv("DDP_EVERY") or "5000")

local machine = manager.machine
local cpu = machine.devices[":maincpu"]
local space = cpu.spaces["program"]
local screen = nil
for tag, s in pairs(machine.screens) do screen = s; break end

-- ---- the inputs: tests/player.h, by the VBLANK ----
local function player_inputs(frame)
	local cycle = frame % 600
	if cycle >= 100 and cycle < 104 then return 0x0100 end
	if cycle >= 160 and cycle < 164 then return 0x0080 end
	local held = (math.floor(frame / 2) % 2 == 1) and 0x0010 or 0
	if math.floor(frame / 240) % 4 == 3 then held = held | 0x0010 end
	held = held | ((math.floor(frame / 90) % 2 == 1) and 0x0004 or 0x0008)
	if math.floor(frame / 45) % 3 == 0 then held = held | 0x0001 end
	return held
end

-- the board's input bits (cave/machine.h: CAVE_UP 0x01 ... CAVE_COIN 0x100) and MAME's names for them
local wanted = {
	{ 0x0001, "P1 Up" }, { 0x0002, "P1 Down" }, { 0x0004, "P1 Left" }, { 0x0008, "P1 Right" },
	{ 0x0010, "P1 Button 1" }, { 0x0020, "P1 Button 2" }, { 0x0040, "P1 Button 3" },
	{ 0x0080, "1 Player Start" }, { 0x0100, "Coin 1" },
}
local fields = {}
for _, want in ipairs(wanted) do
	for _, port in pairs(machine.ioport.ports) do
		local field = port.fields[want[2]]
		if field then fields[#fields + 1] = { want[1], field }; break end
	end
	if not fields[#fields] or fields[#fields][1] ~= want[1] then
		emu.print_error("ddp_check: no input named \"" .. want[2] .. "\" in this MAME")
		machine:exit()
		return
	end
end

local function set_inputs(held)
	for _, f in ipairs(fields) do
		f[2]:set_value((held & f[1]) ~= 0 and 1 or 0)
	end
end

-- ---- the game's frames: its wait made ready ($10120C bit 0, as tests/slowdown_check's at_wait) ----
local arrived = false
local game_frames = 0
local waits = io.open(out .. "/waits.txt", "w")
local clock = cpu.clock    -- (16,000,000: DoDonPachi's 68000)
-- MAME's time now, in the 68000's cycles from the power on. In a tap it is the running CPU's own, to its instruction
-- (the scheduler's time() while a CPU executes). attotime's seconds and attoseconds, exact (as_double, a cycle or
-- so off this far into a run, if this MAME has no such fields)
local attos_per_cycle = 1000000000000000000 // clock
local function cycle_now()
	local now = machine.time
	local ok, cycle = pcall(function() return now.seconds * clock + now.attoseconds // attos_per_cycle end)
	if ok and type(cycle) == "number" then return cycle end
	return math.floor(now:as_double() * clock + 0.5)
end

local ready_tap = space:install_write_tap(0x10120c, 0x10120d, "ddp_check_ready", function(offset, data, mask)
	if (data & 1) ~= 0 and (mask & 1) ~= 0 then
		arrived = true
		game_frames = game_frames + 1
		if waits then
			waits:write(string.format("%d %d\n", game_frames, cycle_now()))
		end
	end
end)

local function signature()
	local sum = 0
	for address = 0x102c8c, 0x102d0a, 2 do sum = sum + space:read_u16(address) end
	sum = sum + space:read_u16(0x10161e) + space:read_u16(0x101620) + space:read_u16(0x101622) + space:read_u16(0x101624)
	return sum & 0xffffffff
end

local function write_file(path, data)
	local file = io.open(path, "wb")
	if file then file:write(data); file:close() end
end

local log = io.open(out .. "/log.txt", "w")
if not log then
	emu.print_error("ddp_check: cannot write " .. out .. "/log.txt (make the folder first)")
	machine:exit()
	return
end
log:write("; ddp_check.lua: MAME " .. emu.app_version() .. ", " .. emu.romname() .. ", 68000 at " .. clock .. " Hz\n")

-- The inputs: the port gives player_inputs(N) after its VBLANK N, for the game's interrupt at the next VBLANK to read.
-- MAME's frame notifier for VBLANK N comes after that VBLANK's interrupt has read them already, so here the inputs
-- for VBLANK N + 1 are given at N (0.1.0 gave N's at N: read a VBLANK late). The log's line N names N's, as the port
-- gives them
local vblank = 0
local function on_frame()
	vblank = vblank + 1
	local held = player_inputs(vblank)
	set_inputs(player_inputs(vblank + 1))
	if vblank > 2500 then space:write_u8(0x102cca, 0x80) end    -- (player 1's time without harm: held)
	log:write(string.format("%d %04x %d %08x\n", vblank, held, arrived and 1 or 0, signature()))
	arrived = false
	if vblank % every == 0 then
		write_file(string.format("%s/ram_%d.bin", out, vblank), space:read_range(0x100000, 0x10ffff, 8))
		if screen then
			local pixels, width, height = screen:pixels()
			write_file(string.format("%s/screen_%d.bin", out, vblank), pixels)
			write_file(out .. "/screen_size.txt", string.format("%d %d\n", width, height))
		end
	end
	if vblank >= frames then
		log:close()
		if waits then waits:close() end
		emu.print_info("ddp_check: done, " .. vblank .. " VBLANKs in " .. out)
		machine:exit()
	end
end

set_inputs(player_inputs(1))
if emu.add_machine_frame_notifier then
	ddp_check_frames = emu.add_machine_frame_notifier(on_frame)    -- (kept: the subscription ends when collected)
else
	emu.register_frame_done(on_frame)
end
ddp_check_tap = ready_tap
