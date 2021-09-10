#include <iostream>

#include "src/coroutine.hpp"
#include "src/net_easysocket.hpp"
#include "src/net_actor.hpp"

#include <thread>

struct keklol {
    void mda(const dfdh::address_t& addr, const dfdh::a_ping& ping) {
        LOG_INFO("RECEIVED from {} {}", addr, ping);
    }
};

using namespace dfdh;
int main() {
    net_actor_processor ap;

    auto sock = easysocket(address_t());

    ap.add(sock, "test1", [](net_actor_ctx ctx) {
        std::cout << "LOL" << std::endl;
        ctx.await_for<a_ping>([](const a_ping& ping) {

        });
    });


    /*
    auto coro = coroutine([](coroutine_ctx<void, int&> ctx) {
        int i = 0;
        while (true) {
            ctx.yield(i);
            ++i;
        }
    });

    while (true) {
        auto& v = coro();
        std::cout << v << std::endl;
        ++v;
    }
    */
}
