local log = {}

---comment
---@alias level_t '"info"'|'"warn"'|'"err"'|'"upd"'|'"infoupd"'|'"warnupd"'|'"errupd"'
---@param level level_t
---@param format string
function log.log(level, format, ...)
    log_impl(level, string.format(format, ...))
end

return log
