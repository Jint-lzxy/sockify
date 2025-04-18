#include "error.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <string>
#include <system_error>
#include <utility>

using namespace sockify;

static std::error_code to_std_errc(std::errc ec) { return std::make_error_code(ec); }

TEST_CASE("socket_errc traits and conversions", "[error][traits]")
{
  STATIC_REQUIRE(std::is_error_condition_enum<socket_errc>::value);

  SECTION("make_error_code and make_error_condition roundtrip")
  {
    for (int i = 0; i <= static_cast<int>(socket_errc::tls_cert_verify); ++i) {
      auto err = static_cast<socket_errc>(i);
      DYNAMIC_SECTION("Testing value " << i)
      {
        std::error_code ec        = make_error_code(err);
        std::error_condition cond = make_error_condition(err);

        CHECK(ec.value() == i);
        CHECK(cond.value() == i);

        CHECK(&ec.category() == &socket_category());
        CHECK(&cond.category() == &socket_category());
      }
    }
  }
}

TEST_CASE("socket_category name and messages", "[error][category][message]")
{
  SECTION("Category name is correct")
  {
    // should be exactly "sockify::socket_category"
    CHECK(std::string(socket_category().name()) == "sockify::socket_category");
  }

  SECTION("Known messages are mapped")
  {
    for (int i = 0; i <= static_cast<int>(socket_errc::tls_cert_verify); ++i) {
      DYNAMIC_SECTION("Message for code " << i)
      {
        auto msg = socket_category().message(i);
        CHECK_FALSE(msg.empty());
        CHECK(msg != "unknown sockify socket error");
      }
    }
  }

  SECTION("Unknown values return fallback message")
  {
    CHECK(socket_category().message(-1) == "unknown sockify socket error");
    CHECK(socket_category().message(999) == "unknown sockify socket error");
  }
}

TEST_CASE("equivalent() works as expected", "[error][equivalence]")
{
  SECTION("Reflexivity within socket_category")
  {
    for (int i = 0; i <= static_cast<int>(socket_errc::tls_cert_verify); ++i) {
      auto code = make_error_code(static_cast<socket_errc>(i));
      DYNAMIC_SECTION("Code " << i << " equivalent to itself") { CHECK(socket_category().equivalent(code, i)); }
    }
  }

  SECTION("Equivalence to std::errc mappings")
  {
    using MappingPair            = std::pair<socket_errc, std::errc>;
    const MappingPair mappings[] = {
        {        socket_errc::address_in_use,          std::errc::address_in_use},
        { socket_errc::address_not_available,   std::errc::address_not_available},
        {   socket_errc::bad_file_descriptor,     std::errc::bad_file_descriptor},
        {    socket_errc::connection_refused,      std::errc::connection_refused},
        {      socket_errc::connection_reset,        std::errc::connection_reset},
        {      socket_errc::host_unreachable,        std::errc::host_unreachable},
        {     socket_errc::message_too_large,            std::errc::message_size},
        {   socket_errc::network_unreachable,     std::errc::network_unreachable},
        {       socket_errc::no_buffer_space,         std::errc::no_buffer_space},
        {          socket_errc::not_a_socket,            std::errc::not_a_socket},
        {         socket_errc::not_connected,           std::errc::not_connected},
        {         socket_errc::not_supported, std::errc::operation_not_supported},
        { socket_errc::operation_in_progress,   std::errc::operation_in_progress},
        { socket_errc::operation_would_block,   std::errc::operation_would_block},
        {     socket_errc::permission_denied,       std::errc::permission_denied},
        {socket_errc::protocol_not_supported,  std::errc::protocol_not_supported},
        {             socket_errc::timed_out,               std::errc::timed_out},
        {     socket_errc::already_connected,       std::errc::already_connected},
        {              socket_errc::io_error,                std::errc::io_error},
        {   socket_errc::operation_cancelled,      std::errc::operation_canceled},
        {socket_errc::peer_closed_connection,        std::errc::connection_reset},
        {    socket_errc::resource_exhausted,         std::errc::no_buffer_space},
    };

    for (const auto& [sock_err, std_err] : mappings) {
      DYNAMIC_SECTION("Mapping " << static_cast<int>(sock_err) << " to std::errc")
      {
        auto std_ec = to_std_errc(std_err);
        CHECK(socket_category().equivalent(std_ec, static_cast<int>(sock_err)));
      }
    }
  }

  SECTION("Unknown mappings return false")
  {
    std::error_code unrelated = std::make_error_code(std::errc::function_not_supported);
    CHECK_FALSE(socket_category().equivalent(unrelated, static_cast<int>(socket_errc::tls_zero_return)));
  }
}

TEST_CASE("socket_error exception works correctly", "[error][exception]")
{
  auto ec               = make_error_code(socket_errc::not_connected);
  const std::string msg = "Connection failed";

  SECTION("Constructed with std::string")
  {
    socket_error err(msg, ec);
    CHECK(err.code() == ec);
    CHECK_THAT(err.what(), Catch::Matchers::ContainsSubstring(msg));
  }

  SECTION("Constructed with C-string")
  {
    socket_error err(msg.c_str(), ec);
    CHECK(err.code() == ec);
    CHECK_THAT(err.what(), Catch::Matchers::ContainsSubstring("Connection failed"));
  }

  SECTION("Copy construction and assignment")
  {
    socket_error original("copy", ec);
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization): This copy is intentional.
    socket_error copy = original;
    CHECK(copy.code() == ec);
    socket_error assigned("temp", make_error_code(socket_errc::success));
    assigned = original;
    CHECK(assigned.code() == ec);
  }

  SECTION("Can be caught as std::system_error")
  {
    try {
      throw socket_error("boom", ec);
    }
    catch (const std::system_error& se) {
      CHECK(se.code() == ec);
      CHECK_THAT(se.what(), Catch::Matchers::ContainsSubstring("boom"));
    }
  }
}
