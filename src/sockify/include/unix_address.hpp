//===-- unix_address.hpp - Unix Address Interfaces -------------------*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Provides the support for unix-based addresses, through the UnixDomainAddress
/// class which inherits from the Address base class within address.hpp.
///
//===----------------------------------------------------------------------===//

#ifndef UNIX_ADDRESS_HPP
#define UNIX_ADDRESS_HPP

#include "address.hpp"
#include "config.hpp"

#include <filesystem>

namespace sockify {

class SOCKIFY_EXPORT UnixDomainAddress final : public Address {
public:
  // Constructors
  UnixDomainAddress() noexcept = default;
  UnixDomainAddress(const value_type& address) noexcept;
  explicit UnixDomainAddress(std::filesystem::path path);

  // Copy Constructor and Copy Assignment Operator
  UnixDomainAddress(const UnixDomainAddress& other) noexcept            = default;
  UnixDomainAddress& operator=(const UnixDomainAddress& other) noexcept = default;

  // Move Constructor and Move Assignment Operator
  UnixDomainAddress(UnixDomainAddress&& other) noexcept            = default;
  UnixDomainAddress& operator=(UnixDomainAddress&& other) noexcept = default;

  // Destructor
  ~UnixDomainAddress() override = default;

  // Path Accessor
  const std::filesystem::path& path() const&;
  std::filesystem::path path() &&;

  // Override virtual functions
  socklen_t size() const noexcept override;
  string_type to_string() const override;

protected:
  void do_swap(Address& other) noexcept override;
  int compare(const Address& other) const noexcept override;

private:
  std::filesystem::path socket_path; // Cached socket file path
};

}; // namespace sockify

#endif // ifndef
