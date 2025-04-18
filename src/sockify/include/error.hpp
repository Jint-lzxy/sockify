//===-- error.hpp - Error handling interfaces -------------------*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Provides error enums, categories and exception types used throughout the
/// library. This includes integration with the standard error system.
///
//===----------------------------------------------------------------------===//

#ifndef SOCKIFY_ERROR_HPP
#define SOCKIFY_ERROR_HPP

#include "config.hpp"

#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>

namespace sockify {

/// Error codes for socket operations.
///
/// These represent standard and library-specific socket-related failure states.
/// Compatible with std::error_code and std::error_condition.
enum class socket_errc : std::uint8_t {
  //--- Standard-compatible values ---
  success = 0,            ///< No error.
  address_in_use,         ///< Address already in use.
  address_not_available,  ///< Requested address is not available.
  bad_file_descriptor,    ///< Invalid file descriptor.
  connection_refused,     ///< Connection refused.
  connection_reset,       ///< Connection reset by peer.
  host_unreachable,       ///< Remote host is unreachable.
  message_too_large,      ///< Message size exceeds allowed maximum.
  network_unreachable,    ///< Network unreachable.
  no_buffer_space,        ///< No buffer space available.
  not_a_socket,           ///< Operation attempted on a non-socket.
  not_connected,          ///< Not connected.
  not_supported,          ///< Operation not supported.
  operation_in_progress,  ///< Non-blocking operation in progress.
  operation_would_block,  ///< Would block (try again).
  permission_denied,      ///< Permission denied.
  protocol_not_supported, ///< Chosen protocol is not supported.
  timed_out,              ///< Operation timed out.

  //--- Sockify-specific error conditions ---
  already_connected,      ///< Already connected.
  already_open,           ///< Socket already open.
  event_loop_closed,      ///< Event loop closed.
  io_error,               ///< Generic I/O error.
  not_open,               ///< Socket not open.
  operation_cancelled,    ///< Operation was cancelled.
  peer_closed_connection, ///< Remote peer closed the connection.
  resource_exhausted,     ///< System resources exhausted.
  shutdown_requested,     ///< Operation failed due to shutdown request.

  //--- TLS-specific error codes ---
  tls_zero_return, ///< Clean TLS shutdown during read/write.
  tls_want_read,   ///< TLS requires more input data.
  tls_want_write,  ///< TLS wants to write more data.
  tls_syscall,     ///< TLS-level system call failure.
  tls_eof,         ///< Unexpected TLS EOF.
  tls_cert_verify  ///< Certificate verification failure.
};

/// Converts a socket_errc to a std::error_code.
/// \param ec The error enum to convert.
/// \returns A std::error_code in the socket_category().
SOCKIFY_EXPORT std::error_code make_error_code(socket_errc ec) noexcept;

/// Converts a socket_errc to a std::error_condition.
/// \param ec The error enum to convert.
/// \returns A std::error_condition in the socket_category().
SOCKIFY_EXPORT std::error_condition make_error_condition(socket_errc ec) noexcept;

} // namespace sockify

namespace std {

/// Enables implicit conversion from sockify::socket_errc to std::error_condition.
template <>
struct is_error_condition_enum<sockify::socket_errc> : true_type {};

} // namespace std

namespace sockify {
namespace details {

/// Custom error category for socket errors.
///
/// Used internally to provide category name, human-readable messages,
/// and condition equivalence logic.
class SOCKIFY_HIDDEN socket_category_impl : public std::error_category {
public:
  /// Returns the name of the category ("sockify::socket_category").
  const char* name() const noexcept override;

  /// Returns a human-readable message for the given error code.
  std::string message(int ev) const override;

  /// Implements equivalence testing between conditions and error codes.
  bool equivalent(const std::error_code& code, int condition) const noexcept override;
};

} // namespace details

/// Returns the singleton sockify error category instance.
/// \returns A reference to the internal socket_category_impl.
/// \note Thread-safe and lazily constructed.
inline SOCKIFY_EXPORT const details::socket_category_impl& socket_category() noexcept
{
  static details::socket_category_impl singleton;
  return singleton;
}

/// Exception type for socket-related failures.
///
/// Thin wrapper around std::system_error used for semantic clarity.
/// This type behaves almost identically to std::system_error.
class SOCKIFY_EXPORT socket_error : public std::system_error {
public:
  /// Constructs a socket_error with an explanatory message and error code.
  socket_error(const std::string& what_arg, std::error_code ec);

  /// Constructs a socket_error with a C-string and error code.
  socket_error(const char* what_arg, std::error_code ec);

  /// Copy constructor.
  socket_error(const socket_error& other) noexcept = default;

  /// Copy assignment operator.
  socket_error& operator=(const socket_error& other) noexcept = default;

  /// Destructor.
  ~socket_error() override = default;
};

} // namespace sockify

#endif
