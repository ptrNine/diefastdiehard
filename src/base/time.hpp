#pragma once

#include <sstream>
#include <chrono>
#include <iomanip>

#include "types.hpp"

namespace dfdh
{
struct time_info_t {
    std::chrono::hours        hour;
    std::chrono::minutes      minute;
    std::chrono::seconds      second;
    std::chrono::milliseconds millisecond;
    std::chrono::microseconds microsecond;
    std::chrono::nanoseconds  nanosecond;
};

inline time_info_t get_current_time() {
    namespace chr = std::chrono;
    using namespace std::chrono_literals;

    auto now = chr::system_clock::now().time_since_epoch();
    time_info_t t; // NOLINT

    auto acc = 0ns + chr::floor<chr::days>(now);

    t.hour = chr::floor<chr::hours>(now) - chr::floor<chr::days>(acc);
    acc += t.hour;
    t.minute = chr::floor<chr::minutes>(now) - chr::floor<chr::hours>(acc);
    acc += t.minute;
    t.second = chr::floor<chr::seconds>(now) - chr::floor<chr::minutes>(acc);
    acc += t.second;
    t.millisecond = chr::floor<chr::milliseconds>(now) - chr::floor<chr::seconds>(acc);
    acc += t.millisecond;
    t.microsecond = chr::floor<chr::microseconds>(now) - chr::floor<chr::milliseconds>(acc);
    acc += t.microsecond;
    t.nanosecond = chr::duration_cast<chr::nanoseconds>(now) - chr::floor<chr::microseconds>(acc);

    return t;
}

inline std::string current_datetime(std::string_view format) {
    namespace chr = std::chrono;

    std::stringstream ss;

    auto time = get_current_time();

    std::array<bool, 256> charmap = {false};
    charmap['D'] = charmap['M'] = charmap['Y'] = charmap['h'] = charmap['m'] = charmap['s'] =
        charmap['x'] = charmap['u'] = charmap['n'] = true;

    static constexpr auto padprint = [](std::stringstream& ss, int c, auto v) {
        ss << std::setfill('0') << std::setw(c) << v;
    };

    int counter = 0;

    auto printtm = [&](char f) {
        if (counter) {
            switch (f) {
            //case 'D': padprint(ss, counter, u32(ymd.day)); break;
            //case 'M': padprint(ss, counter, u32(ymd.month)); break;
            //case 'Y': padprint(ss, counter, int(ymd.year)); break;
            case 'h': padprint(ss, counter, time.hour.count()); break;
            case 'm': padprint(ss, counter, time.minute.count()); break;
            case 's': padprint(ss, counter, time.second.count()); break;
            case 'x': padprint(ss, counter, time.millisecond.count()); break;
            case 'u': padprint(ss, counter, time.microsecond.count()); break;
            case 'n': padprint(ss, counter, time.nanosecond.count()); break;
            default: break;
            }
            counter = 0;
        }
    };

    char last_fmt = '\0';
    for (auto c : format) {
        auto is_fmt = charmap[static_cast<u8>(c)];

        if (!is_fmt || last_fmt != c)
            printtm(last_fmt);

        if (is_fmt)
            ++counter;
        else {
            ss << c;
            counter = 0;
        }

        if (is_fmt && last_fmt != c)
            last_fmt = c;
    }
    printtm(last_fmt);

    return ss.str();
}
} // namespace dfdh
