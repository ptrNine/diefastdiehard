local log = require("mod/log").log
local ai  = require("mod/ai")

local ai_action = {
    move_left = 0,
    move_right = 1,
    stop = 2,
    shot = 3,
    relax = 4,
    jump = 5,
    jump_down = 6,
    enable_long_shot = 7,
    disable_long_shot = 8,
    COUNT = 9
}

local ai_mode = {
    target_search = 0,
    saving_life = 1,
    COUNT = 2
}

local ai_movement = {
    off = 0,
    left = 1,
    right = 2,
    COUNT = 3
}

local dummy_ai = {}
local players_data = {}

dummy_ai.init = function(ai_operator)
    players_data[ai_operator.player_name] = {
        movement = ai_movement.off
    }
end

---@param ai_operator ai_operator_t
---@param data ai_data_t
dummy_ai.update = function(ai_operator, data)
    local pl_data = players_data[ai_operator.player_name]
    dummy_ai_update(ai_operator, data.players[ai_operator.player_name], data, pl_data)
end

AI = {
    dummy = dummy_ai
}

local insane_shooter_relax_timeout    = 0.5
local insane_shooter_player_size_coef = 1.0

---@param op ai_operator_t
---@param plr ai_player_t
---@param world ai_data_t
---@param plr_data table<string, any>
---@return ai_player_t?
---@return boolean?
local function insane_shooter(op, plr, world, plr_data)
    local target, change_dir, long_shot_setting, too_close =
        ai.find_victim(plr, world, insane_shooter_player_size_coef,
            plr_data.movement == ai_movement.off)

    local switch_target_required = false
    -- Switch target if it required
    if not target and plr_data.target_name then
        switch_target_required = true
        plr_data.target_name = nil
    elseif target and target.name ~= plr_data.target_name then
        switch_target_required = true
        plr_data.target_name = target.name
    end

    -- Enable/disable long shot mode if it required
    if long_shot_setting ~= nil then
        if long_shot_setting then
            op:produce_action(ai_action.enable_long_shot)
        else
            op:produce_action(ai_action.disable_long_shot)
        end
    end

    -- Change direction if it required
    if change_dir then
        if change_dir < 0 then
            op:produce_action(ai_action.move_left)
        else
            op:produce_action(ai_action.move_right)
        end
        op:produce_action(ai_action.stop)
    end

    if switch_target_required then
        if plr_data.target_name then
            op:produce_action(ai_action.shot)
        else
            -- Schedule relax
            plr_data.relax_timer = timer.new()
        end
    end

    -- Produce relax on timeout
    if plr_data.relax_timer and
       plr_data.relax_timer:elapsed() >= insane_shooter_relax_timeout * (plr.gun_fire_rate / 1000) then
        plr_data.relax_timer = nil
        op:produce_action(ai_action.relax)
    end

    return target, too_close
end

---@param op ai_operator_t
---@param plr ai_player_t
---@param world ai_data_t
---@param plr_data table<string, any>
local function insane_survival(op, plr, world, plr_data)
    local all_is_ok = true



    return all_is_ok
end

---@param op ai_operator_t
---@param plr ai_player_t
---@param world ai_data_t
---@param plr_data table<string, any>
function dummy_ai_update(op, plr, world, plr_data)
    target, too_close = insane_shooter(op, plr, world, plr_data)
    insane_survival(op, plr, world, plr_data)
end
