local log_mod = require("mod/log")
local log = log_mod.log
local log_info = log_mod.log_info
local ai  = require("mod/ai")

local ai_mode = {
    target_search = 0,
    saving_life = 1,
    COUNT = 2
}

local insane_ai = {}
local players_data = {}

insane_ai.init = function(ai_operator)
    players_data[ai_operator.player_name] = {
        movement   = ai.movement.off,
        ai_mode    = ai_mode.target_search,
        jump_timer = timer.new(),
    }
end

---@param ai_operator ai_operator_t
---@param data ai_data_t
insane_ai.update = function(ai_operator, data)
    local pl_data = players_data[ai_operator.player_name]
    insane_ai_update(ai_operator, data.players[ai_operator.player_name], data, pl_data)
end

AI = {
    insane = insane_ai
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
            plr_data.ai_mode ~= ai_mode.saving_life)

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
            op:produce_action(ai.action.enable_long_shot)
        else
            op:produce_action(ai.action.disable_long_shot)
        end
    end

    -- Change direction if it required
    if change_dir then
        if change_dir < 0 then
            op:produce_action(ai.action.move_left)
        else
            op:produce_action(ai.action.move_right)
        end
        op:produce_action(ai.action.stop)
    end

    if switch_target_required then
        if plr_data.target_name then
            op:produce_action(ai.action.shot)
        else
            -- Schedule relax
            plr_data.relax_timer = timer.new()
        end
    end

    -- Produce relax on timeout
    if plr_data.relax_timer and
       plr_data.relax_timer:elapsed() >= insane_shooter_relax_timeout * (plr.gun_fire_rate / 1000) then
        plr_data.relax_timer = nil
        op:produce_action(ai.action.relax)
    end

    return target, too_close
end

---@param op ai_operator_t
---@param plr ai_player_t
---@param world ai_data_t
---@param plr_data table<string, any>
local function insane_survival(op, plr, world, plr_data)
    local plr_pos = plr.pos
    --local plr_sz = plr.size
    local plr_center = plr.pos.x + plr.size.x * 0.5

    local _, falling_on, nearest = ai.find_active_platforms(plr, world)
    if falling_on then
        local vel_x = plr.vel.x
        local plat_center = (falling_on.pos1.x + falling_on.pos2.x) * 0.5

        if ai.is_flying_to_death(plr, world) then
            if plr.vel.x > plr.max_vel_x * 0.2 then
                ai.move_left(op, plr, plr_data)
            elseif plr.vel.x < -plr.max_vel_x * 0.2 then
                ai.move_right(op, plr, plr_data)
            end
            ai.jump(op, plr, plr_data)
        end

        if vel_x > plr.max_vel_x * 1.1 and plr_center > plat_center then
            plr_data.ai_mode = ai_mode.saving_life
            ai.move_left(op, plr, plr_data)
        elseif vel_x < -plr.max_vel_x * 1.1 and plr_center < plat_center then
            plr_data.ai_mode = ai_mode.saving_life
            ai.move_right(op, plr, plr_data)
        elseif math.abs(vel_x) < plr.max_vel_x * 0.25 or
               (vel_x > 0 and plr_data.movement == ai.movement.right) or
               (vel_x < 0 and plr_data.movement == ai.movement.left) then
            plr_data.ai_mode = ai_mode.target_search
            ai.stop(op, plr, plr_data)
        end
    else
        plr_data.ai_mode = ai_mode.saving_life

        if plr_pos.y > nearest.pos1.y then
            ai.jump(op, plr, plr_data)
        end
        if plr_pos.x > nearest.pos2.x then
            ai.move_left(op, plr, plr_data)
        else
            ai.move_right(op, plr, plr_data)
        end
    end
end

---@param op ai_operator_t
---@param plr ai_player_t
---@param world ai_data_t
---@param plr_data table<string, any>
function insane_ai_update(op, plr, world, plr_data)
    target, too_close = insane_shooter(op, plr, world, plr_data)
    insane_survival(op, plr, world, plr_data)
end
