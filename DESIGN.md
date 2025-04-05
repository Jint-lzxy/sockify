# Sockify Design Document

## Overview

**Sockify** is a modern, cross-platform C++17 socket library aimed at delivering a versatile and efficient API for network communication.

### Features

Sockify provides a uniform interface for network programming on all major platforms, abstracting away platform-dependent socket operations and is written to work well with the C++ standard library. For example, sockets can be treated as regular I/O streams. This allows applications to read from and write to network sockets through well-known `std::istream` and `std::ostream` interfaces.

All of Sockify's advanced features are optional. Secure connections are provided via integration with [_OpenSSL_](https://github.com/openssl/openssl) or [_MbedTLS_](https://github.com/Mbed-TLS/mbedtls), if it is enabled at compile time. If [_libevent_](https://github.com/libevent/libevent) is present, Sockify also uses an event-based model of I/O, with support for multiplexing across multiple sockets with little overhead. Without _libevent_, a simpler minimal default is provided.

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
                 +------------------+       +-----------------+
                 |  AsyncEventLoop  |       | SimpleEventLoop |
                 | (using libevent) |       |    (minimal)    |
                 +------------------+       +-----------------+
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

The `Address` class encapsulates a complete network address. Internally, it stores a raw network address in a `sockaddr_storage` (designated as its `value_type`), along with a cached port number and protocol indicator (IPv4 or IPv6). Other address details (such as IPv6 flow information) are accessible by directly examining the raw structure via the `value()` member function. The textual representation of the address shall be suitable for conversion and display as a `std::wstring`.

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

- **Effects:**
  - Constructs an `Address` with a default-initialized `sockaddr_storage`. The values of `port` and `protocol` are undefined.
  - An internal validity flag is set to indicate the object is empty.
- **Complexity:** Constant.

2. **Constructor (from raw address)**

```cpp
Address(const_pointer addr) noexcept;
```

- **Effects:**
  - Constructs an `Address` object by copying the data from the memory pointed to by `addr` into the internal `address` member. The constructor then deduces the port number and protocol from the contents of the raw address structure.
  - Specifically, if `addr->ss_family` is `AF_INET`, the constructor extracts the port from the corresponding `sockaddr_in` structure and sets the protocol to `Protocol::IPv4`; if `addr->ss_family` is `AF_INET6`, it extracts the port from the corresponding `sockaddr_in6` structure and sets the protocol to `Protocol::IPv6`.
- **Preconditions:**
  - `addr` shall not be a null pointer.
  - `addr` must point to a valid socket address (either IPv4 or IPv6).
- **Complexity:** Constant.

3. **Constructor (from string representation and port number)**

```cpp
Address(std::string_view addr_str, uint16_t port);
```

- **Effects:**
  - Parses `addr_str` (e.g., `"127.0.0.1"` or `"::1"`) to initialize `address` and deduce `protocol`; caches the given port in `port`.
  - If the string cannot be parsed into a valid address, an exception is thrown (see _Exceptions_).
- **Preconditions:**
  - `addr_str` must not be empty.
- **Exceptions:**
  - `std::invalid_argument`: Thrown if the string cannot be parsed into a valid IP address.
  - `std::out_of_range`: Thrown if the port number is outside the valid range for a port (0–65535).
- **Complexity:** Linear in the length of `addr_str`.

4. **Constructor (from string representation)**

```cpp
explicit Address(std::string_view str);
```

- **Effects:**
  - Constructs an `Address` object from the provided string `str`. This string must represent the address in the exact format returned by the `to_string()` member function, which is:
    - For IPv4 addresses: `"ip:port"` (e.g., `"192.168.0.1:8080"`).
    - For IPv6 addresses: `"[ip]:port"` (e.g., `"[2001:0db8:85a3:0000:0000:8a2e:0370:7334]:8080"`).
  - The constructor will parse the string, extract the IP address and port, and initialize the internal `sockaddr_storage` object accordingly.
  - If the string cannot be parsed or doesn't match the expected format, an exception is thrown (see _Exceptions_).
- **Preconditions:**
  - `str` must not be empty.
- **Exceptions:**
  - `std::invalid_argument`: Thrown if the string cannot be parsed as a valid IP address in the correct format, or if the port number contains non-numeric characters (e.g., letters or special symbols).
  - `std::out_of_range`: Thrown if the parsed port number is outside the valid range (0–65535).
- **Complexity:** Linear in the length of `str`.

5. **Copy Constructor**

```cpp
Address(const Address& other) noexcept;
```

- **Effects:**
  - Constructs an `Address` as a copy of other, copying `address`, `port`, and `protocol`.
- **Complexity:** Constant.

6. **Move Constructor**

```cpp
Address(Address&& other) noexcept;
```

- **Effects:**
  - Constructs an `Address` by moving the members of `other`.
  - After the move `other` will be in a valid but unspecified state.
- **Complexity:** Constant.

7. **Copy Assignment Operator**

```cpp
Address& operator=(const Address& other) noexcept;
```

- **Effects:**
  - Copies `other` into `this`, updating `address`, `port` and `protocol`.
- **Complexity:** Constant.

8. **Move Assignment Operator**

```cpp
Address& operator=(Address&& other) noexcept;
```

- **Effects:**
  - Moves the members of `other` into `this`.
  - After the move `other` will be in a valid but unspecified state.
- **Complexity:** Constant.

9. **Destructor**

```cpp
~Address();
```

- **Effects:**
  - Destroys the `Address` object.
  - The destructor is trivial if `sockaddr_storage` is trivially destructible.

10. **Accessors**

> [!WARNING]
> Calling any accessors on an `Address` object while it is in its empty state (e.g., after `reset()` with no arguments) results in undefined behavior.

```cpp
const_pointer value() const noexcept;
```

- **Returns:**
  - A `const_pointer` to the raw network address obtained via `std::addressof(address)`.
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
- **Notes:**
  - Empty `Address` objects compare equal to each other, but not to any non-empty `Address` object.
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
- **Notes:**
  - Empty `Address` objects are always ordered before any non-empty `Address` objects.
- **Complexity:** Linear in the length of the string representations of `lhs` and `rhs`.

12. **Observers**

```cpp
explicit operator bool() const noexcept;
bool has_value() const noexcept;
```

- **Returns:**
  - `true` if the `Address` object contains a valid (non-empty) address; `false` if it is in the empty state.

```cpp
void reset() noexcept;
```

- **Effects:**
  - Resets the `Address` object to its empty state. After calling `reset()`, the internal `address` is undefined.

```cpp
void reset(const_pointer addr) noexcept;
```

- **Effects:**
  - Resets the `Address` object using the data pointed to by `addr`. This replaces the internal `address` with the raw socket address structure pointed to by `addr`.
  - This is semantically equivalent to assigning from a temporary `Address` constructed with `Address(addr)`, but may be more efficient.
- **Preconditions:**
  - `addr` shall not be a null pointer.
  - `addr` must point to a valid socket address (either IPv4 or IPv6).
- **Complexity:** Constant.

```cpp
value_type release() noexcept;
```

- **Effects:**
  - Releases ownership of the internal raw network address, returning the stored `address`. After this call, the `Address` object is reset to its empty state.
- **Returns:** The released raw address data.

13. **Non-Member Functions**

```cpp
friend std::ostream& operator<<(std::ostream& strm, const Address& addr);
```

- **Effects:**
  - Inserts the result of `addr.to_string()` into the output stream `strm`.
- **Returns:**

  - A reference to `strm`.

14. **Swap Support**

```cpp
void swap(Address& other) noexcept;
friend void swap(Address& lhs, Address& rhs) noexcept;
```

- **Effects:**
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
- **Parameters:**
  - `ev`: The error code to map.
- **Return Value:** A corresponding std::error_condition.

#### `socket_category()`

The function `socket_category()` provides access to the singleton instance of the `socket_category_impl`. This function ensures thread-safe access to the error category instance. Synopsis:

```cpp
const sockify::details::socket_category_impl& socket_category() noexcept;
```

#### `socket_error`

The class `socket_error` defines an exception object that is thrown on failure by the functions in the Sockify library.

1. **Synopsis:**

```cpp
class socket_error : public std::system_error
```

2. **Constructors**

```cpp
explicit socket_error(const std::string& message,
                      const std::error_code& ec);
explicit socket_error(const char* message,
                      const std::error_code& ec);
```

- **Effects:**
  - Constructs the exception object using `message` as explanation string which can later be retrieved using `what()`. `ec` is used to identify the specific reason for the failure.

3. **Copy Constructor**

```cpp
socket_error(const socket_error& other) noexcept;
```

- **Effects:**
  - Initialize the contents with those of `other`. If `*this` and `other` both have dynamic type `socket_error` then `std::strcmp(what(), other.what()) == 0`.

4. **Copy Assignment Operator**

```cpp
socket_error& operator=(const socket_error& other) noexcept;
```

- **Effects:**
  - Assigns the contents with those of `other`. If `*this` and `other` both have dynamic type `socket_error` then `std::strcmp(what(), other.what()) == 0` after assignment.
- **Returns:** `*this`

5. **Member Function**

```cpp
virtual const char* what() const noexcept override;
```

- **Returns:** The explanatory string. Pointer to an implementation-defined null-terminated string with explanatory information. The string is suitable for conversion and display as a `std::wstring`. The pointer is guaranteed to be valid at least until the exception object from which it is obtained is destroyed, or until a non-const member function (e.g. copy assignment operator) on the exception object is called.

---

### `Buffer`

The `Buffer` class is an alias for `std::vector<std::byte>`, providing a dynamic, resizable buffer specifically designed to store raw binary data.

#### Synopsis

```cpp
using Buffer = std::vector<std::byte>;
```

---

### `Socket`

The `Socket` class defines a unified, protocol-agnostic interface for synchronous sockets. It encapsulates the basic lifecycle, option management and read/write interfaces, and is intended to be inherited by concrete socket types such as `TCPSocket` and `UDPSocket`.

#### Member Types

| Name                 | Explanation                                                                               |
| -------------------- | ----------------------------------------------------------------------------------------- |
| `duration`           | Type representing durations for timeouts (`std::chrono::milliseconds` on most platforms). |
| `buffer_type`        | Type representing buffers used for socket I/O operations (always `Buffer`).               |
| `address_type`       | Type representing socket addresses (always `Address`).                                    |
| `native_handle_type` | Platform-specific socket handle.                                                          |

#### Data Members

| Name            | Type                      | Explanation                                                  |
| --------------- | ------------------------- | ------------------------------------------------------------ |
| `blocking`      | `bool`                    | Whether the socket is in blocking mode.                      |
| `inheritable`   | `bool`                    | Whether the socket handle is inheritable by child processes. |
| `native_handle` | `native_handle_type`      | Platform-specific socket handle.                             |
| `timeout`       | `std::optional<duration>` | The timeout duration for socket operations.                  |

#### Member Functions

1. **Destructor**

```cpp
virtual ~Socket();
```

- **Effects:**
  - Destroys the `Socket` object, releasing any associated resources. This is a virtual destructor to ensure proper cleanup when derived classes are destroyed.

2. **Operations**

```cpp
virtual void bind(const address_type& address) = 0;
```

- **Effects:**
  - Binds the socket to the specified local address. Required before calling `listen()` on server sockets.
- **Parameters:**
  - `address`: the local endpoint
- **Exceptions:**
  - `socket_error` on failure, such as if the address is already in use or the socket is invalid.
- **Complexity:** Constant to linear, depending on internal address resolution and validation.

```cpp
virtual void connect(const address_type& address) = 0;
```

- **Effects:**
  - Establishes a connection to the specified remote address.
  - If the connection is interrupted by a signal, the method waits until the connection completes, or raises an exception on timeout (see _Exceptions_), if the signal handler doesn't raise an exception and the socket is blocking or has a timeout. For non-blocking sockets, the method raises an exception if the connection is interrupted by a signal (or the exception raised by the signal handler) (see _Exceptions_).
- **Parameters:**
  - `address`: A valid remote socket address.
- **Exceptions:**
  - `socket_error` on failure like timeout, unreachable host or permission denied.
- **Complexity:** Potentially unbounded (depends on network latency and TCP handshake).

```cpp
virtual void listen(int backlog = SOMAXCONN);
```

- **Effects:**
  - Marks the socket as passive, used to accept incoming connection requests.
- **Parameters:**
  - `backlog`: Maximum number of pending connections; defaults to `SOMAXCONN`. If this parameter is specified, it must be at least `0` (if it is lower, it is set to `0`).
- **Exceptions:**
  - `socket_error` on failure, such as if the socket is invalid or was not bound.
- **Complexity:** Constant

```cpp
virtual std::unique_ptr<Socket> accept();
```

- **Effects:**
  - Accepts a pending connection and returns a new `Socket` instance for the connection.
- **Exceptions:**
  - `socket_error` on failure, such as if the socket is not in listening mode or a network error occurs.
- **Complexity:** Blocking or constant, depending on mode; may block if no pending connection exists.

```cpp
virtual void close() noexcept = 0;
```

- **Effects:**
  - Closes the socket, invalidating its handle and releasing all associated resources.
- **Complexity:** Constant

```cpp
virtual native_handle_type detach() = 0;
```

- **Effects:**
  - Releases ownership of the underlying socket handle and returns it to the caller.
- **Exceptions:**
  - `socket_error` if the handle is already detached or invalid.
- **Complexity:** Constant

```cpp
virtual void setblocking(bool would_block);
```

- **Effects:**
  - Enables or disables blocking mode on the socket.
- **Parameters:**
  - `would_block`: `true` for blocking mode; `false` for non-blocking.
- **Notes:**
  - This method is a shorthand for certain `settimeout()` calls:
    - `setblocking(true)` is equivalent to `settimeout(duration{-1})`
    - `setblocking(false)` is equivalent to `settimeout(duration{0})`
- **Exceptions:**
  - `socket_error` if the operation is unsupported on the socket type or fails at the OS level.
- **Complexity:** Constant

```cpp
virtual bool getblocking() const noexcept;
```

- **Returns:**
  - `true` if the socket is in blocking mode, otherwise `false`.
- **Notes:** This is equivalent to checking `gettimeout() != 0`.
- **Complexity:** Constant

```cpp
virtual void settimeout(duration timeout) noexcept;
```

- **Effects:**
  - Sets the timeout value for blocking socket operations like `recv()` or `send()`.
- **Parameters:**
  - `timeout`: The maximum time to block.
- **Notes:**
  - The `timeout` value can be a non-negative integer in `duration`, or a negative value indicating blocking behavior. Specifically:
    - If a non-zero positive value is provided, subsequent socket operations will raise exceptions if the operation isn't completed within the specified timeout period.
    - If `duration{0}` is provided, the socket is set to non-blocking mode, meaning operations will not block and will immediately return if they cannot complete.
    - If a negative value is provided, the socket is set to blocking mode, meaning operations will block indefinitely until completion.
- **Complexity:** Constant

```cpp
virtual std::optional<duration> gettimeout() const noexcept;
```

- **Returns:**
  - The current timeout for blocking operations, or `std::nullopt` if no timeout is set.

```cpp
virtual void setsockopt(int level, int optname, int value);
```

- **Effects:**
  - Sets the value of a socket option.
- **Parameters:**
  - `level`: Protocol level (e.g., `SOL_SOCKET`).
  - `optname`: Option name.
  - `value`: Value to set.
- **Exceptions:**
  - `socket_error` if the option is unsupported or the socket is invalid.
- **Complexity:** Constant

```cpp
virtual int getsockopt(int level, int optname) const;
```

- **Returns:**
  - The value of the requested socket option.
- **Exceptions:**
  - `socket_error` on error
- **Complexity:** Constant

```cpp
virtual void setinheritable(bool inheritable);
```

- **Effects:**
  - Configures whether the socket handle is inherited by child processes on `fork()`/`exec()`.
- **Parameters:**
  - `inheritable`: `true` to allow inheritance; `false` to prevent it.
- **Exceptions:**
  - `socket_error` if the operation fails due to platform restrictions.
- **Complexity:** Constant

```cpp
virtual bool getinheritable() const noexcept;
```

- **Returns:**
  - `true` if the socket is inheritable by child processes, `false` otherwise.
- **Complexity:** Constant

```cpp
virtual address_type getsockname() const = 0;
```

- **Returns:**
  - The local address to which the socket is bound.
- **Exceptions:**
  - `socket_error` if the socket is not bound or invalid.
- **Complexity:** Constant

```cpp
virtual address_type getpeername() const = 0;
```

- **Returns:**
  - The address of the remote peer to which the socket is connected.
- **Exceptions:**
  - `socket_error` if the socket is not connected.
- **Complexity:** Constant

```cpp
virtual std::size_t send(const buffer_type& buf, int flags = 0);
```

- **Effects:**
  - Sends data from `buf` to the connected peer. `flags` modifies behavior (e.g., `MSG_DONTWAIT`).
- **Returns:**
  - The number of bytes sent.
- **Exceptions:**
  - `socket_error` if the socket is not connected or sending fails.
- **Complexity:** Linear in the size of `buf`, but depends on system buffer state and network I/O.

```cpp
virtual std::size_t sendto(const buffer_type& buf, const address_type& dest, int flags = 0);
```

- **Effects:**
  - Sends the data to the specified destination address (used primarily for datagram sockets).
- **Returns:**
  - The number of bytes sent.
- **Exceptions:**
  - `socket_error` on failure, such as if the socket is connected.
- **Complexity:** Linear in the size of `buf`.

```cpp
virtual std::size_t sendall(const buffer_type& buf, int flags = 0);
```

- **Effects:**
  - Sends all data in `buf`, retrying if needed until all bytes are sent or an error occurs.
- **Returns:**
  - Total number of bytes sent.
- **Exceptions:**
  - `socket_error` if the socket is not connected. Will not throw if an error occurs, check the return value instead.
- **Complexity:** Linear in the size of `buf`, possibly higher due to retries.

```cpp
virtual std::size_t sendfile(std::ifstream& file, std::streampos offset = 0, std::size_t count = 0);
```

- **Effects**:
  - Sends data from the provided file over the socket. The file must be a regular file object opened in binary mode.
- **Parameters:**
  - `offset` specifies the position in the file from which to start reading. If not provided, it defaults to `0`.
  - `count` specifies the total number of bytes to send. If not provided, the function sends the file until EOF is reached.
- **Returns:** The total number of bytes sent.
- **Exceptions**:
  - `std::invalid_argument` if the file is not open or the socket is not of `SOCK_STREAM` type.
  - `std::ios_base::failure` if an error occurs while reading from the file.
  - `socket_error` depending on the underlying socket implementation.
- **Notes:**:
  - This function is designed to send the entire content of the file (or a portion, if `count` is provided) in chunks over the socket.
  - Non-blocking sockets are not supported for this operation.

```cpp
virtual buffer_type recv(std::size_t count, int flags = 0);
```

- **Effects:**
  - Receives data from the connected peer, reading up to `count` bytes.
  - The function blocks until at least some data is received or the connection is closed, unless the socket is non-blocking or a timeout is set.
- **Parameters:**
  - `count`: The maximum number of bytes to be received from the socket.
  - `flags`: Optional flags that modify the behavior of the call.
- **Returns:**
  - A buffer containing received data.
- **Exceptions:**
  - `socket_error` on failure, such as if the socket is not connected.
- **Complexity:** Linear in the size of received data.

```cpp
virtual buffer_type recvfrom(std::size_t count, address_type& src, int flags = 0);
```

- **Effects:**
  - Receives data from the socket up to a maximum of `count` bytes.
  - The function blocks until some data is received, the specified number of bytes is read, or the connection is closed, unless the socket is configured for non-blocking operation or a timeout is set.
  - Fills the `src` parameter with the sender's address if possible.
- **Parameters:**
  - `count`: The maximum number of bytes to be received.
  - `src`: Sender's address information.
  - `flags`: Optional flags to modify the behavior of the call.
- **Returns:**
  - A buffer containing received data.
- **Exceptions:**
  - `socket_error` on failure, such as a network failure, an interrupted system call, or if the socket is invalid.
- **Complexity:** Linear in the size of received data.

```cpp
virtual void shutdown(int how) = 0;
```

- **Effects:**
  - Shuts down the socket's communication in the specified direction (`how` can be `SHUT_RD`, `SHUT_WR`, or `SHUT_RDWR`).
- **Exceptions:**
  - `socket_error` on failure, such as invalid `how`.
- **Complexity:** Constant.

```cpp
virtual native_handle_type native_handle() const noexcept;
```

- **Returns:**
  - The platform-specific socket handle.

3. **Swap Support**

```cpp
void swap(Socket& other) noexcept;
friend void swap(Socket& lhs, Socket& rhs) noexcept;
```

- **Effects:**
  - Swaps the internal state of two `Socket` objects. After the swap, the internal state of `lhs` and `rhs` is exchanged.
  - Specifically, the derived class should implement this function to swap any internal resources.
- **Complexity:** Constant.

---

### `SocketEvent`

The `SocketEvent` enumeration represents the types of I/O events that can occur on a socket. It is defined as a bitmask type so that multiple event flags may be combined.

#### Synopsis

```cpp
enum class SocketEvent : uint32_t {
  None     = 0,                 // No event.
  Readable = /* unspecified */, // The socket is ready for reading.
  Writable = /* unspecified */, // The socket is ready for writing.
  Error    = /* unspecified */, // An error condition has been detected.
  Timeout  = /* unspecified */  // A timeout has occurred.
};
```

#### Requirements

`SocketEvent` satisfies the requirements of [_BitmaskType_](https://eel.is/c++draft/bitmask.types) (which means the bitwise operators `operator&`, `operator|`, `operator^`, `operator~`, `operator&=`, `operator|=`, and `operator^=` are defined for this type).

---

### `EventLoop`

The `EventLoop` class provides an abstraction for I/O management within sockify. It is responsible for registering sockets for I/O events, dispatching callbacks when these events occur, and controlling the polling mechanism. Derived classes must implement its pure virtual functions. The event loop may be used by the `Socket` hierarchy to integrate different I/O behaviors.

#### Member Types

| Name           | Explanation                                                                                                                                              |
| -------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `socket_type`  | An alias for the socket type managed by the event loop. Defined as `Socket`, which is the base type for all Sockify sockets.                             |
| `event_type`   | An alias for `SocketEvent`, which represents the types of I/O events that can occur on a socket.                                                         |
| `handler_type` | A callable type defined as `std::function<void(socket_type&, event_type)>`. This callback is invoked when a registered socket is signaled with an event. |
| `duration`     | An alias for the duration type used for timeouts, typically defined as `std::chrono::milliseconds` on most platforms.                                    |

#### Member Objects (Protected)

| Name             | Type                                             | Explanation                                                                            |
| ---------------- | ------------------------------------------------ | -------------------------------------------------------------------------------------- |
| `event_handlers` | `std::unordered_map<socket_type*, handler_type>` | A mapping from registered socket pointers to their associated event handler callbacks. |
| `poll_timeout`   | `duration`                                       | The maximum duration for which the event loop blocks while waiting for events.         |

#### Member Functions

1. **Destructor**

```cpp
virtual ~EventLoop();
```

- **Effects:**
  - Destroys the event loop object, releasing all associated resources.
- **Complexity:** Constant.

2. **Interfaces**

```cpp
virtual bool register_socket(socket_type& socket, handler_type handler) = 0;
```

- **Effects:**
  - Registers the specified socket with the event loop, associating it with the provided handler.
  - Upon detection of an I/O event (readable, writable, error or timeout), the event loop shall invoke the handler, passing the socket and the appropriate combination of `event_type` flags.
- **Parameters:**
  - `socket`: A reference to a `Socket` object to be monitored.
  - `handler`: A callable object of type `handler_type` to be invoked when an event occurs.
- **Preconditions:**
  - `socket` shall be valid.
- **Returns:**
  - `true` if registration succeeds; otherwise, `false`.
- **Exceptions**:
  - `socket_error` if an unrecoverable error occurs during registration.
- **Complexity:** Amortized constant time.

```cpp
virtual bool unregister_socket(socket_type& socket) noexcept = 0;
```

- **Effects:**
  - Unregisters the specified socket from the event loop so that no further events are dispatched for it.
- **Parameters:**
  - `socket`: A reference to the socket to be removed.
- **Returns:**
  - `true` if the socket was successfully unregistered; otherwise, `false`.
- **Complexity:** Amortized constant time.

```cpp
virtual void run() = 0;
```

- **Effects:**
  - Enters the event loop, continuously monitoring for I/O events and dispatching the corresponding event handlers. The loop continues until `stop()` is invoked or a fatal error occurs.
- **Exceptions:**
  - `socket_error` if an unrecoverable error occurs during event processing.
- **Complexity:** Dependent on the underlying polling mechanism and the number of events processed.

```cpp
virtual void stop() noexcept = 0;
```

- **Effects:**
  - Signals the event loop to terminate, causing any ongoing `run()` invocation to return as soon as possible.
- **Complexity:** Constant.

3. **Member Functions**

```cpp
duration poll_timeout() const noexcept;
```

- **Effects:**
  - Returns the current timeout value that the event loop is using for polling.
- **Returns:**
  - The current poll timeout value. If none was previously set, returns the default duration.
- **Complexity:** Constant.

```cpp
duration poll_timeout(duration timeout) noexcept;
```

- **Effects:**
  - Sets the poll timeout duration, which determines how long the event loop blocks while waiting for I/O events.
- **Parameters:**
  - `timeout`: The new timeout duration.
- **Returns:**
  - The previous poll timeout value. If none was previously set, returns the default duration.
- **Complexity:** Constant.

4. **Swap Support**

```cpp
void swap(EventLoop& other) noexcept;
friend void swap(EventLoop& lhs, EventLoop& rhs) noexcept;
```

- **Effects:**
  - Swaps the internal state of two `EventLoop` objects. After the swap, the internal state (such as event handlers, associated resources, etc.) of `lhs` and `rhs` is exchanged.
  - Specifically, the derived class should implement this function to swap any internal resources such as event bases or associated handlers.
- **Complexity:** Constant.

#### Non-Member Function

1. `make_default_event_loop`

```cpp
std::unique_ptr<EventLoop> make_default_event_loop() noexcept;
```

- **Effects:**
  - Attempts to create and return a default instance of an `EventLoop` appropriate for the current platform. The returned instance will be dynamically allocated and managed by a `std::unique_ptr<EventLoop>`.
- **Returns:**
  - A `std::unique_ptr<EventLoop>` managing the newly created event loop instance.
  - If the event loop cannot be created (e.g., due to resource exhaustion), returns an empty `std::unique_ptr<EventLoop>`.
- **Notes:**
  - The caller must check whether the returned pointer is valid before using it (e.g., via `operator bool`).
- **Complexity:** Constant.

---

### `AsyncEventLoop`

The `AsyncEventLoop` class implements the asynchronous event loop using libevent. It overrides all pure virtual functions from the base `EventLoop` interface. This type is movable only; copy construction and copy assignment are deleted. It manages a libevent event base internally, releasing that resource upon destruction.

#### Synopsis

```cpp
class AsyncEventLoop : public EventLoop
```

#### New Member Types

| Name           | Explanation                              |
| -------------- | ---------------------------------------- |
| `base_type`    | An alias for libevent's event base type. |
| `base_pointer` | `base_type*`                             |

#### New Member Objects

| Name   | Type           | Explanation                                                                                                                               |
| ------ | -------------- | ----------------------------------------------------------------------------------------------------------------------------------------- |
| `base` | `base_pointer` | A pointer to the libevent event base object. This resource is acquired in the default constructor and must be released in the destructor. |

#### Additional Member Functions

1. **Default Constructor**

```cpp
AsyncEventLoop();
```

- **Effects:**
  - Acquires a new libevent event base (stored in `base`) and initializes libevent-specific state.
- **Exceptions:**
  - `socket_error` if the libevent event base cannot be created.
- **Complexity:** Constant.

2. **Move Constructor**

```cpp
AsyncEventLoop(AsyncEventLoop&& other) noexcept;
```

- **Effects:**
  - Transfers ownership of `other.base` and any associated state from `other` to `*this`.
  - After the move, `other` is in a valid but unspecified state.
- **Complexity:** Constant.

3. **Move Assignment Operator**

```cpp
AsyncEventLoop& operator=(AsyncEventLoop&& other) noexcept;
```

- **Effects:**
  - Releases any current libevent event base owned by `*this` and transfers ownership of `other.base` and state from other to `*this`. After the move, `other` is left in a valid but unspecified state.
- **Returns:** A reference to `*this`.
- **Complexity:** Constant.

4. **Copy Constructor and Copy Assignment Operator**

```cpp
AsyncEventLoop(const AsyncEventLoop&) = delete;
AsyncEventLoop& operator=(const AsyncEventLoop&) = delete;
```

- **Effects:**
  - Copying of `AsyncEventLoop` objects is disabled to ensure exclusive ownership of libevent resources.

5. **Destructor**

```cpp
virtual ~AsyncEventLoop() noexcept override;
```

- **Effects:**
  - Releases the libevent event base (by calling the appropriate libevent deallocation function) and cleans up any associated resources.
- **Complexity:** Constant.

6. **Swap Support**

```cpp
void swap(AsyncEventLoop& other) noexcept;
friend void swap(AsyncEventLoop& lhs, AsyncEventLoop& rhs) noexcept;
```

- **Effects:**
  - Swaps the internal state of two `AsyncEventLoop` objects. Specifically, it swaps the internal resources such as the underlying libevent base or other associated handlers between `lhs` and `rhs`.
  - After the swap, both `lhs` and `rhs` will be in a valid, usable state. The event loop base and other resources are exchanged.
- **Complexity:** Constant.

---

### `SimpleEventLoop`

The `SimpleEventLoop` class is a minimal, lightweight event loop with fewer dependencies and less complexity than `AsyncEventLoop`. Unlike `AsyncEventLoop`, which relies on an external library like _libevent_, `SimpleEventLoop` is implemented with basic system calls (such as `select()` or `poll()`) to provide fundamental event loop functionality.

#### Synopsis

```cpp
class SimpleEventLoop : public EventLoop
```

#### Additional Member Functions

1. **Default Constructor**

```cpp
SimpleEventLoop() noexcept;
```

- **Effects:**
  - Initializes a basic `SimpleEventLoop` with an empty `socket_handlers` map and sets a default poll timeout (e.g., no timeout or a reasonable default).
- **Complexity:** Constant.

2. **Move Constructor**

```cpp
SimpleEventLoop(SimpleEventLoop&& other) noexcept;
```

- **Effects:**
  - Transfers ownership of resources from `other` to `*this`. After the move, `other` is left in a valid but unspecified state.
- **Complexity:** Constant.

3. **Move Assignment Operator**

```cpp
SimpleEventLoop& operator=(SimpleEventLoop&& other) noexcept;
```

- **Effects:**
  - Releases any current resources held by `*this` and transfers ownership of resources from `other` to `*this`. After the move, `other` is left in a valid but unspecified state.
- **Returns:** A reference to `*this`.
- **Complexity:** Constant.

4. **Copy Constructor and Copy Assignment Operator**

```cpp
SimpleEventLoop(const SimpleEventLoop&) = delete;
SimpleEventLoop& operator=(const SimpleEventLoop&) = delete;
```

- **Effects:**
  - Copy operations are deleted to prevent shallow copying of the event loop, which could lead to resource management issues.

5. **Destructor**

```cpp
virtual ~SimpleEventLoop() noexcept override;
```

- **Effects:**
  - Cleans up any resources associated with the event loop (e.g., unregistering any active sockets).
- **Complexity:** Constant.

6. **Swap Support**

```cpp
void swap(SimpleEventLoop& other) noexcept;
friend void swap(SimpleEventLoop& lhs, SimpleEventLoop& rhs) noexcept;
```

- **Effects:**
  - Swaps the internal state of two `SimpleEventLoop` objects. Specifically, it swaps the internal resources such as the associated handlers between `lhs` and `rhs`.
  - After the swap, both `lhs` and `rhs` will be in a valid, usable state.
- **Complexity:** Constant.
