//===-- test_buffer.cpp -----------------------------------------*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//

#include "buffer.hpp"

#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

using namespace sockify;

TEST_CASE("make_buffer produces a zeroed buffer of the requested length", "[buffer][make_buffer.zero]")
{
  auto buf = make_buffer(5);
  CHECK(buf.size() == 5);
  for (auto byte : buf)
    CHECK(byte == std::byte{0});
}

TEST_CASE("make_buffer copies bytes and rejects nullptr", "[buffer][make_buffer.copy][make_buffer.error]")
{
  const std::array<std::byte, 4> src = {std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}};
  const auto buf                     = make_buffer(src.data(), src.size());
  CHECK(buf.size() == src.size());
  CHECK(std::equal(buf.begin(), buf.end(), src.begin()));

  SECTION("nullptr with zero count is OK")
  {
    CHECK_NOTHROW(make_buffer(nullptr, 0));
  }
  SECTION("nullptr with positive count throws")
  {
    CHECK_THROWS_AS(make_buffer(nullptr, 1), std::invalid_argument);
  }
}

TEST_CASE("buffer_cast allows type-safe reinterpret-cast of underlying bytes", "[buffer][buffer_cast]")
{
  auto buf   = make_buffer(2 * sizeof(int));
  auto* pbuf = buffer_cast<int>(buf);
  pbuf[0]    = 42;
  pbuf[1]    = -13;

  SECTION("Mutable cast works")
  {
    CHECK(buffer_cast<int>(buf)[0] == 42);
    CHECK(buffer_cast<int>(buf)[1] == -13);
  }
  SECTION("Const cast works")
  {
    const auto* pibuf = buffer_cast<int>(std::as_const(buf));
    CHECK(pibuf[0] == 42);
    CHECK(pibuf[1] == -13);
  }
}

TEST_CASE("to_buffer/from_buffer round-trip for trivially copyable types", "[buffer][to_from]")
{
  struct Point {
    double x, y;
  };

  static_assert(std::is_trivially_copyable_v<Point>);

  Point p1{3.14, -2.71};
  auto buf = to_buffer(p1);

  SECTION("Round-trip yields identical values")
  {
    // The values should match exactly, so we can afford to be strict
    constexpr double tolerance = 1e-12;

    auto p2 = from_buffer<Point>(buf);
    CHECK_THAT(p2.x, Catch::Matchers::WithinRel(3.14, tolerance));
    CHECK_THAT(p2.y, Catch::Matchers::WithinRel(-2.71, tolerance));
  }

  SECTION("from_buffer throws on too-small buffer")
  {
    auto too_small = make_buffer(sizeof(Point) - 1);
    CHECK_THROWS_AS(from_buffer<Point>(too_small), std::out_of_range);
  }
}

TEST_CASE("buffer_slice handles positive, negative, default and empty ranges", "[buffer][slice]")
{
  auto buf = make_buffer(5);
  for (std::size_t i = 0; i < buf.size(); ++i)
    buf[i] = static_cast<std::byte>(i);

  SECTION("Positive indices")
  {
    const auto slice                 = buffer_slice(buf, 1, 4);
    const std::vector<std::byte> exp = {std::byte{1}, std::byte{2}, std::byte{3}};
    CHECK(std::equal(slice.begin(), slice.end(), exp.begin()));
  }
  SECTION("Negative indices")
  {
    const auto slice                 = buffer_slice(buf, -3, -1);
    const std::vector<std::byte> exp = {std::byte{2}, std::byte{3}};
    CHECK(std::equal(slice.begin(), slice.end(), exp.begin()));
  }
  SECTION("Start >= stop yields empty")
  {
    CHECK(buffer_slice(buf, 3, 2).empty());
  }
  SECTION("Default stop to end")
  {
    const auto tail                  = buffer_slice(buf, 3);
    const std::vector<std::byte> exp = {std::byte{3}, std::byte{4}};
    CHECK(std::equal(tail.begin(), tail.end(), exp.begin()));
  }
}

TEST_CASE("buffer_concat concatenates two buffers", "[buffer][concat]")
{
  auto first = make_buffer(2);
  first[0]   = std::byte{0xAA};
  first[1]   = std::byte{0xBB};

  auto second = make_buffer(1);
  second[0]   = std::byte{0xCC};

  auto out = buffer_concat(first, second);
  CHECK(out.size() == 3);
  CHECK(out[0] == std::byte{0xAA});
  CHECK(out[1] == std::byte{0xBB});
  CHECK(out[2] == std::byte{0xCC});
}

TEST_CASE("Endian enum values are correct", "[buffer][endian]")
{
  CHECK(static_cast<std::uint16_t>(Endian::Big) == 0xFACE);
  CHECK(static_cast<std::uint16_t>(Endian::Little) == 0xDEAD);
  auto native = static_cast<std::uint16_t>(Endian::Native);
  CHECK((native == 0xFACE || native == 0xDEAD || native == 0xCAFE));
}

TEST_CASE("hash<Buffer> yields deterministic hashes", "[buffer][hash]")
{
  std::hash<Buffer> hasher;

  SECTION("Empty buffers hash equal")
  {
    auto buf1 = make_buffer(0);
    auto buf2 = make_buffer(nullptr, 0);
    CHECK(hasher(buf1) == hasher(buf2));
  }

  SECTION("Identical contents hash equal")
  {
    // create two buffers containing {0x10, 0x20, 0x30, 0x40}
    std::vector<std::byte> data = {std::byte{0x10}, std::byte{0x20}, std::byte{0x30}, std::byte{0x40}};
    auto buf1                   = make_buffer(data.data(), data.size());
    auto buf2                   = make_buffer(data.data(), data.size());

    CHECK(buf1.size() == buf2.size());
    CHECK(std::equal(buf1.begin(), buf1.end(), buf2.begin()));

    auto hash1 = hasher(buf1);
    auto hash2 = hasher(buf2);
    CHECK(hash1 == hash2);
  }

  SECTION("Different contents (single‐byte change) hash differently")
  {
    // start with {1,2,3,4}
    auto buf1 = make_buffer(4);
    for (std::size_t i = 0; i < buf1.size(); ++i)
      buf1[i] = static_cast<std::byte>(i + 1);

    // copy and mutate one byte
    auto buf2 = buf1;
    buf2[2]   = static_cast<std::byte>(0xFF);

    CHECK(buf1 != buf2);
    CHECK(hasher(buf1) != hasher(buf2));
  }

  SECTION("Hash is noexcept")
  {
    static_assert(noexcept(std::hash<Buffer>{}(std::declval<Buffer&>())), "hash<Buffer>::operator() must be noexcept");
    SUCCEED("hash<Buffer>::operator() is noexcept");
  }
}

TEST_CASE("Buffer read/write io", "[buffer][io]")
{
  auto buf = make_buffer(16);

  SECTION("uint16 native read/write")
  {
    const std::uint16_t val = 0x1234;
    buffer_write_value<std::uint16_t>(buf, 0, val);
    CHECK(buffer_read_value<std::uint16_t>(buf, 0) == val);
  }
  SECTION("uint16 big vs little swap")
  {
    const std::uint16_t val = 0x1234;
    buffer_write_value<std::uint16_t, Endian::Big>(buf, 2, val);
    auto read_le = buffer_read_value<std::uint16_t, Endian::Little>(buf, 2);
    CHECK(read_le == static_cast<std::uint16_t>((val << 8) | (val >> 8)));
  }
  SECTION("uint32 big/native")
  {
    const std::uint32_t val = 0xDEADBEEF;
    buffer_write_value<std::uint32_t, Endian::Big>(buf, 4, val);
    CHECK(buffer_read_value<std::uint32_t, Endian::Big>(buf, 4) == val);

    // Native may be swapped
    auto native = buffer_read_value<std::uint32_t, Endian::Native>(buf, 4);
    if constexpr (Endian::Native == Endian::Big)
      CHECK(native == val);
    else
      CHECK(native != val);
  }
  SECTION("uint64 little")
  {
    const std::uint64_t val = 0x0123456789ABCDEFULL;
    buffer_write_value<std::uint64_t, Endian::Little>(buf, 8, val);
    CHECK(buffer_read_value<std::uint64_t, Endian::Little>(buf, 8) == val);
  }
  SECTION("Custom-size struct")
  {
    struct Three {
      std::byte one, two, three;
    };

    static_assert(sizeof(Three) == 3 && std::is_trivially_copyable_v<Three>);

    Three t1{std::byte{5}, std::byte{6}, std::byte{7}};
    buffer_write_value<Three, Endian::Native>(buf, 0, t1);

    auto t2 = buffer_read_value<Three, Endian::Native>(buf, 0);
    CHECK(t2.one == std::byte{5});
    CHECK(t2.two == std::byte{6});
    CHECK(t2.three == std::byte{7});
  }
  SECTION("out-of-range access throws")
  {
    CHECK_THROWS_AS(buffer_write_value<std::uint32_t>(buf, buf.size() - 2, std::uint32_t{0}), std::out_of_range);
    CHECK_THROWS_AS(buffer_read_value<std::uint32_t>(buf, buf.size() - 2), std::out_of_range);
  }
}
