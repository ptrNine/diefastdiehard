#include <catch2/catch_test_macros.hpp>

#include <string>
#include <iostream>
#include <thread>

#include "base/tasks.hpp"

using namespace dfdh;
using namespace std::chrono_literals;
using namespace std::string_view_literals;

using worker_t = task_worker<std::string, int>;

struct successfull_task {
    successfull_task(worker_t* iworker): worker(iworker) {}

    task<int> subtask() {
        co_return co_await worker->for_event("subtask_event");
    }

    autotask<int> operator()() {
        auto r1 = co_await subtask();
        REQUIRE(r1 == 1111);
        auto r2 = co_await worker->for_event("event1", [](const int& n) { return n == 222; });
        REQUIRE(r2 == 222);
        auto r3 = co_await worker->for_event("event2");
        REQUIRE(r3 == 33);
        co_return 0;
    }

    worker_t* worker;
};

void another_thread(worker_t* worker) {
    worker->handle_event("subtask_event", 1111);

    worker->handle_event("event1", 123);
    worker->handle_event("event1", 124);
    worker->handle_event("event1", 222);

    worker->handle_event("event2", 33);
}

TEST_CASE("successfull task") {
    static auto sig_receiver_ok = false;

    struct sig_receiver : public slot_holder {
        void completed(int rc) {
            sig_receiver_ok = rc == 0;
        }
    };
    sig_receiver sig_recv;

    worker_t worker;
    worker.submit<successfull_task>(&worker,
                                    task_on_completion(&sig_recv, &sig_receiver::completed, signal_mode::immediate));

    std::thread(another_thread, &worker).join();
    REQUIRE(sig_receiver_ok);
}

struct failed_task_1 {
    autotask<int> operator()() {
        throw std::runtime_error("early failed");
        co_return 0;
    }
};

TEST_CASE("failed task 1") {
    static auto sig_receiver_ok = false;

    struct sig_receiver : public slot_holder {
        void failed(std::exception_ptr ep) {
            try {
                std::rethrow_exception(ep);
            } catch (const std::runtime_error& e) {
                sig_receiver_ok = e.what() == "early failed"sv;
            }
        }
        worker_t* worker;
    };
    sig_receiver sig_recv;

    worker_t worker;
    worker.submit<failed_task_1>(task_on_exception(&sig_recv, &sig_receiver::failed, signal_mode::immediate));

    REQUIRE(sig_receiver_ok);
}

TEST_CASE("failed task 2") {
    static auto sig_receiver_ok = false;

    worker_t worker;
    worker.submit(
        [w = &worker]() -> autotask<int> {
            auto r = co_await w->for_event("event");
            REQUIRE(r == 1234);
            throw std::runtime_error("failed");
        },
        task_on_exception(
            [](std::exception_ptr ep) {
                try {
                    std::rethrow_exception(ep);
                }
                catch (const std::runtime_error& e) {
                    sig_receiver_ok = e.what() == "failed"sv;
                }
            },
            signal_mode::immediate));

    std::thread([w = &worker] { w->handle_event("event", 1234); }).join();
    REQUIRE(sig_receiver_ok);
}
