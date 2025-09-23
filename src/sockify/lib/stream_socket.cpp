//===-- stream_socket.cpp -----------------------------------------------*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//

#include "stream_socket.hpp"

#include "socket.hpp"

#include <cerrno>
#include <cstddef>
#include <memory>
#include <sys/socket.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace sockify {

StreamSocket::StreamSocket(address_family_type family, bool inheritable) : Socket(family, socket(static_cast<int>(family), SOCK_STREAM, 0), inheritable)
{} 

StreamSocket::StreamSocket(native_handle_type handle, address_family_type family, bool inheritable)
    : Socket{family, handle, inheritable}
{}

// NVI Helpers

StreamSocket::native_handle_type StreamSocket::do_release() noexcept
{
  const native_handle_type handle = native_handle();
  return handle;
}

void StreamSocket::do_shutdown(ShutdownMode how, std::error_code& ec) noexcept
{
  ec = std::error_code(); // reset error code
  if (::shutdown(native_handle(), static_cast<int>(how)) != 0)
    ec = std::error_code(errno, std::system_category()); // correct?
}

void StreamSocket::do_bind(const address_type& address, std::error_code& ec) noexcept
{
  ec = std::error_code();
  if (::bind(native_handle(), reinterpret_cast<const struct sockaddr*>(address.value()), address.size()) != 0)
    ec = std::error_code(errno, std::system_category());
}

void StreamSocket::do_connect(const address_type& address, std::error_code& ec) noexcept
{
  ec = std::error_code();
  if (::connect(native_handle(), reinterpret_cast<const struct sockaddr*>(address.value()), address.size()) != 0)
    ec = std::error_code(errno, std::system_category());
}

void StreamSocket::do_listen(int backlog, std::error_code& ec) noexcept
{
  ec = std::error_code();
  if (::listen(native_handle(), backlog) != 0)
    ec = std::error_code(errno, std::system_category());
}

std::unique_ptr<Socket>
StreamSocket::do_accept(std::error_code& ec) noexcept // incomplete. 
{
  ec = std::error_code();
  auto handle = ::accept(native_handle(), nullptr, nullptr);
  if(handle == -1)
    ec = std::error_code(errno, std::system_category());
  // make a stream ptr here. 
// unless we are implementing a socket that accepts without a sockaddr storage?
}

std::size_t StreamSocket::do_send(
    const void* buf, std::size_t len, const address_type* dest, int flags, std::error_code& ec) noexcept
{
  // dest is unused if this is a TCP socket, which it is.
  ec        = std::error_code();
  auto sent = ::send(native_handle(), buf, len, flags);
  if (sent == -1) {
    ec = std::error_code(errno, std::system_category());
    return 0;
  }
  return sent;
}

std::pair<std::size_t, std::unique_ptr<StreamSocket::address_type>>
StreamSocket::do_recv(void* buf, std::size_t len, int flags, std::error_code& ec) noexcept
{
  ec            = std::error_code();
  auto recieved = ::recv(native_handle(), buf, len, flags);
  if (recieved == -1) {
    ec = std::error_code(errno, std::system_category());
    return {0, nullptr};
  }
  return {recieved, nullptr};
}

} // namespace sockify