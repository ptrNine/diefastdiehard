#include <functional>
#include <memory>
#include <vector>
#include <map>
#include <cstring>
#include <thread>
#include <iomanip>
#include <mutex>

using namespace std::chrono_literals;

enum class internal_slot_mode : uint8_t {};

class slot_alive_checker {
public:
    slot_alive_checker(std::weak_ptr<internal_slot_mode> v): _v(v) {}

    auto lock_alive() const {
        return _v.lock();
    }

private:
    std::weak_ptr<internal_slot_mode> _v;
};


class slot_alive_holder {
public:
    slot_alive_holder(): _v(std::make_shared<internal_slot_mode>()) {}
    slot_alive_holder(const slot_alive_holder&): slot_alive_holder() {}
    slot_alive_holder(slot_alive_holder&&) noexcept: slot_alive_holder() {}
    slot_alive_holder& operator=(const slot_alive_holder&) { return *this; }
    slot_alive_holder& operator=(slot_alive_holder&&) noexcept { return *this; }

    ~slot_alive_holder() {
        while (_v.use_count() > 1)
            std::this_thread::sleep_for(1us);
    }

    [[nodiscard]]
    slot_alive_checker checker() const {
        return {_v};
    }

private:
    std::shared_ptr<internal_slot_mode> _v;
};


class slot_holder {
public:
    [[nodiscard]]
    slot_alive_checker get_checker() const {
        return _online.checker();
    }

private:
    slot_alive_holder _online;
};


class signal_slot_event_updater {
public:
    static signal_slot_event_updater& instance() {
        static signal_slot_event_updater inst;
        return inst;
    }

    void push_task(std::function<void()> task) {
        auto lock = std::lock_guard{mtx};
        task_queue.push_back(std::move(task));
    }

    void operate_tasks() {
        auto lock = std::lock_guard{mtx};
        for (auto& task : task_queue)
            task();
        task_queue.clear();
    }

private:
    /* MPSC queue */
    std::vector<std::function<void()>> task_queue;
    mutable std::mutex mtx;
};

template <typename T>
class signal;

template <typename ReturnT, typename... ArgsT>
class signal<ReturnT(ArgsT...)> {
public:
    struct immediate_return_t {
        slot_alive_checker    alive;
        slot_holder*          holder;
        std::decay_t<ReturnT> value;
    };

    using uniq_id = std::pair<uint64_t, uint64_t>;
    template <typename T>
    uniq_id make_uniq_id(void* ptr, ReturnT(T::*member_ptr)(ArgsT...)) {
        auto id = uniq_id{0, 0};
        std::memcpy(&id.first, &ptr, sizeof(ptr));
        std::memcpy(&id.second, &member_ptr, sizeof(ptr));
        return id;
    }

    /* FNV1A 128 bit */
    inline uniq_id make_uniq_id(std::string_view name) {
        auto mul = (__uint128_t(0x1000000) << 64) + 0x13B;
        auto hash = (__uint128_t(0x6c62272e07bb0142) << 64) + 0x62b821756295c58d;

        for (char c : name)
            hash = (hash ^ uint8_t(c)) * mul;

        uniq_id id;
        memcpy(&id, &hash, sizeof(id));
        return id;
    }

    struct func_pair {
        std::function<immediate_return_t(ArgsT...)> immediate;
        std::function<void(ArgsT...)>               deferred;
    };

    template <typename T, typename F = ReturnT(T::*)(ArgsT...)> // requires std::is_base_of<slot_holder, T>
    void connect_to(T& slot_owner, F slot) {
        _consumers.insert_or_assign(make_uniq_id(&slot_owner, slot), func_pair{
            [checker = slot_owner.get_checker(), owner = &slot_owner, slot](ArgsT... args) -> immediate_return_t {
                auto lock = checker.lock_alive();
                if (!lock)
                    throw std::runtime_error("Slot owner was destroyed!");
                return {checker, owner, (owner->*slot)(args...)};
            },
            [checker = slot_owner.get_checker(), owner = &slot_owner, slot](ArgsT... args) {
                signal_slot_event_updater::instance().push_task(
                    [checker, owner, slot, args...]() {
                        auto lock = checker.lock_alive();
                        if (!lock)
                            throw std::runtime_error("Slot owner was destroyed!");
                        (owner->*slot)(args...);
                    }
                );
            }
        });
    }

    template <typename F>
    void attach_function(std::string_view name, F function) {
        _consumers.insert_or_assign(make_uniq_id(name), func_pair{
            [alive_h = slot_alive_holder(), function](ArgsT... args) -> immediate_return_t {
                return {alive_h.checker(), nullptr, function(args...)};
            },
            [function](ArgsT... args) {
                function(args...);
            }
        });
    }

    void detach_function(std::string_view name) {
        _consumers.erase(make_uniq_id(name));
    }

    template <typename T, typename F = ReturnT(T::*)(ArgsT...)> // requires std::is_base_of<slot_holder, T>
    void unconnect_from(T& slot_owner, F slot) {
        _consumers.erase(make_uniq_id(&slot_owner, slot));
    }

    std::vector<immediate_return_t> emit_immediate(ArgsT... args) {
        std::vector<immediate_return_t> ret;
        std::vector<uniq_id> to_erase;

        for (auto& [id, fp] : _consumers) {
            try {
                ret.emplace_back(fp.immediate(args...));
            } catch (...) {
                to_erase.push_back(id);
            }
        }
        for (auto& id : to_erase)
            _consumers.erase(id);

        return ret;
    }

    void emit_deferred(ArgsT... args) const {
        for (auto& [_, fp] : _consumers)
            fp.deferred(args...);
    }

private:
    std::map<uniq_id, func_pair> _consumers;
};

