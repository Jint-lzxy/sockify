
#include "datagram_socket.hpp"

#include "error.hpp"
#include "socket.hpp"

#include <cerrno>
#include <cstddef>
#include <memory>
#include <sys/socket.h>
#include <system_error>
#include <utility>

namespace sockify {

DatagramSocket::DatagramSocket(address_family_type family, bool inheritable)
    : Socket{family, ::socket(static_cast<int>(family), SOCK_DGRAM, 0), inheritable}
{}

DatagramSocket::DatagramSocket(native_handle_type handle, bool inheritable)
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
             }(),
             handle,
             inheritable} // trivial? (determine using handle)
{}

// NVI Helpers

DatagramSocket::native_handle_type DatagramSocket::do_release() noexcept
{
  return native_handle();
}

void DatagramSocket::do_shutdown(ShutdownMode how, std::error_code& ec) noexcept
{
  if (::shutdown(native_handle(), static_cast<int>(how)) != 0)
    ec = std::error_code(errno, std::system_category()); // correct?
  else
    ec.clear();
}

void DatagramSocket::do_bind(const address_type& address, std::error_code& ec) noexcept
{
  if (::bind(native_handle(), reinterpret_cast<const struct sockaddr*>(address.value()), address.size()) != 0)
    ec = std::error_code(errno, std::system_category());
  else
    ec.clear();
}

void DatagramSocket::do_connect(const address_type& address, std::error_code& ec) noexcept
{
  if (::connect(native_handle(), reinterpret_cast<const struct sockaddr*>(address.value()), address.size()) != 0)
    ec = std::error_code(errno, std::system_category());
  else
    ec.clear();
}

void DatagramSocket::do_listen(int /*backlog*/, std::error_code& ec) noexcept
{
  ec = make_error_code(sockify::socket_errc::not_supported);
}

std::unique_ptr<Socket> DatagramSocket::do_accept(std::error_code& ec) noexcept
{
  ec = make_error_code(sockify::socket_errc::not_supported);
}

std::size_t DatagramSocket::do_send(
    const void* buf, std::size_t len, const address_type* dest, int flags, std::error_code& ec) noexcept
{
  ssize_t sent = 0;
  if (dest == nullptr) {
    sent = ::send(native_handle(), buf, len, flags);
  } else {
    sent = ::sendto(native_handle(), buf, len, flags, reinterpret_cast<const sockaddr*>(dest->value()), dest->size());
  }
  if (sent == -1) {
    ec = std::error_code(errno, std::system_category());
    return 0;
  }
  return sent;
}

std::pair<std::size_t, std::unique_ptr<DatagramSocket::address_type>>
DatagramSocket::do_recv(void* buf, std::size_t len, int flags, std::error_code& ec) noexcept
{
  sockaddr_storage addr{};
  socklen_t addr_len = sizeof(addr);
  auto recieved      = ::recvfrom(native_handle(), buf, len, flags, reinterpret_cast<sockaddr*>(&addr), &addr_len);
  if (recieved == -1) {
    ec = std::error_code(errno, std::system_category());
    return {0, nullptr};
  }
  return {recieved, details::make_address(addr)};
}
} // namespace sockify