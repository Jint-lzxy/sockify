//===-- buffer.hpp - I Don't Fuckin Know -----*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines the polymorphic Address base class for all socket addresses, plus
/// compile‑time enum utilities.
///
//===----------------------------------------------------------------------===//

#ifndef SOCKIFY_BUFFER_HPP
#define SOCKIFY_BUFFER_HPP

#include "config.hpp"

#include <cstddef>
#include <new>
#include <type_traits>
#include <vector>

namespace sockify {

namespace detail {

// aligned_allocator: satisfies C++17 allocator requirements, aligns to Alignment
template <typename Tp, std::size_t Align = alignof(std::max_align_t)>
struct SOCKIFY_HIDDEN AlignedAllocator {
  static_assert(!std::is_const_v<Tp>, "sockify::AlignedAllocator does not support const types");
  static_assert(!std::is_volatile_v<Tp>, "sockify::AlignedAllocator does not support volatile types");

public: // Member types
  using value_type                             = Tp;
  using size_type                              = std::size_t;
  using difference_type                        = std::ptrdiff_t;
  using propagate_on_container_move_assignment = std::true_type;

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

}; // namespace sockify

#endif
