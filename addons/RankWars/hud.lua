local M = {}

local kScreenWidth = 1280
local kIconCentreX = 74
local kRowCentreY = 114
local kStep = 14
local kRadius = 5
local kRingWidth = 1
local kRingLayer = 286
local kFillLayer = 287

local kRing = 0xE6FFFFFF
local kOpen = 0x99000000
local kOwnWin = 0xFF34D15C
local kRivalWin = 0xFFDC1A33

local state = {
	visible = true,
	preview = false,
	loaded = false,
}

local function ensure_loaded()
	if state.loaded then
		return
	end
	state.loaded = true
	state.visible = uni2.config.get_int("RankWars", "ShowHud", 1) ~= 0
end

local function pip_centre_x(side, pip, needed)
	local left = side == 0
	local direction = left and 1 or -1
	local centre = left and kIconCentreX or (kScreenWidth - kIconCentreX)
	local first = centre - math.floor(direction * (kStep * (needed - 1)) / 2)
	return first + direction * kStep * pip
end

local function fill_disc(cx, cy, radius, colour, layer)
	local arm = math.floor(radius * 41 / 100)
	local core = math.floor(radius * 70 / 100)

	uni2.draw.fill(cx - radius, cy - arm, radius * 2 + 1, arm * 2 + 1, colour, layer)
	uni2.draw.fill(cx - arm, cy - radius, arm * 2 + 1, radius * 2 + 1, colour, layer)
	uni2.draw.fill(cx - core, cy - core, core * 2 + 1, core * 2 + 1, colour, layer)
end

local function paint_side(side, wins, needed, own)
	local won = own and kOwnWin or kRivalWin

	for pip = 0, needed - 1 do
		local x = pip_centre_x(side, pip, needed)
		fill_disc(x, kRowCentreY, kRadius + kRingWidth, kRing, kRingLayer)
		fill_disc(x, kRowCentreY, kRadius, pip < wins and won or kOpen, kFillLayer)
	end
end

function M.is_visible()
	ensure_loaded()
	return state.visible
end

function M.set_visible(visible)
	ensure_loaded()
	state.visible = visible
	uni2.config.set_int("RankWars", "ShowHud", visible and 1 or 0)
end

function M.is_previewing()
	return state.preview
end

function M.set_previewing(preview)
	state.preview = preview
end

function M.draw(scoreboard_fn)
	ensure_loaded()

	if not uni2.game.in_match() then
		return
	end

	local local_side = 0
	local is_host = true
	local host_wins = 3
	local guest_wins = 1
	local first_to = 5

	if not state.preview then
		if not state.visible or not uni2.game.is_online() or uni2.game.is_spectating() then
			return
		end

		local board = scoreboard_fn()
		if not board or board.first_to <= 0 then
			return
		end

		local_side = uni2.game.local_side()
		if local_side ~= 0 and local_side ~= 1 then
			return
		end

		is_host = board.is_host
		host_wins = board.score.host
		guest_wins = board.score.guest
		first_to = board.first_to
	end

	for side = 0, 1 do
		local host_side = (side == local_side) == is_host
		local wins = host_side and host_wins or guest_wins
		paint_side(side, wins, first_to, side == local_side)
	end
end

return M
