#include "address.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <netinet/in.h>
#include <sstream>
#include <sys/socket.h>
#include <utility>

using namespace sockify;

class TestAddress : public Address {
public:
  TestAddress() : _size(0), _text("")
  {}

  explicit TestAddress(const sockaddr_storage& ss, std::size_t size_bytes, std::string text) noexcept
      : Address(ss), _size(size_bytes), _text(std::move(text))
  {}

  socklen_t size() const noexcept override
  {
    return static_cast<socklen_t>(_size);
  }

  string_type to_string() const override
  {
    return _text;
  }

protected:
  void do_swap(Address& other) noexcept override
  {
    auto& o = static_cast<TestAddress&>(other);
    std::swap(_size, o._size);
    std::swap(_text, o._text);
  }

  int compare(const Address& other) const noexcept override
  {
    return size() < other.size();
  }

private:
  std::size_t _size;
  std::string _text;
};

TEST_CASE("Empty‑state observers", "[address][empty]")
{
  TestAddress a;
  CHECK(!a);
  CHECK(a.has_value() == false);

  a.reset();
  CHECK(!a);
  CHECK(a.has_value() == false);
}

TEST_CASE("Value‑state construction and accessors", "[address][construct]")
{
  sockaddr_storage ss4{};
  ss4.ss_family = AF_INET;
  TestAddress a(ss4, sizeof(sockaddr_in), "1.2.3.4");

  CHECK(a);
  CHECK(a.has_value());
  CHECK(a.family() == AddressFamily::IPv4);
  CHECK(a.value()->ss_family == AF_INET);
  CHECK(a.size() == sizeof(sockaddr_in));
  CHECK(a.to_string() == "1.2.3.4");
}

TEST_CASE("Unrecognized family yields empty state", "[address][unknown]")
{
  sockaddr_storage ssx{};
  ssx.ss_family = -1; // out-of-bound family
  TestAddress a(ssx, 4, "bogus");

  CHECK(a.family() == AddressFamily::Unknown);
  CHECK(!a);
  CHECK(a.has_value() == false);
}

TEST_CASE("Copy and move semantics", "[address][copymove]")
{
  sockaddr_storage ss6{};
  ss6.ss_family = AF_INET6;
  TestAddress orig(ss6, sizeof(sockaddr_in6), "::1");

  SECTION("Copy constructor")
  {
    TestAddress c{orig};
    CHECK(c.has_value());
    CHECK(c.family() == AddressFamily::IPv6);
    CHECK(c.size() == orig.size());
    CHECK(c.to_string() == "::1");
  }
  SECTION("Copy assignment")
  {
    TestAddress x;
    x = orig;
    CHECK(x.has_value());
    CHECK(x.family() == AddressFamily::IPv6);
    CHECK(x.to_string() == "::1");
  }
  SECTION("Move constructor")
  {
    TestAddress tmp = orig;
    TestAddress m{std::move(tmp)};
    CHECK(m.has_value());
    CHECK(m.to_string() == "::1");
  }
  SECTION("Move assignment")
  {
    TestAddress x;
    TestAddress y = orig;
    x             = std::move(y);
    CHECK(x.has_value());
    CHECK(x.to_string() == "::1");
  }
}

TEST_CASE("Equality and ordering", "[address][compare]")
{
  sockaddr_storage a4{}, b4{};
  a4.ss_family                             = AF_INET;
  b4.ss_family                             = AF_INET;
  reinterpret_cast<unsigned char*>(&a4)[0] = 1;
  reinterpret_cast<unsigned char*>(&b4)[0] = 2;
  TestAddress A(a4, 4, "a"), B(b4, 4, "b");

  SECTION("== and !=")
  {
    TestAddress A2(a4, 4, "a");
    CHECK(A == A2);
    CHECK_FALSE(A != A2);
    CHECK(A != B);
  }
  SECTION("<, <=, >, >=")
  {
    CHECK(A < B);
    CHECK(A <= B);
    CHECK(B > A);
    CHECK(B >= A);
  }
  SECTION("Different sizes")
  {
    TestAddress small(a4, 2, "x"), large(a4, 4, "y");
    CHECK(small < large);
  }
  SECTION("Empty state comparisons")
  {
    TestAddress empty;
    TestAddress empty2;

    CHECK(empty < A);
    CHECK(empty <= A);
    CHECK_FALSE(empty > A);

    CHECK_FALSE(empty < empty2);
    CHECK(empty == empty2);
    empty.reset();
    CHECK_FALSE(empty < empty2);
    CHECK(empty == empty2);
  }
  SECTION("Different families compare by enum")
  {
    sockaddr_storage su{};
    su.ss_family = AF_UNIX;
    TestAddress U(su, 2, "u");
    if (static_cast<int>(A.family()) < static_cast<int>(U.family()))
      CHECK(A < U);
    else
      CHECK(U < A);
  }
}

TEST_CASE("Swap support", "[address][swap]")
{
  sockaddr_storage a4{}, b4{};
  a4.ss_family                             = AF_INET;
  reinterpret_cast<unsigned char*>(&a4)[0] = 5;
  b4.ss_family                             = AF_INET;
  reinterpret_cast<unsigned char*>(&b4)[0] = 9;
  TestAddress A(a4, 4, "five"), B(b4, 4, "nine");

  SECTION("Member swap")
  {
    A.swap(B);
    CHECK(A.to_string() == "nine");
    CHECK(B.to_string() == "five");
  }
  SECTION("std::swap via ADL")
  {
    using std::swap;
    swap(A, B);
    CHECK(A.to_string() == "nine");
    CHECK(B.to_string() == "five");
  }
}

TEST_CASE("Extractor support", "[address][stream]")
{
  sockaddr_storage ss{};
  ss.ss_family = AF_INET;
  TestAddress A(ss, 4, "127.0.0.1");
  std::ostringstream sstr;
  sstr << A;
  CHECK(sstr.str() == "127.0.0.1");
}