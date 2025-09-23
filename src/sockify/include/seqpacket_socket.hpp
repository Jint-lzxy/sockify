
#ifndef SOCKIFY_SEQPACKET_SOCKET_HPP
#define SOCKIFY_SEQPACKET_SOCKET_HPP

#include "address.hpp"
#include "config.hpp"
#include "socket.hpp"

#include <cstddef>
#include <memory>
#include <system_error>
#include <utility>

namespace sockify {
class SOCKIFY_EXPORT SeqPacketSocket : public Socket {
public:
  explicit SeqPacketSocket(address_family_type family = AddressFamily::Unix, bool inheritable = false);

  explicit SeqPacketSocket(native_handle_type handle,
                           bool inheritable           = false);

  SeqPacketSocket(const SeqPacketSocket& other) = default;
  SeqPacketSocket(SeqPacketSocket&& other) noexcept = default;

  SeqPacketSocket& operator=(const SeqPacketSocket& other) = default;
  SeqPacketSocket& operator=(SeqPacketSocket&& other) noexcept = default;

  ~SeqPacketSocket() override = default;

protected: // NVI Helpers
  native_handle_type do_release() noexcept override;
  void do_shutdown(ShutdownMode how, std::error_code& ec) noexcept override;
  void do_bind(const address_type& address, std::error_code& ec) noexcept override;
  void do_connect(const address_type& address, std::error_code& ec) noexcept override;
  void do_listen(int backlog, std::error_code& ec) noexcept override;
  std::unique_ptr<Socket> do_accept(std::error_code& ec) noexcept override;
  std::size_t
  do_send(const void* buf, std::size_t len, const address_type* dest, int flags, std::error_code& ec) noexcept override;
  std::pair<std::size_t, std::unique_ptr<address_type>>
  do_recv(void* buf, std::size_t len, int flags, std::error_code& ec) noexcept override;
};
} // namespace sockify

#endif // ifndef