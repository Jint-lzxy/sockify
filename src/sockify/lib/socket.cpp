//
// Created by Bryan Liu on 5/3/25.
//

#include "socket.hpp"

namespace sockify {

void Socket::listen(int backlog)
{}

void Socket::listen(std::error_code& ec, int backlog) noexcept
{}

void Socket::setblocking(bool would_block)
{
  settimeout(duration{would_block ? -1 : 0});
}

void Socket::setblocking(bool would_block, std::error_code& ec) noexcept
{
  settimeout(duration{would_block ? -1 : 0});
}

bool Socket::getblocking() const noexcept
{
  return timeout->count() != 0;
}

void Socket::settimeout(duration timeout)
{}

void Socket::settimeout(duration timeout, std::error_code& ec) noexcept
{}

std::optional<Socket::duration> Socket::gettimeout() const noexcept
{}

void Socket::setsockopt(int level, int optname, int value)
{}

void Socket::setsockopt(int level, int optname, int value, std::error_code& ec) noexcept
{}

void Socket::setsockopt(int level, int optname, const buffer_type& option_value)
{}

void Socket::setsockopt(int level, int optname, const buffer_type& option_value, std::error_code& ec) noexcept
{}

int Socket::getsockopt(int level, int optname) const
{}

int Socket::getsockopt(int level, int optname, std::error_code& ec) const noexcept
{}

Socket::buffer_type Socket::getsockopt(int level, int optname, std::size_t buflen) const
{}

Socket::buffer_type Socket::getsockopt(int level, int optname, std::size_t buflen, std::error_code& ec) const noexcept
{}

void Socket::setinheritable(bool inheritable)
{}

void Socket::setinheritable(bool inheritable, std::error_code& ec) noexcept
{}

bool Socket::getinheritable() const noexcept
{}

std::size_t Socket::send(const buffer_type& buf, int flags)
{}

std::size_t Socket::send(const buffer_type& buf, std::error_code& ec, int flags) noexcept
{}

std::size_t Socket::sendto(const buffer_type& buf, const address_type& dest, int flags)
{}

std::size_t Socket::sendto(const buffer_type& buf, const address_type& dest, std::error_code& ec, int flags) noexcept
{}

std::size_t Socket::sendall(const Socket::buffer_type& buf, int flags)
{}

std::size_t Socket::sendall(const Socket::buffer_type& buf, std::error_code& ec, int flags) noexcept
{}

std::size_t Socket::sendfile(std::ifstream& file, std::streampos offset, std::size_t count)
{}

std::size_t
Socket::sendfile(std::ifstream& file, std::error_code& ec, std::streampos offset, std::size_t count) noexcept
{}

std::size_t Socket::sendfile(const std::filesystem::path& file_path, std::streampos offset, std::size_t count)
{}

std::size_t Socket::sendfile(const std::filesystem::path& file_path,
                             std::error_code& ec,
                             std::streampos offset,
                             std::size_t count) noexcept
{}

Socket::buffer_type Socket::recv(std::size_t count, int flags)
{}

Socket::buffer_type Socket::recv(std::size_t count, std::error_code& ec, int flags) noexcept
{}

Socket::buffer_type Socket::recvfrom(std::size_t count, address_type& src, int flags)
{}

Socket::buffer_type Socket::recvfrom(std::size_t count, address_type& src, std::error_code& ec, int flags) noexcept
{}

Socket::native_handle_type Socket::native_handle() const noexcept
{}

void Socket::swap(Socket& other) noexcept
{}

void swap(Socket& lhs, Socket& rhs) noexcept
{}

} // namespace sockify