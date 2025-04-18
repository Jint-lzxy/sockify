//===-- error.cpp -----------------------------------------------*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//

#include "error.hpp"

#include <string>
#include <system_error>

namespace sockify {

//===----------------------------------------------------------------------===//
// sockify::socket_category implementation
namespace details {

const char* socket_category_impl::name() const noexcept { return "sockify::socket_category"; }

std::string socket_category_impl::message(int ev) const
{
  switch (static_cast<socket_errc>(ev)) {
  // --- Standard-compatible values ---
  case socket_errc::success:
    return "success";
  case socket_errc::address_in_use:
    return "address already in use";
  case socket_errc::address_not_available:
    return "address not available";
  case socket_errc::bad_file_descriptor:
    return "bad file descriptor";
  case socket_errc::connection_refused:
    return "connection refused";
  case socket_errc::connection_reset:
    return "connection reset by peer";
  case socket_errc::host_unreachable:
    return "remote host unreachable";
  case socket_errc::message_too_large:
    return "message too large";
  case socket_errc::network_unreachable:
    return "network unreachable";
  case socket_errc::no_buffer_space:
    return "no buffer space available";
  case socket_errc::not_a_socket:
    return "not a socket";
  case socket_errc::not_connected:
    return "not connected";
  case socket_errc::not_supported:
    return "operation not supported";
  case socket_errc::operation_in_progress:
    return "operation in progress";
  case socket_errc::operation_would_block:
    return "operation would block";
  case socket_errc::permission_denied:
    return "permission denied";
  case socket_errc::protocol_not_supported:
    return "protocol not supported";
  case socket_errc::timed_out:
    return "timed out";

  // --- Sockify-specific error conditions ---
  case socket_errc::already_connected:
    return "already connected";
  case socket_errc::already_open:
    return "socket already open";
  case socket_errc::event_loop_closed:
    return "event loop closed";
  case socket_errc::io_error:
    return "I/O error";
  case socket_errc::not_open:
    return "socket not open";
  case socket_errc::operation_cancelled:
    return "operation cancelled";
  case socket_errc::peer_closed_connection:
    return "peer closed connection";
  case socket_errc::resource_exhausted:
    return "resource exhausted";
  case socket_errc::shutdown_requested:
    return "shutdown requested";

  // --- TLS-specific error codes ---
  case socket_errc::tls_zero_return:
    return "TLS clean shutdown";
  case socket_errc::tls_want_read:
    return "TLS wants read";
  case socket_errc::tls_want_write:
    return "TLS wants write";
  case socket_errc::tls_syscall:
    return "TLS syscall failure";
  case socket_errc::tls_eof:
    return "TLS unexpected EOF";
  case socket_errc::tls_cert_verify:
    return "TLS certificate verification failure";

  default:
    return "unknown sockify socket error";
  }
}

bool socket_category_impl::equivalent(const std::error_code& code, int condition) const noexcept
{
  // 1) if both are in sockify::socket_category, compare the raw values
  if (code.category() == *this)
    return code.value() == condition;

  // 2) otherwise try to map certain socket_errc to std::errc
  using errc = std::errc;
  switch (static_cast<socket_errc>(condition)) {
  case socket_errc::address_in_use:
    return code == std::make_error_code(errc::address_in_use);
  case socket_errc::address_not_available:
    return code == std::make_error_code(errc::address_not_available);
  case socket_errc::bad_file_descriptor:
    return code == std::make_error_code(errc::bad_file_descriptor);
  case socket_errc::connection_refused:
    return code == std::make_error_code(errc::connection_refused);
  case socket_errc::connection_reset:
    return code == std::make_error_code(errc::connection_reset);
  case socket_errc::host_unreachable:
    return code == std::make_error_code(errc::host_unreachable);
  case socket_errc::message_too_large:
    return code == std::make_error_code(errc::message_size);
  case socket_errc::network_unreachable:
    return code == std::make_error_code(errc::network_unreachable);
  case socket_errc::no_buffer_space:
    return code == std::make_error_code(errc::no_buffer_space);
  case socket_errc::not_a_socket:
    return code == std::make_error_code(errc::not_a_socket);
  case socket_errc::not_connected:
    return code == std::make_error_code(errc::not_connected);
  case socket_errc::not_supported:
    return code == std::make_error_code(errc::operation_not_supported);
  case socket_errc::operation_in_progress:
    return code == std::make_error_code(errc::operation_in_progress);
  case socket_errc::operation_would_block:
    return code == std::make_error_code(errc::operation_would_block);
  case socket_errc::permission_denied:
    return code == std::make_error_code(errc::permission_denied);
  case socket_errc::protocol_not_supported:
    return code == std::make_error_code(errc::protocol_not_supported);
  case socket_errc::timed_out:
    return code == std::make_error_code(errc::timed_out);

  // Map a few of the library‑specific ones where std::errc has a match:
  case socket_errc::already_connected:
    return code == std::make_error_code(errc::already_connected);
  case socket_errc::io_error:
    return code == std::make_error_code(errc::io_error);
  case socket_errc::operation_cancelled:
    return code == std::make_error_code(errc::operation_canceled);
  case socket_errc::peer_closed_connection:
    return code == std::make_error_code(errc::connection_reset);
  case socket_errc::resource_exhausted:
    return code == std::make_error_code(errc::no_buffer_space);

  default:
    return false;
  }
}

} // namespace details

std::error_code make_error_code(socket_errc ec) noexcept { return {static_cast<int>(ec), socket_category()}; }

std::error_condition make_error_condition(socket_errc ec) noexcept { return {static_cast<int>(ec), socket_category()}; }

//===----------------------------------------------------------------------===//
// sockify::socket_error implementation
socket_error::socket_error(const std::string& what_arg, std::error_code ec) : std::system_error(ec, what_arg) {}

socket_error::socket_error(const char* what_arg, std::error_code ec) : std::system_error(ec, what_arg) {}

} // namespace sockify
