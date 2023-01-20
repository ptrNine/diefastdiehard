#pragma once

#include "base/finalizers.hpp"
#include "base/signals.hpp"

#include <coroutine>
#include <map>
#include <set>
#include <optional>
#include <variant>

//#define msg(...) do { std::cout << (__VA_ARGS__) << std::endl; } while (0)
#define msg(...) (void)0

namespace dfdh {

struct type_erased_deleter_base {
    virtual ~type_erased_deleter_base() = default;
};

template <typename T>
struct type_erased_deleter_child : type_erased_deleter_base {
    type_erased_deleter_child(auto&&...args):
        data(static_cast<decltype(args)>(args)...) {}
    T data;
};

class type_erased_deleter {
public:
    template <typename T>
    static std::pair<T*, type_erased_deleter> create(auto&&... args) {
        auto holder = new type_erased_deleter_child<T>(static_cast<decltype(args)>(args)...);
        return {&holder->data, type_erased_deleter{holder}};
    }

    type_erased_deleter() = default;

    type_erased_deleter(type_erased_deleter&& d) noexcept : holder(d.holder) {
        d.holder = nullptr;
    }

    type_erased_deleter& operator=(type_erased_deleter&& d) noexcept {
        if (this != &d) {
            holder   = d.holder;
            d.holder = nullptr;
        }
        return *this;
    }

    ~type_erased_deleter() {
        delete holder;
    }

private:
    type_erased_deleter(type_erased_deleter_base* iholder): holder(iholder) {}

    type_erased_deleter_base* holder = nullptr;
};

struct promise_additional_data {
    type_erased_deleter                         _deleter;
    std::set<std::coroutine_handle<>>::iterator _handle_it;
    std::set<std::coroutine_handle<>>*          _active_handles = nullptr;

    ~promise_additional_data() {
        if (_active_handles)
            _active_handles->erase(_handle_it);
    }
};

template <typename TaskT, typename TaskPromiseT>
struct task_promise_return_object : promise_additional_data {
    TaskT get_return_object() noexcept {
        msg("[task promise]: get_return_object()");
        return {std::coroutine_handle<TaskPromiseT>::from_promise(static_cast<TaskPromiseT&>(*this))};
    }
};

template <typename T, typename TaskT, typename TaskPromiseT>
struct task_promise_storage : task_promise_return_object<TaskT, TaskPromiseT> {
    template <std::convertible_to<T> U>
    void return_value(U&& value) noexcept {
        msg("[task promise]: return_value()");
        result = std::forward<U>(value);
        completion_signal(std::get<T>(result));
    }

    void unhandled_exception() noexcept {
        msg("[task promise]: unhandled_exception()");
        result = std::current_exception();
        exception_signal(std::get<std::exception_ptr>(result));
    }

    T get_result() const {
        if (result.index() == 1)
            std::rethrow_exception(std::get<std::exception_ptr>(result));
        return std::get<T>(result);
    }

    std::variant<T, std::exception_ptr> result;
    signal<void(T)>                     completion_signal;
    signal<void(std::exception_ptr)>    exception_signal;
};


template <typename T, typename TaskT, typename TaskPromiseT>
struct task_promise_storage<T&, TaskT, TaskPromiseT> : task_promise_return_object<TaskT, TaskPromiseT> {
    void return_value(T& value) noexcept {
        msg("[task promise]: return_value()");
        result = &value;
        completion_signal(value);
    }

    void unhandled_exception() noexcept {
        msg("[task promise]: unhandled_exception()");
        result = std::current_exception();
        exception_signal(std::get<std::exception_ptr>(result));
    }

    T& get_result() const {
        if (result.index() == 1)
            std::rethrow_exception(std::get<std::exception_ptr>(result));
        return *std::get<T*>(result);
    }

    std::variant<T*, std::exception_ptr> result;
    signal<void(T&)>                     completion_signal;
    signal<void(std::exception_ptr)>     exception_signal;
};

template <typename TaskT, typename TaskPromiseT>
struct task_promise_storage<void, TaskT, TaskPromiseT> : task_promise_return_object<TaskT, TaskPromiseT> {
    void return_void() noexcept {
        msg("[task promise]: return_void()");
        completion_signal();
    }

    void unhandled_exception() noexcept {
        msg("[task promise]: unhandled_exception()");
        exception = std::current_exception();
        exception_signal(exception);
    }

    void get_result() const {
        if (exception)
            std::rethrow_exception(exception);
    }

    std::exception_ptr               exception;
    signal<void()>                   completion_signal;
    signal<void(std::exception_ptr)> exception_signal;
};

template <typename T = void>
struct autolifetime {
    using type = T;
};

template <typename T>
struct is_autolifetime : std::false_type {};

template <typename T>
struct is_autolifetime<autolifetime<T>> : std::true_type {};

template <typename T>
static inline constexpr bool is_autolifetime_v = is_autolifetime<T>::value;


template <typename T, typename TaskT, typename TaskPromiseT>
struct task_promise_storage<autolifetime<T>, TaskT, TaskPromiseT> : task_promise_storage<T, TaskT, TaskPromiseT> {};

template <typename T = void>
struct task;

template <typename T>
struct task_promise_type : task_promise_storage<T, task<T>, task_promise_type<T>> {
    [[nodiscard]]
    std::suspend_always initial_suspend() const noexcept {
        msg("[task promise]: initial_suspend()");
        return {};
    }

    struct final_suspend_awaiter {
        [[nodiscard]]
        constexpr bool await_ready() const noexcept {
            msg("[task promise final_suspend_awaiter]: await_ready()");
            return continuation == nullptr;
        }

        std::coroutine_handle<> await_suspend(std::coroutine_handle<> /* handle*/) noexcept {
            msg("[task promise final_suspend_awaiter]: await_suspend()");
            return continuation;
        }

        void await_resume() const noexcept {
            msg("[task promise final_suspend_awaiter]: await_resume()");
        }

        std::coroutine_handle<> continuation;
    };

    final_suspend_awaiter final_suspend() const noexcept {
        msg("[task promise]: final_suspend()");
        return {continuation};
    }

    std::coroutine_handle<> continuation;
};

template <typename ResultT>
struct task {
    using promise_type = task_promise_type<ResultT>;

    ~task() {
        if constexpr (!is_autolifetime_v<ResultT>)
            handle.destroy();
    }

    [[nodiscard]]
    bool await_ready() const noexcept {
        msg("[task]: await_ready()");
        return false;
    }

    template <typename T>
    std::coroutine_handle<> await_suspend(std::coroutine_handle<T> awaiter) noexcept {
        msg("[task]: await_suspend()");
        handle.promise().continuation = awaiter;
        return handle;
    }

    ResultT await_resume() const {
        msg("[task]: await_resume()");
        auto scope_guard = finalizer{[&] {
            if constexpr (is_autolifetime_v<ResultT>)
                handle.destroy();
        }};
        return handle.promise().get_result();
    }

    std::coroutine_handle<promise_type> handle;
};

template <typename T>
struct is_autolifetime_task : std::false_type {};

template <typename T>
struct is_autolifetime_task<task<autolifetime<T>>> : std::true_type {};

template <typename T>
concept AutolifetimeTask = is_autolifetime_task<T>::value;

template <typename T>
concept AutolifetimeTaskFunc = requires(T coro_task) {
    { coro_task() } -> AutolifetimeTask;
};

template <typename T = void>
using autotask = task<autolifetime<T>>;

struct task_signal_connector {};

template <typename T>
struct task_on_exception_connector : task_signal_connector {
    task_on_exception_connector(T iconnector): connector(std::move(iconnector)) {}
    T connector;
};

template <typename T>
struct task_on_completion_connector : task_signal_connector {
    task_on_completion_connector(T iconnector): connector(std::move(iconnector)) {}
    T connector;
};

auto task_on_exception(auto&&... args) {
    return task_on_exception_connector{signal_connector{static_cast<decltype(args)>(args)...}};
}

auto task_on_completion(auto&&... args) {
    return task_on_completion_connector{signal_connector{static_cast<decltype(args)>(args)...}};
}

template <typename T>
concept TaskSignalConnector = std::is_base_of_v<task_signal_connector, T>;

template <typename EventId, typename EventData>
class task_worker {
public:
    struct event_data {
        std::optional<EventData> data;
        std::coroutine_handle<>  awaiter;
        bool (*data_test_callback)(const EventData&);
    };

    template <AutolifetimeTaskFunc... Ts>
    task_worker(Ts&&... tasks) {
        (submit(std::forward<Ts>(tasks)), ...);
    }

    ~task_worker() {
        auto handles = active_handles;
        for (auto&& handle : handles)
            handle.destroy();
    }

    friend struct event_awaiter;
    struct event_awaiter {
        [[nodiscard]]
        bool await_ready() const noexcept {
            msg("[event_awaiter]: await_ready()");
            return false;
        }

        template <typename T>
        bool await_suspend(std::coroutine_handle<T> awaiter) noexcept {
            msg("[event_awaiter]: await_suspend()");
            event_pos = _worker->event_awaiters.emplace(_event_id, event_data{{}, awaiter, _data_test_callback});
            return true;
        }

        EventData await_resume() noexcept {
            msg("[event_awaiter]: await_resume()");
            auto scope_guard = finalizer{[&] {
                _worker->event_awaiters.erase(event_pos);
            }};
            return std::move(event_pos->second.data.value());
        }

        task_worker* _worker;
        EventId      _event_id;
        bool (*_data_test_callback)(const EventData&)                   = nullptr;
        typename std::multimap<EventId, event_data>::iterator event_pos = {};
    };

    event_awaiter for_event(const EventId& event_id) {
        return {this, event_id};
    }

    event_awaiter for_event(const EventId& event_id, bool data_test_callback(const EventData&)) {
        return {this, event_id, data_test_callback};
    }

    void handle_event(const EventId& event_id, EventData data) {
        auto range = event_awaiters.equal_range(event_id);
        for (auto it = range.first; it != range.second;) {
            auto&& test = it->second.data_test_callback;
            if (test && !test(data)) {
                ++it;
                continue;
            }

            it->second.data = std::move(data);
            auto cur = it++;
            cur->second.awaiter.resume();
        }
    }

    template <AutolifetimeTaskFunc T>
    auto submit(auto&&... task_args, TaskSignalConnector auto&&... connectors) {
        auto&& [task, deleter] = type_erased_deleter::create<T>(static_cast<decltype(task_args)>(task_args)...);
        return submit_internal(task, std::move(deleter), static_cast<decltype(connectors)>(connectors)...);
    }

    template <AutolifetimeTaskFunc T>
    auto submit(T&& coro_task, TaskSignalConnector auto&&... connectors) {
        auto&& [task, deleter] = type_erased_deleter::create<T>(std::forward<T>(coro_task));
        return submit_internal(task, std::move(deleter), static_cast<decltype(connectors)>(connectors)...);
    }

    template <typename T>
    auto submit(task<autolifetime<T>>(*coro_task)(), TaskSignalConnector auto&&... connectors) {
        auto future = coro_task();

        auto&& handle  = future.handle;
        auto&& promise = handle.promise();

        connect(static_cast<decltype(connectors)>(connectors)...);

        auto [handle_it, _]     = active_handles.emplace(handle);
        promise._active_handles = &active_handles;
        promise._handle_it      = handle_it;

        future.handle.resume();

        return std::tuple(&promise.completion_signal, &promise.exception_signal);
    }

private:
    void connect(auto&) {}

    template <typename T>
    void connect(auto& promise, task_on_completion_connector<T> connector, TaskSignalConnector auto&&... connectors) {
        promise.completion_signal.connect(std::move(connector.connector));
        connect(promise, static_cast<decltype(connectors)>(connectors)...);
    }

    template <typename T>
    void connect(auto& promise, task_on_exception_connector<T> connector, TaskSignalConnector auto&&... connectors) {
        promise.exception_signal.connect(std::move(connector.connector));
        connect(promise, static_cast<decltype(connectors)>(connectors)...);
    }

    template <typename T>
    auto submit_internal(T&& task, type_erased_deleter deleter, TaskSignalConnector auto&&... connectors) {
        auto   future  = (*task)();
        auto&& handle  = future.handle;
        auto&& promise = handle.promise();

        connect(promise, static_cast<decltype(connectors)>(connectors)...);
        if constexpr (requires { task->completion_signal = &promise.completion_signal; })
            task->completion_signal = &promise.completion_signal;
        if constexpr (requires { task->exception_signal = &promise.exception_signal; })
            task->exception_signal = &promise.exception_signal;

        auto [handle_it, _]     = active_handles.emplace(handle);
        promise._deleter        = std::move(deleter);
        promise._active_handles = &active_handles;
        promise._handle_it      = handle_it;

        handle.resume();

        return std::tuple(&promise.completion_signal, &promise.exception_signal);
    }

    std::multimap<EventId, event_data> event_awaiters;
    std::set<std::coroutine_handle<>>  active_handles;
};
}
