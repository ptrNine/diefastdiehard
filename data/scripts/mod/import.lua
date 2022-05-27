local log = require("mod/log").log

local function import(modname)
    local mod = require(modname)

    for k,v in pairs(mod) do
        if _G[k] then
            log("warn", "symbol '%s' already exists in global namespace", k)
        else
            _G[k] = v
        end
    end
end

return import
