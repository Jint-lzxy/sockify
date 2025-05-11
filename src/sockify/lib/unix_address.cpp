//===-- unix_address.cpp ----------------------------------------*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//

#include "unix_address.hpp"

#include "address.hpp"
#include "config.hpp"

#include <cstring>
#include <filesystem>
#include <iterator>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <utility>

// Anonymous namespace for misc helpers, should not be exported from this file
namespace {

sockaddr_storage SOCKIFY_HIDDEN make_unix(const std::string& path)
{
  sockaddr_storage addr{};
  std::memset(&addr, 0, sizeof(addr));

  auto& un_addr       = reinterpret_cast<sockaddr_un&>(addr);
  const auto max_size = std::size(un_addr.sun_path);
  un_addr.sun_family  = AF_UNIX;
  std::strncpy(un_addr.sun_path, path.c_str(), max_size);

  if (un_addr.sun_path[max_size - 1] != '\0')
    throw std::overflow_error("Path size exceeds available buffer size");
  return addr;
}

} // namespace

namespace sockify {

UnixDomainAddress::UnixDomainAddress(const value_type& address) noexcept : Address{address}
{
  if (family() != address_family_type::Unix)
    reset();
}

UnixDomainAddress::UnixDomainAddress(std::filesystem::path path)
    : Address{make_unix(path.u8string())}, socket_path{std::move(path)}
{}

const std::filesystem::path& UnixDomainAddress::path() const&
{
  return socket_path;
}

std::filesystem::path UnixDomainAddress::path() &&
{
  return std::move(socket_path);
}

socklen_t UnixDomainAddress::size() const noexcept
{
  return sizeof(sockaddr_un);
}

UnixDomainAddress::string_type UnixDomainAddress::to_string() const
{
  return socket_path.u8string();
}

void UnixDomainAddress::do_swap(Address& other) noexcept
{
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast): other is known to be of the same type.
  auto& rhs = static_cast<UnixDomainAddress&>(other);
  std::swap(socket_path, rhs.socket_path);
}

int UnixDomainAddress::compare(const Address& other) const noexcept
{
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast): other is known to be of the same type.
  return socket_path.compare(static_cast<const UnixDomainAddress&>(other).socket_path);
}

}; // namespace sockify
