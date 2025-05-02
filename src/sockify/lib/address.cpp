//===-- address.cpp -----------------------------------------------*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//

#include "address.hpp"

#include <cstring>
#include <memory>
#include <ostream>
#include <utility>

namespace sockify {

Address::Address(const_reference address) noexcept : address{address}, address_family{address.ss_family}
{
  if (!details::is_one_of<address_family_type,
                          address_family_type::Unknown,
                          address_family_type::IPv4,
                          address_family_type::IPv6,
                          address_family_type::Unix>(address_family))
    address_family = address_family_type::Unknown;
}

Address& Address::operator=(const Address& other) noexcept
{
  if (this == &other)
    return *this;

  address        = other.address;
  address_family = other.address_family;
  return *this;
}
Address::Address(Address&& other) noexcept : address{other.address}, address_family{other.address_family}
{
  other.reset();
}

Address& Address::operator=(Address&& other) noexcept
{
  if (this == &other) {
    return *this;
  }

  address        = other.address;
  address_family = other.address_family;
  other.reset();

  return *this;
}

// accessors
// Return the address of the socket.
Address::const_pointer Address::value() const noexcept
{
  return std::addressof(address);
}

Address::address_family_type Address::family() const noexcept
{
  return address_family;
}

// observers

Address::operator bool() const noexcept
{
  return has_value();
}

bool Address::has_value() const noexcept
{
  return address_family != AddressFamily::Unknown;
}

void Address::reset() noexcept
{
  address_family = AddressFamily::Unknown;
}

int Address::compare(const Address& other) const noexcept
{
  const socklen_t socksize{size()};
  const socklen_t other_socksize{other.size()};

  if (socksize < other_socksize)
    return -1;
  if (socksize > other_socksize)
    return 1;
  return std::memcmp(value(), other.value(), socksize);
}

void Address::swap(Address& other) noexcept
{
  std::swap(address, other.address);
  std::swap(address_family, other.address_family);
  do_swap(other);
}

// friend-based methods

bool operator==(const Address& lhs, const Address& rhs)
{
  return lhs.address_family == rhs.address_family && lhs.compare(rhs) == 0;
}

bool operator!=(const Address& lhs, const Address& rhs)
{
  return !(lhs == rhs);
}

bool operator<(const Address& lhs, const Address& rhs)
{
  if (!lhs || !rhs)
    return rhs.has_value();

  // both not empty
  if (lhs.address_family == rhs.address_family)
    return lhs.compare(rhs) < 0;
  return lhs.address_family < rhs.address_family;
}

bool operator<=(const Address& lhs, const Address& rhs)
{
  return !(rhs < lhs);
}

bool operator>(const Address& lhs, const Address& rhs)
{
  return rhs < lhs;
}

bool operator>=(const Address& lhs, const Address& rhs)
{
  return !(lhs < rhs);
}

void swap(Address& lhs, Address& rhs) noexcept
{
  lhs.swap(rhs);
}

} // namespace sockify

std::ostream& operator<<(std::ostream& strm, const sockify::Address& addr)
{
  strm << addr.to_string();
  return strm;
}