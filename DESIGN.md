# Sockify Design Document

## Overview

**Sockify** is a modern, cross-platform C++17 socket library aimed at delivering a versatile and efficient API for network communication.

### Features

Sockify provides a uniform interface for network programming on all major platforms, abstracting away platform-dependent socket operations and is written to work well with the C++ standard library. For example, sockets can be treated as regular I/O streams. This allows applications to read from and write to network sockets through well-known `std::istream` and `std::ostream` interfaces.

All of Sockify's advanced features are optional. Secure connections are provided via integration with [_OpenSSL_](https://github.com/openssl/openssl) or [_MbedTLS_](https://github.com/Mbed-TLS/mbedtls), if it is enabled at compile time. If [_libevent_](https://github.com/libevent/libevent) is present, Sockify also supports an event-based model of I/O, with support for multiplexing across multiple sockets with little overhead. Without _libevent_, a simpler minimal default is provided.

The library is built to be extensible, so it can support other transport protocols in the future.

### Design Choices

As you program with Sockify, keep in mind the following rules to help you get best performance, maintainability and usability:

- Use modern C++ features such as `std::unique_ptr` and `std::shared_ptr` to manage resources. Use RAII patterns wherever feasible so that resources are cleaned up properly.
- Sockify is designed to be low overhead by default. SSL/TLS and event-driven I/O are add-on features that only need to be enabled if necessary. If your app doesn't need SSL, for instance, simply don't enable it at compile time to keep the library lean and fast.
- Sockify does **not** implement thread-safe sockets by default. This is to avoid the performance overhead of locking mechanisms. If your application requires concurrency, use message-passing, worker threads and/or external synchronization mechanisms. For multi-threaded applications, ensure that each socket instance is only used within a single thread unless explicitly stated otherwise.
- Sockify supports `std::error_code` for non-fatal errors, allowing you to handle failures without relying on exceptions. If you prefer not to use exception handling, simply call the overloads that accept a `std::error_code&` parameter.
- Sockify abstracts platform-specific details, but do remember to bear in mind platform-specific optimizations or limitations. For instance, Windows does its own socket management differently from Unix-based systems.

## Primary Library Components

```console
                         +-------------------------+
                         |     sockify::Socket     |  <-- Abstract base class
                         | (common socket methods) |  <-- for all sockets
                         +-------------------------+
                                      ^
                                      |
                   +------------------+--------------------+            <-- Additional protocols can be
                   |                  |                    |            <-- defined (and some are already
                   |                  |                    |            <-- included in the library) by
                   |                  |                    |            <-- deriving from sockify::Socket
        +----------------------+      |       +------------------------+
        |     StreamSocket     |      |       |     DatagramSocket     |
        | (stream socket impl) |      |       | (datagram socket impl) |
        +----------------------+      |       +------------------------+
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

## Interface Reference

> [!IMPORTANT]
> For simplicity and brevity, the `sockify::` namespace is omitted from all definitions in the sections that follow. Unless otherwise specified, assume that all names belong to this namespace. Additionally, this document is intended as a design reference and is **NOT** a comprehensive manual. For the complete API reference, please refer to the generated Doxygen documentation.

> [!CAUTION]
> When invoking `swap(Type& other) noexcept` or the non-member `friend void swap(Type& lhs, Type& rhs) noexcept` in this library, it is the caller's responsibility to ensure that both objects are of the same dynamic type. **If the objects have mismatched dynamic types, the behavior is undefined.** This precondition is enforced (in debug builds) via an assertion rather than by throwing an exception, in order to maintain the `noexcept` guarantee of the swap operation. **DO NOT OVERLOOK THIS REQUIREMENT**, as swapping objects of different types may result in data corruption, resource mismanagement or undefined behavior.

### `AddressFamily`

The `AddressFamily` scoped enumeration represents the family (or domain) of a network address. It is used to control the interpretation of socket and address types when performing network operations.

#### Synopsis

```cpp
enum class AddressFamily {
  Unknown = /* unspecified */,
  IPv4    = /* unspecified */,
  IPv6    = /* unspecified */,
  Unix    = /* unspecified */
  ...
};
```

#### Constants

| Enumerator | Meaning                                                             |
| ---------- | ------------------------------------------------------------------- |
| `Unknown`  | The address family is unknown or uninitialized. May indicate error. |
| `IPv4`     | Internet Protocol version 4 address (corresponds to `AF_INET`).     |
| `IPv6`     | Internet Protocol version 6 address (corresponds to `AF_INET6`).    |
| `Unix`     | Unix domain socket address (corresponds to `AF_UNIX`).              |

#### Notes

The set of enumerators may be extended in future versions to include additional address families (e.g., Bluetooth). Existing enumerators are stable and portable across supported platforms.

---

### `Address`

The `Address` class encapsulates a protocol-agnostic network address. It serves as a common base for address families such as IPv4, IPv6 and Unix domain sockets. Concrete derived classes implement address-specific behavior while exposing a unified interface for networking operations. Internally, all address data is stored using `sockaddr_storage`, aliased as its `value_type`. All constructors are protected. This class is abstract and not intended to be instantiated directly.

#### Member Types

| Name                  | Explanation                                                                                       |
| --------------------- | ------------------------------------------------------------------------------------------------- |
| `value_type`          | An alias for the raw network address type. (always `sockaddr_storage`)                            |
| `reference`           | `value_type&`                                                                                     |
| `const_reference`     | `const value_type&`                                                                               |
| `pointer`             | `value_type*`                                                                                     |
| `const_pointer`       | `const value_type*`                                                                               |
| `string_type`         | An alias for the type used to hold textual representations of the address. (always `std::string`) |
| `address_family_type` | An alias for the scoped enumeration `AddressFamily`.                                              |

#### Data Members

| Name             | Type                  | Explanation                                         |
| ---------------- | --------------------- | --------------------------------------------------- |
| `address`        | `value_type`          | The raw network address in a system-defined format. |
| `address_family` | `address_family_type` | The family (or domain) of the address.              |

#### Member Functions

1. **Constructors (Protected)**

```cpp
Address() noexcept;
```

- **Effects:**
  - Constructs an empty `Address` object. The internal address and address family are invalidated. `has_value()` returns `false`.
- **Complexity:** Constant.

```cpp
Address(const_reference address) noexcept;
```

- **Effects:**
  - Constructs an `Address` object using the specified raw address `address`.
  - The address family is automatically deduced from `storage.ss_family` and stored in the cached `family` member.
  - If the family cannot be recognized, the result is an empty address.
- **Notes:**
  - This constructor is intended solely for use by derived classes to initialize the base class state.
  - The deduced address family must match one of the recognized `AddressFamily` values.
- **Complexity:** Constant.

2. **Copy Constructor (Protected)**

```cpp
Address(const Address& other) noexcept;
```

- **Effects:**
  - Constructs an `Address` object as a copy of `other`, copying the internal raw storage and cached address family.
- **Complexity:** Constant.

3. **Copy Assignment Operator (Protected)**

```cpp
Address& operator=(const Address& other) noexcept;
```

- **Effects:**
  - Assigns other to `*this`, copying the internal address and address family.
- **Returns:**
  - `*this`.
- **Complexity:** Constant.

4. **Move Constructor (Protected)**

```cpp
Address(Address&& other) noexcept;
```

- **Effects:**
  - Constructs an `Address` object by moving the contents of `other`. After the move, `other` is in a valid but unspecified state.
- **Complexity:** Constant.

5. **Move Assignment Operator (Protected)**

```cpp
Address& operator=(Address&& other) noexcept;
```

- **Effects:**
  - Moves the internal state of `other` into `*this`. After the move, `other` is in a valid but unspecified state.
- **Returns:**
  - `*this`.
- **Complexity:** Constant.

6. **Destructor**

```cpp
virtual ~Address();
```

- **Effects:**
  - Destroys the `Address` object. The destructor is virtual to ensure proper cleanup of derived classes.
  - The destructor is trivial if `sockaddr_storage` is trivially destructible.
- **Complexity:** Constant.

7. **Accessors**

> [!WARNING]
> Calling any accessors on an `Address` (or any of its derived classes) object while it is in its empty state (e.g., after `reset()` with no arguments) results in undefined behavior.

```cpp
const_pointer value() const noexcept;
```

- **Returns:**
  - A `const_pointer` to the raw network address obtained via `std::addressof(address)`.
- **Complexity:** Constant.

```cpp
address_family_type family() const noexcept;
```

- **Returns:**
  - The address family of the current address (e.g., `AddressFamily::IPv4`).
- **Complexity:** Constant.

```cpp
virtual socklen_t size() const noexcept = 0;
```

- **Returns:**
  - The number of bytes actually occupied by the stored socket address structure.
- **Complexity:** Constant.

```cpp
virtual string_type to_string() const = 0;
```

- **Returns:**
  - A textual representation of the address. The textual representation of the address shall be suitable for conversion and display as a `std::wstring`.
- **Complexity:** Implementation-specific.

8. **Observers**

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

9. **Comparison Operators**

```cpp
bool operator==(const Address& other) const noexcept;
```

- **Returns:**
  - `true` if both `Address` objects have the same address family and identical contents over `size()` bytes of their internal storage; otherwise, `false`.
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
  - Establishes a strict total order over all `Address` objects by applying the following rules:
    - If the two addresses have the same family, the result is delegated to `compare()`.
    - Otherwise, the result is determined by comparing their `family` enumerator values.
- **Returns:**
  - `true` or `false` based on the result of the comparison.
- **Notes:**
  - Empty `Address` objects are always ordered before any non-empty `Address` objects.
  - This ordering ensures that every pair of valid `Address` objects is comparable, and the result is stable and deterministic.
  - Derived classes may override the comparison mechanism by redefining the `compare()` function (see below) if they require additional ordering logic.
- **Complexity:** Linear in the number of bytes compared.

10. **Comparison Operators (Protected)**

```cpp
virtual int compare(const Address& other) const noexcept;
```

- **Effects:**
  - Compares `*this` with `other`, assuming both objects have the same address family.
- **Returns:**
  - A signed result with the following meaning:
    - Negative if `*this < other`,
    - Zero if `*this == other`,
    - Positive if `*this > other`.
- **Complexity:** Linear in the number of bytes compared.
- **Notes:**
  - The default implementation compares `size()` first, then performs a byte-wise comparison of `value()` over that size.
  - This function must not be called unless `family() == other.family()`.
  - Derived classes may override this function to provide additional semantic ordering.

11. **Non-Member Functions**

```cpp
friend std::ostream& operator<<(std::ostream& strm, const Address& addr);
```

- **Effects:**
  - Inserts the result of `addr.to_string()` into the output stream `strm`.
- **Returns:**
  - A reference to `strm`.

12. **Swap Support (Protected)**

```cpp
virtual void do_swap(Address& other) noexcept = 0;
```

- **Effects:**
  - Performs swapping of derived-class–specific internal state.
  - Must be overridden by each concrete derived class to ensure that all additional members are exchanged correctly.
- **Complexity:** Constant.

13. **Swap Support**

```cpp
void swap(Address& other) noexcept;
friend void swap(Address& lhs, Address& rhs) noexcept;
```

- **Effects:**
  - Swaps the internal state of the base `Address` subobject (including `address` and `family`) as well as any additional state managed by the most-derived class.
  - Calls the protected pure virtual function `do_swap(Address& other)` to swap the derived class's additional state.
- **Preconditions:**
  - Both `*this` and `other` must be of the same dynamic type. Otherwise, the behavior is undefined.
- **Complexity:** Constant.

---

### `IPAddress`

The `IPAddress` class represents a network address specifically for IP-based addresses, such as IPv4 and IPv6. It extends the `Address` base class to handle the specific behaviors related to IP addresses, including parsing, formatting and managing the IP address itself, as well as providing functionality for associated ports.

This class can be instantiated with either an IP address string (with or without a port) or directly from the raw address data. It provides the necessary functions to extract and manipulate the IP address and port, with support for IPv4 and IPv6 address families.

#### Synopsis

```cpp
class IPAddress final : public Address {
public:
  // Constructors
  IPAddress() noexcept;
  IPAddress(const_reference address) noexcept;
  explicit IPAddress(std::string_view ip_address);
  IPAddress(std::string_view ip_address, uint16_t port);

  // Copy Constructor and Copy Assignment Operator
  IPAddress(const IPAddress& other) noexcept;
  IPAddress& operator=(const IPAddress& other) noexcept;

  // Move Constructor and Move Assignment Operator
  IPAddress(IPAddress&& other) noexcept;
  IPAddress& operator=(IPAddress&& other) noexcept;

  // Destructor
  ~IPAddress() override;

  // Address/Port Accessors
  string_type ip() const;
  uint16_t port() const noexcept;

  // Override virtual functions
  socklen_t size() const noexcept override;
  string_type to_string() const override;

protected:
  void do_swap(Address& other) noexcept override;

private:
  uint16_t port; // Cached port number
};
```

#### Data Members

| Name   | Type       | Explanation                                            |
| ------ | ---------- | ------------------------------------------------------ |
| `port` | `uint16_t` | The cached port number associated with the IP address. |

#### Member Functions

1. **Default Constructor**

```cpp
IPAddress() noexcept;
```

- **Effects:**
  - Constructs an empty `IPAddress` object.
- **Complexity:** Constant.

2. **Constructor (from raw address)**

```cpp
IPAddress(const_reference address) noexcept;
```

- **Effects:**
  - Constructs an `IPAddress` object by copying the data from `addr` into the internal `address` member. The constructor then deduces the port number and address family from the contents of the raw address structure.
  - Specifically, if `addr->ss_family` is `AF_INET`, the constructor extracts the port from the corresponding `sockaddr_in` structure and sets the address family to `AddressFamily::IPv4`; if `addr->ss_family` is `AF_INET6`, it extracts the port from the corresponding `sockaddr_in6` structure and sets the protocol to `AddressFamily::IPv6`.
  - If the family cannot be recognized, the result is an empty address.
- **Preconditions:**
  - `addr` shall not be a null pointer.
  - `addr` must point to a valid socket address.
- **Complexity:** Constant.

3. **Constructor (from string representation and port number)**

```cpp
IPAddress(std::string_view ip_address, uint16_t port);
```

- **Effects:**
  - If `ip_address` represents an IP address in string form (e.g., `"127.0.0.1"` or `"::1"`), it will initialize the object with that address.
  - If `ip_address` is a hostname (e.g., `"localhost"`), it will resolve the hostname to an IP address using DNS and initialize the object with the resolved IP.
  - The port number is parsed from `port` and associated with the address.
  - If the string cannot be parsed into a valid address or the DNS resolution fails, an exception is thrown (see _Exceptions_).
- **Preconditions:**
  - `ip_address` must not be empty.
- **Exceptions:**
  - `std::invalid_argument`: Thrown if the string cannot be parsed into a valid IP address or if the hostname cannot be resolved.
  - `std::out_of_range`: Thrown if the port number is outside the valid range for a port (0–65535).
- **Notes:**
  - If a hostname is used to specify the IPv4/IPv6 address, the program may exhibit nondeterministic behavior. This is due to how the operating system or the library resolves the hostname into an IP address, which may vary based on DNS resolution results and host configurations.
  - For deterministic behavior, it is recommended to use a numeric IP address instead of a hostname.
- **Complexity:** Linear in the length of `ip_address`.

4. **Constructor (from string representation)**

```cpp
explicit IPAddress(std::string_view ip_address);
```

- **Effects:**
  - Constructs an `Address` object from the provided string `ip_address`. This string must represent the address in the exact format returned by the `to_string()` member function:
    - For IPv4 addresses: `"ip:port"` (e.g., `"192.168.0.1:8080"`).
    - For IPv6 addresses: `"[ip]:port"` (e.g., `"[2001:0db8:85a3:0000:0000:8a2e:0370:7334]:8080"`).
  - If the string represents an IP address (e.g., `"127.0.0.1"` or `"::1"`), the constructor will parse the string, extract the IP address and port, and initialize the internal `sockaddr_storage` object accordingly.
  - If the string represents a hostname (e.g., `"localhost"`), the constructor will resolve the hostname using DNS and then initialize the object with the resolved IP address (using the first address returned from the DNS resolution).
  - If the string cannot be parsed into a valid IP address or the hostname cannot be resolved, an exception is thrown (see _Exceptions_).
- **Preconditions:**
  - `ip_address` must not be empty.
- **Exceptions:**
  - `std::invalid_argument`: Thrown if the string cannot be parsed as a valid IP address or if the hostname cannot be resolved.
  - `std::out_of_range`: Thrown if the parsed port number is outside the valid range for a port (0–65535).
- **Notes:**
  - If a hostname is used in the `ip_address`, the behavior may be nondeterministic. This is due to how the operating system or library resolves the hostname into an IP address, which may vary based on DNS resolution results and host configurations.
  - For deterministic behavior, it is recommended to use a numeric IP address instead of a hostname.
- **Complexity:** Linear in the length of `ip_address`.

5. **Copy Constructor**

```cpp
IPAddress(const IPAddress& other) noexcept;
```

- **Effects:**
  - Constructs an `IPAddress` object as a copy of `other`, copying the internal address and port values.
- **Complexity:** Constant.

6. **Copy Assignment Operator**

```cpp
IPAddress& operator=(const IPAddress& other) noexcept;
```

- **Effects:**
  - Assigns the values of `other` to `*this`, copying the internal address and port values.
- **Returns:**
  - `*this`.
- **Complexity:** Constant.

7. **Move Constructor**

```cpp
IPAddress(IPAddress&& other) noexcept;
```

- **Effects:**
  - Constructs an `IPAddress` object by moving the contents of `other`.
  - After the move `other` will be in a valid but unspecified state.
- **Complexity:** Constant.

8. **Move Assignment Operator**

```cpp
IPAddress& operator=(IPAddress&& other) noexcept;
```

- **Effects:**
  - Moves the internal state of `other` into `*this`.
  - After the move `other` will be in a valid but unspecified state.
- **Returns:**
  - `*this`.
- **Complexity:** Constant.

9. **Destructor**

```cpp
~IPAddress() override;
```

- **Effects:**
  - Destroys the `IPAddress` object, releasing any resources.
- **Complexity:** Constant.

10. **Additional Accessors**

> [!WARNING]
> Calling any accessors on an `IPAddress` object while it is in its empty state (e.g., after `reset()` with no arguments) results in undefined behavior.

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
  - The port associated with the IP address, obtained directly from the cached `port` member.
- **Complexity:** Constant.

11. **Overridden Virtual Functions**

```cpp
socklen_t size() const noexcept override;
```

- **Returns:**
  - The number of bytes occupied by the stored socket address structure.
  - For IPv4, it returns `sizeof(sockaddr_in)`.
  - For IPv6, it returns `sizeof(sockaddr_in6)`.
- **Complexity:** Constant.

```cpp
string_type to_string() const override;
```

- **Returns:**
  - A string representation of the address in the format `"ip:port"`. For IPv6 addresses, the IP is enclosed in square brackets (e.g., `"[::1]:80"`).
- **Complexity:** Linear in the length of the address and port.

```cpp
void do_swap(Address& other) noexcept override;
```

- **Effects:**
  - Swaps the internal state of the current object (`*this`) with the state of the `other` `IPAddress` object.
- **Complexity:** Constant.

---

### `UnixDomainAddress`

The `UnixDomainAddress` class encapsulates a Unix domain socket address, inheriting from the `Address` class. It provides a specific implementation for Unix domain socket addresses, which are used for inter-process communication on the same host. This class handles Unix domain socket file paths and provides a unified interface for networking operations.

#### Synopsis

```cpp
class UnixDomainAddress final : public Address {
public:
  // Constructors
  UnixDomainAddress() noexcept;
  UnixDomainAddress(const_reference address) noexcept;
  explicit UnixDomainAddress(std::filesystem::path path);

  // Copy Constructor and Copy Assignment Operator
  UnixDomainAddress(const UnixDomainAddress& other) noexcept;
  UnixDomainAddress& operator=(const UnixDomainAddress& other) noexcept;

  // Move Constructor and Move Assignment Operator
  UnixDomainAddress(UnixDomainAddress&& other) noexcept;
  UnixDomainAddress& operator=(UnixDomainAddress&& other) noexcept;

  // Destructor
  ~UnixDomainAddress() override;

  // Path Accessor
  const std::filesystem::path& path() const&;
  std::filesystem::path path() &&;

  // Override virtual functions
  socklen_t size() const noexcept override;
  string_type to_string() const override;

protected:
  void do_swap(Address& other) noexcept override;
  int compare(const Address& other) const noexcept override;

private:
  std::filesystem::path socket_path; // Cached socket file path
};
```

#### Data Members

| Name          | Type                    | Explanation                          |
| ------------- | ----------------------- | ------------------------------------ |
| `socket_path` | `std::filesystem::path` | Cached Unix domain socket file path. |

#### Member Functions

1. **Default Constructor**

```cpp
UnixDomainAddress() noexcept;
```

- **Effects:**
  - Constructs an empty `UnixDomainAddress` object. `has_value()` returns `false`.
- **Complexity:** Constant.

2. **Constructor (from raw address)**

```cpp
UnixDomainAddress(const_reference address) noexcept;
```

- **Effects:**
  - Constructs a `UnixDomainAddress` object using the specified raw address `address`.
  - The address family is automatically deduced from `storage.ss_family` and stored in the cached family member.
  - If the family cannot be recognized as Unix domain, the result is an empty address.
- **Complexity:** Constant.

3. **Constructor (from filesystem path)**

```cpp
explicit UnixDomainAddress(std::filesystem::path path);
```

- **Effects:**
  - Constructs a `UnixDomainAddress` object from a Unix domain socket file path.
  - The address is constructed using the path provided, and the family is set to `AddressFamily::Unix`.
- **Exceptions:**
  - `std::overflow_error`: Thrown if the path is too long to fit in the address structure.

4. **Copy Constructor**

```cpp
UnixDomainAddress(const UnixDomainAddress& other) noexcept;
```

- **Effects:**
  - Constructs a `UnixDomainAddress` object as a copy of `other`, copying the internal raw storage, socket path and cached address family.
- **Complexity:** Constant.

5. Copy Assignment Operator

```cpp
UnixDomainAddress& operator=(const UnixDomainAddress& other) noexcept;
```

- **Effects:**
  - Assigns `other` to `*this`, copying the internal address, socket path and address family.
- **Returns:**
  - `*this`.
- **Complexity:** Constant.

6. **Move Constructor**

```cpp
UnixDomainAddress(UnixDomainAddress&& other) noexcept;
```

- **Effects:**
  - Constructs a `UnixDomainAddress` object by moving the contents of `other`. After the move, `other` is in a valid but unspecified state.
- **Complexity:** Constant.

7. **Move Assignment Operator**

```cpp
UnixDomainAddress& operator=(UnixDomainAddress&& other) noexcept;
```

- **Effects:**
  - Moves the internal state of `other` into `*this`. After the move, `other` is in a valid but unspecified state.
- **Returns:**
  - `*this`.
- **Complexity:** Constant.

8. **Destructor**

```cpp
~UnixDomainAddress() override;
```

- **Effects:**
  - Destroys the `UnixDomainAddress` object. The destructor is trivial as no dynamic memory is explicitly managed.
- **Complexity:** Constant.

9. **Additional Accessors**

> [!WARNING]
> Calling any accessors on an `UnixDomainAddress` object while it is in its empty state (e.g., after `reset()` with no arguments) results in undefined behavior.

```cpp
const std::filesystem::path& path() const&;
std::filesystem::path path() &&;
```

- **Returns:**
  - The Unix domain socket path as a `std::filesystem::path`.
- **Complexity:** Implementation-specific.

10. **Overridden Virtual Functions**

```cpp
socklen_t size() const noexcept override;
```

- **Effects:**
  - Provides access to the underlying Unix domain socket file path.
- **Returns:**
  - For `const&`: A constant reference to the stored Unix domain socket path.
  - For `&&`: A `path` object, moved from the internal storage.
- **Complexity:** Constant.

```cpp
string_type to_string() const override;
```

- **Returns:**
  - An implementation-defined textual representation of the address, which is the Unix domain socket path.
- **Complexity:** Implementation-specific.

```cpp
void do_swap(Address& other) noexcept override;
```

- **Effects:**
  - Swaps the internal state of the current object (`*this`) with the state of the `other` `UnixDomainAddress` object.
- **Complexity:** Constant.

```cpp
int compare(const Address& other) const noexcept override;
```

- **Returns:**
  - A negative value if `*this` is less than other, 0 if equal, or a positive value if greater.
- **Notes:**
  - Used for ordering and equality comparisons of addresses.
- **Complexity:** Constant.

---

### Error Handling

#### `socket_errc`

The `socket_errc` scoped enumeration is used to define error conditions for various socket-related operations within the Sockify library. These error conditions are designed to be used with the `std::error_condition` mechanism and represent socket-specific error conditions.

The scoped enumeration `socket_errc` defines the following constant values representing socket-related error conditions:

```cpp
enum class socket_errc {
  // Standard-compatible values
  success = 0,            // No error
  address_in_use,         // Address already in use
  address_not_available,  // Requested address is not available
  already_connected,      // Socket is already connected
  bad_file_descriptor,    // Invalid file descriptor
  connection_refused,     // Connection refused
  connection_reset,       // Connection reset by peer
  host_unreachable,       // Remote host is unreachable
  message_too_large,      // Message size exceeds allowed maximum
  network_unreachable,    // Network unreachable
  no_buffer_space,        // No buffer space available
  not_a_socket,           // Operation attempted on a non-socket
  not_connected,          // Not connected
  not_supported,          // Operation not supported
  operation_in_progress,  // Non-blocking operation in progress
  operation_would_block,  // Would block (try again)
  permission_denied,      // Permission denied
  protocol_not_supported, // Chosen protocol is not supported
  timed_out,              // Operation timed out

  // Abstracted socket error conditions
  already_connected,      // Already connected
  already_open,           // Socket already open
  event_loop_closed,      // Event loop closed
  io_error,               // Generic I/O error
  not_open,               // Socket not open
  operation_cancelled,    // Operation was cancelled (e.g. due to shutdown)
  peer_closed_connection, // Remote peer closed the connection
  resource_exhausted,     // System ran out of some necessary resource (e.g. buffers)
  shutdown_requested,     // Operation failed due to shutdown request

  // TLS-specific errors
  tls_zero_return,        // Clean TLS shutdown during read/write;
                          // The underlying transport (e.g. TCP) may still be open.
  tls_want_read,          // TLS wants more input data
  tls_want_write,         // TLS wants to write more data
  tls_syscall,            // TLS-level system call failure
  tls_eof,                // Unexpected TLS EOF
  tls_cert_verify,        // Certificate verification failure
};
```

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
std::string message(int ev) const override;
```

- **Description:**
  - Converts an integer error code to a human-readable string by mapping `socket_errc` and `std::errc` values to descriptive messages.
- **Parameters:**
  - `ev`: The error code to map.
- **Return Value:** A descriptive string (e.g., `"Connection refused"`, `"Network unreachable"`, etc.).

```cpp
bool equivalent(const std::error_code& code, int condition) const override;
```

- **Description:**
  - Implements error equivalence logic so that error codes (possibly originating from different categories, such as `std::errc`) can be tested for equivalence with Sockify error conditions.
- **Parameters:**
  - `code`: The error code to compare.
  - `condition`: The integer value representing the error condition.
- **Return Value:** `true` if the provided `std::error_code` is considered equivalent to the error condition; otherwise, `false`.

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
explicit socket_error(const std::string& message, const std::error_code& ec);
explicit socket_error(const char* message, const std::error_code& ec);
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
const char* what() const noexcept override;
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

The `Socket` class defines a unified, protocol-agnostic interface for synchronous sockets. It encapsulates the basic lifecycle, option management and read/write interfaces, and is intended to be inherited by concrete socket types such as `StreamSocket` and `DatagramSocket`.

#### Member Types

| Name                  | Explanation                                                                               |
| --------------------- | ----------------------------------------------------------------------------------------- |
| `duration`            | Type representing durations for timeouts (`std::chrono::milliseconds` on most platforms). |
| `buffer_type`         | Type representing buffers used for socket I/O operations (always `Buffer`).               |
| `address_type`        | Type representing socket addresses (always `Address`).                                    |
| `address_family_type` | An alias for the scoped enumeration `AddressFamily`.                                      |
| `native_handle_type`  | Platform-specific socket handle.                                                          |

#### Data Members

| Name            | Type                      | Explanation                                                  |
| --------------- | ------------------------- | ------------------------------------------------------------ |
| `timeout`       | `std::optional<duration>` | The timeout duration for socket operations.                  |
| `inheritable`   | `bool`                    | Whether the socket handle is inheritable by child processes. |
| `native_handle` | `native_handle_type`      | Platform-specific socket handle.                             |

#### Member Functions

1. **Destructor**

```cpp
virtual ~Socket();
```

- **Effects:**
  - Destroys the `Socket` object, releasing any associated resources. This is a virtual destructor to ensure proper cleanup when derived classes are destroyed.

2. **Operations**

> [!NOTE]
> For each member function that may throw a `socket_error` (or other exceptions), a non-throwing overload is provided. These overloads accept an additional parameter of type `std::error_code& ec` (placed before any default arguments when applicable) that is used to report errors instead of throwing an exception.

```cpp
virtual void bind(const address_type& address) = 0;
virtual void bind(const address_type& address, std::error_code& ec) noexcept = 0;
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
virtual void connect(const address_type& address, std::error_code& ec) noexcept = 0;
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
virtual void listen(std::error_code& ec, int backlog = SOMAXCONN) noexcept;
```

- **Effects:**
  - Marks the socket as passive, used to accept incoming connection requests.
- **Parameters:**
  - `backlog`: Maximum number of pending connections; defaults to `SOMAXCONN`. If this parameter is specified, it must be at least `0` (if it is lower, it is set to `0`).
- **Exceptions:**
  - `socket_error` on failure, such as if the socket is invalid or was not bound.
- **Complexity:** Constant

```cpp
virtual std::unique_ptr<Socket> accept() = 0;
virtual std::unique_ptr<Socket> accept(std::error_code& ec) noexcept = 0;
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
virtual native_handle_type detach(std::error_code& ec) noexcept = 0;
```

- **Effects:**
  - Releases ownership of the underlying socket handle and returns it to the caller.
- **Exceptions:**
  - `socket_error` if the handle is already detached or invalid.
- **Complexity:** Constant

```cpp
virtual void setblocking(bool would_block);
virtual void setblocking(bool would_block, std::error_code& ec) noexcept;
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
virtual void settimeout(duration timeout);
virtual void settimeout(duration timeout, std::error_code& ec) noexcept;
```

- **Effects:**
  - Sets the timeout value for blocking socket operations like `recv()` or `send()`.
- **Parameters:**
  - `timeout`: The maximum time to block.
- **Exceptions:**
  - `socket_error` if the operation is unsupported on the socket type or fails at the OS level.
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
virtual void setsockopt(int level, int optname, int value, std::error_code& ec) noexcept;
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
virtual void setsockopt(int level, int optname, const buffer_type& option_value);
virtual void setsockopt(int level, int optname, const buffer_type& option_value, std::error_code& ec) noexcept;
```

- **Effects:**
  - Sets the value of a socket option, which is stored as raw binary data in the `buffer_type` class.
- **Parameters:**
  - `level`: Protocol level (e.g., `SOL_SOCKET`).
  - `optname`: Option name.
  - `option_value`: A buffer containing the raw binary data for the option.
- **Exceptions:**
  - `socket_error` if the option is unsupported or the socket is invalid.
- **Complexity:** Constant.

```cpp
virtual int getsockopt(int level, int optname) const;
virtual int getsockopt(int level, int optname, std::error_code& ec) const noexcept;
```

- **Returns:**
  - The value of the requested socket option.
- **Exceptions:**
  - `socket_error` on error
- **Complexity:** Constant

```cpp
virtual buffer_type getsockopt(int level, int optname, std::size_t buflen) const;
virtual buffer_type getsockopt(int level, int optname, std::size_t buflen, std::error_code& ec) const noexcept;
```

- **Returns:** The value of a socket option, which may involve returning raw binary data in a buffer.
- **Parameters:**
  - `level`: Protocol level (e.g., `SOL_SOCKET`).
  - `optname`: Option name.
  - `buflen`: The maximum length of the buffer to receive the option's value.
- **Exceptions:**
  - `socket_error` if the option is unsupported or the socket is invalid.
- **Complexity:** Constant.

```cpp
virtual void setinheritable(bool inheritable);
virtual void setinheritable(bool inheritable, std::error_code& ec) noexcept;
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
virtual address_type getsockname(std::error_code& ec) const noexcept = 0;
```

- **Returns:**
  - The local address to which the socket is bound.
- **Exceptions:**
  - `socket_error` if the socket is not bound or invalid.
- **Complexity:** Constant

```cpp
virtual address_type getpeername() const = 0;
virtual address_type getpeername(std::error_code& ec) const noexcept = 0;
```

- **Returns:**
  - The address of the remote peer to which the socket is connected.
- **Exceptions:**
  - `socket_error` if the socket is not connected.
- **Complexity:** Constant

```cpp
virtual std::size_t send(const buffer_type& buf, int flags = 0);
virtual std::size_t send(const buffer_type& buf, std::error_code& ec, int flags = 0) noexcept;
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
virtual std::size_t sendto(const buffer_type& buf, const address_type& dest, std::error_code& ec,
                           int flags = 0) noexcept;
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
virtual std::size_t sendall(const buffer_type& buf, std::error_code& ec, int flags = 0) noexcept;
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
virtual std::size_t sendfile(std::ifstream& file, std::error_code& ec, std::streampos offset = 0,
                             std::size_t count = 0) noexcept;

virtual std::size_t sendfile(const std::filesystem::path& file_path, std::streampos offset = 0, std::size_t count = 0);
virtual std::size_t sendfile(const std::filesystem::path& file_path, std::error_code& ec, std::streampos offset = 0,
                             std::size_t count = 0) noexcept;
```

- **Effects**:
  - Sends data from the provided file over the socket. The file must be a regular file object opened in binary mode.
    - When a filesystem path is provided, the function opens the file in binary mode and then sends its content over the socket.
- **Parameters:**
  - `offset` specifies the position in the file from which to start reading. If not provided, it defaults to `0`.
  - `count` specifies the total number of bytes to send. If not provided, the function sends the file until EOF is reached.
- **Returns:** The total number of bytes sent.
- **Exceptions**:
  - `std::invalid_argument` if the file is not open, cannot be opened, or the socket is not of `SOCK_STREAM` type.
  - `std::ios_base::failure` if an error occurs while reading from the file.
  - `socket_error` depending on the underlying socket implementation.
- **Notes:**:
  - This function is designed to send the entire content of the file (or a portion, if `count` is provided) in chunks over the socket.
  - Non-blocking sockets are not supported for this operation.

```cpp
virtual buffer_type recv(std::size_t count, int flags = 0);
virtual buffer_type recv(std::size_t count, std::error_code& ec, int flags = 0) noexcept;
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
virtual buffer_type recvfrom(std::size_t count, address_type& src, std::error_code& ec, int flags = 0) noexcept;
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
virtual void shutdown(int how, std::error_code& ec) noexcept = 0;
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

3. **Swap Support (Protected)**

```cpp
virtual void do_swap(Socket& other) noexcept = 0;
```

- **Effects:**
  - Swaps the internal state of `*this` and `other`, specific to the most-derived type.
  - This function is called by the base `swap(Socket&)` to allow derived classes to exchange any resources or members they own beyond the base class.
  - This function must be overridden by all derived classes that support swapping.
- **Preconditions:**
  - `*this` and `other` must have the same dynamic type.
  - It is the caller's responsibility (e.g., in `swap()`) to ensure this is true.
- **Notes:**
  - Typically, the implementation will perform a `static_cast` to the concrete type and `std::swap` any members unique to that type.
  - This pattern avoids virtual slicing and ensures the base class can orchestrate full, polymorphism-aware swapping without knowing type-specific internals.
- **Complexity:** Constant.

4. **Swap Support**

```cpp
void swap(Socket& other) noexcept;
friend void swap(Socket& lhs, Socket& rhs) noexcept;
```

- **Effects:**
  - Exchanges the internal state of `*this` and `other`. After the call, each object has assumed the state previously held by the other.
  - This function handles swapping of base class resources and delegates swapping of derived-class-specific state to the virtual `do_swap(Socket& other)` function.
  - Derived classes must override `do_swap()` to implement the swap of their own internal members.
  - The behavior is undefined unless `*this` and `other` are of the same dynamic type.
- **Preconditions:**
  - `typeid(*this) == typeid(other)`, i.e., both objects must have the same dynamic type.
- **Complexity:** Constant.

---

### `StreamSocket`

The `StreamSocket` class encapsulates a stream-oriented network socket, providing a high-level interface for managing socket operations using the `SOCK_STREAM` type and an automatically detected protocol (protocol set to `0`). It supports the complete set of standard operations required to create, bind, connect, read from, write to and close a stream socket, while ensuring robust resource management and error handling.

#### Synopsis

```cpp
class StreamSocket : public Socket {
public:
  // Constructors & Destructor
  StreamSocket();                                     // Default constructor
  explicit StreamSocket(const address_type& address); // Constructor from address
  explicit StreamSocket(native_handle_type handle,
                     const address_type& address = address_type()); // Constructor from native handle

  StreamSocket(const StreamSocket& other);            // Copy constructor
  StreamSocket& operator=(const StreamSocket& other); // Copy assignment operator

  StreamSocket(StreamSocket&& other) noexcept;            // Move constructor
  StreamSocket& operator=(StreamSocket&& other) noexcept; // Move assignment operator

  ~StreamSocket() override; // Destructor

  // Socket operations
  void bind(const address_type& address) override;
  void bind(const address_type& address, std::error_code& ec) noexcept override;

  void connect(const address_type& address) override;
  void connect(const address_type& address, std::error_code& ec) noexcept override;

  void listen(int backlog = SOMAXCONN) override;
  void listen(std::error_code& ec, int backlog = SOMAXCONN) noexcept override;

  std::unique_ptr<Socket> accept() override;
  std::unique_ptr<Socket> accept(std::error_code& ec) noexcept override;

  void close() noexcept override;

  native_handle_type detach() override;
  native_handle_type detach(std::error_code& ec) noexcept override;

  void shutdown(int how) override;
  void shutdown(int how, std::error_code& ec) noexcept override;

  address_type getsockname() const override;
  address_type getsockname(std::error_code& ec) const noexcept override;

  address_type getpeername() const override;
  address_type getpeername(std::error_code& ec) const noexcept override;

  // I/O operations
  std::size_t send(const buffer_type& buf, int flags = 0) override;
  std::size_t send(const buffer_type& buf, std::error_code& ec, int flags = 0) noexcept override;

  std::size_t sendall(const buffer_type& buf, int flags = 0) override;
  std::size_t sendall(const buffer_type& buf, std::error_code& ec, int flags = 0) noexcept override;

  std::size_t sendfile(std::ifstream& file, std::streampos offset = 0, std::size_t count = 0) override;
  std::size_t sendfile(std::ifstream& file,
                       std::error_code& ec,
                       std::streampos offset = 0,
                       std::size_t count     = 0) noexcept override;

  std::size_t
  sendfile(const std::filesystem::path& file_path, std::streampos offset = 0, std::size_t count = 0) override;
  std::size_t sendfile(const std::filesystem::path& file_path,
                       std::error_code& ec,
                       std::streampos offset = -1,
                       std::size_t count     = 0) noexcept override;

  buffer_type recv(std::size_t count, int flags = 0) override;
  buffer_type recv(std::size_t count, std::error_code& ec, int flags = 0) noexcept override;

  buffer_type recvfrom(std::size_t count, address_type& src, int flags = 0) override;
  buffer_type recvfrom(std::size_t count, address_type& src, std::error_code& ec, int flags = 0) noexcept override;

protected:
  // Swap support
  void do_swap(Socket& other) noexcept override;
};
```

#### Additional Member Functions

1. **Constructor (from address):**

```cpp
explicit StreamSocket(address_family_type family = AddressFamily::IPv4);
```

- **Effects:**
  - Creates a stream socket with the specified address family, using `SOCK_STREAM` as the socket type and protocol set to `0` (auto-detected).
  - The default address family is `AddressFamily::IPv4`.
- **Exceptions:**
  - Throws `socket_error` if the socket creation fails (e.g., the per-process descriptor table is full).

2. **Constructor (from native handle):**

```cpp
explicit StreamSocket(native_handle_type handle,
                      address_family_type family = AddressFamily::Unknown);
```

- **Effects:**
  - Takes ownership of an existing socket handle.
  - The socket's address family is auto-detected from the handle unless overridden by the `family` argument.
- **Notes:**
  - This allows the socket to be used with the appropriate family and protocol without duplicating the handle. Useful when a socket is created outside this class (e.g., using `accept()`).
- **Exceptions:**
  - Throws `socket_error` if the handle is invalid or not a stream socket.
- **Complexity:** Constant.

3. **Copy Constructor:**

```cpp
StreamSocket(const StreamSocket& other);
```

- **Effects:**
  - Creates a copy of the other `StreamSocket` object. This duplicates the socket handle and copies the internal state (e.g., timeout settings).
- **Exceptions:**
  - Throws `socket_error` if copying the socket fails (e.g., invalid handle).

4. **Copy Assignment Operator:**

```cpp
StreamSocket& operator=(const StreamSocket& other);
```

- **Effects:**
  - Copies the state of another `StreamSocket` into this one. This duplicates the socket handle and copies the internal state (e.g., timeout settings).
- **Exceptions:**
  - Throws `socket_error` if copying the socket fails (e.g., invalid handle).

5. **Move Constructor:**

```cpp
StreamSocket(StreamSocket&& other) noexcept;
```

- **Effects:**
  - Transfers ownership of the socket handle and associated resources from `other` to this object.
  - After the move `other` will be in a valid but unspecified state.
- **Complexity:** Constant.

6. **Move Assignment Operator:**

```cpp
StreamSocket& operator=(StreamSocket&& other) noexcept;
```

- **Effects:**
  - Transfers ownership of the socket handle and associated resources from `other` to this object, invalidating `other`.
  - After the move `other` will be in a valid but unspecified state.
- **Complexity:** Constant.

7. **Destructor:**

```cpp
~StreamSocket() override;
```

- **Effects:**
  - Closes the socket and releases any allocated resources. This also invalidates the socket handle.
- **Complexity:** Constant

#### Swap Function

To allow efficient swapping of `StreamSocket` objects (including during exception handling or container operations like `std::swap`):

```cpp
void do_swap(Socket& other) noexcept override;
```

- **Effects:**
  - Swaps members specific to the `StreamSocket` class.
  - Performs a `static_cast` of other to `StreamSocket&` and then exchanges any additional state.
- **Preconditions:**
  - `other` must be of dynamic type `StreamSocket`.
  - This precondition is guaranteed by the base class `swap()` implementation and must not be checked redundantly here.
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

4. **Swap Support (Protected)**

```cpp
virtual void do_swap(EventLoop& other) noexcept = 0;
```

- **Effects:**
  - Performs swapping of derived-class–specific internal state.
  - Must be overridden by each concrete derived class to ensure that all additional members are exchanged correctly.
- **Complexity:** Constant.

5. **Swap Support**

```cpp
void swap(EventLoop& other) noexcept;
friend void swap(EventLoop& lhs, EventLoop& rhs) noexcept;
```

- **Effects:**
  - Swaps the internal state of the two `EventLoop` objects (`lhs` and `rhs`). This includes swapping event handlers, associated resources (such as event bases, file descriptors, etc.), and other internal state specific to the derived class.
  - The derived class must implement the `do_swap()` function to swap any resources specific to its type. This ensures the efficient swap of more complex internal states in derived classes. The base class `swap()` will delegate this operation to the derived class via the virtual `do_swap()` method.
- **Preconditions:**
  - Both `*this` and `other` must be of the same dynamic type. Otherwise, the behavior is undefined.
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
class AsyncEventLoop : public EventLoop {
public:
  // Member Types
  using base_type    = event_base; // libevent's event base type.
  using base_pointer = base_type*; // Pointer to libevent event base.

public:
  // Constructor & Destructor
  AsyncEventLoop();           // Initializes and acquires a libevent event base.
  ~AsyncEventLoop() override; // Releases libevent resources upon destruction.

  // Move Semantics
  AsyncEventLoop(AsyncEventLoop&& other) noexcept;            // Move constructor.
  AsyncEventLoop& operator=(AsyncEventLoop&& other) noexcept; // Move assignment operator.

  // Copy Semantics (Deleted)
  AsyncEventLoop(const AsyncEventLoop&)            = delete; // Copy constructor is deleted.
  AsyncEventLoop& operator=(const AsyncEventLoop&) = delete; // Copy assignment is deleted.

  // Overridden Methods from EventLoop
  bool register_socket(socket_type& socket, handler_type handler) override; // Register a socket with a handler.
  bool unregister_socket(socket_type& socket) noexcept override;            // Unregister a socket.
  void run() override;                                                      // Run the event loop.
  void stop() noexcept override;                                            // Stop the event loop.

  // Additional Methods for Timeout Control
  duration poll_timeout() const noexcept override;           // Retrieve the current poll timeout.
  duration poll_timeout(duration timeout) noexcept override; // Set a new poll timeout.

protected:
  // Swap Support for AsyncEventLoop
  void do_swap(EventLoop& other) noexcept override; // Swap internal state of two AsyncEventLoop objects.

private:
  base_pointer base; // Pointer to the libevent event base object.
};
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
~AsyncEventLoop() override;
```

- **Effects:**
  - Releases the libevent event base (by calling the appropriate libevent deallocation function) and cleans up any associated resources.
- **Complexity:** Constant.

6. **Swap Support**

```cpp
void do_swap(EventLoop& other) noexcept override;
```

- **Effects:**
  - Swaps the internal state of two `AsyncEventLoop` objects. Specifically, it swaps the internal resources such as the underlying libevent base or other associated handlers between `lhs` and `rhs`.
  - After the swap, both `lhs` and `rhs` will be in a valid, usable state. The event loop base and other resources are exchanged.
- **Preconditions:**
  - `other` must be of dynamic type `AsyncEventLoop`.
  - This precondition is guaranteed by the base class `swap()` implementation and must not be checked redundantly here.
- **Complexity:** Constant.

---

### `SimpleEventLoop`

The `SimpleEventLoop` class is a minimal, lightweight event loop with fewer dependencies and less complexity than `AsyncEventLoop`. Unlike `AsyncEventLoop`, which relies on an external library like _libevent_, `SimpleEventLoop` is implemented with basic system calls (such as `select()` or `poll()`) to provide fundamental event loop functionality.

#### Synopsis

```cpp
class SimpleEventLoop : public EventLoop {
public:
  // Constructor & Destructor
  SimpleEventLoop() noexcept;  // Initializes a basic SimpleEventLoop with default settings.
  ~SimpleEventLoop() override; // Cleans up any resources associated with the event loop.

  // Move Semantics
  SimpleEventLoop(SimpleEventLoop&& other) noexcept;            // Move constructor.
  SimpleEventLoop& operator=(SimpleEventLoop&& other) noexcept; // Move assignment operator.

  // Copy Semantics (Deleted)
  SimpleEventLoop(const SimpleEventLoop&)            = delete; // Copy constructor is deleted.
  SimpleEventLoop& operator=(const SimpleEventLoop&) = delete; // Copy assignment is deleted.

  // Overridden Methods from EventLoop
  bool register_socket(socket_type& socket, handler_type handler) override; // Registers a socket with a handler.
  bool unregister_socket(socket_type& socket) noexcept override;            // Unregisters a socket from the event loop.
  void run() override;           // Runs the event loop using basic system calls like select() or poll().
  void stop() noexcept override; // Stops the event loop.

  // Additional Methods for Timeout Control
  duration poll_timeout() const noexcept override;           // Gets the current poll timeout.
  duration poll_timeout(duration timeout) noexcept override; // Sets the new poll timeout.

protected:
  // Swap Support for SimpleEventLoop
  void do_swap(EventLoop& other) noexcept override; // Swaps internal state of two SimpleEventLoop objects.
};
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
~SimpleEventLoop() override;
```

- **Effects:**
  - Cleans up any resources associated with the event loop (e.g., unregistering any active sockets).
- **Complexity:** Constant.

6. **Swap Support**

```cpp
void do_swap(EventLoop& other) noexcept override;
```

- **Effects:**
  - Swaps the internal state of two `SimpleEventLoop` objects. Specifically, it swaps the internal resources such as the associated handlers between `lhs` and `rhs`.
  - After the swap, both `lhs` and `rhs` will be in a valid, usable state.
- **Preconditions:**
  - `other` must be of dynamic type `SimpleEventLoop`.
  - This precondition is guaranteed by the base class `swap()` implementation and must not be checked redundantly here.
- **Complexity:** Constant.
