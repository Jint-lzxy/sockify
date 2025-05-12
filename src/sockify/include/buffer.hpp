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
#include <iterator>
#include <limits>
#include <new>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>

namespace sockify {
namespace details {

/// Allocator that always returns memory aligned to \p Align.
/// \tparam Tp     Value type to allocate.
/// \tparam Align  Desired alignment for all allocations.
/// \warning Does not support const or volatile \p Tp.
template <typename Tp, std::size_t Align = std::alignment_of_v<std::max_align_t>>
struct SOCKIFY_HIDDEN AlignedAllocator {
  static_assert(!std::is_const_v<Tp>, "sockify::AlignedAllocator does not support const types");
  static_assert(!std::is_volatile_v<Tp>, "sockify::AlignedAllocator does not support volatile types");

public: // Member types
  /// Type of values allocated.
  using value_type = Tp;
  /// Size type used by the allocator.
  using size_type = std::size_t;
  /// Difference type used by the allocator.
  using difference_type = std::ptrdiff_t;
  /// Propagate on container move assignment.
  using propagate_on_container_move_assignment = std::true_type;

  // NOLINTBEGIN(readability-identifier-naming): Standard allocator rebind idiom.

  /// Rebind to allocator for another type.
  /// \tparam Up  Other value type.
  template <typename Up>
  struct rebind {
    using other = AlignedAllocator<Up, Align>;
  };

  // NOLINTEND(readability-identifier-naming)

public: // Member functions
  /// Default constructor.
  AlignedAllocator() noexcept = default;

  /// Copy-constructor from a different instantiation.
  /// \tparam OtherType  Another value type.
  template <typename OtherType>
  AlignedAllocator(const AlignedAllocator<OtherType, Align>& /*other*/) noexcept
  {}

  /// Allocate storage for \p count objects of type \p Tp.
  /// \param count  Number of objects to allocate.
  /// \return Pointer to aligned memory block.
  /// \throws std::bad_alloc on allocation failure.
  /// \throws std::bad_array_new_length if `std::numeric_limits<std::size_t>::max() / sizeof(Tp) < count`.
  [[nodiscard]] Tp* allocate(size_type count)
  {
    // NOLINTNEXTLINE(bugprone-sizeof-expression): Intentional use of sizeof on possibly incomplete type.
    static_assert(sizeof(Tp) >= 0, "cannot allocate memory for an incomplete type");

    const std::size_t howmany = count * sizeof(Tp);
    if constexpr (Align <= __STDCPP_DEFAULT_NEW_ALIGNMENT__)
      return static_cast<Tp*>(::operator new(howmany));
    else
      return static_cast<Tp*>(::operator new(howmany, std::align_val_t{Align}));
  }

  /// Deallocate memory previously allocated by this allocator.
  /// \param ptr   Pointer to memory block.
  /// \param size  Unused. Size is not required for deallocation.
  void deallocate(Tp* ptr, size_type /*size*/) noexcept
  {
    if constexpr (Align <= __STDCPP_DEFAULT_NEW_ALIGNMENT__)
      ::operator delete(ptr);
    else
      ::operator delete(ptr, std::align_val_t{Align});
  }
};

/// Equality comparison: two allocators compare equal if their alignments match.
/// \relates AlignedAllocator
template <typename T1, std::size_t Align1, typename T2, std::size_t Align2>
constexpr bool operator==(const AlignedAllocator<T1, Align1>& /*lhs*/,
                          const AlignedAllocator<T2, Align2>& /*rhs*/) noexcept
{
  return Align1 == Align2;
}

/// Inequality comparison for allocators.
/// \relates AlignedAllocator
template <typename T1, std::size_t Align1, typename T2, std::size_t Align2>
constexpr bool operator!=(const AlignedAllocator<T1, Align1>& lhs, const AlignedAllocator<T2, Align2>& rhs) noexcept
{
  return !(lhs == rhs);
}

} // namespace details

/// Dynamically resizable contiguous storage of `std::byte` with maximal alignment.
///
/// Buffer provides a contiguous memory region whose starting address is guaranteed to be
/// aligned to `alignof(std::max_align_t)`, sufficient for storing any object `T` where
/// `alignof(T) <= alignof(std::max_align_t)`.
///
/// Specifically, for any such type `T`, if `buffer.size() >= sizeof(T)`, then
/// `reinterpret_cast<T*>(buffer.data())` yields a pointer that is correctly aligned for `T`.
///
/// \warning This buffer does **not** provide storage suitable for types with stricter alignment
/// than `std::max_align_t`. Reinterpreting the storage as such types is undefined behavior
/// unless external guarantees are made. For portable access to over-aligned types, prefer
/// utilities like \ref from_buffer or \ref buffer_read_value, which avoid alignment issues by
/// copying into a properly aligned temporary.
///
/// \see std::vector
using Buffer = std::vector<std::byte, details::AlignedAllocator<std::byte>>;

/// Create a buffer of \p count bytes, value-initialized to zero.
/// \param count  Number of bytes in the new buffer.
/// \return Zeroed Buffer of size \p count.
/// \throws std::bad_alloc on allocation failure.
inline SOCKIFY_EXPORT Buffer make_buffer(std::size_t count)
{
  return Buffer{count, std::byte{0}};
}

/// Create a buffer by copying \p count bytes from \p data.
/// \param data   Pointer to source bytes.
/// \param count  Number of bytes to copy.
/// \return Buffer containing the copied bytes.
/// \throws std::invalid_argument if \p data is \p nullptr and `count > 0`.
/// \throws std::bad_alloc on allocation failure.
inline SOCKIFY_EXPORT Buffer make_buffer(const void* data, std::size_t count)
{
  if (data == nullptr && count > 0)
    throw std::invalid_argument{"data pointer is null but count > 0"};

  const auto* src = static_cast<const std::byte*>(data);
  return Buffer{src, src + count};
}

/// Reinterpret the underlying bytes of \p buffer as an array of \p T.
/// \tparam T      Type to cast to. Alignment of \p T must be <= max_align_t.
/// \param buffer  The Buffer to cast.
/// \return Pointer to the first element of type \p T.
/// \note Does not perform bounds checking.
template <typename T>
SOCKIFY_EXPORT T* buffer_cast(Buffer& buffer) noexcept
{
  return reinterpret_cast<T*>(buffer.data());
}

/// Deleted overload to prevent casting from temporary buffers.
/// \tparam T  Type to cast to.
template <typename T>
SOCKIFY_EXPORT T* buffer_cast(Buffer&&) = delete;

/// Const overload of buffer_cast.
/// \tparam T  Type to cast to.
template <typename T>
SOCKIFY_EXPORT const T* buffer_cast(const Buffer& buffer) noexcept
{
  return reinterpret_cast<const T*>(buffer.data());
}

/// Deleted rvalue overload for const buffer.
/// \tparam T  Type to cast to.
template <typename T>
SOCKIFY_EXPORT const T* buffer_cast(const Buffer&&) = delete;

/// Pack a trivially-copyable object \p obj into a Buffer.
/// \tparam T   Trivially-copyable type.
/// \param obj  Object to serialize.
/// \return Buffer containing the raw bytes of \p obj.
template <typename T>
SOCKIFY_EXPORT Buffer to_buffer(const T& obj)
{
  static_assert(std::is_trivially_copyable_v<T>, "to_buffer only works on trivially copyable types");

  return make_buffer(&obj, sizeof(T));
}

/// Unpack a trivially-copyable object of type \p T from \p buf.
/// \tparam T   Trivially-copyable type.
/// \param buf  Buffer containing at least \p sizeof(T) bytes.
/// \return Copy of the object reconstructed from \p buf.
/// \throws std::out_of_range if `buf.size() < sizeof(T)`.
template <typename T>
SOCKIFY_EXPORT T from_buffer(const Buffer& buf)
{
  static_assert(std::is_trivially_copyable_v<T>, "from_buffer only works on trivially copyable types");

  if (buf.size() < sizeof(T))
    throw std::out_of_range{"buffer too small"};

  T obj;
  std::memcpy(&obj, buf.data(), sizeof(T));
  return obj;
}

/// Extract a slice of \p buffer from index \p start to \p stop (exclusive).
/// Negative indices wrap from end.
/// \param buffer  Input Buffer.
/// \param start   Starting index (can be negative).
/// \param stop    One-past-the-end index (can be negative, defaults to end).
/// \return New Buffer containing the selected range.
/// \pre start and stop are in [-size, size].
inline SOCKIFY_EXPORT Buffer buffer_slice(const Buffer& buffer,
                                          std::ptrdiff_t start,
                                          std::ptrdiff_t stop = std::numeric_limits<std::ptrdiff_t>::max())
{
  const auto size = static_cast<std::ptrdiff_t>(buffer.size());

  // Handle negative indices and clamp
  start = std::clamp<std::ptrdiff_t>(start < 0 ? start + size : start, 0, size);
  stop  = std::clamp<std::ptrdiff_t>(stop < 0 ? stop + size : stop, 0, size);

  if (stop <= start)
    return Buffer{};

  return Buffer{std::next(buffer.begin(), start), std::next(buffer.begin(), stop)};
}

/// Concatenate two Buffers into one.
/// \param first   First buffer.
/// \param second  Second buffer.
/// \return New Buffer containing first followed by second.
inline SOCKIFY_EXPORT Buffer buffer_concat(const Buffer& first, const Buffer& second)
{
  Buffer out;
  out.reserve(first.size() + second.size());

  out.insert(out.end(), first.begin(), first.end());
  out.insert(out.end(), second.begin(), second.end());

  return out;
}

}; // namespace sockify

namespace sockify {

/// Enumeration of byte orderings.
/// \see buffer_read_value, buffer_write_value
enum class Endian : std::uint16_t {
  Big    = 0xFACE, ///< Big-endian
  Little = 0xDEAD, ///< Little-endian
#if SOCKIFY_BIG_ENDIAN
  Native = Big, ///< Native-endian
#elif SOCKIFY_LITTLE_ENDIAN
  Native = Little, ///< Native-endian
#else
  // Unknown/mixed
  Native = 0xCAFE ///< Native-endian
#endif
};

} // namespace sockify

namespace std {

/// Hash specialization for sockify::Buffer, treating its bytes as a string_view.
/// \relates sockify::Buffer
template <>
struct SOCKIFY_EXPORT hash<sockify::Buffer> {
  /// Compute hash of the raw bytes in \p buffer.
  /// \param buffer  The Buffer to hash.
  /// \return size_t hash value.
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
namespace details {

/// True if reading/writing Endian E requires byte swapping on this platform.
template <Endian E>
using needs_swap = std::integral_constant<bool,
                                          (E == Endian::Big && Endian::Native == Endian::Little) ||
                                              (E == Endian::Little && Endian::Native == Endian::Big)>;

template <Endian E>
inline constexpr bool needs_swap_v = needs_swap<E>::value;

/// Reverse the byte order in-place for a fixed-size array.
/// \tparam N     Number of bytes.
/// \param bytes  Byte array to reverse.
template <std::size_t N>
SOCKIFY_HIDDEN constexpr void byte_swap(std::array<std::byte, N>& bytes) noexcept
{
  for (std::size_t start = 0, end = N - 1; start < end; ++start, --end) {
    auto imdt    = std::move(bytes[start]);
    bytes[start] = std::move(bytes[end]);
    bytes[end]   = std::move(imdt);
  }
}

// We do not conditionally compile for uint16_t here, as we can
// reasonably assume it is available on all modern platforms.
/// Swap bytes in a 16-bit unsigned integer.
/// \param val  Input value.
/// \return Byte-swapped value.
SOCKIFY_HIDDEN constexpr std::uint16_t bswap16(std::uint16_t val) noexcept
{
  return static_cast<std::uint16_t>((val << 8) | (val >> 8));
}

#ifdef UINT32_MAX
/// Swap bytes in a 32-bit unsigned integer.
/// \param val  Input value.
/// \return Byte-swapped value.
SOCKIFY_HIDDEN constexpr std::uint32_t bswap32(std::uint32_t val) noexcept
{
  return ((val & 0x000000FFU) << 24) | ((val & 0x0000FF00U) << 8) | ((val & 0x00FF0000U) >> 8) |
         ((val & 0xFF000000U) >> 24);
}
#endif

#ifdef UINT64_MAX
/// Swap bytes in a 64-bit unsigned integer.
/// \param val  Input value.
/// \return Byte-swapped value.
SOCKIFY_HIDDEN constexpr std::uint64_t bswap64(std::uint64_t val) noexcept
{
  return ((val & 0x00000000000000FFULL) << 56) | ((val & 0x000000000000FF00ULL) << 40) |
         ((val & 0x0000000000FF0000ULL) << 24) | ((val & 0x00000000FF000000ULL) << 8) |
         ((val & 0x000000FF00000000ULL) >> 8) | ((val & 0x0000FF0000000000ULL) >> 24) |
         ((val & 0x00FF000000000000ULL) >> 40) | ((val & 0xFF00000000000000ULL) >> 56);
}
#endif

} // namespace details

/// Read a trivially-copyable value of type \p Tp from \p buffer at byte \p offset,
/// interpreting bytes according to endian \p E.
/// \tparam Tp     Trivially-copyable type to read.
/// \tparam E      Endianness to use (default = Native).
/// \param buffer  Source buffer.
/// \param offset  Byte offset within buffer.
/// \return Value of type \p Tp read from buffer.
/// \throws std::out_of_range if `offset + sizeof(Tp) > buffer.size()`.
template <typename Tp, Endian E = Endian::Native>
SOCKIFY_EXPORT Tp buffer_read_value(const Buffer& buffer, std::size_t offset)
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
    if constexpr (details::needs_swap_v<E>)
      raw = details::bswap16(raw);

    auto value = static_cast<Access>(raw);
    std::memcpy(&result, &value, typesize);
  }
#ifdef UINT32_MAX
  else if constexpr (typesize == sizeof(std::uint32_t)) {
    using Up     = std::uint32_t;
    using Access = std::conditional_t<std::is_signed_v<Tp>, std::make_signed_t<Up>, Up>;

    Up raw{};
    std::memcpy(&raw, buffer.data() + offset, typesize);
    if constexpr (details::needs_swap_v<E>)
      raw = details::bswap32(raw);

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
    if constexpr (details::needs_swap_v<E>)
      raw = details::bswap64(raw);

    auto value = static_cast<Access>(raw);
    std::memcpy(&result, &value, typesize);
  }
#endif
  else {
    std::array<std::byte, typesize> bytes;
    std::copy_n(buffer.data() + offset, typesize, bytes.begin());
    if constexpr (details::needs_swap_v<E>)
      details::byte_swap(bytes);

    std::memcpy(&result, bytes.data(), typesize);
  }

  return result;
}

/// Write a trivially-copyable value \p value of type \p Tp into \p buffer at byte \p offset,
/// storing bytes according to endian \p E.
/// \tparam Tp     Trivially-copyable type to write.
/// \tparam E      Endianness to use (default = Native).
/// \param buffer  Destination buffer.
/// \param offset  Byte offset within buffer.
/// \param value   Value to write.
/// \throws std::out_of_range if `offset + sizeof(Tp) > buffer.size()`.
template <typename Tp, Endian E = Endian::Native>
SOCKIFY_EXPORT void buffer_write_value(Buffer& buffer, std::size_t offset, const Tp& value)
{
  static_assert(std::is_trivially_copyable_v<Tp>, "buffer_write_value only works for trivially copyable types");

  constexpr std::size_t typesize = sizeof(Tp);
  if (offset + typesize > buffer.size())
    throw std::out_of_range{"buffer_write_value out of range"};

  auto* out = buffer.data() + offset;

  if constexpr (typesize == sizeof(std::uint16_t)) {
    auto data = static_cast<std::uint16_t>(value);
    if constexpr (details::needs_swap_v<E>)
      data = details::bswap16(data);

    std::copy_n(reinterpret_cast<const std::byte*>(&data), typesize, out);
  }
#ifdef UINT32_MAX
  else if constexpr (typesize == sizeof(std::uint32_t)) {
    auto data = static_cast<std::uint32_t>(value);
    if constexpr (details::needs_swap_v<E>)
      data = details::bswap32(data);

    std::copy_n(reinterpret_cast<const std::byte*>(&data), typesize, out);
  }
#endif
#ifdef UINT64_MAX
  else if constexpr (typesize == sizeof(std::uint64_t)) {
    auto data = static_cast<std::uint64_t>(value);
    if constexpr (details::needs_swap_v<E>)
      data = details::bswap64(data);

    std::copy_n(reinterpret_cast<const std::byte*>(&data), typesize, out);
  }
#endif
  else {
    // For any other trivially copyable type
    std::array<std::byte, typesize> bytes;
    std::copy_n(reinterpret_cast<const std::byte*>(&value), typesize, bytes.begin());
    if constexpr (E != Endian::Native && details::needs_swap_v<E>)
      details::byte_swap(bytes);

    std::copy_n(bytes.begin(), typesize, out);
  }
}

} // namespace sockify

#endif
