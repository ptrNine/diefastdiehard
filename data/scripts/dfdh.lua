cmd [[
    level current lvl_aes
    player create kek
    player controller0 kek
    player create 'name=lol group=1'
    ai bind lol
    ai difficulty lol hard
]]


GS.pause = false

G = {}
G.game_update = function()
end

G.handle_event = function(evt)
    if evt.type == "KeyPressed" and evt.keycode == "G" then
        log("info", tostring(Cfg))
    end
end
