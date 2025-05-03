//===-- unix_address.cpp -----------------------------------------------*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//

#include "unix_address.hpp"

#include "address.hpp"

#include <filesystem>
#include <sys/socket.h>
#include <utility>

namespace sockify {

// Do I need to make sure that the socket itself is a sockaddr_un, or is that implicit?

UnixDomainAddress::UnixDomainAddress(const value_type& address) noexcept : Address{address}
{
  if (family() != address_family_type::Unix)
    reset();
}

// FIXME: update this ctor
// memset the ss variable;
// initialize the storage, by casting it to sockaddr_un
// and fill in the family and path (extracted from `path` argument)
UnixDomainAddress::UnixDomainAddress(std::filesystem::path path)
    : Address([] {
        sockaddr_storage ss{};
        ss.ss_family = static_cast<sa_family_t>(
            AddressFamily::Unix); // use the given address struct, cast to sa_family_t to match types.
        return ss;
      }()),
      socket_path(std::move(path))
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
  return sizeof(*Address::value());
}

UnixDomainAddress::string_type UnixDomainAddress::to_string() const
{
  return socket_path.u8string();
}

void UnixDomainAddress::do_swap(Address& other) noexcept
{
  auto* rhs = dynamic_cast<UnixDomainAddress*>(&other);
  std::swap(socket_path, rhs->socket_path);
}

int UnixDomainAddress::compare(const Address& other) const noexcept
{
  // NOTE: other is known to be of the same type, will not throw an exception.
  return socket_path.compare(dynamic_cast<const UnixDomainAddress&>(other).socket_path);
}

}; // namespace sockify
