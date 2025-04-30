//===-- address.cpp -----------------------------------------------*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//

#include "address.hpp"

#include <cstring> // memcmp
#include <utility>

Address::Address()
{}

Address::Address(const value_type& address) noexcept : address_family{address.ss_family}
{}

Address::Address(const Address& other) noexcept : address{other.address}, address_family{other.address_family}
{}

Address& Address::operator=(const Address& other) noexcept
{
  if (this != &other) {
    address        = other.address;
    address_family = other.address_family;
  }
  return *this;
}

Address::Address(Address&& other) noexcept
{
  address        = other.address;
  address_family = other.address_family;
  // placing sockaddr_storage into an unspecified state?
  other.reset();
}

Address& Address::operator=(Address&& other) noexcept
{
  if (this != &other) { // prevent moving into itself
    address        = other.address;
    address_family = other.address_family;

    // placing sockaddr_storage into an unspecified state?
    other.reset();
  }
  return *this;
}

// accessors
/// Return the address of the socket (?).
const value_type* Address::value() const noexcept
{
  return std::addressof(address);
}

address_family_type Address::family() const noexcept
{
  return address_family;
}

// observers

explicit Address::operator bool() const noexcept
{ // correct?
  return (address_family != AddressFamily::Unknown) && (size() > 0);
}

bool Address::has_value() const noexcept
{
  return static_cast<bool>(*this);
}

void Address::reset() noexcept
{
  address        = {};
  address_family = AddressFamily::Unknown;
}

bool Address::operator==(const Address& other) const noexcept
{ // not O(1)?
  if (address_family == other.address_family && compare(other) == 0)
    return true;
  return false;
}

bool Address::operator!=(const Address& other) const noexcept
{
  return !(*this == other);
}

int Address::compare(const Address& other) const noexcept
{
  socklen_t socksize{size()};
  socklen_t other_socksize{other.size()};

  if (socksize < other_socksize)
    return -1;
  else if (socksize > other_socksize)
    return 1;
  return std::memcmp(value(), other.value(), socksize); // check if this handles 0-byte values
}

void Address::swap(Address& other) noexcept
{
  std::swap(address, other.address);
  std::swap(address_family, other.address_family);
  do_swap(other);
}

// friend-based methods

bool operator<(const Address& lhs, const Address& rhs)
{
  if (lhs.address_family == rhs.address_family)
    return lhs.compare(rhs) < 0;

  bool lhs_empty = static_cast<bool>(lhs);
  bool rhs_empty = static_cast<bool>(rhs);

  if (!lhs_empty && !rhs_empty) {
    return static_cast<int>(lhs.address_family) < static_cast<int>(rhs.address_family);
  } else if (rhs_empty || (lhs_empty && rhs_empty)) {
    return false;
  } else if (lhs_empty) {
    return true;
  }

  // since socket type ID is dependent on OS, not sure if it is good idea?
  // I can't rely on iota to assign values to impose a string order
}

bool operator<=(const Address& lhs, const Address& rhs)
{
  return (lhs < rhs || lhs == rhs);
}

bool operator>(const Address& lhs, const Address& rhs)
{
  return !(lhs <= rhs);
}

bool operator>=(const Address& lhs, const Address& rhs)
{
  return !(lhs < rhs);
}

std::ostream& operator<<(std::ostream& strm, const Address& addr)
{
  strm << addr.to_string();
  return strm;
}

void swap(Address& lhs, Address& rhs) noexcept
{ // correct implementation?
  lhs.swap(rhs);
}