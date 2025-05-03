//===-- address.hpp - Base socket address class and helpers -----*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines the polymorphic Address base class for all socket addresses, plus
/// compile‑time enum utilities.
///
//===----------------------------------------------------------------------===//

#ifndef SOCKIFY_ADDRESS_HPP
#define SOCKIFY_ADDRESS_HPP

#include "config.hpp"

#include <ostream>
#include <string>
#include <sys/socket.h>
#include <type_traits>

namespace sockify {
namespace details {

/// A compile‑time set of enum values, with constexpr "contains" checks.
/// \tparam EnumType       The enum type to work on (must satisfy std::is_enum).
/// \tparam AllowedValues  A parameter pack of enum values permitted in the set.
template <typename EnumType, EnumType... AllowedValues>
struct SOCKIFY_HIDDEN EnumSet {
  static_assert(std::is_enum_v<EnumType>, "EnumSet may only be used with enum types");

public:
  using enum_type       = EnumType;
  using underlying_type = std::underlying_type_t<EnumType>;

public:
  /// Checks if \p value is one of the AllowedValues.
  /// \param value  The enum value to test.
  /// \return true if \p value equals any AllowedValues.
  static constexpr bool contains(EnumType value) noexcept
  {
    return ((value == AllowedValues) || ...);
  }

  /// Checks if the raw integer \p raw matches any AllowedValues.
  /// \param raw  The underlying integer to test.
  /// \return true if \p raw equals any AllowedValues' underlying value.
  static constexpr bool contains_value(underlying_type raw) noexcept
  {
    return ((raw == static_cast<underlying_type>(AllowedValues)) || ...);
  }
};

/// Checks if an enum value is one of a fixed list.
/// \tparam Enum           The enum type.
/// \tparam AllowedValues  The pack of allowed enum values.
/// \param value           The enum value to check.
/// \return true if \p value is in AllowedValues.
template <typename Enum, Enum... AllowedValues>
SOCKIFY_HIDDEN constexpr bool is_one_of(Enum value) noexcept
{
  return EnumSet<Enum, AllowedValues...>::contains(value);
}

/// Checks if an integer corresponds to one of a fixed list of enum values.
/// \tparam Enum           The enum type.
/// \tparam AllowedValues  The pack of allowed enum values.
/// \param raw             The underlying integer to check.
/// \return true if \p raw matches any AllowedValues when cast to Enum.
template <typename Enum, Enum... AllowedValues>
SOCKIFY_HIDDEN constexpr bool is_one_of_value(std::underlying_type_t<Enum> raw) noexcept
{
  return EnumSet<Enum, AllowedValues...>::contains_value(raw);
}

} // namespace details

/// System-dependent codes for address families (AF_*).
/// \see [socket(2)](https://man.freebsd.org/cgi/man.cgi?socket(2))
enum class AddressFamily {
  Unknown = AF_UNSPEC, ///< Unspecified / Unknown
  Unix    = AF_UNIX,   ///< UNIX domain
  IPv4    = AF_INET,   ///< IPv4 Internet
  IPv6    = AF_INET6,  ///< IPv6 Internet
};

/// The `Address` class encapsulates a protocol‑agnostic network address. It serves as a common base for
/// IPv4, IPv6 and Unix domain socket addresses, storing all data in `sockaddr_storage`. Designed for
/// extension by concrete address families; not for direct instantiation.
/// \warning
/// Calling any accessors on an Address (or any of its derived classes) object while it is in its empty
/// state (e.g., after reset() with no arguments) results in undefined behavior.
class SOCKIFY_EXPORT Address {
public:
  using address_family_type = AddressFamily;    ///< Address family enum
  using string_type         = std::string;      ///< String type for textual representation
  using value_type          = sockaddr_storage; ///< Raw socket address storage
  using reference           = value_type&;
  using const_reference     = const value_type&;
  using pointer             = value_type*;
  using const_pointer       = const value_type*;

public:
  /// Destroys the Address object and any derived state.
  virtual ~Address() = default;

protected:
  /// Default-construct an empty `Address` with no valid address. has_value() returns false.
  Address() noexcept = default;

  /// Construct from raw sockaddr_storage. This constructor also caches its family,
  /// and leaves it empty if unrecognized.
  /// \param addr  The raw sockaddr_storage to copy.
  Address(const_reference addr) noexcept;

  /// Copy-construct from another Address.
  /// \param other  The Address to copy.
  Address(const Address& other) noexcept = default;

  /// Copy-assign from another Address.
  /// \param other  The Address to copy.
  /// \return *this
  Address& operator=(const Address& other) noexcept;

  /// Move-construct from another Address.
  /// \param other  The Address to move.
  Address(Address&& other) noexcept;

  /// Move-assign from another Address.
  /// \param other  The Address to move.
  /// \return *this
  Address& operator=(Address&& other) noexcept;

protected:
  /// Compare this Address to \p other for ordering.
  /// \param other  The Address to compare.
  /// \return Negative if *this < other, zero if equal, positive if *this > other.
  virtual int compare(const Address& other) const noexcept;

  /// Swap internal state with \p other. Effectively exchange all stored data with
  /// another Address **of the same type**.
  /// \param other  The Address to swap with.
  virtual void do_swap(Address& other) noexcept = 0;

public:
  /// Get pointer to internal sockaddr_storage.
  /// \return const pointer to stored sockaddr_storage.
  const_pointer value() const noexcept;

  /// Get the address family.
  /// \return The AddressFamily of this Address.
  address_family_type family() const noexcept;

  /// Get size of this address for socket calls.
  /// \return The number of bytes occupied by the stored socket address.
  virtual socklen_t size() const noexcept = 0;

  /// Convert to human-readable form.
  /// \return A string representation (e.g. "127.0.0.1:80").
  virtual string_type to_string() const = 0;

  /// Boolean check for validity.
  /// \return true if family() is not Unknown.
  explicit operator bool() const noexcept;

  /// Check if Address has a non-Unknown family.
  /// \return true if family() is not Unknown.
  bool has_value() const noexcept;

  /// Reset Address to unspecified state.
  void reset() noexcept;

  /// Swap with another Address.
  /// \param other  The Address to swap with.
  void swap(Address& other) noexcept;

  friend bool operator==(const Address& lhs, const Address& rhs);
  friend bool operator<(const Address& lhs, const Address& rhs);

private:
  value_type address{};
  address_family_type address_family{address_family_type::Unknown};
};

/// Equality comparison for Addresses.
/// \param lhs  Left Address.
/// \param rhs  Right Address.
/// \return true if both have same family and storage bytes.
SOCKIFY_EXPORT bool operator==(const Address& lhs, const Address& rhs);

/// Inequality comparison for Addresses.
/// \param lhs  Left Address.
/// \param rhs  Right Address.
/// \return !(lhs == rhs).
SOCKIFY_EXPORT bool operator!=(const Address& lhs, const Address& rhs);

/// Less-than comparison for Addresses.
/// \param lhs  Left Address.
/// \param rhs  Right Address.
/// \return true if lhs.compare(rhs) < 0.
SOCKIFY_EXPORT bool operator<(const Address& lhs, const Address& rhs);

/// Less-than-or-equal comparison for Addresses.
/// \param lhs  Left Address.
/// \param rhs  Right Address.
/// \return true if lhs < rhs or lhs == rhs.
SOCKIFY_EXPORT bool operator<=(const Address& lhs, const Address& rhs);

/// Greater-than comparison for Addresses.
/// \param lhs  Left Address.
/// \param rhs  Right Address.
/// \return true if lhs.compare(rhs) > 0.
SOCKIFY_EXPORT bool operator>(const Address& lhs, const Address& rhs);

/// Greater-than-or-equal comparison for Addresses.
/// \param lhs  Left Address.
/// \param rhs  Right Address.
/// \return true if lhs > rhs or lhs == rhs.
SOCKIFY_EXPORT bool operator>=(const Address& lhs, const Address& rhs);

/// Enable unqualified swap via ADL.
/// \param lhs  First Address.
/// \param rhs  Second Address.
/// \note Calls lhs.swap(rhs).
SOCKIFY_EXPORT void swap(Address& lhs, Address& rhs) noexcept;

/// Send an Address to an ostream.
/// \param strm  The output stream.
/// \param addr  The Address to print.
/// \return \p strm after writing addr.to_string().
SOCKIFY_EXPORT std::ostream& operator<<(std::ostream& strm, const Address& addr);

} // namespace sockify

#endif // SOCKIFY_ADDRESS_HPP
