//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/plugin/child_manager.h"
#include "rotor/supervisor.h"
//#include <iostream>

using namespace rotor;
using namespace rotor::plugin;

namespace {
namespace to {
struct address_mapping {};
struct discard_request {};
struct init_request {};
struct init_timeout {};
struct lifetime {};
struct manager {};
struct parent {};
struct policy {};
struct shutdown_timeout {};
struct state {};
struct system_context {};
struct synchronize_start {};
struct timers_map {};
struct assign_shutdown_reason {};
} // namespace to
} // namespace

template <> auto &supervisor_t::access<to::address_mapping>() noexcept { return address_mapping; }
template <> auto supervisor_t::access<to::discard_request, request_id_t>(request_id_t request_id) noexcept {
    return discard_request(request_id);
}
template <> auto &actor_base_t::access<to::init_request>() const noexcept { return init_request; }
template <> auto &actor_base_t::access<to::init_request>() noexcept { return init_request; }
template <> auto &actor_base_t::access<to::init_timeout>() noexcept { return init_timeout; }
template <> auto &actor_base_t::access<to::lifetime>() noexcept { return lifetime; }
template <> auto &supervisor_t::access<to::manager>() noexcept { return manager; }
template <> auto &supervisor_t::access<to::parent>() noexcept { return parent; }
template <> auto &supervisor_t::access<to::policy>() noexcept { return policy; }
template <> auto &actor_base_t::access<to::shutdown_timeout>() noexcept { return shutdown_timeout; }
template <> auto &actor_base_t::access<to::state>() noexcept { return state; }
template <> auto &supervisor_t::access<to::system_context>() noexcept { return context; }
template <> auto &supervisor_t::access<to::synchronize_start>() noexcept { return synchronize_start; }
template <> auto &supervisor_t::access<to::timers_map>() noexcept { return timers_map; }

template <>
auto actor_base_t::access<to::assign_shutdown_reason, extended_error_ptr_t>(extended_error_ptr_t reason) noexcept {
    return assign_shutdown_reason(std::move(reason));
}

const void *child_manager_plugin_t::class_identity = static_cast<const void *>(typeid(child_manager_plugin_t).name());

const void *child_manager_plugin_t::identity() const noexcept { return class_identity; }

void child_manager_plugin_t::activate(actor_base_t *actor_) noexcept {
    plugin_base_t::activate(actor_);
    static_cast<supervisor_t &>(*actor_).access<to::manager>() = this;
    subscribe(&child_manager_plugin_t::on_create);
    subscribe(&child_manager_plugin_t::on_init);
    subscribe(&child_manager_plugin_t::on_shutdown_trigger);
    subscribe(&child_manager_plugin_t::on_shutdown_confirm);
    reaction_on(reaction_t::INIT);
    reaction_on(reaction_t::SHUTDOWN);
    reaction_on(reaction_t::START);
    actors_map.emplace(actor->get_address(), actor_state_t(actor));
    actor->configure(*this);
}

void child_manager_plugin_t::deactivate() noexcept {
    auto &sup = static_cast<supervisor_t &>(*actor);
    if (sup.access<to::address_mapping>().empty()) {
        if (actors_map.size() == 1)
            remove_child(sup);
        plugin_base_t::deactivate();
    }
}

void child_manager_plugin_t::remove_child(const actor_base_t &child) noexcept {
    auto it_actor = actors_map.find(child.get_address());
    assert(it_actor != actors_map.end());
    bool child_started = it_actor->second.strated;
    auto &state = actor->access<to::state>();

    auto shutdown_reason = extended_error_ptr_t{};
    if (state == state_t::INITIALIZING) {
        if (!child_started) {
            auto &policy = static_cast<supervisor_t *>(actor)->access<to::policy>();
            if (policy == supervisor_policy_t::shutdown_failed) {
                auto ec = make_error_code(shutdown_code_t::child_init_failed);
                shutdown_reason = make_error(ec);
            } else {
                auto &init_request = actor->access<to::init_request>();
                if (init_request) {
                    auto ec = make_error_code(error_code_t::failure_escalation);
                    actor->reply_with_error(*init_request, make_error(ec));
                    init_request.reset();
                }
            }
        }
    }

    if (shutdown_reason) {
        actor->do_shutdown(std::move(shutdown_reason));
    }

    cancel_init(&child);
    actors_map.erase(it_actor);

    if (state == state_t::SHUTTING_DOWN && (actors_map.size() <= 1)) {
        actor->shutdown_continue();
    }

    init_continue();
}

void child_manager_plugin_t::init_continue() noexcept {
    if (actor->access<to::state>() == state_t::INITIALIZING) {
        auto &init_request = actor->access<to::init_request>();
        if (init_request && !has_initializing()) {
            reaction_off(reaction_t::INIT);
            actor->init_continue();
        }
    }
}

void child_manager_plugin_t::create_child(const actor_ptr_t &child) noexcept {
    auto &sup = static_cast<supervisor_t &>(*actor);
    child->do_initialize(sup.access<to::system_context>());
    auto &timeout = child->access<to::init_timeout>();
    sup.send<payload::create_actor_t>(actor->get_address(), child, timeout);
    actors_map.emplace(child->get_address(), actor_state_t(child));
    if (static_cast<actor_base_t &>(sup).access<to::state>() == state_t::INITIALIZING) {
        reaction_on(reaction_t::INIT);
    }
}

void child_manager_plugin_t::on_create(message::create_actor_t &message) noexcept {
    auto &sup = static_cast<supervisor_t &>(*actor);
    auto &actor = message.payload.actor;
    auto &actor_address = actor->get_address();
    assert(actors_map.count(actor_address) == 1);
    sup.template request<payload::initialize_actor_t>(actor_address).send(message.payload.timeout);
}

void child_manager_plugin_t::on_init(message::init_response_t &message) noexcept {
    auto &address = message.payload.req->address;
    auto &ec = message.payload.ec;

    auto &sup = static_cast<supervisor_t &>(*actor);
    bool continue_init = false;
    continue_init = !ec && !has_initializing();
    auto it_actor = actors_map.find(address);
    bool actor_found = it_actor != actors_map.end();

    auto &self_state = actor->access<to::state>();
    if (ec) {
        auto &policy = sup.access<to::policy>();
        auto shutdown_self = self_state == state_t::INITIALIZING && policy == supervisor_policy_t::shutdown_self;
        if (shutdown_self) {
            continue_init = false;
            auto &init_request = actor->access<to::init_request>();
            if (init_request) {
                auto reply_ec = make_error_code(error_code_t::failure_escalation);
                actor->reply_with_error(*init_request, make_error(reply_ec, ec));
                init_request.reset();
            } else {
                auto reason = make_error(make_error_code(shutdown_code_t::child_init_failed), ec);
                actor->do_shutdown(reason);
            }
        } else {
            auto source_actor = actor_found ? it_actor->second.actor : this->actor;
            auto reason = make_error(make_error_code(shutdown_code_t::init_failed), ec);
            source_actor->do_shutdown(reason);
        }
    } else {
        /* the if is needed for the very rare case when supervisor was immediately shut down
           right after creation */
        if (actor_found) {
            it_actor->second.initialized = true;
            bool do_start = (address == actor->get_address()) ? (self_state <= state_t::OPERATIONAL)
                                                              : !sup.access<to::synchronize_start>();
            if (do_start) {
                sup.template send<payload::start_actor_t>(address);
                it_actor->second.strated = true;
            }
        }
    }
    if (continue_init) {
        init_continue();
    }
    // no need of treating self as a child
    if (address != actor->get_address()) {
        sup.on_child_init(actor_found ? it_actor->second.actor.get() : nullptr, ec);
    }
}

void child_manager_plugin_t::on_shutdown_trigger(message::shutdown_trigger_t &message) noexcept {
    auto &source_addr = message.payload.actor_address;
    auto &actor_state = actors_map.at(source_addr);
    request_shutdown(actor_state, message.payload.reason);
}

void child_manager_plugin_t::on_shutdown_fail(actor_base_t &actor, const extended_error_ptr_t &ec) noexcept {
    actor.get_supervisor().access<to::system_context>()->on_error(ec);
}

void child_manager_plugin_t::cancel_init(const actor_base_t *child) noexcept {
    auto &sup = static_cast<supervisor_t &>(*actor);
    auto &init_request = child->access<to::init_request>();
    if (init_request) {
        // options: answer instead of actor (easier, but unexpected message can be seen)
        // or forget the init-request.
        auto &timer_id = init_request->payload.id;
        if (sup.access<to::timers_map>().count(timer_id)) {
            sup.access<to::discard_request, request_id_t>(timer_id);
        }
    }
}

void child_manager_plugin_t::on_shutdown_confirm(message::shutdown_response_t &message) noexcept {
    auto &source_addr = message.payload.req->address;
    auto &actor_state = actors_map.at(source_addr);
    actor_state.shutdown = request_state_t::CONFIRMED;
    auto &ec = message.payload.ec;
    auto child_actor = actor_state.actor;
    if (ec) {
        on_shutdown_fail(*child_actor, ec);
    }
    // std::cout << "shutdown confirmed from " << (void*) source_addr.get() << " on " << (void*)actor->address.get() <<
    // "\n";
    auto &sup = static_cast<supervisor_t &>(*actor);
    auto &address_mapping = sup.access<to::address_mapping>();
    if (address_mapping.has_subscriptions(*child_actor)) {
        auto action = [&](auto &info) { static_cast<actor_base_t &>(sup).access<to::lifetime>()->unsubscribe(info); };
        address_mapping.each_subscription(*child_actor, action);
    } else {
        remove_child(*child_actor);
    }
    // no need of treating self as a child
    if (child_actor.get() != actor) {
        sup.on_child_shutdown(child_actor.get(), ec);
    }
}

bool child_manager_plugin_t::handle_init(message::init_request_t *) noexcept { return !has_initializing(); }

bool child_manager_plugin_t::handle_shutdown(message::shutdown_request_t *req) noexcept {
    /* prevent double sending req, i.e. from parent and from self */
    auto &self = actors_map.at(actor->get_address());
    self.shutdown = request_state_t::CONFIRMED;
    request_shutdown(req->payload.request_payload.reason);

    /* only own actor left, which will be handled differently */
    return actors_map.size() == 1 && plugin_base_t::handle_shutdown(req);
}

void child_manager_plugin_t::request_shutdown(actor_state_t &actor_state, const extended_error_ptr_t &reason) noexcept {
    if (actor_state.shutdown == request_state_t::NONE) {
        auto &sup = static_cast<supervisor_t &>(*actor);
        auto &source_actor = actor_state.actor;

        cancel_init(source_actor.get());
        if (source_actor == actor) {
            if (sup.access<to::parent>()) {
                // will be routed via shutdown request
                sup.do_shutdown(reason);
                actor_state.shutdown = request_state_t::SENT;
            } else {
                // do not do shutdown-request on self
                if (actor->access<to::state>() != state_t::SHUTTING_DOWN) {
                    actor_state.shutdown = request_state_t::CONFIRMED;
                    actor->access<to::assign_shutdown_reason>(reason);
                    actor->shutdown_start();
                    request_shutdown(reason);
                    actor->shutdown_continue();
                }
            }
        } else {
            auto &address = source_actor->get_address();
            auto &timeout = source_actor->access<to::shutdown_timeout>();
            sup.request<payload::shutdown_request_t>(address).send(timeout);
            actor_state.shutdown = request_state_t::SENT;
        }
        actor_state.shutdown = request_state_t::SENT;
    }
}

void child_manager_plugin_t::request_shutdown(const extended_error_ptr_t &reason) noexcept {
    for (auto &it : actors_map) {
        request_shutdown(it.second, reason);
    }
}

bool child_manager_plugin_t::handle_unsubscription(const subscription_point_t &point, bool external) noexcept {
    if (point.owner_tag == owner_tag_t::SUPERVISOR) {
        auto &sup = static_cast<supervisor_t &>(*actor);
        auto &address_mapping = sup.access<to::address_mapping>();
        address_mapping.remove(point);
        if (!address_mapping.has_subscriptions(*point.owner_ptr)) {
            remove_child(*point.owner_ptr);
        }
        if (actors_map.size() == 0) {
            plugin_base_t::deactivate();
        }
        if (address_mapping.empty())
            plugin_base_t::deactivate();
        return false; // handled by lifetime
    } else {
        return plugin_base_t::handle_unsubscription(point, external);
    }
}

void child_manager_plugin_t::handle_start(message::start_trigger_t *trigger) noexcept {
    auto &sup = static_cast<supervisor_t &>(*actor);
    if (sup.access<to::synchronize_start>()) {
        for (auto &it : actors_map) {
            auto &address = it.first;
            if (address == actor->get_address())
                continue;
            sup.template send<payload::start_actor_t>(address);
            it.second.strated = true;
        }
    }
    return plugin_base_t::handle_start(trigger);
}

bool child_manager_plugin_t::has_initializing() const noexcept {
    auto init_predicate = [&](auto &it) {
        auto &state = it.second.actor->template access<to::state>();
        bool still_initializing =
            (it.first != actor->get_address()) && (state <= state_t::INITIALIZING) && !it.second.initialized;
        return still_initializing;
    };
    bool has_any = std::any_of(actors_map.begin(), actors_map.end(), init_predicate);
    return has_any;
}
