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

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <system_error>

namespace sockify {

class Socket {
public:
  // Member types
  using duration            = std::chrono::milliseconds;
  using buffer_type         = Buffer;
  using address_type        = Address;
  using address_family_type = AddressFamily;
  using native_handle_type  = int; // Adjust per platform (e.g., SOCKET on Windows)

public:
  // Destructor
  virtual ~Socket();

  // Socket operations
  virtual void bind(const address_type& address)                               = 0;
  virtual void bind(const address_type& address, std::error_code& ec) noexcept = 0;

  virtual void connect(const address_type& address)                               = 0;
  virtual void connect(const address_type& address, std::error_code& ec) noexcept = 0;

  virtual void listen(int backlog = SOMAXCONN);
  virtual void listen(std::error_code& ec, int backlog = SOMAXCONN) noexcept;

  virtual std::unique_ptr<Socket> accept()                             = 0;
  virtual std::unique_ptr<Socket> accept(std::error_code& ec) noexcept = 0;

  virtual void close() noexcept = 0;

  virtual native_handle_type detach()                             = 0;
  virtual native_handle_type detach(std::error_code& ec) noexcept = 0;

  virtual void setblocking(bool would_block);
  virtual void setblocking(bool would_block, std::error_code& ec) noexcept;

  virtual bool getblocking() const noexcept;

  virtual void settimeout(duration timeout);
  virtual void settimeout(duration timeout, std::error_code& ec) noexcept;

  virtual std::optional<duration> gettimeout() const noexcept;

  virtual void setsockopt(int level, int optname, int value);
  virtual void setsockopt(int level, int optname, int value, std::error_code& ec) noexcept;

  virtual void setsockopt(int level, int optname, const buffer_type& option_value);
  virtual void setsockopt(int level, int optname, const buffer_type& option_value, std::error_code& ec) noexcept;

  virtual int getsockopt(int level, int optname) const;
  virtual int getsockopt(int level, int optname, std::error_code& ec) const noexcept;

  virtual buffer_type getsockopt(int level, int optname, std::size_t buflen) const;
  virtual buffer_type getsockopt(int level, int optname, std::size_t buflen, std::error_code& ec) const noexcept;

  virtual void setinheritable(bool inheritable);
  virtual void setinheritable(bool inheritable, std::error_code& ec) noexcept;

  virtual bool getinheritable() const noexcept;

  virtual std::unique_ptr<address_type> getsockname() const                             = 0;
  virtual std::unique_ptr<address_type> getsockname(std::error_code& ec) const noexcept = 0;

  virtual std::unique_ptr<address_type> getpeername() const                             = 0;
  virtual std::unique_ptr<address_type> getpeername(std::error_code& ec) const noexcept = 0;

  virtual std::size_t send(const buffer_type& buf, int flags = 0);
  virtual std::size_t send(const buffer_type& buf, std::error_code& ec, int flags = 0) noexcept;

  virtual std::size_t sendto(const buffer_type& buf, const address_type& dest, int flags = 0);
  virtual std::size_t
  sendto(const buffer_type& buf, const address_type& dest, std::error_code& ec, int flags = 0) noexcept;

  virtual std::size_t sendall(const buffer_type& buf, int flags = 0);
  virtual std::size_t sendall(const buffer_type& buf, std::error_code& ec, int flags = 0) noexcept;

  virtual std::size_t sendfile(std::ifstream& file, std::streampos offset = 0, std::size_t count = 0);
  virtual std::size_t
  sendfile(std::ifstream& file, std::error_code& ec, std::streampos offset = 0, std::size_t count = 0) noexcept;

  virtual std::size_t
  sendfile(const std::filesystem::path& file_path, std::streampos offset = 0, std::size_t count = 0);
  virtual std::size_t sendfile(const std::filesystem::path& file_path,
                               std::error_code& ec,
                               std::streampos offset = 0,
                               std::size_t count     = 0) noexcept;

  virtual buffer_type recv(std::size_t count, int flags = 0);
  virtual buffer_type recv(std::size_t count, std::error_code& ec, int flags = 0) noexcept;

  virtual buffer_type recvfrom(std::size_t count, address_type& src, int flags = 0);
  virtual buffer_type recvfrom(std::size_t count, address_type& src, std::error_code& ec, int flags = 0) noexcept;

  virtual void shutdown(int how)                               = 0;
  virtual void shutdown(int how, std::error_code& ec) noexcept = 0;

  virtual native_handle_type native_handle() const noexcept;

  // Swap support
  void swap(Socket& other) noexcept;
  friend void swap(Socket& lhs, Socket& rhs) noexcept;

protected:
  virtual void do_swap(Socket& other) noexcept = 0;

private:
  // Data members
  std::optional<duration> timeout;
  bool inheritable          = false;
  native_handle_type handle = -1;
};

} // namespace sockify

#endif
