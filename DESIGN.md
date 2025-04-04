# Sockify Design Document

## Overview

**Sockify** is a modern, cross-platform C++17 socket library aimed at delivering a versatile and efficient API for network communication.

### Features

Sockify provides a uniform interface for network programming on all major platforms, abstracting away platform-dependent socket operations and is written to work well with the C++ standard library. For example, sockets can be treated as regular I/O streams. This allows applications to read from and write to network sockets through well-known `std::istream` and `std::ostream` interfaces.

All of Sockify's advanced features are optional. Secure connections are provided via integration with [_OpenSSL_](https://github.com/openssl/openssl) or [_MbedTLS_](https://github.com/Mbed-TLS/mbedtls), if it is enabled at compile time. If [_libevent_](https://github.com/libevent/libevent) is present, Sockify also uses an event-based model of I/O, with support for multiplexing across multiple sockets with little overhead. Without _libevent_, a synchronous default is provided.

The library is built to be extensible, so it can support other transport protocols in the future.

### Design Choices

As you program with Sockify, keep in mind the following rules to help you get best performance, maintainability and usability:

- Use modern C++ features such as `std::unique_ptr` and `std::shared_ptr` to manage resources. Use RAII patterns wherever feasible so that resources are cleaned up properly.
- Sockify is designed to be low overhead by default. SSL/TLS and event-driven I/O are add-on features that only need to be enabled if necessary. If your app doesn't need SSL, for instance, simply don't enable it at compile time to keep the library lean and fast.
- Sockify does **not** implement thread-safe sockets by default. This is to avoid the performance overhead of locking mechanisms. If your application requires concurrency, use message-passing, worker threads and/or external synchronization mechanisms. For multi-threaded applications, ensure that each socket instance is only used within a single thread unless explicitly designed otherwise.
- Sockify uses `std::error_code` for non-fatal errors so that you can handle failures without exceptions. However, if your application requires exception handling for errors, configure your project to use exceptions where required.
- Sockify abstracts platform-specific details, but do remember to bear in mind platform-specific optimizations or limitations. For instance, Windows does its own socket management differently from Unix-based systems.

## Primary Library Components

```console
                         +-------------------------+
                         |     sockify::Socket     |  <-- Abstract base class
                         | (common socket methods) |  <-- for all sockets
                         +-------------------------+
                                      ^
                                      |
                   +------------------+------------------+            <-- Additional protocols can be
                   |                  |                  |            <-- defined (and some are already
                   |                  |                  |            <-- included in the library) by
                   |                  |                  |            <-- deriving from sockify::Socket
         +--------------------+       |       +--------------------+
         | sockify::TCPSocket |       |       | sockify::UDPSocket |
         | (TCP socket impl)  |       |       | (UDP socket impl)  |
         +--------------------+       |       +--------------------+
                                      |
                                      |
                         +-------------------------+
                         |    sockify::TLSSocket   |  <-- Abstract templated base class for TLS
                         |     (Secure socket)     |  <-- socket and wraps any underlying socket
                         +-------------------------+
                                      ^
                                      |
                    +-----------------+-----------------+
                    |                                   |
            +---------------+                   +---------------+
            | OpenSSLSocket |                   | MbedTLSSocket |
            | (OpenSSL TLS) |                   | (MbedTLS TLS) |
            +---------------+                   +---------------+
             [Wraps any sockify::Socket, not limited to TCP/UDP]

                      +-----------------------------------+
                      |        sockify::EventLoop         |  <-- Abstract base class
                      |       (handles socket I/O)        |  <-- for event-driven I/O
                      +-----------------------------------+
                             /                     \
                            /                       \
                   +----------------+       +---------------+
                   |  LibeventLoop  |       | SyncEventLoop |
                   | (asynchronous) |       | (synchronous) |
                   +----------------+       +---------------+
                  (Sockets register with an EventLoop for I/O)

               +-------------------------------------------------+
               |             sockify::SocketStreamBuf            |
               |     (wraps a sockify::Socket for stream I/O)    |
               +-------------------------------------------------+
                     /                    |                   \
                    /                     |                    \
         +-------------------+  +--------------------+  +-----------------+
         | SocketInputStream |  | SocketOutputStream |  |  SocketStream   |
         |   (std::istream)  |  |   (std::ostream)   |  | (std::iostream) |
         +-------------------+  +--------------------+  +-----------------+
            (Any sockify::Socket can be wrapped by these stream classes)
```

## Interface Reference (Helpers)

> [!CAUTION]
> For simplicity and brevity, the `sockify::` namespace is omitted from all definitions in the sections that follow. Unless otherwise specified, assume that all names belong to this namespace. Additionally, this document is intended as a design reference and is **NOT** a comprehensive manual. For the complete API reference, please refer to the generated Doxygen documentation.

### `Address`

The `Address` class encapsulates a complete network address. Internally, it stores a raw network address in a `sockaddr_storage` (designated as its `value_type`), along with a cached port number and protocol indicator (IPv4 or IPv6). Other address details (such as IPv6 flow information) are accessible by directly examining the raw structure via the `get()` member function. The textual representation of the address is produced as a locale-defined multibyte string.

#### Member Types

| Name              | Explanation                                                                                          |
| ----------------- | ---------------------------------------------------------------------------------------------------- |
| `value_type`      | An alias for the raw network address type. (always `sockaddr_storage`)                               |
| `reference`       | `value_type&`                                                                                        |
| `const_reference` | `const value_type&`                                                                                  |
| `pointer`         | `value_type*`                                                                                        |
| `const_pointer`   | `const value_type*`                                                                                  |
| `string_type`     | An alias for the type used to hold textual representations of the IP address. (always `std::string`) |
| `Protocol`        | A scoped enumeration type that distinguishes between IPv4 and IPv6 addresses.                        |

#### Data Members

| Name       | Type         | Explanation                                                                |
| ---------- | ------------ | -------------------------------------------------------------------------- |
| `address`  | `value_type` | The raw network address in a system-defined format.                        |
| `port`     | `uint16_t`   | The port number associated with the address.                               |
| `protocol` | `Protocol`   | The protocol of the address (either `Protocol::IPv4` or `Protocol::IPv6`). |

#### Member Functions

1. **Default Constructor**

```cpp
Address() noexcept;
```

- **Effect(s):**
  - Constructs an `Address` with a default-initialized `sockaddr_storage`. The values of `port` and `protocol` are undefined.
- **Complexity:** Constant.

2. **Constructor (from raw address)**

```cpp
Address(const_pointer addr) noexcept;
```

- **Effect(s):**

  - Constructs an `Address` object by copying the data from the memory pointed to by `addr` into the internal `address` member. The constructor then deduces the port number and protocol from the contents of the raw address structure.
  - Specifically, if `addr->ss_family` is `AF_INET`, the constructor extracts the port from the corresponding `sockaddr_in` structure and sets the protocol to `Protocol::IPv4`; if `addr->ss_family` is `AF_INET6`, it extracts the port from the corresponding `sockaddr_in6` structure and sets the protocol to `Protocol::IPv6`.

- **Precondition(s):**

  - `addr` shall not be a null pointer.
  - `addr` must point to a valid socket address (either IPv4 or IPv6).

- **Complexity:** Constant.

3. **Constructor (from string representation and port number)**

```cpp
Address(std::string_view addr_str, uint16_t port);
```

- **Effect(s):**

  - Parses `addr_str` (e.g., `"127.0.0.1"` or `"::1"`) to initialize `address` and deduce `protocol`; caches the given port in `port`.
  - If the string cannot be parsed into a valid address, an exception is thrown (see _Exceptions_).

- **Precondition(s):**

  - `addr_str` must not be empty.

- **Exceptions:**

  - `std::invalid_argument`: Thrown if the string cannot be parsed into a valid IP address.
  - `std::out_of_range`: Thrown if the port number is outside the valid range for a port (0–65535).

- **Complexity:** Linear in the length of `addr_str`.

4. **Constructor (from string representation)**

```cpp
explicit Address(std::string_view str);
```

- **Effect(s):**

  - Constructs an `Address` object from the provided string `str`. This string must represent the address in the exact format returned by the `to_string()` member function, which is:
    - For IPv4 addresses: `"ip:port"` (e.g., `"192.168.0.1:8080"`).
    - For IPv6 addresses: `"[ip]:port"` (e.g., `"[2001:0db8:85a3:0000:0000:8a2e:0370:7334]:8080"`).
  - The constructor will parse the string, extract the IP address and port, and initialize the internal `sockaddr_storage` object accordingly.
  - If the string cannot be parsed or doesn't match the expected format, an exception is thrown (see _Exceptions_).

- **Precondition(s):**

  - `str` must not be empty.

- **Exceptions:**

  - `std::invalid_argument`: Thrown if the string cannot be parsed as a valid IP address in the correct format, or if the port number contains non-numeric characters (e.g., letters or special symbols).
  - `std::out_of_range`: Thrown if the parsed port number is outside the valid range (0–65535).

- **Complexity:** Linear in the length of `str`.

5. **Copy Constructor**

```cpp
Address(const Address& other) noexcept;
```

- **Effect(s):**

  - Constructs an `Address` as a copy of other, copying `address`, `port`, and `protocol`.

- **Complexity:** Constant.

6. **Move Constructor**

```cpp
Address(Address&& other) noexcept;
```

- **Effect(s):**

  - Constructs an `Address` by moving the members of `other`.
  - After the move `other` will be in a valid but unspecified state.

- **Complexity:** Constant.

7. **Copy Assignment Operator**

```cpp
Address& operator=(const Address& other) noexcept;
```

- **Effect(s):**

  - Copies `other` into `this`, updating `address`, `port` and `protocol`.

- **Complexity:** Constant.

8. **Move Assignment Operator**

```cpp
Address& operator=(Address&& other) noexcept;
```

- **Effect(s):**

  - Moves the members of `other` into `this`.
  - After the move `other` will be in a valid but unspecified state.

- **Complexity:** Constant.

9. **Destructor**

```cpp
~Address();
```

- **Effect(s):**
  - Destroys the `Address` object.
  - The destructor is trivial if `sockaddr_storage` is trivially destructible.

10. **Accessors**

```cpp
pointer get() noexcept;
const_pointer get() const noexcept;
```

- **Returns:**

  - A pointer to the raw network address obtained via `std::addressof(address)`.

- **Complexity:** Constant.

```cpp
string_type ip() const;
```

- **Returns:**

  - A string representing the IP address (e.g., `"127.0.0.1"` or `"::1"`), derived from the raw `address`.

- **Complexity:** Implementation-specific.

```cpp
uint16_t port() const noexcept;
```

- **Returns:**

  - The cached port number.

- **Complexity:** Constant.

```cpp
Protocol protocol() const noexcept;
```

- **Returns:**

  - The cached protocol (either `Protocol::IPv4` or `Protocol::IPv6`).

- **Complexity:** Constant.

```cpp
string_type to_string() const;
```

- **Returns:**

  - A string representation of the address in the format `"ip:port"`. For IPv6 addresses, the IP is enclosed in square brackets (e.g., `"[::1]:80"`).

- **Complexity:** Implementation-specific.

11. **Comparison Operators**

```cpp
bool operator==(const Address& other) const noexcept;
```

- **Returns:**

  - `true` if the raw address data, cached port, and cached protocol are identical; otherwise, `false`.

- **Complexity:** Constant.

```cpp
bool operator!=(const Address& other) const noexcept;
```

- **Returns:**

  - `!(*this == other)`.

- **Complexity:** Constant.

```cpp
friend bool operator< (const Address& lhs, const Address& rhs);
friend bool operator<=(const Address& lhs, const Address& rhs);
friend bool operator> (const Address& lhs, const Address& rhs);
friend bool operator>=(const Address& lhs, const Address& rhs);
```

- **Effects:**

  - Performs a lexicographical comparison of `lhs.to_string()` and `rhs.to_string()` as if by `std::lexicographical_compare`.

- **Returns:**

  - `true` or `false` depending on the result of the string comparison.

- **Complexity:** Linear in the length of the string representations of `lhs` and `rhs`.

12. **Non-Member Functions**

```cpp
friend std::ostream& operator<<(std::ostream& strm, const Address& addr);
```

- **Effect(s):**

  - Inserts the result of `addr.to_string()` into the output stream `strm`.

- **Returns:**

  - A reference to `strm`.

13. **Swap Support**

```cpp
void swap(Address& other) noexcept;
friend void swap(Address& lhs, Address& rhs) noexcept;
```

- **Effect(s):**

  - Swaps the internal state of two `Address` objects, including cached `protocol` and `port`.

- **Complexity:** Constant.

---

### Error Handling

#### `socket_errc`

The `socket_errc` scoped enumeration is used to define error conditions for various socket-related operations within the Sockify library. These error conditions are designed to be used with the `std::error_condition` mechanism and represent socket-specific error conditions.

The scoped enumeration `socket_errc` defines the following constant values representing socket-related error conditions:

```cpp
enum class socket_errc : int {
  success = 0,             // Indicates no error.
  already_open,            // Attempt to open an already open socket.
  not_open,                // Socket is not open for operations.
  already_connected,       // Attempt to connect an already connected socket.
  not_connected,           // Socket is not connected for operations.
  shutdown_requested,      // Socket operation failed due to requested shutdown.
  operation_aborted,       // Operation was aborted or canceled.
  tls_handshake_failed,    // TLS handshake failed.
  tls_verification_failed, // TLS certificate verification failed.
  event_loop_closed,       // Event loop was closed.
  unsupported_protocol,    // Unsupported protocol requested.
  invalid_argument,        // Invalid argument was passed.
  io_error                 // Generic I/O error code for socket operations.
};
```

There are no non-member functions directly related to the `socket_errc` scoped enumeration. However, `socket_errc` is intended to be used with the `make_error_code` function, which provides integration with the standard `std::errc`.

#### `make_error_code(socket_errc)`

- **Synopsis:**

```cpp
std::error_code make_error_code(socket_errc e) noexcept;
```

- **Description:** The `make_error_code` function converts a `socket_errc` enumeration value into a `std::error_code`. It is used to integrate the socket error conditions in the library with the standard error handling mechanisms in C++.

- **Parameters:**

  - `e`. A value of type `socket_errc`, representing the error code to be converted to a `std::error_code`.

- **Return Value:** A `std::error_code` that represents the error code corresponding to the `socket_errc` value `e`.

#### `make_error_condition(socket_errc)`

- **Synopsis:**

```cpp
std::error_condition make_error_condition(socket_errc e) noexcept;
```

- **Description:** The `make_error_condition` function converts a `socket_errc` enumeration value into a `std::error_condition`.

- **Parameters:**

  - `e`. A value of type `socket_errc`, representing the error code to be converted to a `std::error_condition`.

- **Return Value:** A `std::error_condition` that represents the error code corresponding to the `socket_errc` value `e`.

#### `std::is_error_condition_enum<sockify::socket_errc>`

- **Synopsis:**

```cpp
namespace std {
  template <>
  struct is_error_condition_enum<sockify::socket_errc> : true_type {};
}
```

- **Description:** For `socket_errc`, we specialize `is_error_condition_enum` to inherit from `std::true_type`, allowing `socket_errc` to be used with `std::error_condition` and ensuring compatibility with the standard C++ error handling framework.

#### `details::socket_category_impl`

`details::socket_category_impl` is an internal class that implements the `std::error_category` interface and is used to handle error codes specific to Sockify. It provides a way to map `socket_errc` values to `std::error_code` and `std::error_condition`.

1. **Synopsis**

```cpp
class socket_category_impl : public std::error_category
```

2. **Member Functions**

```cpp
const char* name() const noexcept override;
```

- **Description:**

  - Returns the name of the error category, which is used for debugging and inspecting error categories.

- **Return Value:** A constant C-string representing the name of the error category: `"sockify::socket_category"`.

```cpp
std::error_condition default_error_condition(int ev) const noexcept override;
```

- **Description:**

  - Maps an integer error code (`ev`) to a `std::error_condition` based on `socket_errc` values.

- **Parameter(s):**

  - `ev`: The error code to map.

- **Return Value:** A corresponding std::error_condition.

#### `socket_category()`

The function `socket_category()` provides access to the singleton instance of the `socket_category_impl`. This function ensures thread-safe access to the error category instance. Synopsis:

```cpp
const sockify::details::socket_category_impl& socket_category() noexcept;
```

---

### `Buffer`

The `Buffer` class is an alias for `std::vector<std::byte>`, providing a dynamic, resizable buffer specifically designed to store raw binary data. It is optimized for use in networking contexts, such as socket communication, where you need to handle and manipulate data at a byte level.

#### Synopsis

```cpp
using Buffer = std::vector<std::byte>;
```
