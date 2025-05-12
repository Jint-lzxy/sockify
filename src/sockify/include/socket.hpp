//===-- address.hpp - Base socket address class and helpers -----*- C++ -*-===//
//
// Part of the Sockify Project, under the BSD 3-Clause License.
// SPDX-License-Identifier: BSD-3-Clause
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines the abstract Socket base class.
///
//===----------------------------------------------------------------------===//

#ifndef SOCKIFY_SOCKET_HPP
#define SOCKIFY_SOCKET_HPP

#include "address.hpp"
#include "buffer.hpp"
#include "config.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <sys/socket.h>
#include <system_error>
#include <type_traits>
#include <utility>

namespace sockify {

// Scoped enumeration representing shutdown modes for socket communication
// Determines whether send and/or receive operations are permitted after shutdown is called.
enum class ShutdownMode : std::uint8_t {
  Receive, // Disables further receive operations (POSIX SHUT_RD)
  Send,    // Disables further send operations (POSIX SHUT_WR)
  Both     // Disables both send and receive operations (POSIX SHUT_RDWR)
};

// The Socket class defines a unified, protocol-agnostic interface for synchronous sockets.
// Encapsulates lifecycle, option management, and I/O, intended for inheritance by
// concrete socket types such as StreamSocket and DatagramSocket.
class SOCKIFY_EXPORT Socket {
public:
  using duration            = std::chrono::milliseconds;
  using buffer_type         = Buffer;
  using address_type        = Address;
  using address_family_type = AddressFamily; // scoped enum AddressFamily
  using native_handle_type =
      std::function<std::remove_pointer_t<decltype(&::socket)>>::result_type; // platform-specific handle

public:
  // Destructor
  virtual ~Socket();

protected:
  // Constructors
  Socket() noexcept;
  Socket(address_family_type family, native_handle_type handle, bool inheritable = false);
  Socket(const Socket& other);
  Socket(Socket&& other) noexcept;

  // Assignment Operators
  Socket& operator=(const Socket& other);
  Socket& operator=(Socket&& other) noexcept;

public:
  // Observers
  explicit operator bool() const noexcept;
  bool valid() const noexcept;

  // Operations
  void close() noexcept;
  native_handle_type release() noexcept;

  void bind(const address_type& address);
  void bind(const address_type& address, std::error_code& ec) noexcept;

  void listen(int backlog);
  void listen(int backlog, std::error_code& ec) noexcept;

  std::unique_ptr<Socket> accept();
  std::unique_ptr<Socket> accept(std::error_code& ec) noexcept;

  void connect(const address_type& address);
  void connect(const address_type& address, std::error_code& ec) noexcept;

  void setblocking(bool would_block);
  void setblocking(bool would_block, std::error_code& ec) noexcept;
  bool getblocking() const noexcept;

  void settimeout(duration timeout);
  void settimeout(duration timeout, std::error_code& ec) noexcept;
  std::optional<duration> gettimeout() const noexcept;

  void setsockopt(int level, int optname, int value);
  void setsockopt(int level, int optname, int value, std::error_code& ec) noexcept;
  void setsockopt(int level, int optname, const buffer_type& option_value);
  void setsockopt(int level, int optname, const buffer_type& option_value, std::error_code& ec) noexcept;

  int getsockopt(int level, int optname) const;
  int getsockopt(int level, int optname, std::error_code& ec) const noexcept;
  buffer_type getsockopt(int level, int optname, std::size_t buflen) const;
  buffer_type getsockopt(int level, int optname, std::size_t buflen, std::error_code& ec) const noexcept;

  void setinheritable(bool inheritable);
  void setinheritable(bool inheritable, std::error_code& ec) noexcept;
  bool getinheritable() const noexcept;

  std::unique_ptr<address_type> getsockname() const;
  std::unique_ptr<address_type> getsockname(std::error_code& ec) const noexcept;

  std::unique_ptr<address_type> getpeername() const;
  std::unique_ptr<address_type> getpeername(std::error_code& ec) const noexcept;

  std::size_t send(const buffer_type& buf, std::unique_ptr<address_type> dest = nullptr, int flags = 0);
  std::size_t send(const buffer_type& buf,
                   std::error_code& ec,
                   std::unique_ptr<address_type> dest = nullptr,
                   int flags                          = 0) noexcept;

  std::size_t sendall(const buffer_type& buf, std::unique_ptr<address_type> dest = nullptr, int flags = 0);
  std::size_t sendall(const buffer_type& buf,
                      std::error_code& ec,
                      std::unique_ptr<address_type> dest = nullptr,
                      int flags                          = 0) noexcept;

  std::size_t sendfile(std::ifstream& file,
                       std::streampos offset              = 0,
                       std::size_t count                  = 0,
                       std::unique_ptr<address_type> dest = nullptr,
                       int flags                          = 0);
  std::size_t sendfile(std::ifstream& file,
                       std::error_code& ec,
                       std::streampos offset              = 0,
                       std::size_t count                  = 0,
                       std::unique_ptr<address_type> dest = nullptr,
                       int flags                          = 0) noexcept;

  std::size_t sendfile(const std::filesystem::path& file_path,
                       std::streampos offset              = 0,
                       std::size_t count                  = 0,
                       std::unique_ptr<address_type> dest = nullptr,
                       int flags                          = 0);
  std::size_t sendfile(const std::filesystem::path& file_path,
                       std::error_code& ec,
                       std::streampos offset              = 0,
                       std::size_t count                  = 0,
                       std::unique_ptr<address_type> dest = nullptr,
                       int flags                          = 0) noexcept;

  std::pair<buffer_type, std::unique_ptr<address_type>> recv(std::size_t count, int flags = 0);
  std::pair<buffer_type, std::unique_ptr<address_type>>
  recv(std::size_t count, std::error_code& ec, int flags = 0) noexcept;

  void shutdown(ShutdownMode how);
  void shutdown(ShutdownMode how, std::error_code& ec) noexcept;

  void swap(Socket& other) noexcept;
  friend void swap(Socket& lhs, Socket& rhs) noexcept;

protected:
  // Non-Virtual Interface (NVI) helper functions
  native_handle_type native_handle() noexcept;

  virtual native_handle_type do_release() noexcept                         = 0;
  virtual void do_shutdown(ShutdownMode how, std::error_code& ec) noexcept = 0;

  virtual void do_bind(const address_type& address, std::error_code& ec) noexcept    = 0;
  virtual void do_connect(const address_type& address, std::error_code& ec) noexcept = 0;
  virtual void do_listen(int backlog, std::error_code& ec) noexcept                  = 0;
  virtual std::unique_ptr<Socket> do_accept(std::error_code& ec) noexcept            = 0;
  virtual std::size_t
  do_send(const void* buf, std::size_t len, const address_type* dest, int flags, std::error_code& ec) noexcept = 0;
  virtual std::pair<std::size_t, std::unique_ptr<address_type>>
  do_recv(void* buf, std::size_t len, int flags, std::error_code& ec) noexcept = 0;
  virtual void do_swap(Socket& other) noexcept                                 = 0;

private:
  // Data Members
  std::optional<duration> timeout; // Timeout for socket operations
  bool inheritable;                // Handle inheritable by child processes
  native_handle_type handle;       // Platform-specific socket handle
};

} // namespace sockify

#endif
