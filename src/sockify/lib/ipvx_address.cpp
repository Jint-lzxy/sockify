//===-- ipvx_address.cpp -----------------------------------------------*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//

#include "ipvx_address.hpp"

#include "address.hpp"

#include <arpa/inet.h>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <netdb.h>
#include <netinet/in.h>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <utility>

namespace sockify {
// private
addrinfo* IPAddress::resolve(const std::string_view& ip_address, uint16_t port, bool has_port)
{
  addrinfo hint{};
  addrinfo* ret{};
  std::memset(&hint, 0x0, sizeof(hint));
  hint.ai_family = AF_UNSPEC;
  hint.ai_flags  = AI_DEFAULT;

  // set the ip_address and the port corresponding to the input data.
  // need to parse the string_view to strip for the port and feed into the function normally
  // or use a boolean eval as an argument? Quicker, less overhead.
  // grab last ":" character and read onwards for port number
  // [<ipv6 address>]:port
  // <ipv4 address>:port
  if(has_port) {   // refactor into separate function
      std::string host_part{};
      std::string port_str{};

      if(ip_address[0] == '[') {   // IPv6
          const size_t closing_bracket = ip_address.rfind(']');
          if(closing_bracket != std::string_view::npos && ip_address[closing_bracket + 1] == ':') {
              host_part = std::string(ip_address.substr(1, closing_bracket - 1));
              port_str = std::string(ip_address.substr(closing_bracket + 2));
          }
      }
      else {  // IPv4
          const size_t port_delim = ip_address.rfind(':');
          if(port_delim != std::string_view::npos) {
              host_part = std::string(ip_address.substr(0, port_delim));  // inefficient slicing?
              port_str = std::string(ip_address.substr(port_delim + 1));
          }
      }
      
      if(getaddrinfo(host_part.c_str(), port_str.c_str(), &hint, &ret) != 0)
          throw std::invalid_argument("Invalid IP address or hostname provided");
      return ret;
  }

  if (getaddrinfo(std::string(ip_address).c_str(), std::to_string(port).c_str(), &hint, &ret) != 0)
    throw std::invalid_argument("Invalid IP address or hostname provided");
  return ret;
}

Address::value_type IPAddress::create_storage(const std::string_view& ip_address, uint16_t port)
{
  sockaddr_storage data{};
  addrinfo* resolved_ip = resolve(ip_address, port);

  if (resolved_ip->ai_family == AF_INET)   // redundant?
    data.ss_family = AF_INET;
  if (resolved_ip->ai_family == AF_INET6)
    data.ss_family = AF_INET6;

  // safe when resolved_ip returned from getaddrinfo
  memcpy(&data, resolved_ip->ai_addr, resolved_ip->ai_addrlen); 
  freeaddrinfo(resolved_ip);
  return data;
}

// public

IPAddress::IPAddress(const_reference address) noexcept : Address{address}
{
  // no need to set sockaddr_in/in6's family since the address.ss_family already holds the family.
  if (address.ss_family == AF_INET) {
    const auto& ipv4 = reinterpret_cast<const sockaddr_in&>(address);
    port_id          = ntohs(ipv4.sin_port);
    return;
  }
  if (address.ss_family == AF_INET6) {
    const auto& ipv6 = reinterpret_cast<const sockaddr_in6&>(address);
    port_id          = ntohs(ipv6.sin6_port);
    return;
  }
  reset(); // if no valid family, reset to empty.
}

IPAddress::IPAddress(std::string_view ip_address) : Address{create_storage(ip_address)}
{}

IPAddress::IPAddress(std::string_view ip_address, uint16_t port)
    : Address{create_storage(ip_address, port)}, port_id{port}
{}

Address::string_type IPAddress::ip() const
{
  if (value()->ss_family == AF_INET) {
    const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(value());
    char buf[INET_ADDRSTRLEN];
    std::stringstream sstr;
    sstr << inet_ntop(AF_INET, &ipv4->sin_addr, buf, std::size(buf));
    return sstr.str();
  }
  // known to be IPv6, if not AF_INET
  const auto* ipv6 = reinterpret_cast<const sockaddr_in6*>(value());
  char buf[INET6_ADDRSTRLEN];
  std::stringstream sstr6;
  sstr6 << '[' << inet_ntop(AF_INET6, &ipv6->sin6_addr, buf, std::size(buf)) << ']';
  return sstr6.str();
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
  std::stringstream sstr;
  sstr << ip() << ':' << port();
  return sstr.str();
}

void IPAddress::do_swap(Address& other) noexcept
{
  auto* ip_swap = reinterpret_cast<IPAddress*>(&other);
  std::swap(port_id, ip_swap->port_id);
}

} // namespace sockify