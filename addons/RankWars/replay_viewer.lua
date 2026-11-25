local client_mod = require("client")

local M = {}

local kToastTitle = "RANK WARS"
local kMainMenuSettleFrames = 30
local kLeaveTrainingRetryMs = 3000

local state = {
	stage = "off",
	api = nil,
	replay = "",
	path = "",
	matchup = "",
	leave_asked_at = 0,
}

local function failure_text(status)
	if status == 401 then
		return "The Watch link expired. Press Watch again on the panel"
	end
	if status == 403 or status == 404 then
		return "This replay is not on Rank Wars"
	end
	return "The replay could not be downloaded"
end

function M.cancel()
	if state.stage == "off" then
		return
	end
	uni2.log("replay " .. state.replay .. " dropped, a match link came in")
	state.stage = "off"
	state.api = nil
end

function M.start(link, match_live)
	if match_live then
		uni2.toast("warning", kToastTitle, "Finish the Rank Wars match first, then press Watch again")
		return
	end

	state.replay = link.replay
	state.path = ""
	state.matchup = ""
	state.stage = "downloading"
	state.api = client_mod.new(link.api, link.token, "0")

	uni2.toast("info", kToastTitle, "Downloading the replay")
	state.api:fetch_replay(link.replay, function(answer)
		if state.stage ~= "downloading" then
			return
		end

		if not answer.ok or answer.body == "" then
			uni2.toast("error", kToastTitle, failure_text(answer.status))
			state.stage = "off"
			state.api = nil
			return
		end

		local folder = uni2.replay.folder()
		uni2.fs.mkdir(folder)
		local path = folder .. "RankWars_" .. state.replay .. ".rep"

		if not uni2.fs.write(path, answer.body) then
			uni2.toast("error", kToastTitle, "The replay could not be saved")
			state.stage = "off"
			state.api = nil
			return
		end

		uni2.replay.refresh()
		state.path = path
		state.matchup = uni2.replay.matchup_of(path)
		state.stage = "waiting"
		state.leave_asked_at = 0
		uni2.toast("info", kToastTitle, state.matchup .. " is ready. It starts in the main menu")
	end)
end

function M.on_frame()
	if state.api then
		state.api:tick()
	end

	if state.stage ~= "waiting" then
		return
	end

	if uni2.game.is_training() then
		local now = uni2.time()
		if now - state.leave_asked_at >= kLeaveTrainingRetryMs then
			state.leave_asked_at = now
			uni2.game.leave_training()
		end
		return
	end

	if uni2.game.scene() ~= uni2.offsets.SCENE_MAIN_MENU or uni2.game.scene_frames() < kMainMenuSettleFrames then
		return
	end

	if not uni2.replay.can_play() then
		return
	end

	state.stage = "off"
	state.api = nil

	local ok, err = uni2.replay.request_playback(state.path)
	if not ok then
		uni2.toast("error", kToastTitle, "The replay could not start: " .. tostring(err))
		return
	end

	if uni2.replay.is_loading_patch() then
		uni2.toast("info", kToastTitle, state.matchup .. ": loading game version, starts automatically")
		return
	end

	uni2.toast("success", kToastTitle, "Playing " .. state.matchup)
end

return M
