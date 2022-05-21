local log = {}

---comment
---@alias level_t '"info"'|'"warn"'|'"err"'|'"upd"'|'"infoupd"'|'"warnupd"'|'"errupd"'
---@param level level_t
---@param format string
---@vararg any
function log.log(level, format, ...)
    log_impl(level, string.format(format, ...))
end

---comment
---@param format string
---@vararg any
function log.log_info(format, ...)
    log_impl("info", string.format(format, ...))
end

---comment
---@param format string
---@vararg any
function log.log_infoupd(format, ...)
    log_impl("infoupd", string.format(format, ...))
end

---comment
---@param format string
---@vararg any
function log.log_warn(format, ...)
    log_impl("warn", string.format(format, ...))
end

---comment
---@param format string
---@vararg any
function log.log_warnupd(format, ...)
    log_impl("warnupd", string.format(format, ...))
end

---comment
---@param format string
---@vararg any
function log.log_err(format, ...)
    log_impl("err", string.format(format, ...))
end

---comment
---@param format string
---@vararg any
function log.log_errupd(format, ...)
    log_impl("errupd", string.format(format, ...))
end

return log
