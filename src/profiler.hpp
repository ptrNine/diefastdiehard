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
        profiler_scope(std::string measure_name, profiler& prof_instance, bool increment_counter = true):
            name(std::move(measure_name)), prof(prof_instance), increment(increment_counter) {
            prof.start(name);
        }

        ~profiler_scope() {
            prof.end(name, increment);
        }

        std::string name;
        profiler&   prof;
        bool        increment;
    };

    void start(const std::string& measure_name) {
        measures[measure_name].start = std::chrono::steady_clock::now();
    }

    void end(const std::string& measure_name, bool increment_counter) {
        auto found = measures.find(measure_name);
        if (found != measures.end()) {
            auto& cur = found->second;
            auto  dur = std::chrono::steady_clock::now() - found->second.start;
            cur.dur_sum += dur;
            if (increment_counter) {
                ++cur.count;
                if (dur > cur.last_max_dur)
                    cur.last_max_dur = dur;
                if (dur < cur.last_min_dur)
                    cur.last_min_dur = dur;
            }
        }
    }

    profiler_scope scope(const std::string& measure_name, bool increment_counter = true) {
        return {measure_name, *this, increment_counter};
    }

    friend std::ostream& operator<<(std::ostream& os, const profiler& prof) {
        if (prof.short_print_format()) {
            auto sum = 0.f;
            for (auto& [_, time] : prof.measures)
                sum +=
                    std::chrono::duration_cast<std::chrono::duration<float>>(time.dur_sum).count() / float(time.count);

            print_any(os, '{');
            for (auto i = prof.measures.begin(); i != prof.measures.end();) {
                print_any(os, '{');

                print_any(os, i->first);
                print_any(os, ", ");
                auto t = 100.f *
                         (std::chrono::duration_cast<std::chrono::duration<float>>(i->second.dur_sum) /
                          float(i->second.count))
                             .count() /
                         sum;
                char st[32];
                sprintf(st, "%05.2f%%", t);
                print_any(os, st);

                auto next = std::next(i);
                print_any(os, next == prof.measures.end() ? "}" : "}, ");
                i = next;
            }
            print_any(os, '}');
        }
        else {
            print_any(os, prof.measures);
        }
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

    [[nodiscard]]
    bool short_print_format() const {
        return short_format;
    }

    void short_print_format(bool value) {
        short_format = value;
    }

private:
    std::map<std::string, time_data> measures;

    std::chrono::steady_clock::time_point last_print = std::chrono::steady_clock::now();
    std::chrono::nanoseconds              print_period = 1s;
    bool                                  short_format = true;
};

}
