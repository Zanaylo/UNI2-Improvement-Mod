local session = require("session")
local hud = require("hud")

local M = {}

local kAccent = { 0.063, 0.725, 0.506, 1.00 }
local kAccentDark = { 0.055, 0.240, 0.180, 1.00 }
local kShade = { 0.00, 0.00, 0.00, 0.35 }
local kCard = { 0.075, 0.098, 0.090, 0.96 }
local kCardEdge = { 0.118, 0.161, 0.145, 1.00 }
local kDone = { 0.204, 0.827, 0.600, 1.00 }
local kIdle = { 0.35, 0.42, 0.39, 1.00 }
local kText = { 0.922, 0.949, 0.933, 1.00 }
local kMuted = { 0.490, 0.573, 0.537, 1.00 }
local kGlow = { 0.063, 0.725, 0.506, 0.35 }
local kProblem = { 0.320, 0.078, 0.118, 0.65 }
local kProblemEdge = { 0.957, 0.247, 0.369, 0.85 }

local ui_state = {
	confirming = false,
	reason = "",
	test_loaded = false,
	test_tools = false,
	test_api = "",
	test_password = "",
	test_first_to = 0,
}

local function load_test_settings()
	if ui_state.test_loaded then
		return
	end
	ui_state.test_loaded = true
	ui_state.test_tools = uni2.config.get_int("RankWars", "TestTools", 0) ~= 0
	ui_state.test_api = uni2.config.get_string("RankWars", "TestApi", "")
	ui_state.test_password = uni2.config.get_string("RankWars", "TestPassword", "")
end

local function is_working(phase)
	return phase == "creating_test" or phase == "waiting_steam" or phase == "waiting_room" or
		phase == "reporting_room" or phase == "waiting_join" or phase == "joining"
end

local function dots()
	local n = math.floor(uni2.clock() * 2.5) % 4
	return string.rep(".", n)
end

local function draw_pips(cx, y, wins, needed)
	local r = uni2.ui.scaled(5.0)
	local gap = uni2.ui.scaled(14.0)
	local left = cx - gap * (needed - 1) * 0.5

	for pip = 0, needed - 1 do
		local x = left + gap * pip
		if pip < wins then
			uni2.ui.draw_circle_filled(x, y, r, kAccent)
		else
			uni2.ui.draw_circle(x, y, r, kIdle, uni2.ui.scaled(1.5))
		end
	end
end

local function draw_header(view)
	local width = uni2.ui.avail_width()
	local height = uni2.ui.scaled(46.0)
	local x, y = uni2.ui.reserve(height)
	local x2 = x + width
	local y2 = y + height

	uni2.ui.draw_rect_filled_multi(x, y, x2, y2, kAccent, kAccentDark)
	local _, th = uni2.ui.calc_text_size("RANK WARS", 1.35)
	uni2.ui.draw_text(x + uni2.ui.scaled(12.0), y + (height - th) * 0.5, kText, "RANK WARS", 1.35)

	if view.role == "" then
		return
	end

	local pill = view.is_host and "HOST" or "GUEST"
	local pw, ph = uni2.ui.calc_text_size(pill, 1.0)
	local px = uni2.ui.scaled(8.0)
	local py = uni2.ui.scaled(3.0)
	local mid = y2 - height * 0.5
	local rx2 = x2 - uni2.ui.scaled(12.0)
	local rx1 = rx2 - pw - px * 2.0
	local ry1 = mid - ph * 0.5 - py
	local ry2 = mid + ph * 0.5 + py

	uni2.ui.draw_rect_filled(rx1, ry1, rx2, ry2, kShade, uni2.ui.scaled(10.0))
	uni2.ui.draw_text(rx1 + px, ry1 + py, kText, pill, 1.0)
	uni2.ui.text_muted("Match " .. view.match)
end

local function draw_scoreboard(view)
	if view.role == "" then
		return
	end

	local width = uni2.ui.avail_width()
	local height = uni2.ui.scaled(96.0)
	local x, y = uni2.ui.reserve(height)
	local x2 = x + width
	local y2 = y + height
	local pad = uni2.ui.scaled(12.0)
	local third = width / 3.0

	uni2.ui.draw_rect_filled(x, y, x2, y2, kCard, uni2.ui.scaled(8.0))
	uni2.ui.draw_rect(x, y, x2, y2, kCardEdge, uni2.ui.scaled(8.0))

	local host_name = view.host ~= "" and view.host or "Host"
	local guest_name = view.guest ~= "" and view.guest or "Guest"

	local function draw_player(col_x, name, is_you)
		local cx = col_x + third * 0.5
		uni2.ui.push_clip_rect(col_x, y, col_x + third, y2)
		uni2.ui.draw_text_centered(cx, y + pad, is_you and kText or kMuted, name, 1.1)
		uni2.ui.pop_clip_rect()
		if is_you then
			local _, nh = uni2.ui.calc_text_size(name, 1.1)
			uni2.ui.draw_text_centered(cx, y + pad + nh + uni2.ui.scaled(2.0), kAccent, "YOU", 0.85)
		end
	end

	draw_player(x, host_name, view.is_host)
	draw_player(x + third * 2.0, guest_name, not view.is_host)

	local score_str = string.format("%d - %d", view.score.host, view.score.guest)
	local _, sh = uni2.ui.calc_text_size(score_str, 2.4)
	uni2.ui.draw_text_centered(x + width * 0.5, y + (height - sh) * 0.5 - pad * 0.4, kText, score_str, 2.4)

	if view.first_to <= 0 then
		return
	end

	local pip_y = y2 - pad
	draw_pips(x + third * 0.5, pip_y, view.score.host, view.first_to)
	draw_pips(x + third * 2.5, pip_y, view.score.guest, view.first_to)

	local ft_text = "First to " .. tostring(view.first_to)
	local _, fh = uni2.ui.calc_text_size(ft_text, 0.85)
	uni2.ui.draw_text_centered(x + width * 0.5, pip_y - fh * 0.5, kMuted, ft_text, 0.85)
end

local function draw_steps(view)
	uni2.ui.dummy(0.0, uni2.ui.scaled(6.0))
	local width = uni2.ui.avail_width()
	local x, y = uni2.ui.reserve(uni2.ui.scaled(54.0))
	local radius = uni2.ui.scaled(9.0)
	local pad = uni2.ui.scaled(21.0)
	local labels = { "Link", "Rank Wars", view.is_host and "Room" or "Join", "Games", "Result" }
	local span = (width - pad * 2.0) / 4.0
	local cy = y + radius + uni2.ui.scaled(4.0)
	local line_w = uni2.ui.scaled(2.0)
	local finished = view.phase == "finished"

	for i = 0, 3 do
		uni2.ui.draw_line(x + pad + span * i, cy, x + pad + span * (i + 1), cy, i < view.step and kDone or kIdle, line_w)
	end

	for i = 0, 4 do
		local cx = x + pad + span * i
		if i < view.step or (finished and i == view.step) then
			uni2.ui.draw_circle_filled(cx, cy, radius, kDone)
			uni2.ui.draw_line(cx - radius * 0.45, cy + radius * 0.02, cx - radius * 0.10, cy + radius * 0.38, kText, line_w)
			uni2.ui.draw_line(cx - radius * 0.10, cy + radius * 0.38, cx + radius * 0.50, cy - radius * 0.35, kText, line_w)
		elseif i > view.step then
			uni2.ui.draw_circle(cx, cy, radius, kIdle, line_w)
		else
			local pulse = 0.5 + 0.5 * math.sin(uni2.clock() * 4.0)
			uni2.ui.draw_circle_filled(cx, cy, radius + uni2.ui.scaled(3.0) * pulse, kGlow)
			uni2.ui.draw_circle_filled(cx, cy, radius, kAccent)
		end

		uni2.ui.draw_text_centered(cx, cy + radius + uni2.ui.scaled(6.0), i <= view.step and kText or kMuted, labels[i + 1], 0.85)
	end
end

function M.draw()
	load_test_settings()
	local view = session.view()

	draw_header(view)
	draw_scoreboard(view)
	draw_steps(view)

	uni2.ui.text_wrapped(view.phase_text .. (is_working(view.phase) and dots() or ""))

	if view.detail ~= "" then
		uni2.ui.text_muted(view.detail)
	end

	if view.last_game ~= "" then
		uni2.ui.text_wrapped(view.last_game)
	end

	if view.pending > 0 then
		uni2.ui.text_muted(string.format("Sending %d update(s)%s", view.pending, dots()))
	end

	if view.error ~= "" then
		uni2.ui.dummy(0.0, uni2.ui.scaled(6.0))
		if uni2.ui.begin_child_card("##rw_problem", kProblem, kProblemEdge, 8.0) then
			uni2.ui.text_wrapped(view.error)
			if session.can_retry() and uni2.ui.button("Try again", uni2.ui.scaled(150.0), 0.0) then
				session.retry()
			end
		end
		uni2.ui.end_child_card()
	end

	if view.sandbox then
		uni2.ui.dummy(0.0, uni2.ui.scaled(6.0))
		uni2.ui.text_muted("Test match: it does not count for the ranking")
		if view.guest_link ~= "" then
			if uni2.ui.button("Copy the guest's link", uni2.ui.scaled(210.0), 0.0) then
				uni2.ui.set_clipboard(view.guest_link)
			end
			if uni2.ui.is_item_hovered() then
				uni2.ui.set_tooltip("Send it to the other player to open. They join your room by themselves")
			end
		end
	end

	if session.can_abort() then
		uni2.ui.dummy(0.0, uni2.ui.scaled(6.0))
		uni2.ui.separator()
		if not ui_state.confirming then
			if uni2.ui.small_button("Cancel this match") then
				ui_state.confirming = true
			end
		else
			uni2.ui.text_wrapped("Tell Rank Wars this match will not happen?")
			uni2.ui.set_next_item_width(-1.0)
			local _, reason = uni2.ui.input_text("##rw_reason", "Reason (optional)", ui_state.reason, false)
			ui_state.reason = reason

			uni2.ui.push_button_color(kAccent)
			local confirm = uni2.ui.button("Cancel the match")
			uni2.ui.pop_style_color(1)

			if confirm then
				session.abort(ui_state.reason)
				ui_state.confirming = false
			end
			uni2.ui.same_line()
			if uni2.ui.button("Keep playing") then
				ui_state.confirming = false
			end
		end
	end

	uni2.ui.dummy(0.0, uni2.ui.scaled(6.0))
	local changed, visible = uni2.ui.checkbox("Score in the fight", hud.is_visible())
	if changed then
		hud.set_visible(visible)
	end
	if uni2.ui.is_item_hovered() then
		uni2.ui.set_tooltip("Shows a dot for every win under each character's icon: yours in green, the opponent's in red.")
	end

	if ui_state.test_tools then
		local preview = hud.is_previewing()
		if uni2.ui.button(preview and "Hide the test HUD" or "Show the test HUD", uni2.ui.scaled(210.0), 0.0) then
			hud.set_previewing(not preview)
		end

		if session.can_start_test() then
			uni2.ui.dummy(0.0, uni2.ui.scaled(6.0))
			if uni2.ui.collapsing_header("Test match") then
				uni2.ui.set_next_item_width(-1.0)
				local _, api = uni2.ui.input_text("##rw_api", "API", ui_state.test_api, false)
				ui_state.test_api = api

				uni2.ui.set_next_item_width(-1.0)
				local _, pw = uni2.ui.input_text("##rw_pw", "Test password", ui_state.test_password, true)
				ui_state.test_password = pw

				uni2.ui.set_next_item_width(uni2.ui.scaled(150.0))
				local _, ft = uni2.ui.slider_int("First to", ui_state.test_first_to, 0, 7,
					ui_state.test_first_to == 0 and "default" or "%d")
				ui_state.test_first_to = ft

				uni2.ui.begin_disabled(ui_state.test_password == "")
				if uni2.ui.button("Create a test match", uni2.ui.scaled(210.0), 0.0) then
					uni2.config.set_string("RankWars", "TestApi", ui_state.test_api)
					uni2.config.set_string("RankWars", "TestPassword", ui_state.test_password)
					session.start_test(ui_state.test_api, ui_state.test_password, ui_state.test_first_to)
				end
				uni2.ui.end_disabled()
				uni2.ui.text_muted("You become the host. It does not touch the ranking or tell Discord")
			end
		end
	end
end

return M
