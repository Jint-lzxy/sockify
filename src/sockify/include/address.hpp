//===-- address.hpp - Address interfaces -------------------*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Provides the address base class which is inherited by all derived
/// address classes used throughout the library.
///
//===----------------------------------------------------------------------===//

#ifndef ADDRESS_HPP
#define ADDRESS_HPP

#include "config.hpp"

#include <cstdint>
#include <ostream>
#include <string>
#include <sys/socket.h>
#include <type_traits>

namespace sockify {

namespace details {
/// A compile‑time set of enum values, with fast constexpr “contains” checks.
template <typename E, E... Vs>
struct EnumSet {
  static_assert(std::is_enum_v<E>, "enum_set may only be used with enum types");

  using underlying_t = std::underlying_type_t<E>;

  /// Returns true if v is one of the Vs...
  static constexpr bool contains(E v) noexcept
  {
    return ((v == Vs) || ...);
  }

  /// Returns true if the underlying integer u matches one of the Vs...
  static constexpr bool contains_value(underlying_t u) noexcept
  {
    return ((u == static_cast<underlying_t>(Vs)) || ...);
  }
};

template <typename E, E... Vs>
constexpr bool is_one_of(E v) noexcept
{
  return EnumSet<E, Vs...>::contains(v);
}

template <typename E, E... Vs>
constexpr bool is_one_of_value(std::underlying_type_t<E> u) noexcept
{
  return EnumSet<E, Vs...>::contains_value(u);
}
} // namespace details

/// System-dependent codes for Address Types.
///
/// These represent the os-specific integer codes assigned to each address
/// type, which are necessary for interfacing with socket functions.
enum class AddressFamily : std::uint8_t { Unknown = AF_UNSPEC, IPv4 = AF_INET, IPv6 = AF_INET6, Unix = AF_UNIX };

/// Address Base Class.
///
/// Inherited by all derived address classes to define
/// basic behavior for different socket types (UNIX, IPv4/6, etc.)
class SOCKIFY_EXPORT Address {
public:
  /// Aliases for types.
  using address_family_type = AddressFamily;
  using string_type         = std::string;
  using value_type          = sockaddr_storage;
  using reference           = value_type&;
  using const_reference     = const value_type&;
  using pointer             = value_type*;
  using const_pointer       = const value_type*;

private:
  value_type address{};
  address_family_type address_family{address_family_type::Unknown};

protected:
  /// Default Constructor.
  Address() noexcept = default;

  /// Address Constructor (from value_type).
  Address(const_reference address) noexcept;

  /// Address Copy Constructor.
  Address(const Address& other) noexcept = default;

  /// Address Copy Assignment.
  Address& operator=(const Address& other) noexcept;

  /// Address Move Constructor.
  Address(Address&& other) noexcept;

  /// Address Move Assignment.
  Address& operator=(Address&& other) noexcept;

public:
  /// Destructor
  virtual ~Address() = default;

protected:
  // comparison operators
  /// Explicit comparision between two address classes of the same type.
  /// Undefined behavior if the address classes are not of the same type.
  virtual int compare(const Address& other) const noexcept;

  // swap support
  /// Swaps the internal state of the address. (IMPLEMENTED IN DERIVED CLASSES)
  virtual void do_swap(Address& other) noexcept = 0;

public: // accessors
  /// Returns the address of the address (sockaddr_storage) data member.
  const_pointer value() const noexcept;

  /// Returns the value stored at address_family data member.
  address_family_type family() const noexcept;

  /// Returns the size of the socket. (IMPLEMENTED IN DERIVED CLASSES)
  virtual socklen_t size() const noexcept = 0;

  /// String representation of the socket. Used in the operator<< definition (IMPLEMENTED IN DERIVED CLASSES)
  virtual string_type to_string() const = 0; // must be suitable for std::wstring as well

  // observers

  /// Determines the truthiness of the address class.
  explicit operator bool() const noexcept;

  /// incomplete.
  bool has_value() const noexcept;

  /// Resets the address class into an unspecified state.
  void reset() noexcept;

  // comparison
  /// Equality comparision between address classes.
  friend bool operator==(const Address& lhs, const Address& rhs);
  // /// Inequality comparison between address classes.
  // friend bool operator!=(const Address& lhs, const Address& rhs);

  /// Strictly Ordered Less-than comparision between address classes.
  friend bool operator<(const Address& lhs, const Address& rhs);
  // /// Strictly Ordered Less-than-equal-to comparision between address classes.
  // friend bool operator<=(const Address& lhs, const Address& rhs);
  // /// Strictly Ordered Greater-than comparision between address classes.
  // friend bool operator>(const Address& lhs, const Address& rhs);
  // /// Strictly Ordered Greater-than-equal-to comparision between address classes.
  // friend bool operator>=(const Address& lhs, const Address& rhs);

  /// Implements general swap functionality between two Address classes. (uses do_swap)
  void swap(Address& other) noexcept;
  // /// Implements general swap functionality between two Address classes through lhs and rhs references. (uses
  // do_swap) friend void swap(Address& lhs, Address& rhs) noexcept;
};

SOCKIFY_EXPORT bool operator==(const Address& lhs, const Address& rhs);
SOCKIFY_EXPORT bool operator!=(const Address& lhs, const Address& rhs);
SOCKIFY_EXPORT bool operator<(const Address& lhs, const Address& rhs);
SOCKIFY_EXPORT bool operator<=(const Address& lhs, const Address& rhs);
SOCKIFY_EXPORT bool operator>(const Address& lhs, const Address& rhs);
SOCKIFY_EXPORT bool operator>=(const Address& lhs, const Address& rhs);

SOCKIFY_EXPORT void swap(Address& lhs, Address& rhs) noexcept;

} // namespace sockify

SOCKIFY_EXPORT std::ostream& operator<<(std::ostream& strm, const sockify::Address& addr);

#endif // ifndef
