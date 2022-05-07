local ai = {}

local log = require("mod/log").log

---@param lhs vec2f
---@param rhs vec2f
---@return number
local function cross2z(lhs, rhs)
    return lhs.x * rhs.y - lhs.y * rhs.x
end

---@param n number
---@return boolean
local function sign(n)
   return n < 0
end


---@param start_pos vec2f
---@param next_pos vec2f
---@param platform ai_platform_t
---@return vec2f?
local function fall_on_platform(start_pos, next_pos, platform)
    local c1 = next_pos - start_pos
    local c2 = platform.pos2 - platform.pos1

    local pz1 = cross2z(c1, platform.pos1 - start_pos)
    local pz2 = cross2z(c1, platform.pos2 - start_pos)

    if sign(pz1) == sign(pz2) or pz1 == 0 or pz2 == 0 then
        return nil
    end

    pz1 = cross2z(c2, start_pos - platform.pos1)
    pz2 = cross2z(c2, next_pos - platform.pos1)

    if sign(pz1) == sign(pz2) or pz1 == 0 or pz2 == 0 then
        return nil
    end

    return vec2f.new(start_pos.x + c1.x * math.abs(pz1) / math.abs(pz2 - pz1),
                     start_pos.y + c1.y * math.abs(pz1) / math.abs(pz2 - pz1))
end

---@param start_pos vec2f
---@param next_pos vec2f
---@param world ai_data_t
---@return vec2f?
local function fall_on_platforms(start_pos, next_pos, world)
    local result  = nil
    local min_y = math.huge

    for _, platform in pairs(world.platforms) do
        local res = fall_on_platform(start_pos, next_pos, platform)
        if res and res.y < min_y then
            result = res
            min_y = res.y
        end
    end

    return result
end

---@param plr ai_player_t
---@param target ai_player_t
---@param world ai_data_t
---@param player_size_coef number
---@return number
local function get_actual_damage(plr, target, world, player_size_coef)
    local dmg = 0
    local dir_sign = plr.barrel_pos.x > target.pos.x
    local tpos = target.pos + vec2f.new(target.size.x * 0.5, -target.size.y * 0.5)

    for _, bullet in pairs(world.bullets) do
        if bullet.group == plr.group and dir_sign == (bullet.vel.x < 0) then
            local bpos = bullet.pos
            local bvel = bullet.vel

            local delta_s = tpos.x - bpos.x       ---@type number
            local delta_v = bvel.x - target.vel.x ---@type number
            local t       = delta_s / delta_v     ---@type number

            local g_s = world.physic_sim.gravity * t * t * 0.5
            local tpos_next = tpos + target.vel * t + (target.is_y_locked and vec2f.new(0) or g_s)
            ---@type number
            local bpos_next_y = bpos.y + bvel.y * t +
                (world.physic_sim.enable_gravity_for_bullets and g_s.y or 0)

            -- Check for platforms
            if tpos_next.y > tpos.y then
                local fall_position = fall_on_platforms(tpos, tpos_next, world)
                if fall_position then
                    tpos_next.x = fall_position.x + target.size.x * 0.5
                    tpos_next.y = fall_position.y - target.size.y * 0.5
                end
            end

            -- Distance from meet point to target center relative to y
            local y_dist = math.abs(bpos_next_y - tpos_next.y)

            -- The target becomes archievable if distance to the target center
            -- less than half height of the target
            if y_dist < (target.size.y * 0.5) * player_size_coef then
                dmg = dmg + bullet.hit_mass * math.abs(bullet.vel.x) * 0.001
            end
        end
    end

    --log("infoupd", "dmg: %s", dmg)
    return dmg
end

---@param plr ai_player_t
---@param world ai_data_t
---@return boolean
function ai.is_flying_to_death(plr, world)
    local lvl_sz = world.level.level_size
    local center = lvl_sz.x * 0.5
    local t = math.abs(plr.vel.x) / plr.x_accel
    local pos = center

    if plr.pos.x < center then
        pos = plr.pos.x + plr.vel.x * t + plr.x_accel * t * t * 0.5
    else
        pos = plr.pos.x + plr.vel.x * t + -plr.x_accel * t * t * 0.5
    end

    local res = pos < 0 or pos > lvl_sz.x
    log("infoupd", "death: %s", res)

    return res
end

---@param plr ai_player_t
---@param world ai_data_t
---@param player_size_coef number?
---@param change_dir_allowed boolean
---@return ai_player_t?
---@return number?
---@return boolean?
---@return boolean?
function ai.find_victim(plr, world, player_size_coef, change_dir_allowed)
    --log("infoupd", "accel: %s vel: %s", plr.acceleration, plr.vel)
    if change_dir_allowed == nil then
        change_dir_allowed = true
    end

    if not player_size_coef then
        player_size_coef = 1
    end

    local result                = nil     ---@type ai_player_t?
    local res_dist              = math.huge
    local res_change_dir        = nil     ---@type number?
    local res_long_shot_setting = nil     ---@type boolean?
    local res_too_close         = nil     ---@type boolean?

    local bullet_gravity_enabled = world.physic_sim.enable_gravity_for_bullets

    for _, target in pairs(world.players) do
        if target.group ~= plr.group and
           not ai.is_flying_to_death(target, world) and
           get_actual_damage(plr, target, world, player_size_coef) < 1 then
            ---@type number?
            local change_dir = nil
            ---@type vec2f
            local tpos = target.pos + vec2f.new(target.size.x * 0.5, -target.size.y * 0.5)
            ---@type vec2f
            local twidth = target.size.x * 0.5
            ---@type vec2f
            local ppos = plr.pos + vec2f.new(plr.size.x * 0.5, -plr.size.y * 0.5)
            local bpos = plr.barrel_pos

            -- Determine direction to target
            local target_dir = vec2f.new(-1, 0)
            if ppos.x < tpos.x then
                target_dir = vec2f.new(1, 0)
            end

            -- Is direction change needed
            if target_dir.x < 0 and not plr.on_left then
                change_dir = -1
            elseif target_dir.x > 0 and plr.on_left then
                change_dir = 1
            end

            -- Skip if change direction is required and not allowed
            if change_dir ~= nil and not change_dir_allowed then
                goto next_target
            end

            -- Recalculate barrel position if direction change is needed
            if change_dir then
                local barrel_displacement = bpos - ppos
                bpos = vec2f.new(ppos.x - barrel_displacement.x, ppos.y + barrel_displacement.y)
            end

            -- Is target too close to shot
            ---@type boolean
            local too_close = (target_dir.x < 0 and bpos.x < tpos.x - twidth) or
                              (target_dir.x > 0 and bpos.x > tpos.x + twidth)

            if not too_close then
                -- TODO: handle target acceleration (maybe it's useless)

                local long_shot_iter = false
                local dirs = {target_dir}
                if bullet_gravity_enabled then
                    long_shot_iter = true
                    -- Fix long_shot_dir if direction changed
                    local long_shot_dir = plr.long_shot_dir
                    if change_dir then
                        long_shot_dir.x = -long_shot_dir.x
                    end

                    dirs = {long_shot_dir, target_dir}
                end

                for _, bdir in pairs(dirs) do
                    -- Find meet time relative to x
                    -- From Sbullet + Vbullet * t = Starget + Vtarget * t ->
                    --      t = (Starget - Sbullet) / (Vbullet - Vtarget)
                    local delta_s = tpos.x - bpos.x                             ---@type number
                    local delta_v = bdir.x * plr.gun_bullet_vel - target.vel.x  ---@type number
                    local t       = delta_s / delta_v                           ---@type number
                    local g_s     = world.physic_sim.gravity * t * t * 0.5      ---@type vec2f

                    -- Find meet y positions
                    -- pos = S + Vt + gt^2/2
                    ---@type vec2f
                    local tpos_next = tpos + target.vel * t + (target.is_y_locked and vec2f.new(0) or g_s)
                    ---@type number
                    local bpos_next_y = bpos.y + bdir.y * plr.gun_bullet_vel * t + (bullet_gravity_enabled and g_s.y or 0)

                    -- Check for platforms
                    if tpos_next.y > tpos.y then
                        local fall_position = fall_on_platforms(tpos, tpos_next, world)
                        if fall_position then
                            tpos_next.x = fall_position.x + target.size.x * 0.5
                            tpos_next.y = fall_position.y - target.size.y * 0.5
                        end
                    end

                    -- Distance from meet point to target center relative to y
                    local y_dist = math.abs(bpos_next_y - tpos_next.y)

                    -- The target becomes archievable if distance to the target center
                    -- less than half height of the target
                    if y_dist < (target.size.y * 0.5) * player_size_coef then
                        local dist = (tpos - bpos):magnitude() ---@type number

                        -- Set the current target if it is closer
                        if dist < res_dist then
                            result               = target
                            res_dist             = dist
                            res_change_dir       = change_dir
                            res_too_close        = too_close

                            if bullet_gravity_enabled then
                                if plr.long_shot_enabled and not long_shot_iter then
                                    res_long_shot_setting = false
                                elseif not plr.long_shot_enabled and long_shot_iter then
                                    res_long_shot_setting = true
                                else
                                    res_long_shot_setting = nil
                                end
                            end

                            break
                        end
                    end

                    --[[
                    -- Meet time
                    local t = nil ---@type number?
                    local t_on_max_accel = 0
                    local target_vel_max = 0   ---@type number?
                    local tpos_next_x    = nil ---@type number?

                    if target.acceleration.x > 0.001 or target.acceleration.x < -0.001 then
                        local bvel = bdir * plr.gun_bullet_vel ---@type vec2f

                        local ka = target.acceleration.x * 0.5 ---@type number
                        local kb = target.vel.x - bvel.x       ---@type number
                        local kc = tpos.x - bpos.x             ---@type number
                        local d  = kb * kb - 4 * ka * kc;      ---@type number

                        local solve_iteratively = false
                        ---@type number
                        local a = target.acceleration.x + world.physic_sim.gravity.x

                        if d > 0 then
                            local sqrt_d = math.sqrt(d)
                            local t1 = (-kb - sqrt_d) / (2 * ka) ---@type number
                            local t2 = (-kb + sqrt_d) / (2 * ka) ---@type number
                            t = t2
                            if t1 > 0 then
                                t = t1
                            end

                            -- Get result target velocity
                            ---@type number
                            local res_vel = target.vel.x + a * t

                            -- Try to solve iteratively if the velocity limit is violated
                            if (res_vel < -target.max_vel_x and target.vel.x > -target.max_vel_x) or
                               (res_vel > target.max_vel_x and target.vel.x < target.max_vel_x) then
                               solve_iteratively = true
                            else
                                tpos_next_x = tpos.x + target.vel.x * t + a * t * t * 0.5
                            end
                        else
                            solve_iteratively = true
                        end


                        if solve_iteratively then
                            if not t then
                                t = 3
                            end

                            local move_left = true
                            if a > 0 then
                                move_left = false
                            end

                            if target.is_walking then
                                target_vel_max = move_left and -target.max_vel_x or target.max_vel_x
                            end
                            local delta_v = target_vel_max - target.vel.x

                            ---@type number
                            local t_part1 = delta_v / a
                            ---@type number
                            local tpos_acc = tpos.x + target.vel.x * t_part1 +
                                a * t_part1 * t_part1 * 0.5
                            ---@type number
                            local bpos_acc = bpos.x + bvel.x * t_part1 +
                                world.physic_sim.gravity.x * t_part1 * t_part1 * 0.5

                            local t_step  = 0.02
                            local t_part2 = 0

                            ---@type number
                            local hit_dist_x = target.size.x * 0.5 * player_size_coef
                            local closest_dist = math.huge

                            while t_part1 + t_part2 < t * 1.2 do
                                ---@type vec2f
                                tpos_acc = tpos_acc + target_vel_max * t_step
                                bpos_acc = bpos_acc + bvel.x * t_step
                                t_part2 = t_part2 + t_step

                                local dist = math.abs(bpos_acc - tpos_acc)
                                if dist < hit_dist_x then
                                    if dist < closest_dist then
                                        closest_dist = dist
                                        t_on_max_accel = t_part2
                                        tpos_next_x = tpos_acc
                                    end
                                end
                            end

                            if closest_dist > hit_dist_x then
                                t = nil
                            end
                            --if closest_dist < hit_dist_x then
                                --log("info", "HIT! time: %s tpos: %s bpos: %s meet: %s", t + t_on_max_accel, tpos.x, bpos.x, tpos_acc)
                            --else
                                --log("info", "TRY: delta: %s closest: %s", delta_v, closest_dist)
                            --end
                        end
                    else
                        local tm = (bpos.x - tpos.x) / (target.vel.x - bdir.x * plr.gun_bullet_vel)
                        if tm > 0 then
                            t = tm
                            tpos_next_x = tpos.x + target.vel.x * t
                        end
                    end

                    local dist_to_target_x = tpos_next_x and math.abs(tpos_next_x - bpos.x) or nil

                    if t and dist_to_target_x and dist_to_target_x < res_dist then
                        local ut = t + t_on_max_accel
                        local bpos_next_y = bpos.y + bdir.y * plr.gun_bullet_vel * ut +
                            world.physic_sim.gravity.y * ut * ut * 0.5
                        local tpos_next_y = tpos.y + target.vel.y * t +
                            (target.acceleration.y +
                              (target.is_y_locked and 0 or world.physic_sim.gravity.y)) *
                                t * t * 0.5
                        tpos_next_y = tpos_next_y + target_vel_max * t_on_max_accel
                        local tpos_next = vec2f.new(tpos_next_x, tpos_next_y)

                        -- Check for platforms
                        if tpos_next_y > tpos.y then
                            local fall_position = fall_on_platforms(tpos, tpos_next, world)
                            if fall_position then
                                tpos_next.y = fall_position.y - target.size.y * 0.5
                            end
                        end

                        local hit_dist_y = target.size.y * 0.5 * player_size_coef
                        local dist = math.abs(bpos_next_y - tpos_next.y)
                        if dist < hit_dist_y then
                            if bullet_gravity_enabled then
                                if plr.long_shot_enabled and not long_shot_iter then
                                    res_long_shot_setting = false
                                elseif not plr.long_shot_enabled and long_shot_iter then
                                    res_long_shot_setting = true
                                else
                                    res_long_shot_setting = nil
                                end
                            end

                            result = target
                            res_change_dir = change_dir
                            res_too_close = too_close
                            res_dist = dist_to_target_x
                            break
                        end
                    end
                    ]]--

                    long_shot_iter = false
                end
            end

            ::next_target::
        end
    end

    return result, res_change_dir, res_long_shot_setting, res_too_close
end

return ai
