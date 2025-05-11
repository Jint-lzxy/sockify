//===-- ipvx_address.hpp - IPv4/IPv6 Base Class -----------------------*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3‑Clause License.
// SPDX-License-Identifier: BSD‑3‑Clause
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines the inherited IPAddress class to support IPv4 and IPv6 addresses.
/// Inherits from the Address base class.
///
//===----------------------------------------------------------------------===//

#include "address.hpp"
#include "config.hpp"

#include <cstdint>
#include <netdb.h>
#include <string_view>

namespace sockify {
class SOCKIFY_EXPORT IPAddress final : public Address {
public:
  // Constructors
  IPAddress() noexcept = default;
  IPAddress(const_reference address) noexcept;
  explicit IPAddress(std::string_view ip_address);
  IPAddress(std::string_view ip_address, uint16_t port);

  // Copy Constructor and Copy Assignment Operator
  IPAddress(const IPAddress& other) noexcept            = default;
  IPAddress& operator=(const IPAddress& other) noexcept = default;

  // Move Constructor and Move Assignment Operator
  IPAddress(IPAddress&& other) noexcept            = default;
  IPAddress& operator=(IPAddress&& other) noexcept = default;

  // Destructor
  ~IPAddress() override = default; // trivial

  // Address/Port Accessors
  string_type ip() const;
  uint16_t port() const noexcept;

  // Override virtual functions
  socklen_t size() const noexcept override;
  string_type to_string() const override;

protected:
  void do_swap(Address& other) noexcept override;

private:
  static addrinfo* resolve(const std::string_view& ip_address, uint16_t port = -1, bool has_port = false);
  static value_type create_storage(const std::string_view& ip_address, uint16_t port = -1);

  uint16_t port_id{}; // Cached port number
};
} // namespace sockify