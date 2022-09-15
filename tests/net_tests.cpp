#include <catch2/catch_test_macros.hpp>
#include "net_easysocket.hpp"
#include "net_tasks.hpp"

using namespace dfdh;
namespace chr = std::chrono;

static inline constexpr int  ITERS = 10;
static inline constexpr auto DELAY = 10ms;

struct net_task : net_task_base {
    net_task(worker_t* worker, easysocket* socket, address_t iremote):
        net_task_base(worker), sock(socket), remote(iremote) {}
    easysocket* sock;
    address_t   remote;
};

TEST_CASE("ping-pong") {
    struct task1 : net_task {
        using net_task::net_task;

        autotask<int> operator()() {
            auto tp = chr::steady_clock::now();
            sock->send_somehow(remote, a_ping{});

            for (int i = 0; i < ITERS; ++i) {
                co_await for_action_discard<a_ping>(remote);

                auto now    = chr::steady_clock::now();
                auto ping_v = chr::duration_cast<chr::milliseconds>(now - tp);
                tp          = now;

                a_ping ping_response;
                ping_response.ping_ms = u16(ping_v.count());
                sock->send_somehow(remote, ping_response);
            }

            co_return 0;
        }
    };

    struct task2 : net_task {
        using net_task::net_task;

        autotask<chr::milliseconds> operator()() {
            co_await for_action_discard<a_ping>(remote);

            size_t ping_sum = 0;
            for (int i = 0; i < ITERS; ++i) {
                std::this_thread::sleep_for(DELAY);
                sock->send_somehow(remote, a_ping{});

                auto ping = co_await for_action<a_ping>(remote);
                ping_sum += ping.ping_ms;
                // glog().detail("Ping is: {}ms", ping.ping_ms);
            }

            co_return chr::milliseconds{ping_sum / ITERS};
        }
    };

    address_t  addr1{ip_address::localhost(), 6666};
    address_t  addr2{ip_address::localhost(), 6667};
    easysocket host1{addr1};
    easysocket host2{addr2};

    bool finished = false;

    worker_t w{task1{&w, &host1, addr2}};
    w.submit(task2{&w, &host2, addr1}, task_on_completion([&](chr::milliseconds ping) {
                 REQUIRE(ping >= DELAY);
                 finished = true;
             }));

    while (!finished) {
        if (auto result = host1.try_receive())
            w.handle_event(result.address, result.data);
        if (auto result = host2.try_receive())
            w.handle_event(result.address, result.data);
        std::this_thread::sleep_for(0ms);
    }
}
