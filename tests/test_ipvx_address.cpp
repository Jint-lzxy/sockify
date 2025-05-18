#include "ipvx_address.hpp"

#include <catch2/catch_test_macros.hpp>
#include <netinet/in.h>
#include <stdexcept>
#include <string>

using namespace sockify;

TEST_CASE("IPv4 parsing and properties", "[ipaddress][ipv4]")
{
  SECTION("With explicit port in string")
  {
    IPAddress addr{"192.168.1.10:12345"};
    REQUIRE(addr.ip() == "192.168.1.10");
    REQUIRE(addr.port() == 12345);
    REQUIRE(addr.size() == sizeof(struct sockaddr_in));
    REQUIRE(addr.to_string() == "192.168.1.10:12345");
  }

  SECTION("Without port in string")
  {
    IPAddress addr{"10.0.0.5"};
    REQUIRE(addr.ip() == "10.0.0.5");
    REQUIRE(addr.port() == 0);
    REQUIRE(addr.to_string() == "10.0.0.5:0");
  }

  SECTION("Invalid host name")
  {
    REQUIRE_THROWS_AS(IPAddress{"not_a_real_host:80"}, std::invalid_argument);
  }

  SECTION("Explicit port constructor")
  {
    IPAddress addr{"8.8.8.8", 53};
    REQUIRE(addr.ip() == "8.8.8.8");
    REQUIRE(addr.port() == 53);
    REQUIRE(addr.to_string() == "8.8.8.8:53");
  }
}

TEST_CASE("IPv6 parsing and properties", "[ipaddress][ipv6]")
{
  SECTION("Bracketed IPv6 with port")
  {
    IPAddress addr{"[2001:db8::1]:443"};
    REQUIRE(addr.ip() == "[2001:db8::1]");
    REQUIRE(addr.port() == 443);
    REQUIRE(addr.size() == sizeof(struct sockaddr_in6));
    REQUIRE(addr.to_string() == "[2001:db8::1]:443");
  }

  SECTION("Bracketed IPv6 without port")
  {
    IPAddress addr{"[::1]"};
    REQUIRE(addr.ip() == "[::1]");
    REQUIRE(addr.port() == 0);
    REQUIRE(addr.to_string() == "[::1]:0");
  }

  SECTION("Unbracketed IPv6 literal is rejected")
  {
    // "::1" will be split incorrectly by parse_host_port and then fail resolution
    REQUIRE_THROWS_AS(IPAddress{"::1"}, std::invalid_argument);
  }

  SECTION("Malformed bracketed IPv6 inputs")
  {
    REQUIRE_THROWS_AS(IPAddress{"[::1:80"}, std::invalid_argument); // missing ']'
    REQUIRE_THROWS_AS(IPAddress{"[::1]80"}, std::invalid_argument); // missing ':'
  }
}

TEST_CASE("Copy, assignment and swap", "[ipaddress][copy][swap]")
{
  IPAddress addr1{"127.0.0.1:1111"};
  IPAddress addr2{"[::1]:2222"};

  SECTION("Copy constructor")
  {
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization): This copy is intentional.
    IPAddress copy{addr1};
    REQUIRE(copy.ip() == "127.0.0.1");
    REQUIRE(copy.port() == 1111);
  }

  SECTION("Copy assignment")
  {
    IPAddress copy{"8.8.8.8:53"};
    copy = addr2;
    REQUIRE(copy.ip() == "[::1]");
    REQUIRE(copy.port() == 2222);
  }

  SECTION("Swap exchanges contents")
  {
    addr1.swap(addr2);
    REQUIRE(addr1.ip() == "[::1]");
    REQUIRE(addr1.port() == 2222);
    REQUIRE(addr2.ip() == "127.0.0.1");
    REQUIRE(addr2.port() == 1111);
  }
}

TEST_CASE("size() matches family", "[ipaddress][size]")
{
  IPAddress v4{"127.0.0.1:1"};
  IPAddress v6{"[2001:db8::1]:1"};
  REQUIRE(v4.size() == sizeof(struct sockaddr_in));
  REQUIRE(v6.size() == sizeof(struct sockaddr_in6));
}

TEST_CASE("to_string() always includes port", "[ipaddress][to_string]")
{
  IPAddress v4{"10.10.10.10"};
  REQUIRE(v4.to_string() == "10.10.10.10:0");

  IPAddress v6{"[fe80::1]"};
  REQUIRE(v6.to_string() == "[fe80::1]:0");
}

TEST_CASE("Edge cases for host/port parsing", "[ipaddress][parse]")
{
  // Leading/trailing whitespace should be considered part of host and fail
  REQUIRE_THROWS_AS(IPAddress{" 127.0.0.1:80"}, std::invalid_argument);
  REQUIRE_THROWS_AS(IPAddress{"127.0.0.1:80 "}, std::invalid_argument);

  // Extra colons in unbracketed IPv4
  REQUIRE_THROWS_AS(IPAddress{"1.2.3.4:5:6"}, std::invalid_argument);

  // Colon-only inputs
  REQUIRE_THROWS_AS(IPAddress{"::"}, std::invalid_argument);

  // Non‐numeric port
  REQUIRE_NOTHROW(IPAddress{"127.0.0.1:http"});
  REQUIRE_NOTHROW(IPAddress{"[::1]:https"});

  // Port out of valid range
  REQUIRE_THROWS_AS(IPAddress{"127.0.0.1:70000"}, std::invalid_argument);
  REQUIRE_THROWS_AS(IPAddress{"[::1]:65536"}, std::invalid_argument);

  // Invalid IP literals
  REQUIRE_THROWS_AS(IPAddress{"256.256.256.256:80"}, std::invalid_argument);
  REQUIRE_THROWS_AS(IPAddress{"1234::abcd::1:80"}, std::invalid_argument);

  // Illegal host chars
  REQUIRE_THROWS_AS(IPAddress{"täst .local:80"}, std::invalid_argument);
  REQUIRE_THROWS_AS(IPAddress{"exa mple.com:80"}, std::invalid_argument);

  // IPv4‐mapped IPv6 (should work)
  {
    IPAddress addr{"[::ffff:192.168.0.1]:8080"};
    REQUIRE(addr.ip() == "[::ffff:192.168.0.1]");
    REQUIRE(addr.port() == 8080);
  }

  // Empty‐host + port -> binds wildcard
  {
    IPAddress addr{":1234"};
    REQUIRE(addr.port() == 1234);
    REQUIRE_FALSE(addr.ip().empty());
  }
}
