local log = require("mod/log").log

cmd [[
    level current lvl_aes
    player create 'name=lol group=1'
    player controller0 lol
    ai bind lol
    ai difficulty lol dummy
    player create mda
    ai bind mda
    ai difficulty mda easy
    game speed 1
]]

GS.pause = false

--[[
G = {}

G.game_update = function()
end

G.handle_event = function(evt)
    if evt.type == "KeyPressed" and evt.keycode == "G" then
        log("info", "%s", Cfg)
    end
end
--]]
