//===-- buffer.hpp - Aligned buffer utilities -------------------*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//
//
/// \file
/// Provides the Buffer class, a dynamically resizable byte container with
/// guaranteed alignment, along with associated functions for manipulation,
/// type-safe access, and endian-aware I/O operations.
///
//===----------------------------------------------------------------------===//

#ifndef SOCKIFY_BUFFER_HPP
#define SOCKIFY_BUFFER_HPP

#include "config.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <new>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace sockify {

namespace detail {

// aligned_allocator: satisfies C++17 allocator requirements, aligns to Alignment
template <typename Tp, std::size_t Align = std::alignment_of_v<std::max_align_t>>
struct SOCKIFY_HIDDEN AlignedAllocator {
  static_assert(!std::is_const_v<Tp>, "sockify::AlignedAllocator does not support const types");
  static_assert(!std::is_volatile_v<Tp>, "sockify::AlignedAllocator does not support volatile types");

public: // Member types
  using value_type                             = Tp;
  using size_type                              = std::size_t;
  using difference_type                        = std::ptrdiff_t;
  using propagate_on_container_move_assignment = std::true_type;

  // NOLINTBEGIN(readability-identifier-naming): Standard allocator rebind idiom.

  template <typename U>
  struct rebind {
    using other = AlignedAllocator<U, Align>;
  };

  // NOLINTEND(readability-identifier-naming)

public: // Member functions
  AlignedAllocator() noexcept = default;

  template <typename OtherType>
  AlignedAllocator(const AlignedAllocator<OtherType, Align>& /*other*/) noexcept
  {}

  [[nodiscard]] Tp* allocate(size_type count)
  {
    // NOLINTNEXTLINE(bugprone-sizeof-expression): Intentional use of sizeof on possibly incomplete type.
    static_assert(sizeof(Tp) >= 0, "cannot allocate memory for an incomplete type");

    std::size_t howmany = count * sizeof(Tp);
    if constexpr (Align <= __STDCPP_DEFAULT_NEW_ALIGNMENT__)
      return static_cast<Tp*>(::operator new(howmany));
    else
      return static_cast<Tp*>(::operator new(howmany, std::align_val_t{Align}));
  }

  void deallocate(Tp* ptr, size_type /*size*/) noexcept
  {
    if constexpr (Align <= __STDCPP_DEFAULT_NEW_ALIGNMENT__)
      ::operator delete(ptr);
    else
      ::operator delete(ptr, std::align_val_t(Align));
  }
};

// Equality comparisons for allocators
template <typename T1, std::size_t Align1, typename T2, std::size_t Align2>
constexpr bool operator==(const AlignedAllocator<T1, Align1>& /*lhs*/,
                          const AlignedAllocator<T2, Align2>& /*rhs*/) noexcept
{
  return Align1 == Align2;
}

template <typename T1, std::size_t Align1, typename T2, std::size_t Align2>
constexpr bool operator!=(const AlignedAllocator<T1, Align1>& lhs, const AlignedAllocator<T2, Align2>& rhs) noexcept
{
  return !(lhs == rhs);
}

} // namespace detail

// Buffer: dynamically resizable contiguous storage of std::byte, aligned to max_align_t
// Guarantees that data() is suitably aligned for any T with alignof(T) <= alignof(std::max_align_t).
using Buffer = std::vector<std::byte, detail::AlignedAllocator<std::byte>>;

inline Buffer make_buffer(std::size_t count)
{
  return Buffer{count, std::byte{0}};
}

inline Buffer make_buffer(const void* data, std::size_t count)
{
  if (data == nullptr && count > 0)
    throw std::invalid_argument{"data pointer is null but count > 0"};

  Buffer buf{count};
  const auto* src = reinterpret_cast<const std::byte*>(data);
  std::copy_n(src, count, buf.begin());

  return buf;
}

//
// Type Conversion
//

template <typename T>
T* buffer_cast(Buffer& buffer) noexcept
{
  return reinterpret_cast<T*>(buffer.data());
}

// prevent rvalue use
template <typename T>
T* buffer_cast(Buffer&&) = delete;

template <typename T>
const T* buffer_cast(const Buffer& buffer) noexcept
{
  return reinterpret_cast<const T*>(buffer.data());
}

template <typename T>
const T* buffer_cast(const Buffer&&) = delete;

template <typename T>
Buffer to_buffer(const T& obj)
{
  static_assert(std::is_trivially_copyable_v<T>, "to_buffer only works on trivially copyable types");

  Buffer buf{sizeof(T)};
  const auto* src = reinterpret_cast<const std::byte*>(&obj);
  std::copy_n(src, sizeof(T), buf.begin());

  return buf;
}

template <typename T>
T from_buffer(const Buffer& buf)
{
  static_assert(std::is_trivially_copyable_v<T>, "from_buffer only works on trivially copyable types");

  if (buf.size() < sizeof(T))
    throw std::out_of_range{"buffer too small"};

  T obj;
  std::memcpy(&obj, buf.data(), sizeof(T));
  return obj;
}

//
// Buffer Manipulation
//

inline Buffer buffer_slice(const Buffer& buffer,
                           std::ptrdiff_t start,
                           std::ptrdiff_t stop = std::numeric_limits<std::ptrdiff_t>::max())
{
  const auto size = static_cast<std::ptrdiff_t>(buffer.size());

  // compute S
  std::ptrdiff_t begin = (start < 0 ? std::max<std::ptrdiff_t>(0, start + size) : std::min(start, size));
  // compute E
  std::ptrdiff_t end = (stop < 0 ? std::max<std::ptrdiff_t>(0, stop + size) : std::min(stop, size));

  // clamp
  begin = std::max<std::ptrdiff_t>(0, std::min(begin, size));
  end   = std::max<std::ptrdiff_t>(0, std::min(end, size));

  if (end <= begin)
    return Buffer{}; // empty

  auto len = static_cast<std::size_t>(end - begin);
  Buffer out(len);
  std::memcpy(out.data(), buffer.data() + begin, len);
  return out;
}

inline Buffer buffer_concat(const Buffer& first, const Buffer& second)
{
  Buffer out;

  out.reserve(first.size() + second.size());
  out.insert(out.end(), first.data(), first.data() + first.size());
  out.insert(out.end(), second.data(), second.data() + second.size());
  return out;
}

}; // namespace sockify

namespace sockify {

enum class Endian : std::uint16_t {
  Big    = 0xFACE,
  Little = 0xDEAD,
#if SOCKIFY_BIG_ENDIAN
  Native = Big,
#elif SOCKIFY_LITTLE_ENDIAN
  Native = Little,
#else
  /* Mixed or unknown endianness */
  Native = 0xCAFE
#endif
};

} // namespace sockify

namespace std {

template <>
struct hash<sockify::Buffer> {
  std::size_t operator()(const sockify::Buffer& buffer) const noexcept
  {
    // Treat the raw bytes as a char-based string_view, then hash that.
    auto size         = buffer.size();
    const auto* start = reinterpret_cast<const char*>(buffer.data());
    return std::hash<std::string_view>{}(std::string_view{start, size});
  }
};

} // namespace std

namespace sockify {

namespace detail {

template <Endian E>
using needs_swap = std::integral_constant<bool,
                                          (E == Endian::Big && Endian::Native == Endian::Little) ||
                                              (E == Endian::Little && Endian::Native == Endian::Big)>;

template <Endian E>
inline constexpr bool needs_swap_v = needs_swap<E>::value;

template <std::size_t N>
constexpr void byte_swap(std::array<std::byte, N>& bytes) noexcept
{
  for (std::size_t start = 0, end = N - 1; start < end; ++start, --end) {
    auto imdt    = std::move(bytes[start]);
    bytes[start] = std::move(bytes[end]);
    bytes[end]   = std::move(imdt);
  }
}

// We do not conditionally compile for uint16_t here, as we can
// reasonably assume it is available on all modern platforms.
constexpr std::uint16_t bswap16(std::uint16_t val) noexcept
{
  return static_cast<std::uint16_t>((val << 8) | (val >> 8));
}

#ifdef UINT32_MAX
constexpr std::uint32_t bswap32(std::uint32_t val) noexcept
{
  return ((val & 0x000000FFU) << 24) | ((val & 0x0000FF00U) << 8) | ((val & 0x00FF0000U) >> 8) |
         ((val & 0xFF000000U) >> 24);
}
#endif

#ifdef UINT64_MAX
constexpr std::uint64_t bswap64(std::uint64_t val) noexcept
{
  return ((val & 0x00000000000000FFULL) << 56) | ((val & 0x000000000000FF00ULL) << 40) |
         ((val & 0x0000000000FF0000ULL) << 24) | ((val & 0x00000000FF000000ULL) << 8) |
         ((val & 0x000000FF00000000ULL) >> 8) | ((val & 0x0000FF0000000000ULL) >> 24) |
         ((val & 0x00FF000000000000ULL) >> 40) | ((val & 0xFF00000000000000ULL) >> 56);
}
#endif

} // namespace detail

//
// Read a trivially-copyable Tp from the buffer with endian E.
// Throws std::out_of_range on overflow.
template <typename Tp, Endian E = Endian::Native>
Tp buffer_read_value(const Buffer& buffer, std::size_t offset)
{
  static_assert(std::is_trivially_copyable_v<Tp>, "buffer_read_value only works for trivially copyable types");

  constexpr std::size_t typesize = sizeof(Tp);
  if (offset + typesize > buffer.size())
    throw std::out_of_range{"buffer_read_value out of range"};

  Tp result{};

  if constexpr (typesize == sizeof(std::uint16_t)) {
    using Up     = std::uint16_t;
    using Access = std::conditional_t<std::is_signed_v<Tp>, std::make_signed_t<Up>, Up>;

    Up raw{};
    std::memcpy(&raw, buffer.data() + offset, typesize);
    if constexpr (detail::needs_swap_v<E>)
      raw = detail::bswap16(raw);

    auto value = static_cast<Access>(raw);
    std::memcpy(&result, &value, typesize);
  }
#ifdef UINT32_MAX
  else if constexpr (typesize == sizeof(std::uint32_t)) {
    using Up     = std::uint32_t;
    using Access = std::conditional_t<std::is_signed_v<Tp>, std::make_signed_t<Up>, Up>;

    Up raw{};
    std::memcpy(&raw, buffer.data() + offset, typesize);
    if constexpr (detail::needs_swap_v<E>)
      raw = detail::bswap32(raw);

    auto value = static_cast<Access>(raw);
    std::memcpy(&result, &value, typesize);
  }
#endif
#ifdef UINT64_MAX
  else if constexpr (typesize == sizeof(std::uint64_t)) {
    using Up     = std::uint64_t;
    using Access = std::conditional_t<std::is_signed_v<Tp>, std::make_signed_t<Up>, Up>;

    Up raw{};
    std::memcpy(&raw, buffer.data() + offset, typesize);
    if constexpr (detail::needs_swap_v<E>)
      raw = detail::bswap64(raw);

    auto value = static_cast<Access>(raw);
    std::memcpy(&result, &value, typesize);
  }
#endif
  else {
    std::array<std::byte, typesize> bytes;
    std::copy_n(buffer.data() + offset, typesize, bytes.begin());
    if constexpr (detail::needs_swap_v<E>)
      detail::byte_swap(bytes);

    std::memcpy(&result, bytes.data(), typesize);
  }

  return result;
}

//
// Write a trivially-copyable T into the buffer with endian E.
// Throws std::out_of_range on overflow.
template <typename Tp, Endian E = Endian::Native>
void buffer_write_value(Buffer& buffer, std::size_t offset, const Tp& value)
{
  static_assert(std::is_trivially_copyable_v<Tp>, "buffer_write_value only works for trivially copyable types");

  constexpr std::size_t typesize = sizeof(Tp);
  if (offset + typesize > buffer.size())
    throw std::out_of_range{"buffer_write_value out of range"};

  auto* out = buffer.data() + offset;

  if constexpr (typesize == sizeof(std::uint16_t)) {
    auto data = static_cast<std::uint16_t>(value);
    if constexpr (detail::needs_swap_v<E>)
      data = detail::bswap16(data);

    std::copy_n(reinterpret_cast<const std::byte*>(&data), typesize, out);
  }
#ifdef UINT32_MAX
  else if constexpr (typesize == sizeof(std::uint32_t)) {
    auto data = static_cast<std::uint32_t>(value);
    if constexpr (detail::needs_swap_v<E>)
      data = detail::bswap32(data);

    std::copy_n(reinterpret_cast<const std::byte*>(&data), typesize, out);
  }
#endif
#ifdef UINT64_MAX
  else if constexpr (typesize == sizeof(std::uint64_t)) {
    auto data = static_cast<std::uint64_t>(value);
    if constexpr (detail::needs_swap_v<E>)
      data = detail::bswap64(data);

    std::copy_n(reinterpret_cast<const std::byte*>(&data), typesize, out);
  }
#endif
  else {
    // For any other trivially copyable type
    std::array<std::byte, typesize> bytes;
    std::copy_n(reinterpret_cast<const std::byte*>(&value), typesize, bytes.begin());
    if constexpr (E != Endian::Native && detail::needs_swap_v<E>)
      detail::byte_swap(bytes);

    std::copy_n(bytes.begin(), typesize, out);
  }
}

} // namespace sockify

#endif
