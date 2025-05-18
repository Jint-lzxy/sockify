//===-- ip_address.hpp - IPv4/IPv6 socket address interface -----*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Provides the final IPAddress class for handling IPv4 and IPv6 network
/// addresses, extending the Address base class.
///
//===----------------------------------------------------------------------===//

#ifndef SOCKIFY_IPVX_ADDRESS_HPP
#define SOCKIFY_IPVX_ADDRESS_HPP

#include "address.hpp"
#include "config.hpp"

#include <cstdint>
#include <netdb.h>
#include <string_view>

namespace sockify {

/// Represents an IPv4 or IPv6 network address with an associated port.
///
/// This class specializes the Address interface to parse, format and manage
/// both IPv4 and IPv6 addresses, including their ports. It provides constructors
/// from raw socket data, textual IP (with or without port), and hostnames
/// (with DNS resolution).
///
/// \note Calling any accessor on an empty (default-constructed or reset)
///       IPAddress results in undefined behavior.
class IPAddress final : public Address {
public:
  /// Constructs an empty IPAddress object with an unspecified family.
  /// \post The object is in a valid but unspecified state.
  IPAddress() noexcept = default;

  /// Constructs from raw socket address data. Copies raw address data and extracts
  /// port and family based on \p address.ss_family.
  /// \param address  Reference to a raw socket address (sockaddr_storage).
  /// \post  On AF_INET/AF_INET6, \p port_id is set from network-order port.
  /// \post  On unknown family, calls \p reset() to clear internal state.
  IPAddress(const_reference address) noexcept;

  /// Constructs an IPAddress by parsing a host[:port] string.
  /// \param ip_address  A textual IP (e.g. "127.0.0.1", "::1"), hostname
  ///                    (e.g. "localhost"), or empty.
  /// \post  Performs DNS resolution (via getaddrinfo) with service "0" if no
  ///        port is given.
  /// \note  If \p ip_address is empty, the underlying getaddrinfo hint
  ///        uses \p AI_PASSIVE, yielding a wildcard address (0.0.0.0 or ::).
  /// \throw std::invalid_argument if parsing or resolution fails.
  explicit IPAddress(std::string_view ip_address);

  /// Constructs from a textual IP (or hostname) and explicit port. Parses or resolves
  /// \p ip_address, associates the given port, and initializes the internal sockaddr_storage.
  /// \param ip_address  A textual IP (e.g. "127.0.0.1"), hostname (e.g. "localhost"),
  ///                    or empty.
  /// \param port        TCP/UDP port number (0-65535).
  /// \pre   `port <= 65535`.
  /// \post  Performs DNS resolution (via getaddrinfo) for \p ip_address and \p port.
  /// \note  If \p ip_address is empty, the underlying getaddrinfo hint uses
  ///        \p AI_PASSIVE, yielding a wildcard address (0.0.0.0 or ::).
  /// \throw std::invalid_argument if \p ip_address is invalid or resolution fails.
  IPAddress(std::string_view ip_address, uint16_t port);

  /// Copy-constructs an IPAddress from another.
  /// Copies the internal socket data and port cache.
  /// \param other  The IPAddress instance to copy.
  IPAddress(const IPAddress& other) noexcept = default;

  /// Copy-assigns from another IPAddress.
  /// Copies the internal socket data and port cache.
  /// \param other  The IPAddress instance to copy.
  /// \return Reference to *this.
  IPAddress& operator=(const IPAddress& other) noexcept = default;

  /// Move-constructs an IPAddress from another.
  /// Transfers internal state; \p other is left valid but unspecified.
  /// \param other  The IPAddress instance to move.
  /// \post  \p other is valid but in an unspecified state.
  IPAddress(IPAddress&& other) noexcept = default;

  /// Move-assigns from another IPAddress.
  /// Transfers internal state; other is left valid but unspecified.
  /// \param  other  The IPAddress instance to move.
  /// \return Reference to *this.
  /// \post   \p other is valid but in an unspecified state.
  IPAddress& operator=(IPAddress&& other) noexcept = default;

  /// Destroys the IPAddress object and releases any resources.
  /// Trivial Destructor.
  ~IPAddress() override = default;

  /// Returns the textual IP address.
  /// \return A string such as "127.0.0.1" or "::1".
  string_type ip() const;

  /// Returns the cached port number in host byte order.
  /// \return Port number (0–65535).
  uint16_t port() const noexcept;

  /// Returns the size of the stored socket address structure.
  /// \return `sizeof(sockaddr_in)` for IPv4 or `sizeof(sockaddr_in6)` for IPv6.
  socklen_t size() const noexcept override;

  /// Converts this IPAddress to its string representation.
  /// \return "ip:port" for IPv4 or "[ip]:port" for IPv6.
  string_type to_string() const override;

protected:
  /// Swaps internal state with another Address.
  /// Exchanges the socket data, address family and port cache.
  /// \param other  The Address instance to swap with; must be IPAddress.
  /// \pre   dynamic_cast<IPAddress*>(&other) != nullptr.
  void do_swap(Address& other) noexcept override;

private:
  /// Cached port number in host byte order.
  /// \invariant `port_id <= 65535`.
  uint16_t port_id{};
};

} // namespace sockify

#endif
