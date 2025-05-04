//===-- test_address.cpp ----------------------------------------*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//

#include "address.hpp"

#include <arpa/inet.h>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <type_traits>
#include <utility>

using namespace sockify;

namespace {

// helper to convert enum class to underlying value
template <typename Enum>
constexpr auto to_underlying(Enum val)
{
  return static_cast<std::underlying_type_t<Enum>>(val);
}

// helpers to build sockaddr_storage

template <typename RawAddrType>
sockaddr_storage make_storage(const RawAddrType& raw)
{
  sockaddr_storage addr{};
  static_assert(sizeof(addr) >= sizeof(raw), "storage too small");

  std::memcpy(&addr, &raw, sizeof(raw));
  return addr;
}

sockaddr_storage make_v4(const char* ip, std::uint16_t port = 0)
{
  sockaddr_in v4{};
  v4.sin_family = AF_INET;
  v4.sin_port   = htons(port);
  inet_pton(AF_INET, ip, &v4.sin_addr);
  return make_storage(v4);
}

sockaddr_storage make_v6(const char* ip, std::uint16_t port = 0)
{
  sockaddr_in6 v6{};
  v6.sin6_family = AF_INET6;
  v6.sin6_port   = htons(port);
  inet_pton(AF_INET6, ip, &v6.sin6_addr);
  return make_storage(v6);
}

// concrete class to test Address
class TestAddress : public Address {
public:
  TestAddress() = default;

  explicit TestAddress(const sockaddr_storage& addr, std::string text, std::size_t size_bytes) noexcept
      : Address(addr), text(std::move(text)), pseudosize(size_bytes)
  {}

  socklen_t size() const noexcept override
  {
    return static_cast<socklen_t>(pseudosize);
  }

  string_type to_string() const override
  {
    return text;
  }

  int compare(const Address& other) const noexcept override
  {
    return Address::compare(other);
  }

protected:
  void do_swap(Address& other) noexcept override
  {
    auto& rhs = dynamic_cast<TestAddress&>(other);
    std::swap(text, rhs.text);
    std::swap(pseudosize, rhs.pseudosize);
  }

private:
  std::string text;
  std::size_t pseudosize{};
};

} // namespace

TEST_CASE("Empty‑state observers", "[address][empty]")
{
  TestAddress addr;
  CHECK_FALSE(addr);
  CHECK_FALSE(addr.has_value());

  addr.reset();
  CHECK_FALSE(addr);
  CHECK_FALSE(addr.has_value());
}

TEST_CASE("Value‑state construction and accessors", "[address][construct]")
{
  auto v4 = make_v4("1.2.3.4", 1234);
  TestAddress addr{v4, "1.2.3.4", sizeof(sockaddr_in)};

  CHECK(addr);
  CHECK(addr.has_value());
  CHECK(addr.family() == AddressFamily::IPv4);
  CHECK(addr.value()->ss_family == AF_INET);
  CHECK(addr.size() == sizeof(sockaddr_in));
  CHECK(addr.to_string() == "1.2.3.4");
}

TEST_CASE("Unrecognized family yields empty state", "[address][unknown]")
{
  sockaddr_storage bad{};
  bad.ss_family = -1;
  TestAddress addr{bad, "bogus", 4};

  CHECK_FALSE(addr);
  CHECK_FALSE(addr.has_value());
}

TEST_CASE("Copy and move semantics", "[address][copymove]")
{
  auto v6 = make_v6("::1", 4321);
  const TestAddress orig{v6, "::1", sizeof(sockaddr_in6)};

  SECTION("Copy constructor")
  {
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization): This copy is intentional.
    const TestAddress copy{orig};
    CHECK(copy.has_value());
    CHECK(copy.family() == AddressFamily::IPv6);
    CHECK(copy.size() == orig.size());
    CHECK(copy.to_string() == "::1");
  }
  SECTION("Copy assignment")
  {
    TestAddress copy;
    copy = orig;
    CHECK(copy.has_value());
    CHECK(copy.family() == AddressFamily::IPv6);
    CHECK(copy.to_string() == "::1");
  }
  SECTION("Copy self-assignment")
  {
    TestAddress copy = orig;
    copy.operator=(copy);
    CHECK(copy.has_value());
    CHECK(copy.family() == AddressFamily::IPv6);
    CHECK(copy.to_string() == "::1");
  }
  SECTION("Move constructor")
  {
    TestAddress temp = orig;
    const TestAddress moved{std::move(temp)};
    CHECK_FALSE(temp.has_value());
    CHECK(moved.has_value());
    CHECK(moved.to_string() == "::1");
  }
  SECTION("Move self-assignment")
  {
    TestAddress moved = orig;
    moved.operator=(std::move(moved));
    CHECK(moved.has_value());
    CHECK(moved.family() == AddressFamily::IPv6);
  }
  SECTION("Move assignment")
  {
    TestAddress temp = orig;
    TestAddress moved;
    moved = std::move(temp);
    CHECK_FALSE(temp.has_value());
    CHECK(moved.has_value());
    CHECK(moved.to_string() == "::1");
  }
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity): Unit test case with inherent complexity.
TEST_CASE("Equality and ordering", "[address][compare]")
{
  auto v4 = make_v4("10.0.0.1", 1000);
  auto v6 = make_v6("::1", 1000);

  TestAddress addr1{v4, "v4-1", sizeof(sockaddr_in)};
  TestAddress addr2{v6, "v6", sizeof(sockaddr_in6)};

  SECTION("== and !=")
  {
    TestAddress copy{v4, "v4-1", sizeof(sockaddr_in)};
    CHECK(addr1 == copy);
    CHECK_FALSE(addr1 != copy);
    CHECK(addr1 != addr2);
  }

  SECTION("<, <=, >, >=")
  {
    CHECK(addr1 < addr2);
    CHECK(addr1 <= addr2);
    CHECK(addr2 > addr1);
    CHECK(addr2 >= addr1);
  }

  SECTION("Different sizes")
  {
    TestAddress small{v4, "x", 2};
    TestAddress large{v4, "y", 4};
    CHECK(small < large);
  }

  SECTION("Empty state comparisons")
  {
    TestAddress empty1;
    TestAddress empty2;

    CHECK(empty1 < addr1);
    CHECK(empty1 <= addr1);
    CHECK_FALSE(empty1 > addr1);
    CHECK_FALSE(empty1 < empty2);
    CHECK(empty1 == empty2);

    empty1.reset();
    CHECK_FALSE(empty1 < empty2);
    CHECK(empty1 == empty2);
  }

  SECTION("Different families compare by enum")
  {
    sockaddr_storage su{};
    su.ss_family = AF_UNIX;
    TestAddress addr{su, "u", 0};

    if (to_underlying(addr1.family()) < to_underlying(addr.family())) {
      CHECK(addr1 < addr);
    } else {
      CHECK(addr < addr1);
    }
  }

  SECTION("Operators and compare() consistency")
  {
    auto verify = [&](auto& lhs, auto& rhs) {
      const int ret = lhs.compare(rhs);
      CHECK((ret < 0) == (lhs < rhs));
      CHECK((ret == 0) == (lhs == rhs));
      CHECK((ret > 0) == (lhs > rhs));
      CHECK((ret <= 0) == (lhs <= rhs));
      CHECK((ret >= 0) == (lhs >= rhs));
      CHECK((ret != 0) == (lhs != rhs));
    };
    verify(addr1, addr2);
    verify(addr1, addr1);

    TestAddress small{v4, "x", 2};
    TestAddress large{v4, "y", 4};
    verify(small, large);
  }
}

TEST_CASE("Swap support", "[address][swap]")
{
  auto a4 = make_v4("192.0.2.5", 5555);
  auto b4 = make_v4("192.0.2.9", 5555);

  TestAddress addr1{a4, "five", sizeof(sockaddr_in)};
  TestAddress addr2{b4, "nine", sizeof(sockaddr_in)};

  SECTION("member swap")
  {
    addr1.swap(addr2);
    CHECK(addr1.to_string() == "nine");
    CHECK(addr2.to_string() == "five");
  }
  SECTION("ADL std::swap")
  {
    swap(addr1, addr2);
    CHECK(addr1.to_string() == "nine");
    CHECK(addr2.to_string() == "five");
  }
}

TEST_CASE("Extractor support", "[address][stream]")
{
  auto v4 = make_v4("203.0.113.7", 8080);
  const TestAddress addr{v4, "203.0.113.7", sizeof(sockaddr_in)};

  std::ostringstream os;
  os << addr;
  CHECK(os.str() == "203.0.113.7");
}
