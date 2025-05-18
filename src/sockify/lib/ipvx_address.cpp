//===-- ipvx_address.cpp ----------------------------------------*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//

#include "ipvx_address.hpp"

#include "address.hpp"
#include "config.hpp"

#include <arpa/inet.h>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <utility>

// Anonymous namespace for helpers, should not be exported
namespace {

struct SOCKIFY_HIDDEN AddrInfoDeleter {
  void operator()(addrinfo* info) const
  {
    if (info != nullptr)
      freeaddrinfo(info);
  }
};

using AddrInfo = std::unique_ptr<addrinfo, AddrInfoDeleter>;

// Parse "[v6]:port", "host:port" or just "host"
// -> { host, optional<port> }
SOCKIFY_HIDDEN std::pair<std::string_view, std::optional<std::string_view>> parse_host_port(std::string_view addr)
{
  if (addr.empty())
    return {addr, std::nullopt};

  if (addr.front() == '[') {
    // Expect either "[v6]" or "[v6]:port"
    auto end = addr.find(']');
    if (end == std::string_view::npos)
      throw std::invalid_argument("Invalid IPv6 literal: missing ']'");

    // Strip brackets
    auto host = addr.substr(1, end - 1);

    // Exactly "[v6]"
    if (end + 1 == addr.size())
      return {host, std::nullopt};
    // "[v6]:port"
    if (addr[end + 1] == ':')
      return {host, addr.substr(end + 2)};

    // junk after ']', e.g. "[::1]80"
    throw std::invalid_argument("invalid IPv6 literal: unexpected characters after ']'");
  }

  // look for last ':', but only if there's no ']' afterwards
  auto pos = addr.rfind(':');
  if (pos != std::string_view::npos && addr.find(']', pos) == std::string_view::npos)
    return {addr.substr(0, pos), addr.substr(pos + 1)};

  // no port
  return {addr, {}};
}

// Wrap getaddrinfo; throws system_error on failure.
SOCKIFY_HIDDEN AddrInfo resolve_addr(const std::string& host, const std::string& service, int family = AF_UNSPEC)
{
  addrinfo* ai{};
  addrinfo hint{};
  hint.ai_family = family;
  hint.ai_flags  = AI_V4MAPPED // return IPv4-mapped IPv6 addresses
                | (host.empty() ? AI_PASSIVE : AI_ADDRCONFIG);

  auto errc = ::getaddrinfo(host.empty() ? nullptr // listen on 0.0.0.0 / ::
                                         : host.c_str(),
                            service.c_str(),
                            &hint,
                            &ai);
  if (errc != 0)
    throw std::invalid_argument(gai_strerror(errc));
  return AddrInfo{ai};
}

// Copy the first usable sockaddr out of the linked list into a sockaddr_storage
SOCKIFY_HIDDEN sockaddr_storage first_sockaddr(AddrInfo ai)
{
  for (const auto* start = ai.get(); start != nullptr; start = start->ai_next) {
    if (start->ai_addr == nullptr)
      continue;

    // NOLINTNEXTLINE(bugprone-switch-missing-default-case): Only AF_INET and AF_INET6 are expected.
    switch (start->ai_family) {
    case AF_INET: {
      const auto* src = reinterpret_cast<const sockaddr_in*>(start->ai_addr);

      sockaddr_storage ss{};
      std::memset(&ss, 0, sizeof(ss));
      reinterpret_cast<sockaddr_in&>(ss) = *src;
      return ss;
    }
    case AF_INET6: {
      const auto* src = reinterpret_cast<const sockaddr_in6*>(start->ai_addr);

      sockaddr_storage ss{};
      std::memset(&ss, 0, sizeof(ss));
      reinterpret_cast<sockaddr_in6&>(ss) = *src;
      return ss;
    }
    }
  }

  throw std::invalid_argument("No usable address found");
}

} // namespace

namespace sockify {

IPAddress::IPAddress(const_reference address) noexcept : Address{address}
{
  // no need to set sockaddr_in/in6's family since the address.ss_family already holds the family.
  switch (address.ss_family) {
  case AF_INET:
    port_id = ntohs(reinterpret_cast<const sockaddr_in&>(address).sin_port);
    break;
  case AF_INET6:
    port_id = ntohs(reinterpret_cast<const sockaddr_in6&>(address).sin6_port);
    break;
  default:
    reset();
  }
}

IPAddress::IPAddress(std::string_view ip_address)
    : IPAddress([&ip_address] {
        auto [host, port] = parse_host_port(ip_address);
        auto info         = resolve_addr(std::string{host}, std::string{port.value_or("0")});
        return first_sockaddr(std::move(info));
      }())
{}

IPAddress::IPAddress(std::string_view ip_address, uint16_t port)
    : IPAddress([&ip_address, &port] {
        auto info = resolve_addr(std::string{ip_address}, std::to_string(port));
        return first_sockaddr(std::move(info));
      }())
{}

Address::string_type IPAddress::ip() const
{
  if (value()->ss_family == AF_INET) {
    char buf[INET_ADDRSTRLEN];
    const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(value());
    return inet_ntop(AF_INET, &ipv4->sin_addr, buf, std::size(buf));
  }

  // known to be IPv6, if not AF_INET
  char buf[INET6_ADDRSTRLEN];
  const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(value());
  return (std::stringstream{} << '[' << inet_ntop(AF_INET6, &ipv6->sin6_addr, buf, std::size(buf)) << ']').str();
}

uint16_t IPAddress::port() const noexcept
{
  return port_id;
}

socklen_t IPAddress::size() const noexcept
{
  if (value()->ss_family == AF_INET)
    return sizeof(sockaddr_in);
  return sizeof(sockaddr_in6);
}

Address::string_type IPAddress::to_string() const
{
  return (std::stringstream{} << ip() << ':' << port()).str();
}

void IPAddress::do_swap(Address& other) noexcept
{
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast): other is known to be of the same type.
  auto* rhs = static_cast<IPAddress*>(&other);
  std::swap(port_id, rhs->port_id);
}

} // namespace sockify
