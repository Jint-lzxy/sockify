#include "address.hpp"
#include "unix_address.hpp"

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <filesystem>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/un.h>
#include <utility>
#include <vector>

using namespace sockify;
namespace fs = std::filesystem;

TEST_CASE("size() returns the correct size", "[unixaddress][size]")
{
  const UnixDomainAddress addr;
  CHECK(addr.size() == static_cast<socklen_t>(sizeof(sockaddr_un)));
}

TEST_CASE("Default‐constructed address has empty path and string", "[unixaddress][default]")
{
  const UnixDomainAddress addr;

    CHECK(addr.to_string().empty());
    CHECK(addr.path().u8string().empty());
}

TEST_CASE("Construct from valid filesystem::path", "[unixaddress][construction]")
{
  fs::path path = "/tmp/my.sock";
  SECTION("path() returns original path")
  {
    const UnixDomainAddress addr{path};
    CHECK(addr.path() == path);
  }

  SECTION("to_string() matches u8string()")
  {
    const UnixDomainAddress addr{path};
    CHECK(addr.to_string() == path.u8string());
  }
}

TEST_CASE("Boundary and overflow for socket path", "[unixaddress][boundary]")
{
  // determine the sun_path buffer size used by make_unix()
  const sockaddr_un un_addr{};
  auto max_size = sizeof(un_addr.sun_path);

  SECTION("Path length = max_size - 1 is OK")
  {
    std::string ok_str(max_size - 1, 'A');
    const fs::path ok_path{ok_str};
    const UnixDomainAddress ok{ok_path};
    CHECK(ok.path().u8string() == ok_str);
  }

  SECTION("Path length = max_size throws overflow_error")
  {
    const std::string too_long(max_size, 'B');
    const fs::path bad_path{too_long};
    CHECK_THROWS_AS(UnixDomainAddress{bad_path}, std::overflow_error);
  }
}

TEST_CASE("Copy and move semantics", "[unixaddress][copy][move]")
{
  fs::path path = "/tmp/copy_move.sock";

  SECTION("Copy constructor")
  {
    const UnixDomainAddress orig{path};
        // NOLINTNEXTLINE(performance-unnecessary-copy-initialization): This copy is intentional.
    const UnixDomainAddress copy{orig};
    CHECK(copy.path() == path);
  }

  SECTION("Copy assignment")
  {
    const UnixDomainAddress orig{path};
    UnixDomainAddress copy;
    copy = orig;
    CHECK(copy.path() == path);
  }

  SECTION("Move constructor leaves source empty")
  {
    UnixDomainAddress temp{path};
    const UnixDomainAddress moved{std::move(temp)};
    CHECK(moved.path() == path);
    CHECK(temp.path().u8string().empty());
  }

  SECTION("Move‐assignment leaves source empty")
  {
    UnixDomainAddress temp{path};
    UnixDomainAddress moved;
    moved = std::move(temp);
    CHECK(moved.path() == path);
    CHECK(temp.path().u8string().empty());
  }
}

TEST_CASE("path() accessor", "[unixaddress][accessor]")
{
  fs::path path = "/tmp/test.sock";

  SECTION("lvalue returns reference")
  {
    const UnixDomainAddress addr{path};
    const fs::path& pref = addr.path();
    CHECK(&pref == &addr.path());
  }

  SECTION("rvalue returns moved‐out path")
  {
    fs::path moved_out = UnixDomainAddress{path}.path();
    CHECK(moved_out == path);
  }
}

TEST_CASE("to_string() mirrors path().u8string()", "[unixaddress][tostring]")
{
  const fs::path path = "/tmp/another.sock";
  SECTION("to_string equals u8string")
  {
    const UnixDomainAddress addr{path};
    CHECK(addr.to_string() == path.u8string());
  }
}

TEST_CASE("Comparison operators reflect lexicographical path ordering", "[unixaddress][compare]")
{
  SECTION("direct comparisons")
  {
    UnixDomainAddress lhs{fs::path("aaa")};
    UnixDomainAddress rhs{fs::path("bbb")};
    CHECK(lhs < rhs);
    CHECK(rhs > lhs);
    CHECK(lhs != rhs);
    CHECK(lhs == lhs);
  }

  SECTION("std::sort uses operator<")
  {
    const UnixDomainAddress lhs{fs::path("aaa")};
    const UnixDomainAddress rhs{fs::path("bbb")};
    std::vector<UnixDomainAddress> vec{rhs, lhs};
    std::sort(vec.begin(), vec.end());
    CHECK(vec[0].path() == fs::path("aaa"));
    CHECK(vec[1].path() == fs::path("bbb"));
  }
}

TEST_CASE("swap exchanges socket_path", "[unixaddress][swap]")
{
  SECTION("swap function")
  {
    UnixDomainAddress lhs{fs::path("one.sock")};
    UnixDomainAddress rhs{fs::path("two.sock")};
    swap(lhs, rhs);
    CHECK(lhs.path() == fs::path("two.sock"));
    CHECK(rhs.path() == fs::path("one.sock"));
  }
}

TEST_CASE("Construct from non-AF_UNIX value_type resets to empty", "[unixaddress][value_type]")
{
  SECTION("AF_INET in value_type")
  {
    UnixDomainAddress::value_type raw_addr{};
    std::memset(&raw_addr, 0, sizeof(raw_addr));
    auto& in4      = reinterpret_cast<sockaddr_in&>(raw_addr);
    in4.sin_family = AF_INET;

    const UnixDomainAddress addr(raw_addr);
    CHECK(addr.path().u8string().empty());
    CHECK(addr.to_string().empty());
  }
}
