local protocol = require("protocol")

local M = {}

local kApiPath = "/api/im/v1"
local kRetryDelays = { 2000, 5000, 10000, 20000, 30000, 60000 }

local function is_success(status)
	return status >= 200 and status <= 299
end

function M.new(api_url, token, steam_id)
	local self = {
		base = api_url .. kApiPath,
		token = token or "",
		steam_id = tostring(steam_id or "0"),
		queue = {},
		in_flight = false,
		retry_at = 0,
		pending_retry = nil,
	}

	local function build_headers(job)
		local headers = {
			Accept = job.accept or "application/json",
		}
		if self.token ~= "" then
			headers["Authorization"] = "Bearer " .. self.token
		end
		if self.steam_id ~= "" and self.steam_id ~= "0" then
			headers["X-Steam-Id"] = self.steam_id
		end
		if job.content_type and job.content_type ~= "" then
			headers["Content-Type"] = job.content_type
		end
		return headers
	end

	local function dispatch(job)
		self.in_flight = true
		uni2.http.request({
			method = job.method,
			url = self.base .. job.path,
			headers = build_headers(job),
			body = job.body or "",
		}, function(resp)
			self.in_flight = false
			local status = resp.sent and resp.status or 0
			local ok = resp.sent and is_success(status)
			local err = ok and "" or (resp.sent and protocol.describe_error(status, resp.body) or resp.error)

			uni2.log(string.format("%s %s attempt %d -> %d %s", job.method, job.path, job.attempt, status, ok and "ok" or err))

			if not ok and job.retries and protocol.should_retry(status) and job.attempt <= #kRetryDelays then
				local delay = kRetryDelays[job.attempt]
				job.attempt = job.attempt + 1
				self.pending_retry = job
				self.retry_at = uni2.time() + delay
				return
			end

			if job.callback then
				job.callback({
					ok = ok,
					status = status,
					body = resp.body or "",
					error = err,
					game_number = job.game_number or 0,
				})
			end
		end)
	end

	function self:enqueue(job)
		job.attempt = 1
		if job.retries == nil then
			job.retries = true
		end
		self.queue[#self.queue + 1] = job
	end

	function self:tick()
		if self.in_flight then
			return
		end

		if self.pending_retry then
			if uni2.time() < self.retry_at then
				return
			end
			local job = self.pending_retry
			self.pending_retry = nil
			dispatch(job)
			return
		end

		if #self.queue == 0 then
			return
		end

		local next_job = table.remove(self.queue, 1)
		dispatch(next_job)
	end

	function self:pending()
		local count = #self.queue
		if self.in_flight then
			count = count + 1
		end
		if self.pending_retry then
			count = count + 1
		end
		return count
	end

	function self:put_room(lobby, owner, cb)
		self:enqueue({
			method = "PUT",
			path = "/room",
			content_type = "application/json",
			body = protocol.room_body(lobby, owner),
			callback = cb,
		})
	end

	function self:put_game(number, winner_role, host_chara, guest_chara, file_name, replay_bytes, cb)
		local boundary = string.format("----RankWars%08x%s", uni2.time() % 0xFFFFFFFF, self.steam_id)
		local content_type, body = protocol.game_form(boundary, replay_bytes, file_name, winner_role, host_chara, guest_chara)
		self:enqueue({
			method = "PUT",
			path = "/games/" .. tostring(number),
			content_type = content_type,
			body = body,
			game_number = number,
			callback = cb,
		})
	end

	function self:abort(reason, cb)
		self:enqueue({
			method = "POST",
			path = "/abort",
			content_type = "application/json",
			body = protocol.abort_body(reason),
			callback = cb,
		})
	end

	function self:fetch_match(cb)
		self:enqueue({
			method = "GET",
			path = "/match",
			callback = cb,
		})
	end

	function self:create_sandbox(password, first_to, cb)
		self:enqueue({
			method = "POST",
			path = "/debug/session",
			content_type = "application/json",
			body = protocol.sandbox_body(password, first_to),
			retries = false,
			callback = cb,
		})
	end

	function self:fetch_replay(replay_id, cb)
		self:enqueue({
			method = "GET",
			path = "/replays/" .. replay_id,
			accept = "application/octet-stream",
			callback = cb,
		})
	end

	return self
end

return M
