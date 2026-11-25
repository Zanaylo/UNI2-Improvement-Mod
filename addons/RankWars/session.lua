local launch_link = require("launch_link")
local protocol = require("protocol")
local client_mod = require("client")
local replay_viewer = require("replay_viewer")

local M = {}

local kToastTitle = "RANK WARS"
local kLaunchPollMs = 2000
local kJoinTimeoutMs = 45000
local kLeaveTrainingRetryMs = 3000
local kNetworkRetryMs = 3000
local kMainMenuFrames = 30
local kNetworkMenuRunning = 1
local kNetworkListIdle = 1
local kScoreCheckDelays = { 2000, 5000, 10000, 15000 }

local kPhaseText = {
	off = "Open the Rank Wars link on Discord to start",
	creating_test = "Creating the test match",
	bad_link = "The Rank Wars link could not be read",
	waiting_steam = "Waiting for Steam",
	waiting_room = "Creating your room",
	reporting_room = "Sending the room to Rank Wars",
	waiting_join = "Joining the room from the main menu",
	joining = "Joining the room",
	in_match = "In the match. Each game is sent when it ends",
	finished = "Match over and recorded",
	aborted = "Match cancelled",
	closed = "Rank Wars closed this match. Nothing more will be sent",
	failed = "Something went wrong, see below",
}

local kClosedText = {
	UNDER_REVIEW = "The match went to the organisers for review",
	CANCELLED = "Rank Wars cancelled this match",
	VOIDED = "Rank Wars voided this match",
}

local state = {
	phase = "off",
	step = 0,
	link = nil,
	api = nil,
	sandbox = nil,
	sandbox_api = "",
	self_id = "0",
	reported_lobby = "0",
	local_role = "host",
	played = 0,
	local_host_wins = 0,
	local_guest_wins = 0,
	finished = false,
	match_loaded = false,
	match_fetch_at = 0,
	answered_games = 0,
	score_due = false,
	score_reads = 0,
	score_read_at = 0,
	join_started_at = 0,
	launch_polled_at = 0,
	launch_seen = "",
	leave_asked_at = 0,
	network_asked_at = 0,
	presence_note = "",
	error = "",
	last_game = "",
	info = {
		status = "",
		first_to = 0,
		score = { host = 0, guest = 0 },
		host = { name = "", steam_id = "0" },
		guest = { name = "", steam_id = "0" },
		sandbox = false,
		join_url = "",
	},
}

local function step_of(phase)
	if phase == "waiting_steam" then
		return 1
	end
	if phase == "waiting_room" or phase == "reporting_room" or phase == "waiting_join" or phase == "joining" then
		return 2
	end
	if phase == "in_match" then
		return 3
	end
	if phase == "finished" or phase == "aborted" or phase == "closed" then
		return 4
	end
	return 0
end

local function is_live()
	return state.link ~= nil and state.phase ~= "off" and state.phase ~= "bad_link" and
		state.phase ~= "creating_test" and state.phase ~= "finished" and
		state.phase ~= "aborted" and state.phase ~= "closed"
end

local function is_host()
	return state.link ~= nil and state.link.action == "create"
end

local function fail(msg)
	state.phase = "failed"
	state.error = msg
	uni2.warn("rank wars: " .. msg)
end

local function finish()
	if state.phase == "finished" then
		return
	end
	state.finished = true
	state.phase = "finished"
	uni2.toast("success", kToastTitle, string.format("Match over: %d - %d", state.info.score.host, state.info.score.guest))
end

local function close_match(status)
	state.finished = true
	state.phase = "closed"
	local reason = kClosedText[status] or "Rank Wars closed this match"
	uni2.toast("warning", kToastTitle, string.format("%s\nScore %d - %d", reason, state.info.score.host, state.info.score.guest))
end

local function score_decided(score, first_to)
	return first_to > 0 and (score.host >= first_to or score.guest >= first_to)
end

local function check_score()
	local total_server = state.info.score.host + state.info.score.guest
	local settled = not is_live() or state.finished or
		score_decided(state.info.score, state.info.first_to) or
		(total_server >= state.answered_games and
			state.info.score.host >= state.local_host_wins and
			state.info.score.guest >= state.local_guest_wins)

	if not settled and not state.score_due then
		state.score_reads = 0
		state.score_read_at = uni2.time()
	end
	state.score_due = not settled
end

local function follow_status(status)
	local mapped = protocol.state_of(status)
	if mapped == "completed" then
		finish()
	elseif mapped == "closed" then
		close_match(status)
	end
end

local function fetch_match()
	if not state.api then
		return
	end

	state.match_fetch_at = uni2.time()
	state.api:fetch_match(function(answer)
		if not answer.ok then
			if state.phase ~= "in_match" then
				state.error = "The match could not be read: " .. answer.error
			end
			return
		end

		local parsed = protocol.parse_match(answer.body)
		if not parsed then
			return
		end

		local before_h = state.info.score.host
		local before_g = state.info.score.guest
		state.info = parsed
		state.match_loaded = true
		state.error = ""

		local recorded = parsed.score.host + parsed.score.guest
		if recorded > state.played then
			state.played = recorded
		end

		if not is_live() then
			return
		end

		if state.phase == "in_match" and (before_h ~= parsed.score.host or before_g ~= parsed.score.guest) then
			uni2.toast("info", kToastTitle, string.format("Score updated: %d - %d", parsed.score.host, parsed.score.guest))
		end

		if protocol.state_of(parsed.status) == "waiting" then
			fail("This match has not been accepted on Discord yet. Accept it there and try again")
			return
		end

		if score_decided(parsed.score, parsed.first_to) then
			finish()
			return
		end

		follow_status(parsed.status)
		check_score()
	end)
end

local function start_match(link)
	if state.link and state.link.match == link.match and state.link.token == link.token then
		return
	end

	state.link = link
	state.local_role = link.action == "create" and "host" or "guest"
	state.api = nil
	state.reported_lobby = "0"
	state.played = 0
	state.local_host_wins = 0
	state.local_guest_wins = 0
	state.finished = false
	state.match_loaded = false
	state.match_fetch_at = 0
	state.answered_games = 0
	state.score_due = false
	state.error = ""
	state.last_game = ""
	state.phase = "waiting_steam"
	state.step = 1
	state.info = {
		status = "",
		first_to = 0,
		score = { host = 0, guest = 0 },
		host = { name = "", steam_id = "0" },
		guest = { name = "", steam_id = "0" },
		sandbox = false,
		join_url = "",
	}

	replay_viewer.cancel()
	uni2.ui.set_open("rank_wars", true)
	uni2.log("match " .. link.match .. " as " .. launch_link.action_name(link.action))
end

local function take_link(link, err)
	if link and link.action == "replay" then
		replay_viewer.start(link, is_live())
		return
	end

	if link then
		start_match(link)
		return
	end

	if err and err ~= "" then
		if is_live() then
			uni2.warn("ignored bad link while match live: " .. err)
			return
		end
		state.phase = "bad_link"
		state.error = err
		uni2.ui.set_open("rank_wars", true)
	end
end

local function step_leave_training()
	if not uni2.game.is_training() then
		return
	end
	local now = uni2.time()
	if now - state.leave_asked_at >= kLeaveTrainingRetryMs then
		state.leave_asked_at = now
		uni2.game.leave_training()
	end
end

local function report_room(lobby)
	state.reported_lobby = lobby
	state.phase = "reporting_room"
	state.api:put_room(lobby, state.self_id, function(answer)
		if not answer.ok then
			fail("The room could not be sent: " .. answer.error)
			return
		end
		state.phase = "in_match"
		fetch_match()
		uni2.toast("success", kToastTitle, "Room sent. The opponent gets the link now")
	end)
end

local function watch_for_own_room()
	local lobby = uni2.steam.lobby()
	if lobby ~= "0" and uni2.steam.lobby_owner(lobby) == state.self_id then
		report_room(lobby)
		return
	end

	if not state.match_loaded then
		local now = uni2.time()
		if state.api and state.api:pending() == 0 and now - state.match_fetch_at >= kLaunchPollMs then
			fetch_match()
		end
		return
	end

	step_leave_training()

	local stage = uni2.game.room_create_stage()
	if stage == 0 then
		uni2.game.room_create_start(state.info.first_to)
	end
	uni2.game.room_create_set_first_to(state.info.first_to)
	uni2.game.room_create_step()

	if uni2.game.room_create_timed_out() then
		fail("The room could not be created automatically. Create a Player Match room and it is sent by itself")
	end
end

local function wait_for_menu_to_join()
	if uni2.steam.lobby() == state.link.lobby then
		state.phase = "in_match"
		uni2.toast("success", kToastTitle, "You are in the room")
		return
	end

	step_leave_training()

	local scene = uni2.game.scene()
	local frames = uni2.game.scene_frames()
	local in_lobby = uni2.steam.lobby() ~= "0"

	if not in_lobby and scene == uni2.offsets.SCENE_MAIN_MENU and frames >= kMainMenuFrames then
		local now = uni2.time()
		if now - state.network_asked_at >= kNetworkRetryMs then
			state.network_asked_at = now
			uni2.game.enter_network()
		end
		return
	end

	local menu_state = uni2.mem.read_rva_i32(uni2.offsets.NETWORK_MENU + uni2.offsets.NETWORK_MENU_STATE)
	local list_state = uni2.mem.read_rva_i32(uni2.offsets.NETWORK_MENU + uni2.offsets.NETWORK_MENU_LIST)

	if in_lobby or scene ~= uni2.offsets.SCENE_NETWORK or menu_state ~= kNetworkMenuRunning or list_state ~= kNetworkListIdle then
		return
	end

	if not uni2.game.can_join_room() then
		fail("This game version cannot join the room by itself. Join through the Steam invite")
		return
	end

	state.join_started_at = uni2.time()
	state.phase = "joining"
	uni2.game.claim_pad()
	uni2.game.join_room(state.link.lobby, state.link.owner)
end

local function watch_join()
	if uni2.steam.lobby() == state.link.lobby then
		state.phase = "in_match"
		uni2.toast("success", kToastTitle, "You are in the room")
		return
	end

	local flow_err = uni2.mem.read_rva_i32(uni2.offsets.NETWORK_FLOW_ERROR)
	if flow_err ~= 0 then
		fail("The host's room is closed or could not be reached. Ask the host for a new link")
		return
	end

	if uni2.time() - state.join_started_at >= kJoinTimeoutMs then
		fail("Could not get into the room in time. Ask the host for a new link")
	end
end

local function watch_steam_launch()
	local now = uni2.time()
	if now - state.launch_polled_at < kLaunchPollMs then
		return
	end
	state.launch_polled_at = now

	local cmd = uni2.steam.launch_command_line()
	local q_link, q_err = launch_link.parse_query_values(function(k)
		return uni2.steam.launch_query(k)
	end)

	local seen = cmd .. "\n" .. (q_link and q_link.match or "") .. "\n" .. (q_link and q_link.token or "") .. "\n" .. (q_err or "")
	if seen == state.launch_seen then
		return
	end
	state.launch_seen = seen

	if q_link or (q_err and q_err ~= "") then
		take_link(q_link, q_err)
		return
	end

	if cmd ~= "" then
		local link, err = launch_link.parse_launch_parameters(cmd)
		take_link(link, err)
	end
end

local function update_presence()
	local wanted = ""
	if is_live() then
		if state.info.first_to > 0 then
			wanted = string.format("Rank Wars %d-%d, first to %d", state.info.score.host, state.info.score.guest, state.info.first_to)
		else
			wanted = string.format("Rank Wars %d-%d", state.info.score.host, state.info.score.guest)
		end
	end

	if wanted ~= state.presence_note then
		state.presence_note = wanted
		uni2.steam.set_rich_presence_note(wanted)
	end
end

function M.initialize()
	local link, err = launch_link.parse_command_line(uni2.config.command_line())
	take_link(link, err)
end

function M.on_frame()
	watch_steam_launch()
	replay_viewer.on_frame()

	if state.sandbox then
		state.sandbox:tick()
	end

	if state.api then
		state.api:tick()
	end

	if state.phase == "off" or state.phase == "bad_link" or state.phase == "creating_test" then
		update_presence()
		return
	end

	if not state.api then
		local own = uni2.steam.own_id()
		if own == "0" or own == "" then
			return
		end
		state.self_id = own
		state.api = client_mod.new(state.link.api, state.link.token, own)
		fetch_match()
		state.phase = is_host() and "waiting_room" or "waiting_join"
	end

	if state.phase ~= "waiting_room" then
		uni2.game.room_create_stop()
	end

	if state.phase == "waiting_room" then
		watch_for_own_room()
	elseif state.phase == "waiting_join" then
		wait_for_menu_to_join()
	elseif state.phase == "joining" then
		watch_join()
	elseif state.phase == "in_match" then
		if is_host() and state.played == 0 then
			local lobby = uni2.steam.lobby()
			if lobby ~= "0" and lobby ~= state.reported_lobby and uni2.steam.lobby_owner(lobby) == state.self_id then
				report_room(lobby)
			end
		end

		if state.score_due then
			local delay = kScoreCheckDelays[math.min(state.score_reads + 1, #kScoreCheckDelays)]
			if uni2.time() - state.score_read_at >= delay then
				state.score_read_at = uni2.time()
				state.score_reads = state.score_reads + 1
				fetch_match()
			end
		end
	end

	if state.phase ~= "failed" then
		state.step = step_of(state.phase)
	end

	update_presence()
end

function M.on_game_started()
	if not is_live() then
		return
	end
	uni2.ui.set_open("rank_wars", false)
	fetch_match()
end

function M.on_game_ended(report)
	if not is_live() or state.finished or report.ending == "connection_lost" then
		return
	end

	local function role_of(side)
		return side == report.local_side and state.local_role or protocol.other_role(state.local_role)
	end

	state.played = state.played + 1
	local game_num = state.played
	local winner_role = role_of(report.winner)
	local host_first = role_of(0) == "host"
	local host_chara = protocol.slug(uni2.game.character_name(report.chara[host_first and 1 or 2]))
	local guest_chara = protocol.slug(uni2.game.character_name(report.chara[host_first and 2 or 1]))

	if winner_role == "host" then
		state.local_host_wins = state.local_host_wins + 1
	else
		state.local_guest_wins = state.local_guest_wins + 1
	end

	local name0 = ""
	local name1 = ""
	local header = uni2.replay.describe_header(report.record)
	if header then
		name0 = protocol.name_of_steam_id(state.info, header.steam_id_0)
		name1 = protocol.name_of_steam_id(state.info, header.steam_id_1)
	end

	local saved_path = uni2.replay.save_record(report.record, "rankwars-game" .. tostring(game_num), name0, name1)
	local replay_bytes = saved_path ~= "" and uni2.fs.read(saved_path) or nil

	if not replay_bytes or replay_bytes == "" then
		state.last_game = string.format("Game %d: the replay could not be saved, so it was not sent", game_num)
		fetch_match()
		return
	end

	state.last_game = string.format("Game %d: the %s won, sending", game_num, winner_role)
	state.api:put_game(game_num, winner_role, host_chara, guest_chara, "game" .. tostring(game_num) .. ".rep", replay_bytes, function(answer)
		if answer.status == 409 then
			state.last_game = string.format("Game %d: the match was already closed on Rank Wars", game_num)
			fetch_match()
			return
		end

		if not answer.ok then
			state.last_game = string.format("Game %d: not recorded, %s", game_num, answer.error)
			uni2.toast("error", kToastTitle, state.last_game)
			return
		end

		local parsed = protocol.parse_game_answer(answer.body)
		if not parsed then
			return
		end

		if parsed.score.host + parsed.score.guest >= state.info.score.host + state.info.score.guest then
			state.info.score = parsed.score
		end
		if parsed.first_to > 0 then
			state.info.first_to = parsed.first_to
		end
		if game_num > state.answered_games then
			state.answered_games = game_num
		end

		local label = parsed.result == "RECORDED" and "recorded"
			or parsed.result == "DUPLICATE" and "already recorded"
			or parsed.result == "DISPUTED" and "DISPUTED, sent for review"
			or "unknown answer"

		state.last_game = string.format("Game %d: %s", game_num, label)
		local kind = parsed.result == "DISPUTED" and "warning" or "success"
		uni2.toast(kind, kToastTitle, string.format("%s\nScore %d - %d", state.last_game, state.info.score.host, state.info.score.guest))

		if parsed.finished or score_decided(state.info.score, state.info.first_to) then
			finish()
			return
		end

		follow_status(parsed.status)
		check_score()
	end)

	fetch_match()
end

function M.view()
	local detail = ""
	if state.phase == "waiting_room" then
		if not state.match_loaded then
			detail = "Reading match settings"
		else
			local _, text = uni2.game.room_create_stage()
			detail = text
		end
	elseif state.phase == "closed" then
		detail = kClosedText[state.info.status] or "Rank Wars closed this match"
	end

	return {
		phase = state.phase,
		phase_text = kPhaseText[state.phase] or kPhaseText.off,
		detail = detail,
		step = state.step,
		is_host = is_host(),
		role = state.link and state.local_role or "",
		match = state.link and state.link.match or "",
		host = state.info.host.name or "",
		guest = state.info.guest.name or "",
		score = state.info.score,
		first_to = state.info.first_to,
		pending = state.api and state.api:pending() or 0,
		last_game = state.last_game,
		error = state.error,
		sandbox = state.info.sandbox,
		guest_link = state.info.join_url,
	}
end

function M.scoreboard()
	if not state.link or (not is_live() and state.phase ~= "finished") then
		return nil
	end

	return {
		is_host = is_host(),
		score = state.info.score,
		first_to = state.info.first_to,
	}
end

function M.can_retry()
	return state.phase == "failed"
end

function M.retry()
	if not M.can_retry() or not state.api then
		return
	end
	state.error = ""
	state.phase = is_host() and "waiting_room" or "waiting_join"
	fetch_match()
end

function M.can_abort()
	return state.api ~= nil and is_live() and not state.finished
end

function M.abort(reason)
	if not M.can_abort() then
		return
	end
	local final_reason = (reason and reason ~= "") and reason or "cancelada pelo jogo"
	state.api:abort(final_reason, function(answer)
		if not answer.ok then
			state.error = "The match could not be cancelled: " .. answer.error
			return
		end
		state.phase = "aborted"
		uni2.toast("warning", kToastTitle, "Match cancelled")
	end)
end

function M.can_start_test()
	return (not is_live() or state.phase == "failed") and state.phase ~= "creating_test"
end

function M.start_test(api_url, password, first_to)
	if not M.can_start_test() then
		return
	end

	if not state.sandbox or state.sandbox_api ~= api_url then
		state.sandbox_api = api_url
		state.sandbox = client_mod.new(api_url, "", "0")
	end

	state.link = nil
	state.error = ""
	state.last_game = ""
	state.step = 0
	state.phase = "creating_test"

	state.sandbox:create_sandbox(password, first_to, function(answer)
		if not answer.ok then
			state.phase = "off"
			state.error = "The test match could not be created: " .. answer.error
			return
		end

		local parsed = protocol.parse_sandbox(answer.body)
		if not parsed then
			state.phase = "off"
			state.error = "The test match response could not be read"
			return
		end

		local link, err = launch_link.host_link(parsed.match, parsed.host_token, state.sandbox_api)
		if not link then
			state.phase = "off"
			state.error = "The test match link is invalid: " .. tostring(err)
			return
		end

		start_match(link)
	end)
end

return M
