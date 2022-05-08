local log = require("mod/log").log

cmd [[
    level current lvl_aes
    sound volume 20

    player create 'name=lol group=1'
    player controller0 lol
    player wpn lol wpn_vss

    player create mda
    player wpn mda wpn_vss
    ai bind mda
    ai difficulty mda hard

    player create "name=mda1 group=0"
    player wpn mda1 wpn_vss
    ai bind mda1
    ai difficulty mda1 hard

    player create "name=mda2 group=0"
    player wpn mda2 wpn_vss
    ai bind mda2
    ai difficulty mda2 hard

    player create "name=mda3 group=1"
    player wpn mda3 wpn_vss
    ai bind mda3
    ai difficulty mda3 hard
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
