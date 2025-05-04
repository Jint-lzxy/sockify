//===-- unix_address.hpp - Unix domain socket address interface -*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Provides the UnixDomainAddress class for representing and manipulating
/// Unix‑domain (AF_UNIX) socket addresses. Inherits from the generic Address
/// base.
///
//===----------------------------------------------------------------------===//

#ifndef UNIX_ADDRESS_HPP
#define UNIX_ADDRESS_HPP

#include "address.hpp"
#include "config.hpp"

#include <filesystem>
#include <sys/socket.h>

namespace sockify {

/// Represents a Unix domain socket address. Encapsulates a filesystem path for
/// Unix domain socket addressing, typically used for IPC on the same host.
/// Provides a specialized implementation of the Address interface.
class SOCKIFY_EXPORT UnixDomainAddress final : public Address {
public:
  /// Constructs an empty Unix domain address. has_value() returns `false`.
  /// \note Accessing path-related accessors on an empty address is undefined.
  UnixDomainAddress() noexcept = default;

  /// Constructs a Unix domain address from a raw socket address.
  /// \param address  The raw address data, typically of type `sockaddr_un`.
  /// \note If the address family is not recognized as Unix, the result is empty.
  UnixDomainAddress(const value_type& address) noexcept;

  /// Constructs a Unix domain address from a filesystem path.
  /// \param path  Filesystem path to the Unix domain socket file.
  /// \throws std::overflow_error if the path is too long to fit in the address structure.
  /// \post The address family is set to `AddressFamily::Unix`.
  explicit UnixDomainAddress(std::filesystem::path path);

  /// Copy constructor.
  /// \param other  Another `UnixDomainAddress` instance.
  /// Copies the internal raw storage, path and address family.
  UnixDomainAddress(const UnixDomainAddress& other) noexcept = default;

  /// Copy assignment operator.
  /// \param other  Another `UnixDomainAddress` instance.
  /// \return Reference to this object.
  UnixDomainAddress& operator=(const UnixDomainAddress& other) noexcept = default;

  /// Move constructor.
  /// \param other  The address to move from.
  /// \post `other` is in a valid but unspecified state.
  UnixDomainAddress(UnixDomainAddress&& other) noexcept = default;

  /// Move assignment operator.
  /// \param other  The address to move from.
  /// \return Reference to this object.
  /// \post `other` is in a valid but unspecified state.
  UnixDomainAddress& operator=(UnixDomainAddress&& other) noexcept = default;

  /// Trivial destructor. No dynamic resources are managed.
  ~UnixDomainAddress() override = default;

  /// Returns the socket path as a constant reference.
  /// \warning Calling this on an empty address results in undefined behavior.
  /// \return Constant reference to the socket file path.
  const std::filesystem::path& path() const&;

  /// Returns the socket path, moving the value out.
  /// \warning Calling this on an empty address results in undefined behavior.
  /// \return Rvalue path object representing the socket path.
  std::filesystem::path path() &&;

  /// Returns the size of the stored Unix socket address structure.
  /// \return Size in bytes, typically `sizeof(sockaddr_un)`.
  socklen_t size() const noexcept override;

  /// Converts the address to a string.
  /// \return The Unix domain socket path as a string.
  string_type to_string() const override;

protected:
  /// Exchanges internal state with another address.
  /// \param other  The address to swap with.
  /// \pre `other` must be a `UnixDomainAddress` instance.
  void do_swap(Address& other) noexcept override;

  /// Compares this address with another address.
  /// \param other  Address to compare with.
  /// \return A negative value if less, 0 if equal, or a positive value if greater.
  int compare(const Address& other) const noexcept override;

private:
  /// Cached Unix domain socket file path.
  std::filesystem::path socket_path;
};

} // namespace sockify

#endif // UNIX_ADDRESS_HPP
