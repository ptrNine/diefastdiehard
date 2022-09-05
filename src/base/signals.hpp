#pragma once

#include <functional>
#include <memory>
#include <vector>
#include <map>
#include <cstring>
#include <thread>
#include <iomanip>
#include <mutex>
#include <optional>

using namespace std::chrono_literals;

enum class internal_slot_mode : uint8_t {};

class slot_owner_destroyed_error : public std::runtime_error {
public:
    slot_owner_destroyed_error(): std::runtime_error("slot owner was destroyed") {}
};

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
    slot_alive_holder& operator=(const slot_alive_holder&) {
        return *this;
    }
    slot_alive_holder& operator=(slot_alive_holder&&) noexcept {
        return *this;
    }

    ~slot_alive_holder() {
        while (_v.use_count() > 1) std::this_thread::sleep_for(1us);
    }

    [[nodiscard]] slot_alive_checker checker() const {
        return {_v};
    }

private:
    std::shared_ptr<internal_slot_mode> _v;
};

class slot_holder {
public:
    [[nodiscard]] slot_alive_checker get_checker() const {
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
        for (auto& task : task_queue) task();
        task_queue.clear();
    }

private:
    /* MPSC queue */
    std::vector<std::function<void()>> task_queue;
    mutable std::mutex                 mtx;
};

template <typename T>
class signal;

template <typename T>
struct immediate_return_t {
    slot_alive_checker alive;
    slot_holder*       holder;
    std::decay_t<T>    value;
};

template <>
struct immediate_return_t<void> {
    slot_alive_checker alive;
    slot_holder*       holder;
};

enum class signal_mode {
    deferred  = 0,
    immediate = 1
};


template <typename Derived, typename Base>
concept BaseOf = std::is_base_of_v<Base, Derived>;

template <typename T, typename M>
struct signal_connector;

template <BaseOf<slot_holder> T, typename MemberF>
struct signal_connector<T, MemberF> {
    T*                         slot_owner;
    MemberF                    function;
    std::optional<signal_mode> mode = {};
};

template <std::same_as<std::string> T, typename FuncT>
struct signal_connector<T, FuncT> {
    T                          connection_name;
    FuncT                      function;
    std::optional<signal_mode> mode = {};
};

template <typename FuncT>
struct signal_connector<void, FuncT> {
    FuncT                      function;
    std::optional<signal_mode> mode = {};
};


template <typename T, typename MemberF>
signal_connector(T*, MemberF) -> signal_connector<T, MemberF>;

template <typename T, typename MemberF>
signal_connector(T*, MemberF, signal_mode) -> signal_connector<T, MemberF>;

template <typename FuncT>
signal_connector(std::string, FuncT) -> signal_connector<std::string, FuncT>;

template <typename FuncT>
signal_connector(std::string, FuncT, signal_mode) -> signal_connector<std::string, FuncT>;

template <typename FuncT>
signal_connector(const char*, FuncT) -> signal_connector<std::string, FuncT>;

template <typename FuncT>
signal_connector(const char*, FuncT, signal_mode) -> signal_connector<std::string, FuncT>;

template <typename FuncT>
signal_connector(FuncT) -> signal_connector<void, FuncT>;

template <typename FuncT>
signal_connector(FuncT, signal_mode) -> signal_connector<void, FuncT>;


template <typename>
struct is_signal_connector : std::false_type {};

template <typename... Ts>
struct is_signal_connector<signal_connector<Ts...>> : std::true_type {};

template <typename T>
concept SignalConnector = is_signal_connector<T>::value;


template <typename ReturnT, typename... ArgsT>
class signal<ReturnT(ArgsT...)> {
public:
    using uniq_id = std::pair<uint64_t, uint64_t>;
    template <typename T>
    uniq_id make_uniq_id(void* ptr, ReturnT (T::*member_ptr)(ArgsT...)) {
        auto id = uniq_id{0, 0};
        std::memcpy(&id.first, &ptr, sizeof(ptr));
        std::memcpy(&id.second, &member_ptr, sizeof(ptr));
        return id;
    }

    /* FNV1A 128 bit */
    inline uniq_id make_uniq_id(std::string_view name) {
        auto mul  = (__uint128_t(0x1000000) << 64) + 0x13B;
        auto hash = (__uint128_t(0x6c62272e07bb0142) << 64) + 0x62b821756295c58d;

        for (char c : name) hash = (hash ^ uint8_t(c)) * mul;

        uniq_id id;
        memcpy(&id.first, &hash, sizeof(id) / 2);
        memcpy(&id.first, reinterpret_cast<char*>(&hash) + sizeof(uint64_t), sizeof(id) / 2);
        return id;
    }

    struct func_pair {
        std::function<immediate_return_t<ReturnT>(ArgsT...)> immediate;
        std::function<void(ArgsT...)>                        deferred;
    };

    template <typename T, typename F = ReturnT (T::*)(ArgsT...)> requires std::is_base_of_v<slot_holder, T>
    void connect_to(T& slot_owner, F slot) {
        _consumers.insert_or_assign(
            make_uniq_id(&slot_owner, slot),
            func_pair{[checker = slot_owner.get_checker(), owner = &slot_owner, slot](
                          ArgsT... args) -> immediate_return_t<ReturnT> {
                          auto lock = checker.lock_alive();
                          if (!lock)
                              throw slot_owner_destroyed_error();;
                          if constexpr (std::is_same_v<ReturnT, void>) {
                              (owner->*slot)(args...);
                              return {checker, owner};
                          } else {
                              return {checker, owner, (owner->*slot)(args...)};
                          }
                      },
                      [checker = slot_owner.get_checker(), owner = &slot_owner, slot](ArgsT... args) {
                          signal_slot_event_updater::instance().push_task([checker, owner, slot, args...]() {
                              auto lock = checker.lock_alive();
                              if (!lock)
                                  throw slot_owner_destroyed_error();
                              (owner->*slot)(args...);
                          });
                      }});
    }

    template <typename F>
    void attach_function(std::string_view name, F&& function) {
        _consumers.insert_or_assign(
            make_uniq_id(name),
            func_pair{[alive_h = slot_alive_holder(), f = function](ArgsT... args) -> immediate_return_t<ReturnT> {
                          if constexpr (std::is_same_v<ReturnT, void>) {
                              f(args...);
                              return {alive_h.checker(), nullptr};
                          }
                          else
                              return {alive_h.checker(), nullptr, f(args...)};
                      },
                      std::forward<F>(function)});
    }

    template <typename F>
    void attach_function(F&& function) {
        attach_function("", std::forward<F>(function));
    }

    template <typename F>
    void connect(signal_connector<std::string, F> connector) {
        attach_function(connector.connection_name, connector.function);
        if (connector.mode)
            mode(*connector.mode);
    }

    template <typename F>
    void connect(signal_connector<void, F> connector) {
        attach_function("", connector.function);
        if (connector.mode)
            mode(*connector.mode);
    }

    template <BaseOf<slot_holder> T, typename F>
    void connect(signal_connector<T, F> connector) {
        connect_to(*connector.slot_owner, connector.function);
        if (connector.mode)
            mode(*connector.mode);
    }

    void detach_function(std::string_view name) {
        _consumers.erase(make_uniq_id(name));
    }

    template <typename T, typename F = ReturnT (T::*)(ArgsT...)> // requires std::is_base_of<slot_holder, T>
    void unconnect_from(T& slot_owner, F slot) {
        _consumers.erase(make_uniq_id(&slot_owner, slot));
    }

    std::vector<immediate_return_t<ReturnT>> emit_immediate(ArgsT... args) {
        std::vector<immediate_return_t<ReturnT>> ret;
        std::vector<uniq_id>                     to_erase;

        for (auto& [id, fp] : _consumers) {
            try {
                ret.emplace_back(fp.immediate(args...));
            }
            catch (const slot_owner_destroyed_error&) {
                to_erase.push_back(id);
            }
        }
        for (auto& id : to_erase) _consumers.erase(id);

        return ret;
    }

    void emit_deferred(ArgsT... args) const {
        for (auto& [_, fp] : _consumers) fp.deferred(args...);
    }

    void operator()(ArgsT... args) {
        if (_mode == signal_mode::deferred) {
            emit_deferred(args...);
        }
        else {
            std::vector<uniq_id> to_erase;

            for (auto& [id, fp] : _consumers) {
                try {
                    fp.immediate(args...);
                }
                catch (...) {
                    to_erase.push_back(id);
                }
            }
            for (auto& id : to_erase) _consumers.erase(id);
        }
    }

    void mode(signal_mode value) {
        _mode = value;
    }

    [[nodiscard]]
    signal_mode mode() const {
        return _mode;
    }

private:
    std::map<uniq_id, func_pair> _consumers;
    signal_mode                  _mode = signal_mode::deferred;
};
