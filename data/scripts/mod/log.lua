local log = {}

---@alias level_t '"debug"'|'"detail"'|'"info"'|'"warn"'|'"err"'
---@param level level_t
---@param format string
---@vararg any
function log.log(level, format, ...)
    log_impl(level, string.format(format, ...))
end

---@param update_id integer
---@param level level_t
---@param format string
---@vararg any
function log.log_update(update_id, level, format, ...)
    log_update_impl(level, string.format(format, ...))
end

---@param format string
---@vararg any
function log.debug(format, ...)
    log_impl("debug", string.format(format, ...))
end

---@param update_id integer
---@param format string
---@vararg any
function log.debugupd(update_id, format, ...)
    log_update_impl(update_id, "debug", string.format(format, ...))
end

---@param format string
---@vararg any
function log.detail(format, ...)
    log_impl("detail", string.format(format, ...))
end

---@param update_id integer
---@param format string
---@vararg any
function log.detailupd(update_id, format, ...)
    log_update_impl(update_id, "detail", string.format(format, ...))
end

---@param format string
---@vararg any
function log.info(format, ...)
    log_impl("info", string.format(format, ...))
end

---@param update_id integer
---@param format string
---@vararg any
function log.infoupd(update_id, format, ...)
    log_update_impl(update_id, "info", string.format(format, ...))
end

---@param format string
---@vararg any
function log.warn(format, ...)
    log_impl("warn", string.format(format, ...))
end

---@param update_id integer
---@param format string
---@vararg any
function log.warnupd(update_id, format, ...)
    log_update_impl(update_id, "warn", string.format(format, ...))
end

---@param format string
---@vararg any
function log.err(format, ...)
    log_impl("err", string.format(format, ...))
end

---@param update_id integer
---@param format string
---@vararg any
function log.errupd(update_id, format, ...)
    log_update_impl(update_id, "err", string.format(format, ...))
end

return log
