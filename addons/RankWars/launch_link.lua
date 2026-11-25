local M = {}

local kKeys = { "action", "match", "token", "api", "lobby", "owner", "replay" }

local function hex_val(byte)
	if byte >= 48 and byte <= 57 then
		return byte - 48
	end
	if byte >= 65 and byte <= 70 then
		return byte - 55
	end
	if byte >= 97 and byte <= 102 then
		return byte - 87
	end
	return nil
end

function M.url_decode(text)
	if not text or text == "" then
		return ""
	end

	local out = {}
	local i = 1
	local n = #text

	while i <= n do
		local c = text:sub(i, i)
		if c == "+" then
			out[#out + 1] = " "
			i = i + 1
		elseif c == "%" and i + 2 <= n then
			local hi = hex_val(text:byte(i + 1))
			local lo = hex_val(text:byte(i + 2))
			if hi and lo then
				out[#out + 1] = string.char(hi * 16 + lo)
				i = i + 3
			else
				out[#out + 1] = c
				i = i + 1
			end
		else
			out[#out + 1] = c
			i = i + 1
		end
	end

	return table.concat(out)
end

local function find_query(command_line)
	local quoted = false
	local start_idx = nil
	local n = #command_line

	for i = 1, n do
		local c = command_line:sub(i, i)
		local prev = i > 1 and command_line:sub(i - 1, i - 1) or ""
		local token_start = i == 1 or prev == " " or prev == '"'

		if c == '"' then
			if start_idx then
				return command_line:sub(start_idx, i - 1)
			end
			quoted = not quoted
		elseif not start_idx and c == "?" and token_start then
			start_idx = i + 1
		elseif start_idx and c == " " and not quoted then
			return command_line:sub(start_idx, i - 1)
		end
	end

	if not start_idx then
		return ""
	end

	return command_line:sub(start_idx)
end

local function split_fields(query)
	local fields = {}

	for pair in (query .. "&"):gmatch("([^&]*)&") do
		local eq = pair:find("=", 1, true)
		if eq then
			local key = pair:sub(1, eq - 1)
			local val = M.url_decode(pair:sub(eq + 1))
			fields[key] = val
		end
	end

	return fields
end

local function is_decimal_id(text)
	if not text or text == "" then
		return false
	end

	return text:match("^%d+$") ~= nil and text ~= "0"
end

local function is_plain_id(text)
	if not text or text == "" then
		return false
	end

	return text:match("^[%w_%-]+$") ~= nil
end

local function is_http(url)
	return url:sub(1, 8) == "https://" or url:sub(1, 7) == "http://"
end

local function build_link(fields)
	local action = fields.action or ""
	local api = (fields.api or ""):gsub("/+$", "")

	if action ~= "create" and action ~= "join" and action ~= "replay" then
		return nil, "the link does not say whether to create or join a room"
	end

	if not is_http(api) then
		return nil, "the API address in the link is not an http address"
	end

	local link = {
		action = action,
		match = fields.match or "",
		token = fields.token or "",
		api = api,
		lobby = fields.lobby or "0",
		owner = fields.owner or "0",
		replay = fields.replay or "",
	}

	if action == "replay" then
		link.match = ""
		if not is_plain_id(link.replay) then
			return nil, "the replay link has no valid replay"
		end
		if link.token == "" then
			return nil, "the replay link has no token"
		end
		return link, nil
	end

	if link.match == "" or link.token == "" then
		return nil, "the link has no match or token"
	end

	if action == "join" then
		if not is_decimal_id(link.lobby) or not is_decimal_id(link.owner) then
			return nil, "the join link has no valid lobby or owner"
		end
	end

	return link, nil
end

function M.parse_command_line(command_line)
	local query = find_query(command_line or "")
	if query == "" then
		return nil, nil
	end

	return build_link(split_fields(query))
end

function M.parse_launch_parameters(params)
	local bare = (params or ""):gsub("^[/?]+", ""):gsub("/+$", "")
	if bare == "" then
		return nil, nil
	end

	return build_link(split_fields(bare))
end

function M.parse_query_values(getter)
	local fields = {}
	local any = false

	for _, key in ipairs(kKeys) do
		local value = getter(key)
		if value and value ~= "" then
			fields[key] = value
			any = true
		end
	end

	if not any then
		return nil, nil
	end

	return build_link(fields)
end

function M.host_link(match, token, api)
	return build_link({
		action = "create",
		match = match,
		token = token,
		api = api,
	})
end

function M.action_name(action)
	if action == "create" then
		return "host"
	end
	if action == "join" then
		return "guest"
	end
	if action == "replay" then
		return "viewer"
	end
	return "none"
end

return M
