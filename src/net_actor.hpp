#pragma once

#include "net_easysocket.hpp"
#include "net_actions_generated.hpp"
#include "coroutine.hpp"

namespace dfdh {
class net_action_restrictor_base {
public:
    virtual ~net_action_restrictor_base() = default;
    virtual bool operator()(const address_t& from, const packet_t& packet) const = 0;
    virtual bool operator()(const address_t& from, u32 action) const = 0;
    [[nodiscard]]
    virtual std::unique_ptr<net_action_restrictor_base> copy() const = 0;
};

template <NetAction... Actions>
class net_action_restrictor : public net_action_restrictor_base {
public:
    net_action_restrictor() = default;
    net_action_restrictor(const address_t& from): address(from) {}

    template <size_t N = sizeof...(Actions)>
    [[nodiscard]]
    bool test(const address_t& from, u32 action) const {
        if (address && *address != from)
            return false;

        if constexpr (N > 0)
            return false || ((action == Actions::ACTION) || ...);

        return true;
    }

    template <size_t N = sizeof...(Actions)>
    [[nodiscard]]
    bool test(const address_t& from, const packet_t& packet) const {
        return test(from, packet.cast_to<u32>());
    }

    bool operator()(const address_t& from, const packet_t& packet) const override {
        return test(from, packet);
    }

    bool operator()(const address_t& from, u32 action) const override {
        return test(from, action);
    }

    [[nodiscard]]
    std::unique_ptr<net_action_restrictor_base> copy() const override {
        auto res = new net_action_restrictor(); // NOLINT
        res->address = address;
        return std::unique_ptr<net_action_restrictor_base>(res);
    }

private:
    std::optional<address_t> address;
};

class net_action_restrictor_te {
public:
    net_action_restrictor_te() = default;

    template <NetAction... Actions>
    net_action_restrictor_te(type_tuple<Actions...>) {
        holder.reset(new net_action_restrictor<Actions...>()); // NOLINT
    }

    template <NetAction... Actions>
    net_action_restrictor_te(type_tuple<Actions...>, const address_t& from) {
        holder.reset(new net_action_restrictor<Actions...>(from)); // NOLINT
    }

    net_action_restrictor_te(const net_action_restrictor_te& na):
        holder(na.holder ? na.holder->copy() : nullptr) {}
    net_action_restrictor_te& operator=(const net_action_restrictor_te& na) {
        holder = na.holder ? na.holder->copy() : nullptr;
        return *this;
    }
    net_action_restrictor_te(net_action_restrictor_te&& na) = default;
    net_action_restrictor_te& operator=(net_action_restrictor_te&& na) = default;


    bool operator()(const address_t& from, const packet_t& packet) const {
        return holder ? (*holder)(from, packet) : true;
    }

    bool operator()(const address_t& from, u32 action) const {
        return holder ? (*holder)(from, action) : true;
    }

private:
    std::unique_ptr<net_action_restrictor_base> holder;
};

class net_actor_ctx {
public:
    struct value_t {
        easysocket*     socket;
        address_t       address     = {};
        const net_spec* action_base = nullptr;
    };
    using return_t = net_action_restrictor_te;
    using coroutine_t = coroutine<value_t, return_t>;

    template <typename T>
    net_actor_ctx(T&& v): ctx(std::forward<T>(v)) {
        last_sock = ctx.next().socket;
    }

    template <NetAction... Actions>
    void await_for(auto&& overloaded) {
        while (true) {
            ctx.yield(net_action_restrictor_te(type_tuple<Actions...>()));
            auto res = ctx.next();
            last_sock = res.socket;
            if (net_action_downcast(res.address, *res.action_base, overloaded))
                break;
        }
    }

    template <NetAction... Actions>
    void await_for(const address_t& from, auto&& overloaded) {
        while (true) {
            ctx.yield(net_action_restrictor_te(type_tuple<Actions...>(), from));
            auto res = ctx.next();
            last_sock = res.socket;
            if (net_action_downcast(res.address, *res.action_base, overloaded))
                break;
        }
    }

    void await(auto&& overloaded) {
        ctx.yield(net_action_restrictor_te());
        auto res = ctx.next();
        last_sock = res.socket;
        net_action_downcast(res.address, *res.action_base, overloaded);
    }

    easysocket& socket() {
        return *last_sock;
    }

private:
    coroutine_ctx<value_t, return_t> ctx;
    easysocket*                      last_sock;
};

class net_actor {
public:
    template <typename F>
    net_actor(easysocket& socket, F&& accept_operator): coro(std::forward<F>(accept_operator)) {
        coro(net_actor_ctx::value_t{&socket});
    }

    bool try_accept(easysocket&      this_socket,
                    const address_t& receive_address,
                    const packet_t&  packet,
                    u32              action_from_packet) {
        if (ar(receive_address, action_from_packet)) {
            net_action_dispatch(receive_address, packet, [&](const auto& act) {
                auto v = net_actor_ctx::value_t{&this_socket, receive_address, &act};
                ar = coro(v);
            });
            return true;
        }
        return false;
    }

    bool try_accept(easysocket& this_socket, const receive_result<packet_t>& res) {
        if (ar(res.address, res.data)) {
            net_action_dispatch(res.address, res.data, [&](const auto& act) {
                auto v = net_actor_ctx::value_t{&this_socket, res.address, &act};
                ar = coro(v);
            });
            return true;
        }
        return false;
    }

private:
    net_actor_ctx::coroutine_t coro;
    net_action_restrictor_te   ar;
};

class net_actor_processor {
public:
    template <typename F>
    void add(easysocket& socket, const std::string& actor_name, F&& actor_operator) {
        actors.insert_or_assign(actor_name, net_actor(socket, std::forward<F>(actor_operator)));
    }

    bool remove(const std::string& actor_name) {
        return actors.erase(actor_name) != 0;
    }

    template <typename F>
    void receive(easysocket& socket, F&& uncatched_packet_handler) {
        while (auto result = socket.try_receive()) {
            auto action = result->cast_to<u32>();

            bool test = false;
            for (auto& [_, actor] : actors)
                test = actor.try_accept(socket, result.address, result.data, action) || test;

            if (!test)
                uncatched_packet_handler(result.address, result.data);
        }
    }

private:
    std::map<std::string, net_actor> actors;
};

}
