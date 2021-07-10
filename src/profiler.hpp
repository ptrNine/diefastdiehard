#pragma once

#include <map>
#include <chrono>

#include "types.hpp"
#include "print.hpp"

namespace dfdh {
using namespace std::chrono_literals;

class profiler {
public:
    struct time_data {
        std::chrono::steady_clock::time_point start;
        std::chrono::steady_clock::duration   last_min_dur = 24h;
        std::chrono::steady_clock::duration   last_max_dur = 0s;
        std::chrono::steady_clock::duration   dur_sum      = 0s;
        u64                                   count        = 0;

        friend std::ostream& operator<<(std::ostream& os, const time_data& time) {
            fprint(os, time.last_min_dur, '|', time.last_max_dur, '|', time.dur_sum / time.count);
            return os;
        }
    };

    struct profiler_scope {
        profiler_scope(std::string measure_name, profiler& prof_instance):
            name(std::move(measure_name)), prof(prof_instance) {
            prof.start(name);
        }

        ~profiler_scope() {
            prof.end(name);
        }

        std::string name;
        profiler&   prof;
    };

    void start(const std::string& measure_name) {
        measures[measure_name].start = std::chrono::steady_clock::now();
    }

    void end(const std::string& measure_name) {
        auto found = measures.find(measure_name);
        if (found != measures.end()) {
            auto& cur = found->second;
            auto  dur = std::chrono::steady_clock::now() - found->second.start;
            ++cur.count;
            cur.dur_sum += dur;
            if (dur > cur.last_max_dur)
                cur.last_max_dur = dur;
            if (dur < cur.last_min_dur)
                cur.last_min_dur = dur;
        }
    }

    profiler_scope scope(const std::string& measure_name) {
        return {measure_name, *this};
    }

    friend std::ostream& operator<<(std::ostream& os, const profiler& prof) {
        print_any(os, prof.measures);
        return os;
    }

    template <typename Rep, typename Period>
    void set_print_period(const std::chrono::duration<Rep, Period>& period) {
        print_period = std::chrono::duration_cast<decltype(print_period)>(period);
    }

    void try_print(auto&& print_callback) {
        auto now = std::chrono::steady_clock::now();
        if (now - last_print >= print_period) {
            last_print = now;
            print_callback(*this);
            reset();
        }
    }

    void reset() {
        for (auto& [_, time] : measures) {
            time.count        = 0;
            time.dur_sum      = 0s;
            time.last_min_dur = 24h;
            time.last_max_dur = 0s;
        }
    }

private:
    std::map<std::string, time_data> measures;

    std::chrono::steady_clock::time_point last_print = std::chrono::steady_clock::now();
    std::chrono::nanoseconds              print_period = 1s;
};

}
