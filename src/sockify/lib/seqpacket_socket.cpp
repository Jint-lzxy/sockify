//===-- seqpacket_socket.cpp -----------------------------------------------*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//

#include "seqpacket_socket.hpp"
#include "error.hpp"
#include "socket.hpp"

#include <cerrno>
#include <cstddef>
#include <memory>
#include <sys/socket.h>
#include <system_error>
#include <utility>

namespace sockify {
    SeqPacketSocket::SeqPacketSocket(address_family_type family, bool inheritable) : Socket(family, socket(static_cast<int>(family), SOCK_SEQPACKET, 0), inheritable)
{} 

SeqPacketSocket::SeqPacketSocket(native_handle_type handle, bool inheritable)
    : Socket{[&handle]() {
               sockaddr_storage addr{};
               socklen_t len = sizeof(addr);
               if (::getsockname(handle, reinterpret_cast<sockaddr*>(&addr), &len) == -1) {
                 throw socket_error{
                     "Unable to determine the address family of handle",
                     std::error_code{errno, std::system_category()}
                 };
               }
               return address_family_type{addr.ss_family};
             }(), handle, inheritable}
{}

// NVI Helpers

SeqPacketSocket::native_handle_type SeqPacketSocket::do_release() noexcept
{
  return native_handle();
}

void SeqPacketSocket::do_shutdown(ShutdownMode how, std::error_code& ec) noexcept
{
  if (::shutdown(native_handle(), static_cast<int>(how)) != 0)
    ec = std::error_code(errno, std::system_category());
  else
  ec.clear();
}

void SeqPacketSocket::do_bind(const address_type& address, std::error_code& ec) noexcept
{
  if (::bind(native_handle(), reinterpret_cast<const sockaddr*>(address.value()), address.size()) != 0)
    ec = std::error_code(errno, std::system_category());
    else
      ec.clear();
}

void SeqPacketSocket::do_connect(const address_type& address, std::error_code& ec) noexcept
{
  if (::connect(native_handle(), reinterpret_cast<const struct sockaddr*>(address.value()), address.size()) != 0)
    ec = std::error_code(errno, std::system_category());
    else
    ec.clear();
}

void SeqPacketSocket::do_listen(int backlog, std::error_code& ec) noexcept
{
  if (::listen(native_handle(), backlog) != 0)
    ec = std::error_code(errno, std::system_category());
    else 
    ec.clear();
}

std::unique_ptr<Socket>
SeqPacketSocket::do_accept(std::error_code& ec) noexcept
{
  ec = std::error_code();
  auto handle = ::accept(native_handle(), nullptr, nullptr);
  if(handle == -1)
    ec = std::error_code(errno, std::system_category());
  // make a stream ptr here. 
// unless we are implementing a socket that accepts without a sockaddr storage?
}

std::size_t SeqPacketSocket::do_send(
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

std::pair<std::size_t, std::unique_ptr<SeqPacketSocket::address_type>>
SeqPacketSocket::do_recv(void* buf, std::size_t len, int flags, std::error_code& ec) noexcept
{
  ec            = std::error_code();
  auto recieved = ::recvmsg(native_handle(), reinterpret_cast<msghdr*>(buf), len);
  if (recieved == -1) {
    ec = std::error_code(errno, std::system_category());
    return {0, nullptr};
  }
  return {recieved, nullptr};
}

} // namespace sockify