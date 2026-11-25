local session = require("session")
local hud = require("hud")
local ui = require("ui")

uni2.ui.register_window("rank_wars", "Rank Wars", {
	summary = "Automated ranked matches, live score pips HUD and replay uploads.",
	width = 440,
	height = 400,
	min_width = 380,
	min_height = 280,
	max_width = 560,
	max_height = 680,
	open = false,
}, function()
	ui.draw()
end)

session.initialize()

uni2.on("frame", function()
	session.on_frame()
end)

uni2.on("hud", function()
	hud.draw(session.scoreboard)
end)

uni2.on("game_started", function()
	session.on_game_started()
end)

uni2.on("game_ended", function(report)
	session.on_game_ended(report)
end)

uni2.log("Rank Wars Lua add-on initialized")
