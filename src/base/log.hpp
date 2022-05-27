#pragma once

#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <memory>
#include <map>
#include <chrono>
#include <optional>
#include <deque>
#include <cstring>

//#include "string_hash.hpp"
//#include "time.hpp"
//#include "helper_macros.hpp"

#include "io.hpp"
#include "print.hpp"
#include "types.hpp"
#include "time.hpp"
#include "split_view.hpp"
#include "ring_buffer.hpp"
//#include "container_extensions.hpp"

namespace dfdh
{

inline constexpr auto LOG_TIME_FMT = "[hh:mm:ss.xxx]"sv;

class log_fixed_buffer {
public:
    static std::shared_ptr<log_fixed_buffer> create(size_t imax_size) {
        return std::make_shared<log_fixed_buffer>(imax_size);
    }

    log_fixed_buffer(size_t imax_size): lines(imax_size) {}

    void newline() {
        lines.skip();
        last_pos = 0;
    }

    void write(const char* str, std::streamsize size) {
        std::lock_guard lock{mtx};

        auto sv = std::string_view(str, size_t(size));

        if (size && str[0] == '\n') {
            newline();
            ++str;
        }

        size_t count = 0;
        for (auto line : sv / split(split_mode::allow_empty, '\n')) {
            if (count > 1)
                newline();
            ++count;

            auto sv_line = std::string_view(line.begin(), line.end());
            std::string& write_line = lines.back();

            if (!sv_line.empty() && sv_line.front() == '\r') {
                sv_line = std::string_view(sv_line.begin() + 1, sv_line.end());
                last_pos = 0;
            }

            size_t return_count = 0;
            for (auto subl : sv_line / split(split_mode::allow_empty, '\r')) {
                if (return_count > 1)
                    last_pos = 0;
                ++return_count;

                auto sz = size_t(subl.end() - subl.begin());
                if (last_pos + sz > write_line.size()) {
                    write_line.resize(last_pos + sz);
                }

                std::memcpy(write_line.data() + last_pos, subl.begin(), sz);
                last_pos += sz;
            }

            if (sv_line.size() > 1 && sv_line.back() == '\r')
                last_pos = 0;
        }

        if (size > 1 && str[size - 1] == '\n')
            lines.skip();
    }

    void seekp(ssize_t offset, std::ios_base::seekdir dir) {
        if (offset > 0)
            throw std::runtime_error("Only negative offset supported");
        if (dir != std::ios_base::end)
            throw std::runtime_error("Only std::ios_base::end supported");

        auto off = size_t(-offset);

        std::lock_guard lock{mtx};

        while (!lines.empty() && off != 0) {
            auto& back = lines.back();
            if (off <= back.size()) {
                back.resize(back.size() - off);
                off = 0;
            } else {
                off -= back.size() + 1;
                lines.pop();
            }
        }

        if (lines.empty())
            lines.skip();
    }

    template <typename I>
    struct locker_log_range {
        locker_log_range(I ibegin, I iend, std::mutex& mtx):
            b(std::move(ibegin)), e(std::move(iend)), _mtx(mtx) {}

        ~locker_log_range() {
            _mtx.unlock();
        }

        locker_log_range(const locker_log_range&) = delete;
        locker_log_range& operator=(const locker_log_range&) = delete;
        locker_log_range(locker_log_range&&) = delete;
        locker_log_range& operator=(locker_log_range&&) = delete;

        [[nodiscard]]
        auto begin() const {
            return b;
        }

        [[nodiscard]]
        auto end() const {
            return e;
        }

        I b, e;
        std::mutex& _mtx;
    };

    auto get_log_locked(size_t max_lines) {
        mtx.lock();
        size_t start = max_lines < lines.size() ? lines.size() - max_lines : 0;
        return locker_log_range(lines.begin() + ssize_t(start), lines.end(), mtx); // NOLINT
    }

    [[nodiscard]]
    size_t size() const {
        std::lock_guard lock{mtx};
        return lines.size();
    }

    [[nodiscard]]
    size_t max_size() const {
        std::lock_guard lock{mtx};
        return lines.max_size();
    }

    void resize_buf(size_t value) {
        lines.resize(value);
    }

    void clear() {
        std::lock_guard lock{mtx};
        lines.clear();
        last_pos = 0;
    }

    void flush() {
        /* Do nothing */
    }

private:
    ring_buffer<std::string> lines;
    size_t                   last_pos = 0;
    mutable std::mutex       mtx;
};

namespace log_dtls {
    struct holder_base {
        holder_base()          = default;
        virtual ~holder_base() = default;

        holder_base(const holder_base&) = delete;
        holder_base(holder_base&&)      = delete;

        holder_base& operator=(const holder_base&) = delete;
        holder_base& operator=(holder_base&&) = delete;

        virtual void write(std::string_view data) = 0;
        virtual void flush()                 = 0;
        virtual bool try_drop_postfix(size_t count) = 0;

        [[nodiscard]] virtual bool is_file() const               = 0;
        [[nodiscard]] virtual std::optional<std::string> to_string() const = 0;
    };

    template <typename T>
    struct holder : holder_base {
        template <typename U>
        holder(U&& obj): object(std::forward<U>(obj)) {}

        void write(std::string_view data) override {
            object.write(data.data(), static_cast<std::streamsize>(data.size()));
        }

        void flush() override {
            object.flush();
        }

        [[nodiscard]] std::optional<std::string> to_string() const override {
            if constexpr (std::is_same_v<T, std::stringstream>)
                return object.str();
            else
                return std::nullopt;
        }

        [[nodiscard]] bool is_file() const override {
            return false;
        }

        bool try_drop_postfix(size_t count) override {
            if constexpr (std::is_same_v<T, std::stringstream> ||
                          std::is_same_v<T, std::ofstream> || std::is_same_v<T, std::fstream>) {
                object.seekp(-static_cast<ssize_t>(count), std::ios_base::end);
                return true;
            }
            else
                return false;
        }

        T object;
    };

    template <>
    struct holder<std::weak_ptr<log_fixed_buffer>> : holder_base {
        holder(std::weak_ptr<log_fixed_buffer> o): os(move(o)) {}

        void write(std::string_view data) override {
            if (auto o = os.lock())
                o->write(data.data(), static_cast<std::streamsize>(data.size()));
        }

        void flush() override {}

        [[nodiscard]]
        std::optional<std::string> to_string() const override {
            return {};
        }

        [[nodiscard]]
        bool is_file() const override {
            return false;
        }

        bool try_drop_postfix(size_t) override {
            return false;
        }

        std::weak_ptr<log_fixed_buffer> os;
    };

    template <>
    struct holder<std::weak_ptr<std::ostream>> : holder_base {
        holder(std::weak_ptr<std::ostream> o): os(move(o)) {}

        void write(std::string_view data) override {
            if (auto o = os.lock())
                o->write(data.data(), static_cast<std::streamsize>(data.size()));
        }

        void flush() override {
            if (auto o = os.lock())
                o->flush();
        }

        [[nodiscard]]
        std::optional<std::string> to_string() const override {
            if (auto o1 = os.lock())
                if (auto o = std::dynamic_pointer_cast<std::stringstream>(o1))
                    return o->str();
            return nullptr;
        }

        [[nodiscard]] bool is_file() const override {
            if (auto o1 = os.lock(); o1 && (std::dynamic_pointer_cast<std::fstream>(o1) ||
                                            std::dynamic_pointer_cast<std::ofstream>(o1)))
                return true;
            else
                return false;
        }

        bool try_drop_postfix(size_t count) override {
            if (auto o = os.lock()) {
                o->seekp(-static_cast<ssize_t>(count), std::ios_base::end);
                return true;
            }
            else
                return false;
        }

        std::weak_ptr<std::ostream> os;
    };
}

/**
 * @brief Represetns a generic stream
 */
class log_output_stream {
public:
    log_output_stream(): _holder(std::make_unique<log_dtls::holder<std::ofstream>>(nullptr)) {}
    log_output_stream(std::ofstream ofs): _holder(make_unique<log_dtls::holder<std::ofstream>>(move(ofs))) {}
    log_output_stream(std::ostream& os): _holder(make_unique<log_dtls::holder<std::ostream&>>(os)) {}
    //log_output_stream(std::stringstream ss): _holder(make_unique<holder<std::stringstream>>(move(ss))) {}
    log_output_stream(std::weak_ptr<std::ostream> os):
        _holder(make_unique<log_dtls::holder<std::weak_ptr<std::ostream>>>(move(os))) {}
    log_output_stream(std::weak_ptr<log_fixed_buffer> os):
        _holder(make_unique<log_dtls::holder<std::weak_ptr<log_fixed_buffer>>>(move(os))) {}

    /**
     * @brief Creates the log stream with with stdout
     *
     * @return the log stream
     */
    static log_output_stream std_out() {
        return {std::cout};
    }

    /**
     * @brief Writes a string to the stream
     *
     * @param data - the string to be written
     */
    void write(std::string_view data) {
        _holder->write(data);
    }

    /**
     * @brief Flush the stream
     */
    void flush() {
        _holder->flush();
    }

    /**
     * @brief Casts the stream to the string if it possible
     *
     * @return the optional with the stream data or nullopt
     */
    [[nodiscard]]
    std::optional<std::string> to_string() const {
        return _holder->to_string();
    }

    /**
     * @brief determines if the stream is a file
     *
     * @return true if stream is a file
     */
    [[nodiscard]]
    bool is_file() const {
        return _holder->is_file();
    }

    bool try_drop_postfix(size_t count) {
        return _holder->try_drop_postfix(count);
    }

private:
    std::unique_ptr<log_dtls::holder_base> _holder;
};


/**
 * @brief Represetns a logger
 *
 * That implementation sucks and will be refactored or replaced by some external lib
 *
 * All member functions is thread-safe (mutex blocking)
 */
class logger {
public:
    logger(const logger&) = delete;
    logger& operator=(const logger&) = delete;

    static logger& instance() {
        static logger inst;
        return inst;
    }

private:
    /**
     * @brief Constructs logger with std::cout stream
     */
    logger() {
        add_output_stream("stdout", std::cout);
    }

    ~logger() {
        write(Message, "*** LOG END ***\n");
    }


public:
    enum Type { Details = 0, Message = 1, Warning, Error };

    static constexpr std::array<std::string_view, 4> str_types = {": [info] ", ": ", ": [warn] ", ": [error] "};

    /**
     * @brief Flush all logger streams
     */
    void flush() {
        std::lock_guard lock(mtx);
        for (auto& val : _streams) val.second.flush();
    }

    template <int UniqId>
    void write(Type type, std::string_view data) {
        std::lock_guard lock(mtx);
        auto msg = build_string(current_datetime(LOG_TIME_FMT), str_types.at(type), data);

        if (_last_id == UniqId) {
            for (auto& [_, stream] : _streams) {
               if (stream.try_drop_postfix(_last_write_len))
                   stream.write("\n");
               else
                   stream.write("\r");
            }
        } else {
            for (auto& [_, stream] : _streams)
                stream.write("\n");
        }

        size_t append = 0;
        if (_last_write_len > msg.size() + 1)
            append = _last_write_len - (msg.size() + 1);

        _last_id = UniqId;
        _last_write_len = msg.size() + 1 + append;
        _last_write.clear();
        _write_repeats = 1;
        for (auto& [_, stream] : _streams) {
            stream.write(msg);
            if (append)
                stream.write(std::string(append, ' '));
            stream.flush();
        }
    }

    /**
     * @brief Writes a data to all logger streams
     *
     * @param type - the type of message
     * @param data - the data to be written
     */
    void write(Type type, std::string_view data) {
        std::lock_guard lock(mtx);

        auto message = std::string(str_types.at(type)) + std::string(data);
        auto datetime = current_datetime(LOG_TIME_FMT);
        std::string msg;

        if (_last_write == message) {
            msg = build_string(
                datetime, " ("sv, std::to_string(_write_repeats + 1), " times)"sv, message);

            for (auto& [_, stream] : _streams) {
               if (stream.try_drop_postfix(_last_write_len))
                   stream.write("\n");
               else
                   stream.write("\r");
            }
            _last_write_len = msg.size() + 1;
            ++_write_repeats;
        }
        else {
            msg = build_string("\n"sv, datetime, message);

            _last_write = message;
            _last_id = std::numeric_limits<int>::max();
            _write_repeats = 1;
            _last_write_len = msg.size() - (message.size() + 1);
        }

        for (auto& [_, stream] : _streams) {
            stream.write(msg);
            stream.flush();
        }
    }

    /**
     * @brief Writes a data to logger streams for which filter_callback returns true
     *
     * @tparam F - type of filter_callback
     * @param type - the type of message
     * @param data - the data to be written
     * @param filter_callback - the function for streams filtering
     */
    template <typename F>
    void write_filter(Type type, std::string_view data, F&& filter_callback) {
        std::lock_guard lock(mtx);
        for (auto& val : _streams) {
            if (filter_callback(val.second)) {
                auto message =
                    current_datetime(LOG_TIME_FMT) + std::string(str_types.at(type)) + std::string(data);

                val.second.write("\n");
                val.second.write(message);
                val.second.flush();
            }
        }
    }

#if 0
    /**
     * @brief Formats and writes data to logger streams for which filter_callback returns true
     *
     * @tparam F - type of filter_callback
     * @tparam Ts - types of values
     * @param type - the type of message
     * @param fmt - the format string
     * @param filter_callback - the function for streams filtering
     * @param args - arguments for formatting
     */
    template <typename F, typename... Ts>
    void write(Type type, string_view fmt, F&& filter_callback, Ts&&... args) {
        write(type, format(fmt, std::forward<Ts>(args)...), std::forward<F>(filter_callback));
    }
#endif

    /**
     * @brief Formats and writes data to all logger streams
     *
     * @tparam Ts - types of values
     * @param type - the type of message
     * @param fmt - the format string
     * @param args - arguments for formatting
     */
    template <typename... Ts>
    void write(Type type, std::string_view fmt, Ts&&... args) {
        write(type, format(fmt, std::forward<Ts>(args)...));
    }

    /**
     * @brief Formats and writes data to all logger streams using Message type
     *
     * @tparam Ts - types of values
     * @param fmt - the format string
     * @param args - arguments for formatting
     */
    template <typename... Ts>
    void write(std::string_view fmt, Ts&&... args) {
        write(Message, fmt, std::forward<Ts>(args)...);
    }

    /**
     * @brief Formats and writes data to all logger streams
     *
     * Rewrites previous record if it has the same UniqId
     *
     * @tparam UniqId - unique id
     * @tparam Ts - types of values
     * @param type - the type of message
     * @param fmt - the format string
     * @param args - arguments for formatting
     */
    template <int UniqId, typename... Ts>
    void write_update(Type type, std::string_view fmt, Ts&&... args) {
        write<UniqId>(type, format(fmt, std::forward<Ts>(args)...));
    }

    /**
     * @brief Formats and writes data to all logger streams using Message type
     *
     * Rewrites previous record if it has the same UniqId
     *
     * @tparam UniqId - unique id
     * @tparam Ts - types of values
     * @param fmt - the format string
     * @param args - arguments for formatting
     */
    template <int UniqId, typename... Ts>
    void write_update(std::string_view fmt, Ts&&... args) {
        write_update<UniqId>(Message, fmt, std::forward<Ts>(args)...);
    }

#if 0
    /**
     * @brief Formats and writes data to logger streams for which filter_callback returns true
     *
     * @tparam F - type of filter_callback
     * @tparam Ts - types of values
     * @param fmt - the format string
     * @param filter_callback - the function for streams filtering
     * @param args - arguments for formatting
     */
    template <typename F, typename... Ts>
    void write(string_view fmt, F&& filter_callback, Ts&&... args) {
        write(Message, fmt, std::forward<F>(filter_callback), std::forward<Ts>(args)...);
    }
#endif

    /**
     * @brief Add or replace the output stream by the key
     *
     * Supports fstreams, stdout and weak_ptr<stringstream>
     *
     * You can't safely read from weak_ptr<stringstring> if logger used in multiple threads :)
     *
     * @tparam Ts - types of arguments to log_output_stream constructor
     * @param key - the key
     * @param args - arguments to log_output_stream constructor
     */
    template <typename... Ts>
    void add_output_stream(std::string_view key, Ts&&... args) {
        std::lock_guard lock(mtx);
        _streams.insert_or_assign(std::string(key), log_output_stream(std::forward<Ts>(args)...));
    }

    /**
     * @brief Removes output stream by the key
     *
     * @param key - the key
     */
    void remove_output_stream(std::string_view key) {
        std::lock_guard lock(mtx);
        _streams.erase(std::string(key));
    }

    /**
     * @brief Returns streams map
     *
     * @return streams map
     */
    [[nodiscard]]
    const auto& streams() const {
        return _streams;
    }

    std::map<std::string, log_output_stream> _streams;
    std::mutex                               mtx;
    std::string                              _last_write;
    size_t                                   _last_write_len = 0;
    unsigned                                 _write_repeats  = 0;
    int                                      _last_id        = 0;
};

inline logger& log() {
    return logger::instance();
}
} // namespace dfdh

#define LOG_INFO(...) dfdh::logger::instance().write(dfdh::logger::Details, __VA_ARGS__)
#define LOG(...)      dfdh::logger::instance().write(__VA_ARGS__)
#define LOG_WARN(...) dfdh::logger::instance().write(dfdh::logger::Warning, __VA_ARGS__)
#define LOG_ERR(...)  dfdh::logger::instance().write(dfdh::logger::Error, __VA_ARGS__)

#define LOG_INFO_UPDATE(...)                                                                    \
    dfdh::logger::instance().write_update<__COUNTER__>(dfdh::logger::Details, __VA_ARGS__)
#define LOG_UPDATE(...) dfdh::logger::instance().write_update<__COUNTER__>(__VA_ARGS__)
#define LOG_WARN_UPDATE(...)                                                                    \
    dfdh::logger::instance().write_update<__COUNTER__>(dfdh::logger::Warning, __VA_ARGS__)
#define LOG_ERR_UPDATE(...)                                                                      \
    dfdh::logger::instance().write_update<__COUNTER__>(dfdh::logger::Error, __VA_ARGS__)

#ifndef NDEBUG
    #define DLOG_INFO(...) dfdh::logger::instance().write(dfdh::logger::Details, __VA_ARGS__)
    #define DLOG(...)      dfdh::logger::instance().write(__VA_ARGS__)
    #define DLOG_WARN(...) dfdh::logger::instance().write(dfdh::logger::Warning, __VA_ARGS__)
    #define DLOG_ERR(...)  dfdh::logger::instance().write(dfdh::logger::Error, __VA_ARGS__)
#else
    #define DLOG_INFO(...) void(0)
    #define DLOG(...)      void(0)
    #define DLOG_WARN(...) void(0)
    #define DLOG_ERR(...)  void(0)
#endif
