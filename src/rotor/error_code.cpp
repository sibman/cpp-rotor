//
// Copyright (c) 2019-2021 Ivan Baidakou (basiliscos) (the dot dmol at gmail dot com)
//
// Distributed under the MIT Software License
//

#include "rotor/error_code.h"

namespace rotor {
namespace details {

const char *error_code_category::name() const noexcept { return "rotor_error"; }
const char *shutdown_code_category::name() const noexcept { return "rotor_shutdown"; }

std::string error_code_category::message(int c) const {
    switch (static_cast<error_code_t>(c)) {
    case error_code_t::success:
        return "success";
    case error_code_t::cancelled:
        return "request has been cancelled";
    case error_code_t::request_timeout:
        return "request timeout";
    case error_code_t::supervisor_defined:
        return "supervisor is already defined";
    case error_code_t::already_registered:
        return "service name is already registered";
    case error_code_t::actor_misconfigured:
        return "actor is misconfigured";
    case error_code_t::actor_not_linkable:
        return "actor is not linkeable";
    case error_code_t::already_linked:
        return "already linked";
    case error_code_t::failure_escalation:
        return "failure escalation (child actor died)";
    case error_code_t::unknown_service:
        return "the requested service name is not registered";
    }
    return "unknown";
}

} // namespace details
} // namespace rotor

namespace rotor {

const static details::error_code_category error_category;
const static details::shutdown_code_category shutdown_category;
const details::error_code_category &error_code_category() { return error_category; }
const details::shutdown_code_category &shutdown_code_category() { return shutdown_category; }

} // namespace rotor
