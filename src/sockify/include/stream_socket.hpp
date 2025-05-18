//===-- stream_socket.hpp - TCP-based Socket -----*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines the implementation of a Stream Socket, which uses TCP to communicate.
/// Inherits from the Socket Abstract Base Class.
///
//===----------------------------------------------------------------------===//

#ifndef SOCKIFY_STREAM_SOCKET_HPP
#define SOCKIFY_STREAM_SOCKET_HPP

#include "address.hpp"
#include "config.hpp"
#include "socket.hpp"

#include <cstddef>
#include <memory>
#include <system_error>
#include <utility>

namespace sockify {
class SOCKIFY_EXPORT StreamSocket : public Socket {
public:
  explicit StreamSocket(address_family_type family = AddressFamily::IPv4, bool inheritable = false);

  explicit StreamSocket(native_handle_type handle,
                        address_family_type family = AddressFamily::Unknown,
                        bool inheritable           = false);

  StreamSocket(const StreamSocket& other);
  StreamSocket(StreamSocket&& other) noexcept;

  StreamSocket& operator=(const StreamSocket& other);
  StreamSocket& operator=(StreamSocket&& other) noexcept;

  ~StreamSocket() override;

protected: // NVI Helpers
  native_handle_type do_release() noexcept override;
  void do_shutdown(ShutdownMode how, std::error_code& ec) noexcept override;
  void do_bind(const address_type& address, std::error_code& ec) noexcept override;
  void do_connect(const address_type& address, std::error_code& ec) noexcept override;
  void do_listen(int backlog, std::error_code& ec) noexcept override;
  std::unique_ptr<Socket> do_accept(std::error_code& ec) noexcept override;
  std::size_t
  do_send(const void* buf, std::size_t len, const address_type* dest, int flags, std::error_code& ec) noexcept override;
  std::pair<std::size_t, std::unique_ptr<address_type>>
  do_recv(void* buf, std::size_t len, int flags, std::error_code& ec) noexcept override;
};
} // namespace sockify

#endif // ifndef