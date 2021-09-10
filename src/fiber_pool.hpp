#pragma once

#include <boost/fiber/algo/work_stealing.hpp>
#include <boost/fiber/buffered_channel.hpp>
#include <boost/fiber/fiber.hpp>
#include <boost/fiber/future/packaged_task.hpp>
#include <boost/fiber/operations.hpp>

#include "types.hpp"

namespace dfdh
{
namespace fibers        = boost::fibers;
namespace this_job      = boost::this_fiber;
using job_launch_policy = boost::fibers::launch;

template <typename T>
using job_future = boost::fibers::future<T>;
using job_future_status = boost::fibers::future_status;
using job_future_error  = boost::fibers::future_error;
using job_future_errc   = boost::fibers::future_errc;

class fiber_pool {
private:
    class task_t {
    public:
        struct task_holder {
            task_holder()                       = default;
            task_holder(const task_holder&)     = default;
            task_holder(task_holder&&) noexcept = default;
            task_holder& operator=(const task_holder&) = delete;
            task_holder& operator=(task_holder&&) noexcept = default;
            virtual ~task_holder() noexcept                = default;
            virtual void exec()                            = 0;
        };

        template <typename F>
        struct task_holder_impl : public task_holder {
            task_holder_impl(F f): func(std::move(f)) {}
            void exec() override {
                func();
            }
            F func;
        };

        task_t() = default;

        template <typename F>
        task_t(F&& func): holder(std::make_unique<task_holder_impl<F>>(std::forward<F>(func))) {}

        task_t(const task_t&) = delete;
        task_t& operator=(const task_t&) = delete;
        task_t(task_t&&) noexcept        = default;
        task_t& operator=(task_t&&) noexcept = default;
        ~task_t() noexcept                   = default;

        void exec() const {
            holder->exec();
        }

    private:
        std::unique_ptr<task_holder> holder;
    };

    using task_tuple_t = std::tuple<job_launch_policy, task_t>;

public:
    static constexpr size_t default_work_queue_size = 16384;

    static size_t default_threads_count() noexcept {
        auto count = std::thread::hardware_concurrency();
        return count < 3 ? 2 : count - 1;
    }

    fiber_pool(size_t threads_count = default_threads_count(), size_t iwork_queue_size = default_work_queue_size):
        threads_count_(threads_count), work_queue_size_(iwork_queue_size), channel_(iwork_queue_size) {
        try {
            for (uint32_t i = 0; i < threads_count_; ++i) threads_.emplace_back(&fiber_pool::worker, this);
        }
        catch (...) {
            close();
            std::rethrow_exception(std::current_exception());
        }
    }

    fiber_pool(const fiber_pool&) = delete;
    fiber_pool& operator=(const fiber_pool&) = delete;
    fiber_pool(fiber_pool&&)                 = delete;
    fiber_pool& operator=(fiber_pool&&) = delete;

    ~fiber_pool() {
        for (auto& t : threads_)
            t.join();
    }

    void close() {
        channel_.close();
    }

    template <typename F, typename... ArgsT>
    auto submit(job_launch_policy launch_policy, F&& function, ArgsT&&... args) {
        auto capture = [function = std::forward<F>(function), args = std::make_tuple(std::forward<ArgsT>(args)...)]() mutable {
            return std::apply(std::move(function), std::move(args));
        };

        using return_t        = std::invoke_result_t<decltype(capture)>;
        using packaged_task_t = fibers::packaged_task<return_t()>;

        packaged_task_t task{std::move(capture)};
        auto            result = task.get_future();

        auto status = channel_.push(task_tuple_t{launch_policy, task_t{std::move(task)}});
        if (status != fibers::channel_op_status::success)
            throw std::runtime_error("Buffered channel was closed!");

        return result;
    }

    template <typename F, typename... ArgsT>
    auto submit(F&& function, ArgsT&&... args) {
        return submit(fibers::launch::post, std::forward<F>(function), std::forward<ArgsT>(args)...);
    }

    size_t threads_count() const noexcept {
        return threads_count_;
    }

    size_t work_queue_size() const noexcept {
        return work_queue_size_;
    }

private:
    void worker() {
        fibers::use_scheduling_algorithm<fibers::algo::work_stealing>(threads_count_, true);

        task_tuple_t task_tuple;
        while (channel_.pop(task_tuple) == fibers::channel_op_status::success) {
            auto& [launch_policy, task] = task_tuple;
            fibers::fiber(launch_policy, [task = std::move(task)] { task.exec(); }).detach();
        }
    }

private:
    size_t                                 threads_count_ = 1;
    size_t                                 work_queue_size_;
    fibers::buffered_channel<task_tuple_t> channel_;
    std::vector<std::thread>               threads_;
};

inline fiber_pool& global_fiber_pool() {
    static fiber_pool pool;
    return pool;
}

template <typename F, typename... ArgsT>
auto submit_job(fibers::launch launch_policy, F&& function, ArgsT&&... args) {
    return global_fiber_pool().submit(launch_policy, std::forward<F>(function), std::forward<ArgsT>(args)...);
}

template <typename F, typename... ArgsT>
auto submit_job(F&& function, ArgsT&&... args) {
    return global_fiber_pool().submit(std::forward<F>(function), std::forward<ArgsT>(args)...);
}
} // namespace core
