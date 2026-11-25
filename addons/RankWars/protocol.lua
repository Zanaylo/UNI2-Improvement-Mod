local M = {}

local kLineBreak = "\r\n"
local kClosedStatuses = {
	UNDER_REVIEW = true,
	CANCELLED = true,
	VOIDED = true,
}

function M.other_role(role)
	return role == "host" and "guest" or "host"
end

function M.slug(name)
	if not name or name == "" then
		return ""
	end

	return name:lower()
end

function M.room_body(lobby, owner)
	return uni2.http.json_encode({
		lobbyId = tostring(lobby),
		ownerSteamId = tostring(owner),
	})
end

function M.abort_body(reason)
	local trimmed = (reason or ""):sub(1, 500)
	return uni2.http.json_encode({ reason = trimmed })
end

function M.sandbox_body(password, first_to)
	local payload = { password = password or "" }
	if first_to and first_to > 0 then
		payload.firstTo = first_to
	end
	return uni2.http.json_encode(payload)
end

function M.game_form(boundary, replay_bytes, file_name, winner_role, host_chara, guest_chara)
	local parts = {}

	local function add_field(name, value)
		parts[#parts + 1] = "--" .. boundary .. kLineBreak
		parts[#parts + 1] = 'Content-Disposition: form-data; name="' .. name .. '"' .. kLineBreak .. kLineBreak
		parts[#parts + 1] = value .. kLineBreak
	end

	add_field("winner", winner_role)

	if host_chara and host_chara ~= "" then
		add_field("hostCharacter", host_chara)
	end

	if guest_chara and guest_chara ~= "" then
		add_field("guestCharacter", guest_chara)
	end

	parts[#parts + 1] = "--" .. boundary .. kLineBreak
	parts[#parts + 1] = 'Content-Disposition: form-data; name="replay"; filename="' .. file_name .. '"' .. kLineBreak
	parts[#parts + 1] = "Content-Type: application/octet-stream" .. kLineBreak .. kLineBreak
	parts[#parts + 1] = replay_bytes
	parts[#parts + 1] = kLineBreak .. "--" .. boundary .. "--" .. kLineBreak

	return "multipart/form-data; boundary=" .. boundary, table.concat(parts)
end

function M.parse_match(body)
	local root = uni2.http.json_decode(body or "")
	if type(root) ~= "table" then
		return nil
	end

	local score = type(root.score) == "table" and root.score or {}
	local host = type(root.host) == "table" and root.host or {}
	local guest = type(root.guest) == "table" and root.guest or {}

	return {
		id = tostring(root.id or ""),
		status = tostring(root.status or ""),
		role = root.role == "guest" and "guest" or "host",
		first_to = tonumber(root.firstTo) or 0,
		lobby = tostring(root.lobbyId or "0"),
		lobby_owner = tostring(root.lobbyOwnerSteamId or "0"),
		score = {
			host = tonumber(score.host) or 0,
			guest = tonumber(score.guest) or 0,
		},
		host = {
			name = tostring(host.displayName or ""),
			steam_id = tostring(host.steamId64 or "0"),
		},
		guest = {
			name = tostring(guest.displayName or ""),
			steam_id = tostring(guest.steamId64 or "0"),
		},
		sandbox = root.sandbox == true,
		join_url = tostring(root.joinRoomUrl or ""),
	}
end

function M.parse_game_answer(body)
	local root = uni2.http.json_decode(body or "")
	if type(root) ~= "table" then
		return nil
	end

	local match = type(root.match) == "table" and root.match or {}
	local score = type(match.score) == "table" and match.score or {}

	return {
		result = tostring(root.result or ""),
		status = tostring(match.status or ""),
		first_to = tonumber(match.firstTo) or 0,
		finished = match.finished == true,
		score = {
			host = tonumber(score.host) or 0,
			guest = tonumber(score.guest) or 0,
		},
	}
end

function M.parse_sandbox(body)
	local root = uni2.http.json_decode(body or "")
	if type(root) ~= "table" then
		return nil
	end

	local match_id = tostring(root.matchId or "")
	local host_token = tostring(root.hostToken or "")

	if match_id == "" or host_token == "" then
		return nil
	end

	return {
		match = match_id,
		first_to = tonumber(root.firstTo) or 0,
		host_token = host_token,
		guest_token = tostring(root.guestToken or ""),
	}
end

function M.describe_error(status, body)
	local suffix = "HTTP " .. tostring(status or 0)
	local root = uni2.http.json_decode(body or "")
	if type(root) ~= "table" then
		return suffix
	end

	local message = tostring(root.error or "")
	local code = tostring(root.code or "")

	if message == "" then
		return suffix
	end

	if code ~= "" then
		return message .. " (" .. code .. ", " .. suffix .. ")"
	end

	return message .. " (" .. suffix .. ")"
end

function M.should_retry(status)
	return status == 0 or status == 429 or status >= 500
end

function M.state_of(status)
	if status == "PENDING_ACCEPT" then
		return "waiting"
	end
	if status == "COMPLETED" then
		return "completed"
	end
	if kClosedStatuses[status] then
		return "closed"
	end
	return "open"
end

function M.name_of_steam_id(info, steam_id)
	if not info or not steam_id or steam_id == "" or steam_id == "0" then
		return ""
	end
	if info.host and steam_id == info.host.steam_id then
		return info.host.name or ""
	end
	if info.guest and steam_id == info.guest.steam_id then
		return info.guest.name or ""
	end
	return ""
end

return M
