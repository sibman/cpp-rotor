#pragma once

//
// Copyright (c) 2019 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "address.hpp"
#include "message.h"
#include "state.h"
#include "request.h"

namespace rotor {

struct handler_base_t;
using actor_ptr_t = intrusive_ptr_t<actor_base_t>;
using handler_ptr_t = intrusive_ptr_t<handler_base_t>;

namespace payload {

/** \struct initialize_confirmation_t
 *  \brief Message with this payload is sent from an actor to its supervisor to
 * confirm successful initialization
 */
struct initialize_confirmation_t {};

/** \struct initialize_actor_t
 *  \brief Message with this payload is sent from a supervisor to an actor with
 *  initialization request
 */
struct initialize_actor_t {
    using responce_t = initialize_confirmation_t;

    /** \brief target actor address, which is asked for initialization
     *
     * The `actor_address` might be useful for observing the actor initialization
     * in some other actor
     */
    address_ptr_t actor_address;
};

/** \struct start_actor_t
 *  \brief Message with this payload is sent from a supervisor to an actor with
 *  start confirmation
 */
struct start_actor_t {
    /** \brief target actor address, which is asked for start
     *
     * The `actor_address` might be useful for observing the actor start
     * in some other actor
     */
    address_ptr_t actor_address;
};

/** \struct create_actor_t
 *  \brief Message with this payload is sent to supervisor when an actor is
 * created (constructed).
 *
 * The message is needed for internal {@link supervisor_t} housekeeping.
 *
 */
struct create_actor_t {
    /** \brief the intrusive pointer to created actor */
    actor_ptr_t actor;

    pt::time_duration timeout;
};

struct shutdown_trigger_t {
    address_ptr_t actor_address;
};

/** \struct shutdown_confirmation_t
 *  \brief Message with this payload is sent from an actor to its supervisor to
 * confirm successful initialization
 */
struct shutdown_confirmation_t {};

/** \struct shutdown_request_t
 *  \brief Message with this payload is sent from a supervisor to an actor with
 *  shutdown request
 */
struct shutdown_request_t {
    using responce_t = shutdown_confirmation_t;
    /** \brief source actor address, which has been shutted down
     *
     * The `actor_address` might be useful for observing the actor shutting down
     * in some other actor
     */
    address_ptr_t actor_address;
};

/** \struct handler_call_t
 *  \brief Message with this payload is forwarded to the handler's supervisor for
 * the delivery of the original message.
 *
 * An `address` in `rotor` is always generated by a supervisor. All messages to the
 * address are initially pre-processed by the supervisor: if the destination handler
 * supervisor is the same as the message address supervisor, the handler is invoked
 * immediately. Otherwise, if a handler belongs to different supervisor (i.e. may
 * be to different event loop), then the delivery of the message is forwarded to
 * that supersior.
 *
 */
struct handler_call_t {
    /** \brief The original message (intrusive pointer) sent to an address */
    message_ptr_t orig_message;

    /** \brief The handler (intrusive pointer) on some external supervisor,
     * which can process the original message */
    handler_ptr_t handler;
};

/** \struct external_subscription_t
 *  \brief Message with this payload is forwarded to the target address supervisor
 * for recording subscription in the external (foreign) handler
 *
 * When a supervisor process subscription requests from it's (local) actors, it
 * can found that the `target_address` belongs to some other (external/foreign)
 * supervisor. In that case the subscription is forwarded to the external
 * supervisor.
 *
 */
struct external_subscription_t {
    /** \brief The target address for subscription */
    address_ptr_t target_address;

    /** \brief The handler (intrusive pointer) for processing message */
    handler_ptr_t handler;
};

/** \struct subscription_confirmation_t
 *  \brief Message with this payload is sent from a supervisor to an actor when
 *  successfull subscription to the `target` address occurs.
 *
 * The message is needed for internal {@link actor_base_t} housekeeping.
 *
 */
struct subscription_confirmation_t {
    /** \brief The target address for subscription */
    address_ptr_t target_address;

    /** \brief The handler (intrusive pointer) for processing message */
    handler_ptr_t handler;
};

/** \struct external_unsubscription_t
 *  \brief Message with this payload is forwarded to the target address supervisor
 * for recording unsubscription in the external (foreign) handler.
 *
 * The message is symmetrical to the {@link external_subscription_t}.
 *
 */
struct external_unsubscription_t {
    /** \brief The target address for unsubscription */
    address_ptr_t target_address;

    /** \brief The handler (intrusive pointer) for processing message */
    handler_ptr_t handler;
};

/** \struct commit_unsubscription_t
 *  \brief Message with this payload is sent to the target address supervisor
 * for confirming unsubscription in the external (foreign) handler.
 *
 * The message is an actor-reply to {@link external_subscription_t} request.
 *
 */
struct commit_unsubscription_t {
    /** \brief The target address for unsubscription */
    address_ptr_t target_address;

    /** \brief The handler (intrusive pointer) for processing message */
    handler_ptr_t handler;
};

/** \struct unsubscription_confirmation_t
 *  \brief Message with this payload is sent from a supervisor to an actor with
 *  confirmation that `handler` is no longer subscribed to `target_address`
 */
struct unsubscription_confirmation_t {
    /** \brief The target address for unsubscription */
    address_ptr_t target_address;

    /** \brief The handler (intrusive pointer) for processing message */
    handler_ptr_t handler;
};

/** \struct state_response_t
 *  \brief Message with this payload is sent to an actor, which
 * asked the state of the subject actor (represented by it's address)
 *
 */
struct state_response_t {
    /** \brief The state of the asked actor */
    state_t state;
};

/** \struct state_request_t
 *  \brief Message with this payload is sent to supervisor to query
 * actor (defined by it's address - `subject_addr`).
 */
struct state_request_t {
    using responce_t = state_response_t;

    /** \brief The actor address in question */
    address_ptr_t subject_addr;
};

} // namespace payload

namespace message {

using init_request_t = request_traits_t<payload::initialize_actor_t>::request::message_t;
using init_response_t = request_traits_t<payload::initialize_actor_t>::responce::message_t;

using shutdown_trigger_t = message_t<payload::shutdown_trigger_t>;
using shutdown_request_t = request_traits_t<payload::shutdown_request_t>::request::message_t;
using shutdown_responce_t = request_traits_t<payload::shutdown_request_t>::responce::message_t;

using state_request_t = request_traits_t<payload::state_request_t>::request::message_t;
using state_response_t = request_traits_t<payload::state_request_t>::responce::message_t;

} // namespace message

} // namespace rotor
