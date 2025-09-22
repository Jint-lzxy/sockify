# Sockify Design Document

## Overview

**Sockify** is a modern, cross-platform C++17 socket library.

### Features

Sockify provides a uniform interface for network programming across all major platforms, abstracting away platform-dependent socket operations. All advanced functionality is optional. Secure connections are provided via integration with OpenSSL, if enabled at compile time.

The library is built to be extensible, so it can support other transport protocols in the future.

### Design Choices

Sockify is built around several core design principles, as outlined below:

- We decided to utilize modern C++ features extensively, and chose to keep the library lightweight by default, making SSL/TLS support optional compile-time features. Applications that don't require these capabilities shouldn't pay for their overhead.
- We deliberately avoided implementing thread-safe sockets by default to prevent unnecessary locking overhead. Instead, we expect applications to handle concurrency through message-passing, worker threads or external synchronization.
- We decided to support both exception-based and error-code-based error reporting, providing `std::error_code` overloads for applications that prefer to avoid exception handling.
- We chose to abstract platform-specific details with polymorphism while still providing access to the underlying raw platform-specific structures when needed, allowing developers to work around platform limitations or access specialized features not exposed through the abstraction layer.

## Primary Library Components

```console
                         +-------------------------+
                         |     sockify::Socket     |  <-- Abstract base class
                         | (common socket methods) |  <-- for all sockets
                         +-------------------------+
                                      ^
                                      |
                   +------------------+--------------------+            <-- Additional protocols can be
                   |                  ·                    |            <-- defined (and some are already
                   |                  ·                    |            <-- included in the library) by
                   |                  ·                    |            <-- deriving from sockify::Socket
        +----------------------+      ·       +------------------------+
        |     StreamSocket     |      ·       |     DatagramSocket     |
        | (stream socket impl) |      ·       | (datagram socket impl) |
        +----------------------+      ·       +------------------------+
                                      ·
                                      ·
                         +-------------------------+
                         |    sockify::TLSSocket   |  <-- Abstract templated base class for TLS
                         |     (Secure socket)     |  <-- socket and wraps any underlying socket
                         +-------------------------+
                                      ^
                                      |
                              +---------------+
                              | OpenSSLSocket |
                              | (OpenSSL TLS) |
                              +---------------+
             [Wraps any sockify::Socket, not limited to TCP/UDP]

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
            [Any sockify::Socket can be wrapped by these stream classes]
```

## Interface Reference

> [!IMPORTANT]
> For simplicity and brevity, the `sockify::` namespace is omitted from all definitions in the sections that follow. Unless otherwise specified, assume that all names belong to this namespace. Additionally, this document is intended as a design reference and is **NOT** a comprehensive manual. For the complete API reference, please refer to the generated Doxygen documentation.

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

- The set of enumerators may be extended in future versions to include additional address families (e.g., Bluetooth). Existing enumerators are stable and portable across supported platforms.
- `Unix` shall only be present on platforms that support `AF_UNIX`. On platforms that do not, attempts to construct or use `UnixDomainAddress` shall result in a program-defined diagnostic.

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
- **Remarks**
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
- **Complexity:** Constant.

7. **Accessors**

> [!WARNING]
> Calling any accessors on an `Address` (or any of its derived classes) object while it is in its empty state (e.g., after `reset()`) results in undefined behavior.

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
- **Remarks**
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
- **Remarks**
  - Empty `Address` objects are always ordered before any non-empty `Address` objects.
  - This ordering ensures that every pair of valid `Address` objects is comparable, and the result is stable and deterministic.
  - Derived classes may override the comparison mechanism by redefining the `compare()` function (see below) if they require additional ordering logic.
- **Complexity:** Linear in the number of bytes compared.

10. **Comparison Operators (Protected)**

```cpp
virtual int compare(const Address& other) const noexcept;
```

- **Effects:**
  - Compares only the meaningful bytes of the underlying storage (zero-initialized padding is excluded).
  - The comparison first considers semantic elements (e.g., port in host order, IP address bytes, path length).
  - Returns negative/zero/positive values accordingly.
- **Returns:**
  - A signed result with the following meaning:
    - Negative if `*this < other`,
    - Zero if `*this == other`,
    - Positive if `*this > other`.
- **Complexity:** Linear in the number of bytes compared.
- **Remarks**
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

12. **Hash Support**

```cpp
template<>
struct std::hash<Address> {
    std::size_t operator()(const Address& addr) const noexcept;
};
```

- **Effects:** Computes a hash value for the given `Address`.
- **Parameters:**
  - `addr`: The `Address` object to hash.
- **Returns:** A hash value for `addr`.
- **Remarks**
  - Empty addresses hash to a stable implementation-defined value.
  - For IP addresses, hash combines family, port (host order), and normalized bytes.
  - For Unix addresses, hash is computed from the path string.
- **Complexity:** Linear.

---

### `IPAddress`

The `IPAddress` class represents a network address specifically for IP-based addresses, such as IPv4 and IPv6. It extends the `Address` base class to handle the specific behaviors related to IP addresses, including parsing, formatting and managing the IP address itself, as well as providing functionality for associated ports. Ports are always stored in **host byte order**.

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
  IPAddress(std::string_view ip_address, uint16_t port, uint32_t scope_id);

  // Copy Constructor and Copy Assignment Operator
  IPAddress(const IPAddress& other) noexcept;
  IPAddress& operator=(const IPAddress& other) noexcept;

  // Move Constructor and Move Assignment Operator
  IPAddress(IPAddress&& other) noexcept;
  IPAddress& operator=(IPAddress&& other) noexcept;

  // Destructor
  ~IPAddress() override;

  // Address Resolution
  static std::vector<IPAddress> resolve(std::string_view host, std::string_view service);
  static std::vector<IPAddress> resolve(std::string_view host, std::string_view service, AddressFamily family);

  // Address/Port Accessors
  string_type ip() const;
  uint16_t port() const noexcept;

  // IPvx-specific accessors
  uint32_t scope_id() const noexcept;
  uint32_t flow_info() const noexcept;
  bool is_ipv4() const noexcept;
  bool is_ipv6() const noexcept;
  bool is_loopback() const noexcept;
  bool is_multicast() const noexcept;
  bool is_link_local() const noexcept;
  bool is_site_local() const noexcept;
  bool is_global_unicast() const noexcept;
  bool is_ipv4_mapped() const noexcept;
  bool is_ipv4_compatible() const noexcept;

  // Address manipulation
  IPAddress to_ipv4() const;
  IPAddress to_ipv6() const;
  void set_scope_id(uint32_t scope_id) noexcept;
  void set_flow_info(uint32_t flow_info) noexcept;

  // Override virtual functions
  socklen_t size() const noexcept override;
  string_type to_string() const override;
  int compare(const Address& other) const noexcept override;

  // Additional Utilities
  string_type to_string(bool include_scope) const;

private:
  uint16_t port; // Cached port number
};
```

#### Data Members

| Name   | Type       | Explanation                                                                |
| ------ | ---------- | -------------------------------------------------------------------------- |
| `port` | `uint16_t` | The cached port number associated with the IP address, in host byte order. |

#### Member Functions

1. **Default Constructor**

```cpp
IPAddress() noexcept;
```

- **Effects:**
  - Value-initializes the base class and `port`.
- **Postconditions:**
  - `*this` represents an empty address.
- **Complexity:** Constant.

2. **Constructor (from raw address)**

```cpp
IPAddress(const_reference address) noexcept;
```

- **Effects:**
  - Direct-initializes the base class with `address`.
  - If `address->ss_family` is `AF_INET`, extracts the port number from the `sin_port` member of the corresponding `sockaddr_in` structure and converts it to host byte order; sets the address family to `AddressFamily::IPv4`.
  - Otherwise, if `address->ss_family` is `AF_INET6`, extracts the port number from the `sin6_port` member of the corresponding `sockaddr_in6` structure and converts it to host byte order; sets the address family to `AddressFamily::IPv6`.
  - Otherwise, the behavior is as if the default constructor were called.
- **Preconditions:**
  - `address` is not a null pointer.
  - `address` points to an object of type `sockaddr_storage` containing a valid socket address.
- **Complexity:** Constant.

3. **Constructor (from string representation, DNS-Resolved)**

```cpp
explicit IPAddress(std::string_view ip_address);
```

- **Effects:**
  - If `ip_address` is in the format specified for the return value of `to_string()`, constructs an `IPAddress` object equivalent to that which would produce `ip_address` when `to_string()` is called.
  - Otherwise, if `ip_address` represents a hostname, resolves the hostname using the system's name resolution facility and constructs an `IPAddress` object from the first resolved address.
  - The port number is extracted from `ip_address` if present. If the port is specified as a service name (e.g., `"http"`, `"ssh"`), it is resolved to the corresponding port number using the system's service name resolution facility. If no port is specified, it is 0.
  - For IPv6 addresses, if a zone identifier is present in the format `%zone`, it is parsed and stored as the scope identifier; otherwise the scope identifier is 0. The zone identifier may be either a numeric scope ID or an interface name.
- **Preconditions:**
  - `!ip_address.empty()` is `true`.
- **Exceptions:**
  - `std::invalid_argument` if `ip_address` does not represent a valid IP address format and cannot be resolved as a hostname.
  - `std::out_of_range` if a port number is present and is not representable as a `uint16_t`.
  - `std::system_error` if hostname resolution or service name resolution is attempted and fails.
- **Remarks:**
  - When hostname resolution or service name resolution is performed, the result may vary between calls due to system configuration and DNS resolution behavior.
  - Service name resolution follows the same rules as the system's `getaddrinfo()` function with the `AI_NUMERICSERV` flag unset.
- **Complexity:** Linear in `ip_address.size()`.

4. **Constructor (from IP address and port number)**

```cpp
IPAddress(std::string_view ip_address, uint16_t port);
```

- **Effects:**
  - If `ip_address` represents an IP address in string form (e.g., `"127.0.0.1"` or `"::1"`), it will initialize the object with that address.
  - The port number is parsed from `port` and associated with the address.
  - If `ip_address` is a hostname (e.g., `"localhost"`), or if the string cannot be parsed into a valid address, an exception is thrown.
- **Preconditions:**
  - `ip_address` must not be empty.
- **Exceptions:**
  - `std::invalid_argument`: Thrown if the string cannot be parsed into a valid IP address.
  - `std::out_of_range`: Thrown if the port number is outside the valid range for a port (0–65535).
- **Remarks:** This constructor **never** performs DNS resolution.
- **Complexity:** Linear in the length of `ip_address`.

5. **Constructor (from IP address, port and scope ID)**

```cpp
IPAddress(std::string_view ip_address, uint16_t port, uint32_t scope_id);
```

- **Effects:**
  - Equivalent to `IPAddress(ip_address, port)`, except that if the resulting address has address family `AddressFamily::IPv6`, the scope identifier is set to `scope_id`.
- **Preconditions:**
  - `ip_address` must not be empty.
- **Exceptions:**
  - `std::invalid_argument`: Thrown if the string cannot be parsed into a valid IP address.
  - `std::out_of_range`: Thrown if the port number is outside the valid range for a port (0–65535).
- **Remarks:**
  - If the address family is not `AddressFamily::IPv6`, the `scope_id` parameter has no effect.
  - This constructor does not perform hostname resolution.
- **Complexity:** Linear in the length of `ip_address`.

6. **Copy Constructor**

```cpp
IPAddress(const IPAddress& other) noexcept;
```

- **Effects:**
  - Copy-constructs the base class and `port` from the corresponding members of `other`.
- **Postconditions:**
  - `*this == other` is `true`.
- **Complexity:** Constant.

7. **Copy Assignment Operator**

```cpp
IPAddress& operator=(const IPAddress& other) noexcept;
```

- **Effects:**
  - Copy-assigns the base class and `port` from the corresponding members of `other`.
- **Postconditions:**
  - `*this == other` is `true`.
- **Returns:**
  - `*this`.
- **Complexity:** Constant.

8. **Move Constructor**

```cpp
IPAddress(IPAddress&& other) noexcept;
```

- **Effects:**
  - Move-constructs the base class and `port` from the corresponding members of `other`.
- **Postconditions:**
  - `other` is in a valid but unspecified state.
- **Complexity:** Constant.

9. **Move Assignment Operator**

```cpp
IPAddress& operator=(IPAddress&& other) noexcept;
```

- **Effects:**
  - Move-assigns the base class and `port` from the corresponding members of `other`.
- **Postconditions:**
  - `other` is in a valid but unspecified state.
- **Returns:**
  - `*this`.
- **Complexity:** Constant.

10. **Destructor**

```cpp
~IPAddress() override;
```

- **Effects:**
  - Destroys `*this`.
- **Complexity:** Constant.

11. **Address Resolution (two-parameter)**

```cpp
static std::vector<IPAddress> resolve(std::string_view host, std::string_view service);
```

- **Effects:**
  - Equivalent to `resolve(host, service, AddressFamily::Unspecified)`.

12. **Address Resolution (three-parameter)**

```cpp
static std::vector<IPAddress> resolve(std::string_view host, std::string_view service, AddressFamily family);
```

- **Effects:**
  - Resolves `host` and `service` using the system's name resolution facility.
  - Each element of the returned vector represents a resolved socket address.
  - If `family` is `AddressFamily::IPv4`, only IPv4 addresses are returned.
  - If `family` is `AddressFamily::IPv6`, only IPv6 addresses are returned.
  - If `family` is `AddressFamily::Unspecified`, both IPv4 and IPv6 addresses may be returned.
- **Returns:**
  - A vector of `IPAddress` objects representing the resolved addresses, or an empty vector if no addresses were resolved.
- **Exceptions:**
  - `std::system_error` if the resolution fails.
- **Complexity:** Unspecified. The operation may block.

13. **Basic Accessors**

> [!WARNING]
> The behavior is undefined if any of the following accessor functions are called on an `IPAddress` object that represents an empty address.

```cpp
string_type ip() const;
```

- **Returns:**
  - A string representation of the IP address portion only, without port or scope identifier.
  - For IPv4 addresses, the format is dotted decimal notation (e.g., `"192.0.2.1"`).
  - For IPv6 addresses, the format is as specified in RFC 5952 (e.g., `"2001:db8::1"`).
- **Complexity:** Unspecified.

```cpp
uint16_t port() const noexcept;
```

- **Returns:**
  - The port number in host byte order.
- **Complexity:** Constant.

14. **IPvx-Specific Accessors**

```cpp
uint32_t scope_id() const noexcept;
```

- **Returns:**
  - If `family() == AddressFamily::IPv6`, the scope identifier from the `sin6_scope_id` member of the underlying `sockaddr_in6` structure.
  - Otherwise, `0`.
- **Complexity:** Constant.

```cpp
uint32_t flow_info() const noexcept;
```

- **Returns:**
  - If `family() == AddressFamily::IPv6`, the flow information from the `sin6_flowinfo` member of the underlying `sockaddr_in6` structure.
  - Otherwise, `0`.
- **Complexity:** Constant.

```cpp
bool is_ipv4() const noexcept;
```

- **Returns:**
  - `family() == AddressFamily::IPv4`.
- **Complexity:** Constant.

```cpp
bool is_ipv6() const noexcept;
```

- **Returns:**
  - `family() == AddressFamily::IPv6`.
- **Complexity:** Constant.

```cpp
bool is_loopback() const noexcept;
```

- **Returns:**
  - `true` if the address is a loopback address; `false` otherwise.
- **Remarks:**
  - For IPv4, this is `true` if the address is in the range `127.0.0.0/8`.
  - For IPv6, this is `true` if the address is `::1`.
- **Complexity:** Constant.

```cpp
bool is_multicast() const noexcept;
```

- **Returns:**
  - `true` if the address is a multicast address; `false` otherwise.
- **Remarks:**
  - For IPv4, this is `true` if the address is in the range `224.0.0.0/4`.
  - For IPv6, this is `true` if the address has the prefix `ff00::/8`.
- **Complexity:** Constant.

```cpp
bool is_link_local() const noexcept;
```

- **Returns:**
  - `true` if the address is a link-local address; `false` otherwise.
- **Remarks:**
  - For IPv4, this is `true` if the address is in the range `169.254.0.0/16`.
  - For IPv6, this is `true` if the address has the prefix `fe80::/10`.
- **Complexity:** Constant.

```cpp
bool is_site_local() const noexcept;
```

- **Returns:**
  - `true` if the address is a site-local address; `false` otherwise.
- **Remarks:**
  - For IPv4, this is `true` if the address is in any of the ranges `10.0.0.0/8`, `172.16.0.0/12`, or `192.168.0.0/16`.
  - For IPv6, this is `true` if the address has the prefix `fec0::/10`.
- **Complexity:** Constant.

```cpp
bool is_global_unicast() const noexcept;
```

- **Returns:**
  - `true` if the address is a global unicast address; `false` otherwise.
- **Remarks:**
  - An address is considered global unicast if it is not loopback, multicast, link-local, site-local, IPv4-mapped, or IPv4-compatible.
- **Complexity:** Constant.

```cpp
bool is_ipv4_mapped() const noexcept;
```

- **Returns:**
  - `true` if the address is an IPv4-mapped IPv6 address; `false` otherwise.
- **Remarks:**
  - An IPv4-mapped IPv6 address has the prefix `::ffff:0:0/96`.
- **Complexity:** Constant.

```cpp
bool is_ipv4_compatible() const noexcept;
```

- **Returns:**
  - `true` if the address is an IPv4-compatible IPv6 address; `false` otherwise.
- **Remarks:**
  - An IPv4-compatible IPv6 address has the form `::w.x.y.z` where `w.x.y.z` is an IPv4 address.
  - This mechanism is deprecated per RFC 4291.
- **Complexity:** Constant.

15. **Address Manipulation**

```cpp
IPAddress to_ipv4() const;
```

- **Effects:**
  - If `is_ipv4()` is `true`, returns `*this`.
  - Otherwise, if `is_ipv4_mapped()` or `is_ipv4_compatible()` is `true`, returns an `IPAddress` object representing the embedded IPv4 address with the same port number.
  - Otherwise, throws `std::invalid_argument`.
- **Returns:**
  - An `IPAddress` object with address family `AddressFamily::IPv4`.
- **Exceptions:**
  - `std::invalid_argument` if the address cannot be represented as IPv4.
- **Complexity:** Constant.

```cpp
IPAddress to_ipv6() const;
```

- **Effects:**
  - If `is_ipv6()` is `true`, returns `*this`.
  - Otherwise, if `is_ipv4()` is `true`, returns an `IPAddress` object representing an IPv4-mapped IPv6 address with the same port number and scope identifier 0.
  - Otherwise, throws `std::invalid_argument`.
- **Returns:**
  - An `IPAddress` object with address family `AddressFamily::IPv6`.
- **Exceptions:**
  - `std::invalid_argument` if the address cannot be represented as IPv6.
- **Complexity:** Constant.

```cpp
void set_scope_id(uint32_t scope_id) noexcept;
```

- **Effects:**
  - If `family() == AddressFamily::IPv6`, sets the `sin6_scope_id` member of the underlying `sockaddr_in6` structure to `scope_id`.
  - Otherwise, no effect.
- **Complexity:** Constant.

```cpp
void set_flow_info(uint32_t flow_info) noexcept;
```

- **Effects:**
  - If `family() == AddressFamily::IPv6`, sets the `sin6_flowinfo` member of the underlying `sockaddr_in6` structure to `flow_info`.
  - Otherwise, no effect.
- **Complexity:** Constant.

16. **Overridden Virtual Functions**

```cpp
socklen_t size() const noexcept override;
```

- **Returns:**
  - If `family() == AddressFamily::IPv4`, `sizeof(sockaddr_in)`.
  - Otherwise, if `family() == AddressFamily::IPv6`, `sizeof(sockaddr_in6)`.
  - Otherwise, `sizeof(sockaddr_storage)`.
- **Complexity:** Constant.

```cpp
string_type to_string() const override;
```

- **Effects:**
  - Equivalent to `to_string(true)`.

```cpp
string_type to_string(bool include_scope) const;
```

- **Returns:**
  - A string representation of the complete address.
  - If `family() == AddressFamily::IPv4`, the format is `"a.b.c.d:port"`.
  - If `family() == AddressFamily::IPv6`:
    - If `include_scope` is `false` or `scope_id() == 0`, the format is `"[ipv6-address]:port"`.
    - Otherwise, the format is `"[ipv6-address%scope]:port"`, where `scope` is the decimal representation of `scope_id()`.
- **Complexity:** Linear in the size of the address representation.

```cpp
int compare(const Address& other) const noexcept override;
```

- **Effects:**
  - Compares `*this` with `other` to establish ordering between `IPAddress` objects of the same family.
  - The comparison is performed in the following order:
    1. Address bytes (in network byte order)
    2. Port number (in host byte order)
    3. For IPv6 addresses, scope identifier
- **Preconditions:**
  - `family() == other.family()` is `true`.
- **Returns:**
  - A signed result with the following meaning:
    - Negative if `*this` orders before `other`
    - Zero if `*this` is equivalent to `other`
    - Positive if `*this` orders after `other`
- **Complexity:** Linear in the size of the address representation.
- **Remarks:**
  - Two `IPAddress` objects are considered equivalent if they have the same IP address bytes, port number, and (for IPv6) scope identifier.

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

5. **Copy Assignment Operator**

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
> Calling any accessors on an `UnixDomainAddress` object while it is in its empty state (e.g., after `reset()`) results in undefined behavior.

```cpp
const std::filesystem::path& path() const&;
std::filesystem::path path() &&;
```

- **Effects:**
  - Provides access to the underlying Unix domain socket file path.
- **Returns:**
  - For `const&`: A constant reference to the stored Unix domain socket path.
  - For `&&`: A `path` object, moved from the internal storage.
- **Complexity:** Constant.

10. **Overridden Virtual Functions**

```cpp
socklen_t size() const noexcept override;
```

- **Returns:**
  - The actual number of bytes occupied by the address, computed as if by:
    ```cpp
    offsetof(sockaddr_un, sun_path) + path_length
    ```
  - If the path begins with a null byte (abstract namespace), that byte is counted.
- **Complexity:** Constant.

```cpp
string_type to_string() const override;
```

- **Returns:**
  - An implementation-defined textual representation of the address, which is the Unix domain socket path.
- **Complexity:** Implementation-specific.

```cpp
int compare(const Address& other) const noexcept override;
```

- **Returns:**
  - A negative value if `*this` is less than other, 0 if equal, or a positive value if greater.
- **Remarks**
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

The class `Buffer` is a dynamically resizable, contiguous sequence container for binary data, designed specifically for network I/O operations. It extends the functionality of `std::vector<std::byte>` with additional operations commonly needed in socket programming, while maintaining full compatibility with standard algorithms and containers.

> [!CAUTION]
> The storage region exposed by a `Buffer` object via `buffer.data()` shall be suitably aligned for storing objects of any type `T` such that `alignof(T) <= alignof(std::max_align_t)`. In particular, for any such `T`, it is implementation-defined but guaranteed that `reinterpret_cast<T*>(buffer.data())` yields a pointer that is correctly aligned for `T`, provided `buffer.size() >= sizeof(T)`. This requirement ensures that reinterpretation of the buffer storage to access the value of a scalar object, pointer, or aggregate composed thereof is valid and does not result in undefined behavior due to misalignment.
>
> The alignment guarantee is a fundamental correctness requirement for the supporting utility methods, and allows users to portably treat the storage as capable of holding any fundamental or standard-layout type whose alignment does not exceed that of `std::max_align_t`. Implementations are not constrained in how this guarantee is achieved: an implementation may satisfy the alignment requirement by using an over-aligned value type (e.g., `std::aligned_storage_t<1, alignof(std::max_align_t)>`), a custom allocator that returns storage aligned to `std::max_align_t`, or any other means consistent with standard dynamic allocation behavior ([\[new\.delete\]](https://eel.is/c++draft/new.delete), [\[allocator.requirements\]](https://eel.is/c++draft/allocator.requirements)).
>
> This specification does not permit the storage returned by `buffer.data()` to be used to construct or access objects with alignment requirements stricter than `std::max_align_t`. Accessing such objects through `Buffer` results in undefined behavior unless further guarantees are provided externally. Implementations may provide stronger alignment guarantees, but such guarantees are not portable and shall not be relied upon unless explicitly documented.
>
> In other words, in cases where the alignment requirement of a type `T` exceeds `alignof(std::max_align_t)`, the storage returned by `buffer.data()` is not guaranteed to be suitably aligned for `T`. Consequently, forming a pointer to `T` by means of `reinterpret_cast<T*>(buffer.data())` is conditionally-supported and may result in undefined behavior. In such circumstances, alternative access mechanisms that do not require reinterpretation of storage are preferred. Specifically, utility methods such as `read_as<T>(size_type)` and `write_as<T>(const T&, size_type)` are intended to support access to values of type `T` without imposing alignment requirements on the underlying storage. These functions operate by copying the object representation of `T` into a properly aligned temporary object, typically via `std::memcpy`, and as such, avoid violations of the alignment constraints imposed by [\[basic.align\]](https://eel.is/c++draft/basic.align) and [\[expr.reinterpret.cast\]](https://eel.is/c++draft/expr.reinterpret.cast).

#### Synopsis

```cpp
enum class Endian {
  Little = /* implementation-defined */,
  Big    = /* implementation-defined */,
  Native = /* implementation-defined */
};

class Buffer {
public:
  // Member types
  using value_type             = std::byte;
  using allocator_type         = std::allocator<std::byte>;
  using size_type              = std::size_t;
  using difference_type        = std::ptrdiff_t;
  using reference              = value_type&;
  using const_reference        = const value_type&;
  using pointer                = typename std::allocator_traits<allocator_type>::pointer;
  using const_pointer          = typename std::allocator_traits<allocator_type>::const_pointer;
  using iterator               = /* implementation-defined contiguous iterator */;
  using const_iterator         = /* implementation-defined contiguous iterator */;
  using reverse_iterator       = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  // Constructors
  Buffer() noexcept(noexcept(allocator_type()));
  explicit Buffer(const allocator_type& alloc) noexcept;
  explicit Buffer(size_type count, const allocator_type& alloc = allocator_type());
  Buffer(size_type count, value_type value, const allocator_type& alloc = allocator_type());
  template <class InputIt>
  Buffer(InputIt first, InputIt last, const allocator_type& alloc = allocator_type());
  Buffer(std::initializer_list<value_type> init, const allocator_type& alloc = allocator_type());

  Buffer(const Buffer& other);
  Buffer(const Buffer& other, const allocator_type& alloc);
  Buffer(Buffer&& other) noexcept;
  Buffer(Buffer&& other, const allocator_type& alloc);

  // Construct from raw data
  Buffer(const void* data, size_type count, const allocator_type& alloc = allocator_type());

  // Destructor
  ~Buffer();

  // Assignment
  Buffer& operator=(const Buffer& other);
  Buffer& operator=(Buffer&& other) noexcept(/* see below */);
  Buffer& operator=(std::initializer_list<value_type> ilist);

  // Element access
  reference at(size_type pos);
  const_reference at(size_type pos) const;
  reference operator[](size_type pos) noexcept;
  const_reference operator[](size_type pos) const noexcept;
  reference front() noexcept;
  const_reference front() const noexcept;
  reference back() noexcept;
  const_reference back() const noexcept;
  pointer data() noexcept;
  const_pointer data() const noexcept;

  // Iterators
  iterator begin() noexcept;
  const_iterator begin() const noexcept;
  const_iterator cbegin() const noexcept;
  iterator end() noexcept;
  const_iterator end() const noexcept;
  const_iterator cend() const noexcept;
  reverse_iterator rbegin() noexcept;
  const_reverse_iterator rbegin() const noexcept;
  const_reverse_iterator crbegin() const noexcept;
  reverse_iterator rend() noexcept;
  const_reverse_iterator rend() const noexcept;
  const_reverse_iterator crend() const noexcept;

  // Capacity
  bool empty() const noexcept;
  size_type size() const noexcept;
  size_type max_size() const noexcept;
  void reserve(size_type new_cap);
  size_type capacity() const noexcept;
  void shrink_to_fit();

  // Modifiers
  void clear() noexcept;
  iterator insert(const_iterator pos, const value_type& value);
  iterator insert(const_iterator pos, value_type&& value);
  iterator insert(const_iterator pos, size_type count, const value_type& value);
  template <class InputIt>
  iterator insert(const_iterator pos, InputIt first, InputIt last);
  iterator insert(const_iterator pos, std::initializer_list<value_type> ilist);

  template <class... Args>
  iterator emplace(const_iterator pos, Args&&... args);

  iterator erase(const_iterator pos);
  iterator erase(const_iterator first, const_iterator last);

  void push_back(const value_type& value);
  void push_back(value_type&& value);
  template <class... Args>
  reference emplace_back(Args&&... args);
  void pop_back();

  void resize(size_type count);
  void resize(size_type count, const value_type& value);

  void swap(Buffer& other) noexcept(/* see below */);

  // Additional modifiers for binary data
  void append(const void* data, size_type count);
  void append(const Buffer& other);
  void prepend(const void* data, size_type count);
  void prepend(const Buffer& other);

  // Allocator
  allocator_type get_allocator() const noexcept;

  // Type conversion utilities
  template <typename T>
  T* cast() noexcept;
  template <typename T>
  const T* cast() const noexcept;

  template <typename T>
  T read_as(size_type offset = 0) const;
  template <typename T>
  void write_as(const T& value, size_type offset = 0);

  // Endian-aware type conversion utilities
  template <typename T, Endian E = Endian::Native>
  T read_value(size_type offset) const;
  template <typename T, Endian E = Endian::Native>
  void write_value(const T& value, size_type offset);

  // Subrange operations
  Buffer slice(size_type offset, size_type count = npos) const;

  // Comparison with other buffers
  int compare(const Buffer& other) const noexcept;
  bool starts_with(const Buffer& prefix) const noexcept;
  bool ends_with(const Buffer& suffix) const noexcept;

  // Search operations
  size_type find(value_type value, size_type pos = 0) const noexcept;
  size_type find(const Buffer& pattern, size_type pos = 0) const noexcept;

  // Fill operations
  void fill(value_type value) noexcept;
  void zero() noexcept;

public:
  // Constants
  static constexpr size_type npos = std::numeric_limits<size_type>::max();
};

// Non-member functions
bool operator==(const Buffer& lhs, const Buffer& rhs) noexcept;
bool operator!=(const Buffer& lhs, const Buffer& rhs) noexcept;
bool operator<(const Buffer& lhs, const Buffer& rhs) noexcept;
bool operator<=(const Buffer& lhs, const Buffer& rhs) noexcept;
bool operator>(const Buffer& lhs, const Buffer& rhs) noexcept;
bool operator>=(const Buffer& lhs, const Buffer& rhs) noexcept;

void swap(Buffer& lhs, Buffer& rhs) noexcept(noexcept(lhs.swap(rhs)));

// Concatenation
Buffer operator+(const Buffer& lhs, const Buffer& rhs);
Buffer operator+(Buffer&& lhs, const Buffer& rhs);
Buffer operator+(const Buffer& lhs, Buffer&& rhs);
Buffer operator+(Buffer&& lhs, Buffer&& rhs);

// Hash support
template <>
struct std::hash<Buffer> {
  std::size_t operator()(const Buffer& buffer) const noexcept;
};
```

#### Member Functions

1. **Constructors**

```cpp
Buffer() noexcept(noexcept(allocator_type()));
```

- **Effects:** Constructs an empty `Buffer`.
- **Postconditions:** `size() == 0`, `capacity()` is unspecified, `data()` is unspecified.
- **Complexity:** Constant.

```cpp
explicit Buffer(const allocator_type& alloc) noexcept;
```

- **Effects:** Constructs an empty `Buffer` with the given allocator.
- **Postconditions:** `size() == 0`, `capacity()` is unspecified, `get_allocator() == alloc`.
- **Complexity:** Constant.

```cpp
explicit Buffer(size_type count, const allocator_type& alloc = allocator_type());
```

- **Effects:** Constructs a `Buffer` with `count` default-constructed elements.
- **Parameters:**
  - `count`: Number of elements to construct.
  - `alloc`: Allocator to use for memory management.
- **Postconditions:** `size() == count`, `capacity() >= count`.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in `count`.

```cpp
Buffer(size_type count, value_type value, const allocator_type& alloc = allocator_type());
```

- **Effects:** Constructs a `Buffer` with `count` copies of `value`.
- **Parameters:**
  - `count`: Number of elements to construct.
  - `value`: Value to initialize elements with.
  - `alloc`: Allocator to use for memory management.
- **Postconditions:** `size() == count`, `capacity() >= count`.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in `count`.

```cpp
template<class InputIt>
Buffer(InputIt first, InputIt last, const allocator_type& alloc = allocator_type());
```

- **Effects:** Constructs a `Buffer` with the contents of the range `[first, last)`.
- **Parameters:**
  - `first`, `last`: Input iterators defining the source range.
  - `alloc`: Allocator to use for memory management.
- **Postconditions:** `size() == std::distance(first, last)`.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in `std::distance(first, last)`.

```cpp
Buffer(const Buffer& other);
```

- **Effects:** Constructs a `Buffer` with a copy of the contents of `other`.
- **Postconditions:** `*this == other`.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in `other.size()`.

```cpp
Buffer(const Buffer& other, const allocator_type& alloc);
```

- **Effects:** Constructs a `Buffer` with a copy of the contents of `other` using the given allocator.
- **Postconditions:** `size() == other.size()`, `get_allocator() == alloc`.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in `other.size()`.

```cpp
Buffer(Buffer&& other) noexcept;
```

- **Effects:** Constructs a `Buffer` by moving the contents of `other`.
- **Postconditions:** `other` is in a valid but unspecified state.
- **Complexity:** Constant.

```cpp
Buffer(Buffer&& other, const allocator_type& alloc);
```

- **Effects:** Constructs a `Buffer` by moving the contents of `other` using the given allocator.
- **Postconditions:** `get_allocator() == alloc`. If `alloc == other.get_allocator()`, `other` is in a valid but unspecified state.
- **Exceptions:** `std::bad_alloc` if memory allocation fails and `alloc != other.get_allocator()`.
- **Complexity:** Constant if `alloc == other.get_allocator()`, otherwise linear in `other.size()`.

```cpp
Buffer(std::initializer_list<value_type> init, const allocator_type& alloc = allocator_type());
```

- **Effects:** Constructs a `Buffer` with the contents of the initializer list `init`.
- **Postconditions:** `size() == init.size()`.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in `init.size()`.

```cpp
Buffer(const void* data, size_type count, const allocator_type& alloc = allocator_type());
```

- **Effects:** Constructs a `Buffer` with `count` elements, where each element is initialized from the corresponding byte in the memory region pointed to by `data`.
- **Parameters:**
  - `data`: Pointer to source data. If `count` is nonzero, shall point to at least `count` bytes of valid memory.
  - `count`: Number of bytes to copy from `data`.
  - `alloc`: Allocator to use for memory management.
- **Postconditions:** `size() == count`.
- **Exceptions:**
  - `std::invalid_argument` if `data` is null and `count > 0`.
  - `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in `count`.

2. **Destructor**

```cpp
~Buffer();
```

- **Effects:** Destroys all elements in the container and deallocates all allocated storage.
- **Complexity:** Linear in `size()`.

3. **Assignment Operators**

```cpp
Buffer& operator=(const Buffer& other);
```

- **Effects:** Replaces the contents with a copy of the contents of `other`.
- **Returns:** `*this`.
- **Postconditions:** `*this == other`.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in `other.size()`.

```cpp
Buffer& operator=(Buffer&& other) noexcept(/* see below */);
```

- **Effects:** Replaces the contents with those of `other` using move semantics.
- **Returns:** `*this`.
- **Postconditions:** `other` is in a valid but unspecified state.
- **Exceptions:** Nothing if the allocator propagates on move assignment or if `get_allocator() == other.get_allocator()`.
- **Complexity:** Linear in `size()` if allocators do not compare equal and do not propagate, otherwise constant.
- **Remarks** The noexcept specification is `allocator_traits<allocator_type>::propagate_on_container_move_assignment::value || allocator_traits<allocator_type>::is_always_equal::value`.

```cpp
Buffer& operator=(std::initializer_list<value_type> ilist);
```

- **Effects:** Replaces the contents with those identified by initializer list `ilist`.
- **Returns:** `*this`.
- **Postconditions:** `size() == ilist.size()`.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in `ilist.size()`.

4. **Element Access**

```cpp
reference at(size_type pos);
const_reference at(size_type pos) const;
```

- **Effects:** Returns a reference to the element at specified location `pos`, with bounds checking.
- **Returns:** Reference to the requested element.
- **Exceptions:** `std::out_of_range` if `pos >= size()`.
- **Complexity:** Constant.

```cpp
reference operator[](size_type pos) noexcept;
const_reference operator[](size_type pos) const noexcept;
```

- **Effects:** Returns a reference to the element at specified location `pos`. No bounds checking is performed.
- **Parameters:**
  - `pos`: Position of the element to return. Must be less than `size()`.
- **Returns:** Reference to the requested element.
- **Complexity:** Constant.

```cpp
reference front() noexcept;
const_reference front() const noexcept;
```

- **Effects:** Returns a reference to the first element in the container.
- **Preconditions:** `!empty()`.
- **Returns:** Reference to the first element.
- **Complexity:** Constant.

```cpp
reference back() noexcept;
const_reference back() const noexcept;
```

- **Effects:** Returns a reference to the last element in the container.
- **Preconditions:** `!empty()`.
- **Returns:** Reference to the last element.
- **Complexity:** Constant.

```cpp
pointer data() noexcept;
const_pointer data() const noexcept;
```

- **Effects:** Returns a pointer to the underlying array serving as element storage.
- **Returns:** Pointer to the underlying element storage. For non-empty containers, the returned pointer compares equal to the address of the first element.
- **Complexity:** Constant.

5. **Iterators**

```cpp
iterator begin() noexcept;
const_iterator begin() const noexcept;
const_iterator cbegin() const noexcept;
```

- **Effects:** Returns an iterator to the first element of the container.
- **Returns:** Iterator to the first element. If the container is empty, the returned iterator will be equal to `end()`.
- **Complexity:** Constant.

```cpp
iterator end() noexcept;
const_iterator end() const noexcept;
const_iterator cend() const noexcept;
```

- **Effects:** Returns an iterator to the element following the last element of the container.
- **Returns:** Iterator to the element following the last element.
- **Complexity:** Constant.

```cpp
reverse_iterator rbegin() noexcept;
const_reverse_iterator rbegin() const noexcept;
const_reverse_iterator crbegin() const noexcept;
```

- **Effects:** Returns a reverse iterator to the first element of the reversed container.
- **Returns:** Reverse iterator to the first element of the reversed container, corresponding to the last element of the non-reversed container.
- **Complexity:** Constant.

```cpp
reverse_iterator rend() noexcept;
const_reverse_iterator rend() const noexcept;
const_reverse_iterator crend() const noexcept;
```

- **Effects:** Returns a reverse iterator to the element following the last element of the reversed container.
- **Returns:** Reverse iterator to the element following the last element of the reversed container, corresponding to the element preceding the first element of the non-reversed container.
- **Complexity:** Constant.

6. **Capacity**

```cpp
bool empty() const noexcept;
```

- **Effects:** Checks whether the container is empty.
- **Returns:** `true` if the container is empty, `false` otherwise.
- **Complexity:** Constant.

```cpp
size_type size() const noexcept;
```

- **Effects:** Returns the number of elements in the container.
- **Returns:** The number of elements in the container.
- **Complexity:** Constant.

```cpp
size_type max_size() const noexcept;
```

- **Effects:** Returns the maximum number of elements the container is able to hold.
- **Returns:** Maximum number of elements.
- **Complexity:** Constant.

```cpp
void reserve(size_type new_cap);
```

- **Effects:** Increases the capacity of the container to a value that is greater or equal to `new_cap`. If `new_cap` is greater than the current `capacity()`, new storage is allocated, otherwise the function does nothing.
- **Parameters:**
  - `new_cap`: New capacity of the container.
- **Exceptions:** `std::bad_alloc` if memory allocation fails or `std::length_error` if `new_cap > max_size()`.
- **Complexity:** At most linear in `size()`.

```cpp
size_type capacity() const noexcept;
```

- **Effects:** Returns the number of elements that the container has currently allocated space for.
- **Returns:** Capacity of the currently allocated storage.
- **Complexity:** Constant.

```cpp
void shrink_to_fit();
```

- **Effects:** Requests the removal of unused capacity. It is a non-binding request to reduce `capacity()` to `size()`.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** At most linear in `size()`.

7. **Modifiers**

```cpp
void clear() noexcept;
```

- **Effects:** Erases all elements from the container.
- **Postconditions:** `size() == 0`.
- **Remarks** Leaves the `capacity()` unchanged.
- **Complexity:** Linear in `size()`.

```cpp
iterator insert(const_iterator pos, const value_type& value);
```

- **Effects:** Inserts `value` before `pos`.
- **Parameters:**
  - `pos`: Iterator before which the content will be inserted. May be the `end()` iterator.
  - `value`: Element value to insert.
- **Returns:** Iterator pointing to the inserted value.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in the number of operations on `end() - pos`.

```cpp
iterator insert(const_iterator pos, value_type&& value);
```

- **Effects:** Inserts `value` before `pos` using move semantics.
- **Parameters:**
  - `pos`: Iterator before which the content will be inserted.
  - `value`: Element value to insert.
- **Returns:** Iterator pointing to the inserted value.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in the number of operations on `end() - pos`.

```cpp
iterator insert(const_iterator pos, size_type count, const value_type& value);
```

- **Effects:** Inserts `count` copies of `value` before `pos`.
- **Parameters:**
  - `pos`: Iterator before which the content will be inserted.
  - `count`: Number of elements to insert.
  - `value`: Element value to insert.
- **Returns:** Iterator pointing to the first element inserted, or `pos` if `count == 0`.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in `count + (end() - pos)`.

```cpp
template<class InputIt>
iterator insert(const_iterator pos, InputIt first, InputIt last);
```

- **Effects:** Inserts elements from range `[first, last)` before `pos`.
- **Parameters:**
  - `pos`: Iterator before which the content will be inserted.
  - `first`, `last`: The range of elements to insert.
- **Returns:** Iterator pointing to the first element inserted, or `pos` if `first == last`.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in `std::distance(first, last) + (end() - pos)`.

```cpp
iterator insert(const_iterator pos, std::initializer_list<value_type> ilist);
```

- **Effects:** Inserts elements from initializer list `ilist` before `pos`.
- **Returns:** Iterator pointing to the first element inserted, or `pos` if `ilist` is empty.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in `ilist.size() + (end() - pos)`.

```cpp
template<class... Args>
iterator emplace(const_iterator pos, Args&&... args);
```

- **Effects:** Inserts a new element constructed in-place from `args` before `pos`.
- **Parameters:**
  - `pos`: Iterator before which the new element will be constructed.
  - `args`: Arguments to forward to the constructor of the element.
- **Returns:** Iterator pointing to the emplaced element.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in the number of operations on `end() - pos`.

```cpp
iterator erase(const_iterator pos);
```

- **Effects:** Removes the element at `pos`.
- **Parameters:**
  - `pos`: Iterator to the element to remove.
- **Preconditions:** `pos` is a valid dereferenceable iterator of `*this`.
- **Returns:** Iterator following the last element removed. If `pos` refers to the last element, then `end()` is returned.
- **Complexity:** Linear in the number of operations on `end() - pos`.

```cpp
iterator erase(const_iterator first, const_iterator last);
```

- **Effects:** Removes the elements in the range `[first, last)`.
- **Parameters:**
  - `first`, `last`: Range of elements to remove.
- **Preconditions:** `[first, last)` is a valid range in `*this`.
- **Returns:** Iterator following the last element removed. If `last == end()` prior to removal, then `end()` is returned.
- **Complexity:** Linear in the number of operations on `end() - first`.

```cpp
void push_back(const value_type& value);
```

- **Effects:** Appends the given element `value` to the end of the container.
- **Parameters:**
  - `value`: The value of the element to append.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Amortized constant.

```cpp
void push_back(value_type&& value);
```

- **Effects:** Appends the given element `value` to the end of the container using move semantics.
- **Parameters:**
  - `value`: The value of the element to append.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Amortized constant.

```cpp
template<class... Args>
reference emplace_back(Args&&... args);
```

- **Effects:** Appends a new element to the end of the container, constructed in-place from `args`.
- **Parameters:**
  - `args`: Arguments to forward to the constructor of the element.
- **Returns:** Reference to the inserted element.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Amortized constant.

```cpp
void pop_back();
```

- **Effects:** Removes the last element of the container.
- **Preconditions:** `!empty()`.
- **Complexity:** Constant.

```cpp
void resize(size_type count);
```

- **Effects:** Resizes the container to contain `count` elements.
- **Parameters:**
  - `count`: New size of the container.
- **Postconditions:** `size() == count`.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in the number of elements inserted or erased.

```cpp
void resize(size_type count, const value_type& value);
```

- **Effects:** Resizes the container to contain `count` elements. If the current size is less than `count`, additional copies of `value` are appended.
- **Parameters:**
  - `count`: New size of the container.
  - `value`: The value to initialize the new elements with.
- **Postconditions:** `size() == count`.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in the number of elements inserted or erased.

```cpp
void swap(Buffer& other) noexcept(/* see below */);
```

- **Effects:** Exchanges the contents of the container with those of `other`.
- **Parameters:**
  - `other`: Container to exchange the contents with.
- **Complexity:** Constant.
- **Remarks** The noexcept specification is `allocator_traits<allocator_type>::propagate_on_container_swap::value || allocator_traits<allocator_type>::is_always_equal::value`.

8. **Binary Data Modifiers**

```cpp
void append(const void* data, size_type count);
```

- **Effects:** Equivalent to `insert(end(), static_cast<const value_type*>(data), static_cast<const value_type*>(data) + count)`.
- **Parameters:**
  - `data`: Pointer to source data. If `count` is nonzero, shall point to at least `count` bytes of valid memory.
  - `count`: Number of bytes to append.
- **Exceptions:**
  - `std::invalid_argument` if `data` is null and `count > 0`.
  - `std::bad_alloc` if memory allocation fails.
- **Complexity:** Amortized constant if the new size is less than or equal to `capacity()`, otherwise linear in the new size.

```cpp
void append(const Buffer& other);
```

- **Effects:** Equivalent to `insert(end(), other.begin(), other.end())`.
- **Parameters:**
  - `other`: Buffer whose contents are to be appended.
- **Complexity:** Amortized constant if the new size is less than or equal to `capacity()`, otherwise linear in the new size.

```cpp
void prepend(const void* data, size_type count);
```

- **Effects:** Equivalent to `insert(begin(), static_cast<const value_type*>(data), static_cast<const value_type*>(data) + count)`.
- **Parameters:**
  - `data`: Pointer to source data. If `count` is nonzero, shall point to at least `count` bytes of valid memory.
  - `count`: Number of bytes to prepend.
- **Exceptions:**
  - `std::invalid_argument` if `data` is null and `count > 0`.
  - `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in `size() + count`.

```cpp
void prepend(const Buffer& other);
```

- **Effects:** Equivalent to `insert(begin(), other.begin(), other.end())`.
- **Parameters:**
  - `other`: Buffer whose contents are to be prepended.
- **Complexity:** Linear in `size() + other.size()`.

9. **Allocator**

```cpp
allocator_type get_allocator() const noexcept;
```

- **Effects:** Returns the allocator associated with the container.
- **Returns:** The associated allocator.
- **Complexity:** Constant.

10. **Type Conversion Utilities**

```cpp
template<typename T>
T* cast() noexcept;
template<typename T>
const T* cast() const noexcept;
```

- **Effects:** Returns `reinterpret_cast<T*>(data())` or `reinterpret_cast<const T*>(data())` respectively.
- **Returns:** Pointer to the first element interpreted as type `T`.
- **Remarks**
  - The caller is responsible for ensuring `size() >= sizeof(T)` and proper alignment.
  - Behavior is undefined if `T` has stricter alignment requirements than `value_type`. See above.
  - For types requiring specific alignment, use `read_as<T>()` instead.
- **Complexity:** Constant.

```cpp
template<typename T>
T read_as(size_type offset = 0) const;
```

- **Effects:** Reads `sizeof(T)` bytes starting at `offset` and returns the result as an object of type `T`.
- **Parameters:**
  - `offset`: Byte offset within the buffer to start reading from.
- **Returns:** Object of type `T` constructed by copying bytes from the buffer.
- **Exceptions:** `std::out_of_range` if `offset + sizeof(T) > size()`.
- **Remarks**
  - Uses `std::memcpy` internally, avoiding alignment issues.
  - Only valid for trivially copyable types.
  - No endianness conversion is performed.
- **Complexity:** Constant.

```cpp
template<typename T>
void write_as(const T& value, size_type offset = 0);
```

- **Effects:** Writes the object representation of `value` to the buffer starting at `offset`.
- **Parameters:**
  - `value`: Object whose byte representation will be written to the buffer.
  - `offset`: Byte offset within the buffer to start writing to.
- **Exceptions:** `std::out_of_range` if `offset + sizeof(T) > size()`.
- **Remarks**
  - Uses `std::memcpy` internally.
  - Only valid for trivially copyable types.
  - Does not resize the buffer; space must already exist.
- **Complexity:** Constant.

11. **Endian-Aware Type Conversion Utilities**

```cpp
template<typename T, Endian E = Endian::Native>
T read_value(size_type offset) const;
```

- **Effects:** Reads `sizeof(T)` bytes from the buffer starting at `offset`, interprets them according to endianness `E`, and returns the result as type `T`.
- **Parameters:**
  - `offset`: Byte index at which to begin reading.
- **Returns:** A value of type `T` reconstructed from the byte sequence in the specified endianness.
- **Exceptions:** `std::out_of_range` if `offset + sizeof(T) > size()`.
- **Remarks**
  - Only valid for trivially copyable types.
  - Implementations are encouraged to provide optimized specializations for standard unsigned and signed integer types to reduce overhead from generic byte-shuffling.
  - When `E == Endian::Native`, equivalent to `read_as<T>(offset)`.
- **Complexity:** Constant.

```cpp
template<typename T, Endian E = Endian::Native>
void write_value(const T& value, size_type offset);
```

- **Effects:** Copies the object representation of `value` (i.e. its bytes) into the buffer starting at `offset`, in endianness `E`.
- **Parameters:**
  - `value`: The value whose bytes are to be written.
  - `offset`: Byte index within the buffer at which to begin writing.
- **Exceptions:** `std::out_of_range` if `offset + sizeof(T) > size()`.
- **Remarks**
  - Only valid for trivially copyable types.
  - Does not change `size()`.
  - Callers who need to ensure space must call `resize()` or `reserve()` ahead of time.
  - When `E == Endian::Native`, equivalent to `write_as<T>(value, offset)`.
- **Complexity:** Constant.

12. **Subrange Operations**

```cpp
Buffer slice(size_type offset, size_type count = npos) const;
```

- **Effects:** Returns a new `Buffer` containing a copy of the elements from `offset` to `offset + min(count, size() - offset)`.
- **Parameters:**
  - `offset`: Starting position. Must be less than or equal to `size()`.
  - `count`: Maximum number of elements to include in the slice.
- **Returns:** A `Buffer` containing the requested subsequence.
- **Exceptions:** `std::out_of_range` if `offset > size()`.
- **Complexity:** Linear in the number of elements copied.

13. **Comparison Operations**

```cpp
int compare(const Buffer& other) const noexcept;
```

- **Effects:** Compares the contents of this buffer with `other` lexicographically.
- **Returns:**
  - Negative value if `*this` is lexicographically less than `other`.
  - Zero if the contents are equivalent.
  - Positive value if `*this` is lexicographically greater than `other`.
- **Complexity:** Linear in `min(size(), other.size())`.

```cpp
bool starts_with(const Buffer& prefix) const noexcept;
```

- **Effects:** Checks whether the buffer starts with the given prefix.
- **Returns:** `true` if `size() >= prefix.size()` and the first `prefix.size()` elements are equal to the elements of `prefix`.
- **Complexity:** Linear in `prefix.size()`.

```cpp
bool ends_with(const Buffer& suffix) const noexcept;
```

- **Effects:** Checks whether the buffer ends with the given suffix.
- **Returns:** `true` if `size() >= suffix.size()` and the last `suffix.size()` elements are equal to the elements of `suffix`.
- **Complexity:** Linear in `suffix.size()`.

14. **Search Operations**

```cpp
size_type find(value_type value, size_type pos = 0) const noexcept;
```

- **Effects:** Finds the first occurrence of `value` starting from position `pos`.
- **Parameters:**
  - `value`: Element value to search for.
  - `pos`: Position at which to start the search.
- **Returns:** Position of the first occurrence, or `npos` if not found.
- **Complexity:** Linear in `size() - pos`.

```cpp
size_type find(const Buffer& pattern, size_type pos = 0) const noexcept;
```

- **Effects:** Finds the first occurrence of the subsequence `pattern` starting from position `pos`.
- **Parameters:**
  - `pattern`: Subsequence to search for.
  - `pos`: Position at which to start the search.
- **Returns:** Position of the first occurrence, or `npos` if not found.
- **Complexity:** Generally linear in `size() - pos`, but may be worse depending on the pattern.

15. **Fill Operations**

```cpp
void fill(value_type value) noexcept;
```

- **Effects:** Assigns `value` to all elements in the buffer.
- **Parameters:**
  - `value`: Value to assign to all elements.
- **Complexity:** Linear in `size()`.

```cpp
void zero() noexcept;
```

- **Effects:** Sets all bytes in the buffer to zero. Equivalent to `fill(std::byte{0})` but potentially optimized.
- **Complexity:** Linear in `size()`.

#### Non-Member Functions

1. **Comparison Operators**

```cpp
bool operator==(const Buffer& lhs, const Buffer& rhs) noexcept;
```

- **Effects:** Checks if the contents of `lhs` and `rhs` are equal.
- **Returns:** `true` if the contents are equal, `false` otherwise.
- **Complexity:** Linear in the size of the shorter sequence.

```cpp
bool operator!=(const Buffer& lhs, const Buffer& rhs) noexcept;
```

- **Effects:** Checks if the contents of `lhs` and `rhs` are not equal.
- **Returns:** `!(lhs == rhs)`.
- **Complexity:** Linear in the size of the shorter sequence.

```cpp
bool operator<(const Buffer& lhs, const Buffer& rhs) noexcept;
```

- **Effects:** Compares the contents of `lhs` and `rhs` lexicographically.
- **Returns:** `true` if `lhs` is lexicographically less than `rhs`, `false` otherwise.
- **Complexity:** Linear in the size of the shorter sequence.

```cpp
bool operator<=(const Buffer& lhs, const Buffer& rhs) noexcept;
```

- **Effects:** Compares the contents of `lhs` and `rhs` lexicographically.
- **Returns:** `!(rhs < lhs)`.
- **Complexity:** Linear in the size of the shorter sequence.

```cpp
bool operator>(const Buffer& lhs, const Buffer& rhs) noexcept;
```

- **Effects:** Compares the contents of `lhs` and `rhs` lexicographically.
- **Returns:** `rhs < lhs`.
- **Complexity:** Linear in the size of the shorter sequence.

```cpp
bool operator>=(const Buffer& lhs, const Buffer& rhs) noexcept;
```

- **Effects:** Compares the contents of `lhs` and `rhs` lexicographically.
- **Returns:** `!(lhs < rhs)`.
- **Complexity:** Linear in the size of the shorter sequence.

2. **Swap Function**

```cpp
void swap(Buffer& lhs, Buffer& rhs) noexcept(noexcept(lhs.swap(rhs)));
```

- **Effects:** Exchanges the contents of `lhs` and `rhs`. Equivalent to `lhs.swap(rhs)`.
- **Complexity:** Constant.

3. **Concatenation Operators**

```cpp
Buffer operator+(const Buffer& lhs, const Buffer& rhs);
```

- **Effects:** Returns a new `Buffer` containing the concatenation of `lhs` and `rhs`.
- **Returns:** `Buffer` with contents equivalent to `lhs` followed by `rhs`.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in `lhs.size() + rhs.size()`.

```cpp
Buffer operator+(Buffer&& lhs, const Buffer& rhs);
```

- **Effects:** Returns a `Buffer` containing the concatenation of `lhs` and `rhs`, potentially reusing storage from `lhs`.
- **Returns:** `Buffer` with contents equivalent to `lhs` followed by `rhs`.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in `rhs.size()` if `lhs` has sufficient capacity, otherwise linear in `lhs.size() + rhs.size()`.

```cpp
Buffer operator+(const Buffer& lhs, Buffer&& rhs);
```

- **Effects:** Returns a `Buffer` containing the concatenation of `lhs` and `rhs`, potentially reusing storage from `rhs`.
- **Returns:** `Buffer` with contents equivalent to `lhs` followed by `rhs`.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in `lhs.size() + rhs.size()`.

```cpp
Buffer operator+(Buffer&& lhs, Buffer&& rhs);
```

- **Effects:** Returns a `Buffer` containing the concatenation of `lhs` and `rhs`, potentially reusing storage from `lhs`.
- **Returns:** `Buffer` with contents equivalent to `lhs` followed by `rhs`.
- **Exceptions:** `std::bad_alloc` if memory allocation fails.
- **Complexity:** Linear in `rhs.size()` if `lhs` has sufficient capacity, otherwise linear in `lhs.size() + rhs.size()`.

4. **Hash Support**

```cpp
template<>
struct std::hash<Buffer> {
    std::size_t operator()(const Buffer& buffer) const noexcept;
};
```

- **Effects:** Computes a hash value for the given `Buffer`.
- **Parameters:**
  - `buffer`: The `Buffer` object to hash.
- **Returns:** A hash value for `buffer`.
- **Remarks**
  - Two `Buffer` objects that compare equal have the same hash value.
  - The hash function should provide good distribution for typical buffer contents.
- **Complexity:** Linear in `buffer.size()`.

---

### `BufferView`

The class `BufferView` provides a non-owning, read-only view over a contiguous sequence of bytes. It encapsulates a pointer to constant byte data and a size, offering lightweight access to binary data without copying or taking ownership. `BufferView` is designed to be trivially copyable and provides an interface similar to `std::string_view` but specialized for binary data operations.

#### Synopsis

```cpp
class BufferView {
public:
  // Member types
  using value_type             = std::byte;
  using size_type              = std::size_t;
  using difference_type        = std::ptrdiff_t;
  using reference              = value_type&;
  using const_reference        = const value_type&;
  using pointer                = value_type*;
  using const_pointer          = const value_type*;
  using const_iterator         = /* implementation-defined contiguous iterator */;
  using iterator               = const_iterator;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using reverse_iterator       = const_reverse_iterator;

  // Constructors
  constexpr BufferView() noexcept;
  constexpr BufferView(const BufferView& other) noexcept = default;
  constexpr BufferView(const value_type* data, size_type size) noexcept;
  constexpr BufferView(const Buffer& buffer) noexcept;

  template <std::size_t N>
  constexpr BufferView(const value_type (&array)[N]) noexcept;

  template <class Container>
  constexpr BufferView(const Container& container) noexcept(/* see below */);

  // Assignment
  constexpr BufferView& operator=(const BufferView& other) noexcept = default;

  // Element access
  constexpr const_reference at(size_type pos) const;
  constexpr const_reference operator[](size_type pos) const noexcept;
  constexpr const_reference front() const noexcept;
  constexpr const_reference back() const noexcept;
  constexpr const_pointer data() const noexcept;

  // Iterators
  constexpr const_iterator begin() const noexcept;
  constexpr const_iterator cbegin() const noexcept;
  constexpr const_iterator end() const noexcept;
  constexpr const_iterator cend() const noexcept;
  constexpr const_reverse_iterator rbegin() const noexcept;
  constexpr const_reverse_iterator crbegin() const noexcept;
  constexpr const_reverse_iterator rend() const noexcept;
  constexpr const_reverse_iterator crend() const noexcept;

  // Capacity
  constexpr bool empty() const noexcept;
  constexpr size_type size() const noexcept;
  constexpr size_type length() const noexcept;
  constexpr size_type max_size() const noexcept;

  // Modifiers
  constexpr void remove_prefix(size_type n) noexcept;
  constexpr void remove_suffix(size_type n) noexcept;
  constexpr void swap(BufferView& other) noexcept;

  // Subviews
  constexpr BufferView subview(size_type offset, size_type count = npos) const noexcept;
  constexpr BufferView first(size_type count) const noexcept;
  constexpr BufferView last(size_type count) const noexcept;

  // Type conversion utilities
  template <typename T>
  const T* cast() const noexcept;

  template <typename T>
  T read_as(size_type offset = 0) const;

  // Endian-aware type conversion utilities
  template <typename T, Endian E = Endian::Native>
  T read_value(size_type offset) const;

  // Comparison with other views
  constexpr int compare(BufferView other) const noexcept;
  constexpr bool starts_with(BufferView prefix) const noexcept;
  constexpr bool ends_with(BufferView suffix) const noexcept;

  // Search operations
  constexpr size_type find(value_type value, size_type pos = 0) const noexcept;
  size_type find(BufferView pattern, size_type pos = 0) const noexcept;

public:
  // Constants
  static constexpr size_type npos = std::numeric_limits<size_type>::max();
};

// Non-member functions
constexpr bool operator==(BufferView lhs, BufferView rhs) noexcept;
constexpr bool operator!=(BufferView lhs, BufferView rhs) noexcept;
constexpr bool operator<(BufferView lhs, BufferView rhs) noexcept;
constexpr bool operator<=(BufferView lhs, BufferView rhs) noexcept;
constexpr bool operator>(BufferView lhs, BufferView rhs) noexcept;
constexpr bool operator>=(BufferView lhs, BufferView rhs) noexcept;

constexpr void swap(BufferView& lhs, BufferView& rhs) noexcept;

template <>
struct std::hash<BufferView> {
  std::size_t operator()(const BufferView& view) const noexcept;
};
```

#### Member Functions

1. **Default Constructor**

```cpp
constexpr BufferView() noexcept;
```

- **Effects:** Constructs an empty `BufferView`.
- **Postconditions:** `data() == nullptr` and `size() == 0`.

2. **Pointer and Size Constructor**

```cpp
constexpr BufferView(const value_type* data, size_type size) noexcept;
```

- **Effects:** Constructs a `BufferView` referring to the range `[data, data + size)`.
- **Parameters:**
  - `data`: Pointer to the first byte. May be `nullptr` if and only if `size == 0`.
  - `size`: Number of bytes in the view.
- **Postconditions:** `this->data() == data` and `this->size() == size`.
- **Remarks** The behavior is undefined if `data` is `nullptr` and `size > 0`, or if `[data, data + size)` is not a valid range.

3. **Buffer Constructor**

```cpp
constexpr BufferView(const Buffer& buffer) noexcept;
```

- **Effects:** Constructs a `BufferView` referring to the range `[buffer.data(), buffer.data() + buffer.size())`.
- **Parameters:**
  - `buffer`: A `Buffer` object.
- **Postconditions:** `data() == buffer.data()` and `size() == buffer.size()`.

4. **Array Constructor**

```cpp
template <std::size_t N>
constexpr BufferView(const value_type (&array)[N]) noexcept;
```

- **Effects:** Constructs a `BufferView` referring to the array `array`.
- **Parameters:**
  - `array`: A reference to an array of `N` bytes.
- **Postconditions:** `data() == array` and `size() == N`.

5. **Container Constructor**

```cpp
template <class Container>
constexpr BufferView(const Container& container) noexcept(/* see below */);
```

- **Constraints:**
  - `std::is_same_v<std::remove_cv_t<typename Container::value_type>, value_type>` is `true`.
  - `Container` satisfies the requirements of a contiguous container.
  - `Container` is not a specialization of `BufferView`.
- **Effects:** Constructs a `BufferView` referring to the range `[container.data(), container.data() + container.size())`.
- **Parameters:**
  - `container`: A container with contiguous storage of bytes.
- **Postconditions:** `data() == container.data()` and `size() == container.size()`.
- **Remarks:** The expression inside `noexcept` is equivalent to `noexcept(container.data()) && noexcept(container.size())`.

6. **Bounds-Checked Access**

```cpp
constexpr const_reference at(size_type pos) const;
```

- **Effects:** Equivalent to: `return pos < size() ? data()[pos] : throw std::out_of_range{};`
- **Parameters:**
  - `pos`: Position of the byte to return.
- **Returns:** A reference to the byte at position `pos`.
- **Exceptions:** Throws `std::out_of_range` if `pos >= size()`.

7. **Unchecked Access**

```cpp
constexpr const_reference operator[](size_type pos) const noexcept;
```

- **Preconditions:** `pos < size()`.
- **Effects:** Equivalent to: `return data()[pos];`
- **Parameters:**
  - `pos`: Position of the byte to return.
- **Returns:** A reference to the byte at position `pos`.

8. **Front and Back**

```cpp
constexpr const_reference front() const noexcept;
constexpr const_reference back() const noexcept;
```

- **Preconditions:** `!empty()`.
- **Effects:** Equivalent to: `return data()[0];` and `return data()[size() - 1];` respectively.
- **Returns:** A reference to the first or last byte.

9. **Data Access**

```cpp
constexpr const_pointer data() const noexcept;
```

- **Returns:** A pointer to the underlying byte array or `nullptr` if `empty()`.

10. **Iterators**

```cpp
constexpr const_iterator begin() const noexcept;
constexpr const_iterator cbegin() const noexcept;
constexpr const_iterator end() const noexcept;
constexpr const_iterator cend() const noexcept;
```

- **Returns:** An iterator to the beginning or end of the view. `cbegin()` and `cend()` return the same as `begin()` and `end()` respectively.

```cpp
constexpr const_reverse_iterator rbegin() const noexcept;
constexpr const_reverse_iterator crbegin() const noexcept;
constexpr const_reverse_iterator rend() const noexcept;
constexpr const_reverse_iterator crend() const noexcept;
```

- **Returns:** A reverse iterator to the reverse beginning or reverse end of the view.

11. **Capacity**

```cpp
constexpr bool empty() const noexcept;
constexpr size_type size() const noexcept;
constexpr size_type length() const noexcept;
constexpr size_type max_size() const noexcept;
```

- **Returns:**
  - `empty()`: `true` if the view is empty, `false` otherwise.
  - `size()` and `length()`: The number of bytes in the view.
  - `max_size()`: The maximum possible number of bytes in a view, typically `std::numeric_limits<size_type>::max()`.

12. **Modifiers**

```cpp
constexpr void remove_prefix(size_type n) noexcept;
```

- **Effects:** Moves the start of the view forward by `n` characters.
- **Remarks** If `n > size()` is `true`, the result is an empty view.
- **Parameters:**
  - `n`: Number of bytes to remove from the beginning.

```cpp
constexpr void remove_suffix(size_type n) noexcept;
```

- **Effects:** Moves the end of the view back by `n` characters.
- **Remarks** If `n > size()` is `true`, the result is an empty view.
- **Parameters:**
  - `n`: Number of bytes to remove from the beginning.

13. **Swap**

```cpp
constexpr void swap(BufferView& other) noexcept;
```

- **Effects:** Exchanges the values of `*this` and `other`.

14. **Subrange Operations**

```cpp
constexpr BufferView subview(size_type offset, size_type count = npos) const noexcept;
```

- **Effects:** Determines the effective length `rlen` of the returned view as `std::min(count, size() - offset)` if `offset <= size()`, otherwise `0`.
- **Parameters:**
  - `offset`: Position of the first byte in the returned view.
  - `count`: Requested length of the returned view.
- **Returns:** `BufferView(data() + offset, rlen)` if `offset <= size()`, otherwise `BufferView()`.

```cpp
constexpr BufferView first(size_type count) const noexcept;
```

- **Effects:** Equivalent to: `return subview(0, count);`
- **Parameters:**
  - `count`: Number of bytes to include.
- **Returns:** A view of the first `std::min(count, size())` bytes.

```cpp
constexpr BufferView last(size_type count) const noexcept;
```

- **Effects:** Equivalent to: `return count >= size() ? *this : subview(size() - count, count);`
- **Parameters:**
  - `count`: Number of bytes to include.
- **Returns:** A view of the last `std::min(count, size())` bytes.

15. **Type Casting**

```cpp
template <typename T>
const T* cast() const noexcept;
```

- **Effects:** Returns `reinterpret_cast<const T*>(data())`.
- **Preconditions:** `size() >= sizeof(T)` and `data()` is suitably aligned for `T`.
- **Returns:** A pointer to `T` reinterpreted from the first bytes of the view.
- **Remarks** The caller shall ensure proper alignment and sufficient size. See `Buffer`.

16. **Value Reading**

```cpp
template <typename T>
T read_as(size_type offset = 0) const;
```

- **Effects:** Reads `sizeof(T)` bytes from the view starting at `offset` and reconstructs an object of type `T`.
- **Preconditions:** `std::is_trivially_copyable_v<T>` is `true`.
- **Parameters:**
  - `offset`: Byte offset within the view.
- **Returns:** A value of type `T` reconstructed from the bytes.
- **Exceptions:** Throws `std::out_of_range` if `offset + sizeof(T) > size()`.

17. **Endian-Aware Value Reading**

```cpp
template <typename T, Endian E = Endian::Native>
T read_value(size_type offset) const;
```

- **Effects:** Reads `sizeof(T)` bytes from the view starting at `offset`, interprets them according to endianness `E`, and returns the result as type `T`.
- **Preconditions:** `std::is_trivially_copyable_v<T>` is `true`.
- **Parameters:**
  - `offset`: Byte offset within the view.
- **Returns:** A value of type `T` reconstructed from the bytes in the specified endianness.
- **Exceptions:** Throws `std::out_of_range` if `offset + sizeof(T) > size()`.

18. **Comparison**

```cpp
constexpr int compare(BufferView other) const noexcept;
```

- **Effects:** Compares the byte sequences lexicographically.
- **Returns:**
  - A negative value if `*this` is lexicographically less than `other`.
  - Zero if the byte sequences are equivalent.
  - A positive value if `*this` is lexicographically greater than `other`.

19. **Prefix and Suffix Testing**

```cpp
constexpr bool starts_with(BufferView prefix) const noexcept;
constexpr bool ends_with(BufferView suffix) const noexcept;
```

- **Effects:** Determines whether the view starts with `prefix` or ends with `suffix`.
- **Returns:** `true` if the condition is satisfied, `false` otherwise.

20. **Search Operations**

```cpp
constexpr size_type find(value_type value, size_type pos = 0) const noexcept;
size_type find(BufferView pattern, size_type pos = 0) const noexcept;
```

- **Effects:** Finds the first occurrence of `value` or `pattern` starting at position `pos`.
- **Parameters:**
  - `value`: Byte value to search for.
  - `pattern`: Byte sequence to search for.
  - `pos`: Starting position for the search.
- **Returns:** The position of the first match, or `npos` if not found.

#### Non-Member Functions

1. **Comparison Operators**

```cpp
constexpr bool operator==(BufferView lhs, BufferView rhs) noexcept;
constexpr bool operator!=(BufferView lhs, BufferView rhs) noexcept;
constexpr bool operator<(BufferView lhs, BufferView rhs) noexcept;
constexpr bool operator<=(BufferView lhs, BufferView rhs) noexcept;
constexpr bool operator>(BufferView lhs, BufferView rhs) noexcept;
constexpr bool operator>=(BufferView lhs, BufferView rhs) noexcept;
```

- **Effects:** Perform lexicographical comparison of the byte sequences.
- **Returns:** The result of the comparison.

2. **Swap**

```cpp
constexpr void swap(BufferView& lhs, BufferView& rhs) noexcept;
```

- **Effects:** Equivalent to: `lhs.swap(rhs);`

3. **Hash Support**

```cpp
template <>
struct std::hash<BufferView> {
  std::size_t operator()(const BufferView& view) const noexcept;
};
```

- **Effects:** Returns a hash value for the byte sequence.
- **Returns:** A hash value based on the contents of the view.

---

### `ShutdownMode`

The `ShutdownMode` scoped enumeration represents the direction in which a socket's communication is disabled. It determines whether further send and/or receive operations are permitted after shutdown is called.

#### Synopsis

```cpp
enum class ShutdownMode {
  Receive = /* unspecified */,
  Send    = /* unspecified */,
  Both    = /* unspecified */
};
```

#### Constants

| Enumerator | Meaning                                                                       |
| ---------- | ----------------------------------------------------------------------------- |
| `Receive`  | Disables further receive operations (corresponds to POSIX `SHUT_RD`).         |
| `Send`     | Disables further send operations (corresponds to POSIX `SHUT_WR`).            |
| `Both`     | Disables both send and receive operations (corresponds to POSIX `SHUT_RDWR`). |

#### Notes

This enumeration provides a type-safe and portable way to specify shutdown behavior for sockets. Internally, each enumerator maps to the appropriate platform-specific constant. Existing enumerators are stable across supported operating systems.

---

### `SocketKind`

The `SocketKind` scoped enumeration describes the semantic type (service semantics) of a socket. It indicates how the transport interprets byte streams or messages and is used to select correct behavior for I/O, `accept`/`connect` semantics and platform mappings.

#### Synopsis

```cpp
enum class SocketKind {
  Unknown   = /* unspecified */,
  Stream    = /* unspecified */,
  Datagram  = /* unspecified */,
  SeqPacket = /* unspecified */,
  Raw       = /* unspecified */
};
```

#### Constants

| Enumerator  | Meaning                                                                                       |
| ----------- | --------------------------------------------------------------------------------------------- |
| `Unknown`   | Socket type is invalid, uninitialized or cannot be determined.                                |
| `Stream`    | Sequenced, reliable, connection-oriented byte stream (corresponds to POSIX `SOCK_STREAM`).    |
| `Datagram`  | Connectionless, message-oriented datagram semantics (corresponds to POSIX `SOCK_DGRAM`).      |
| `SeqPacket` | Sequenced, reliable, connection-oriented message semantics (corresponds to `SOCK_SEQPACKET`). |
| `Raw`       | Low-level network access to the underlying protocol (corresponds to `SOCK_RAW`).              |

#### Notes

- Implementations should map platform-specific socket kinds to the appropriate enumerator.
- Existing enumerators are stable across supported operating systems.

---

### `SocketOps`

The `SocketOps` abstract interface specifies the minimal platform implementation contract used by the library's hybrid socket layer. It is the thin, low-level, non-throwing surface that concrete platform backends (e.g., POSIX) must implement. Public, user-facing APIs (the `Socket` value type and typed facades) delegate actual system calls to `SocketOps` implementations.

> [!IMPORTANT]
> All classes conforming to the `SocketOps` abstract interface shall:
>
> - Expose only primitives that directly interact with the operating system.
> - Report errors exclusively through `std::error_code&` parameters.
> - Assume that buffer pointers, address objects, and lengths are valid and well-formed.
> - Behave as if `native_handle()` is invalid after `release()`; subsequent operations should fail with an appropriate error (e.g., `EBADF`/`WSAENOTSOCK`) and set `ec`.
> - Neither perform policy checks (such as binding or connection state and type validation) nor retry operations.

#### Synopsis

```cpp
class SocketOps {
public:
  using native_handle_type = /* platform-specific handle type (e.g., int on POSIX, SOCKET on Win32) */;

  virtual ~SocketOps();

  // Ownership and handle
  virtual native_handle_type native_handle() noexcept = 0;
  virtual native_handle_type release() noexcept       = 0;

  // Lifecycle
  virtual void close(std::error_code& ec) noexcept = 0;

  // Address & identity
  virtual void getsockname(Address& out, std::error_code& ec) const noexcept = 0;
  virtual void getpeername(Address& out, std::error_code& ec) const noexcept = 0;

  // Binding / listening / accepting
  virtual void bind(const Address& local, std::error_code& ec) noexcept   = 0;
  virtual void listen(int backlog, std::error_code& ec) noexcept          = 0;
  virtual std::unique_ptr<SocketOps> accept(std::error_code& ec) noexcept = 0;

  // Connecting
  virtual void connect(const Address& remote, std::error_code& ec) noexcept = 0;

  // Data transfer
  virtual std::size_t
  send(const void* buf, std::size_t len, const Address* dest, int flags, std::error_code& ec) noexcept = 0;

  virtual std::pair<std::size_t, std::unique_ptr<Address>>
  recv(void* buf, std::size_t len, int flags, std::error_code& ec) noexcept = 0;

  // Shutdown
  virtual void shutdown(ShutdownMode how, std::error_code& ec) noexcept = 0;

  // Socket options
  virtual void
  setsockopt(int level, int optname, const void* optval, std::size_t optlen, std::error_code& ec) noexcept = 0;
  virtual void
  getsockopt(int level, int optname, void* out, std::size_t& inout_len, std::error_code& ec) const noexcept = 0;

  // Duplication
  virtual std::unique_ptr<SocketOps> duplicate(std::error_code& ec) const noexcept = 0;

  // Introspection (family / kind / protocol)
  virtual AddressFamily family() const noexcept = 0;
  virtual SocketKind kind() const noexcept      = 0;
  virtual int protocol() const noexcept         = 0;
};
```

#### Member Functions

1. **Destructor**

```cpp
virtual ~SocketOps();
```

- **Effects:** Destroys the `SocketOps` object. Implementations must _not_ close the native handle in their destructor if ownership has already been transferred via `release()`. If the `SocketOps` still owns a valid handle, the implementation shall close the handle.
- **Remarks:** The public `Socket` wrapper will typically call `close()` in its destructor; implementations should make closing idempotent.
- **Complexity:** Constant.

2. **Native handle access and release**

```cpp
virtual native_handle_type native_handle() noexcept = 0;
```

- **Returns:** The platform-native socket handle currently owned by the implementation (e.g., file descriptor on POSIX, `SOCKET` on Windows). If the implementation does not currently own a valid handle, return a platform-specific invalid value (e.g., `-1` on POSIX, `INVALID_SOCKET` on Windows).
- **Complexity:** Constant.

```cpp
virtual native_handle_type release() noexcept = 0;
```

- **Effects:**
  - Relinquishes ownership of the underlying native handle and returns it to the caller.
  - After `release()` the `SocketOps` must behave as if it holds no valid handle. Subsequent operations should fail and set `ec` accordingly.
  - `release()` must not close the handle.
- **Returns:** The previously held native handle (platform-specific invalid handle if none was held).
- **Remarks:** If the implementation had a valid handle, it is the caller's responsibility to close the returned native handle.
- **Complexity:** Constant.

3. **Closing**

```cpp
virtual void close(std::error_code& ec) noexcept = 0;
```

- **Effects:** If a valid native handle is owned, perform the platform-specific close operation (e.g., `close(2)` / `closesocket`) and mark the handle invalid. Clear or reset any internal timeout/inheritable state as appropriate.
  - **On success:** `ec` is cleared.
  - **On failure:** `ec` is set to the platform error.
- **Remarks:** Implementations should make `close()` idempotent (closing an already-closed handle should not throw; it should set `ec` to indicate invalid handle or simply clear it).
- **Complexity:** Constant.

4. **Address queries**

```cpp
virtual void getsockname(Address& out, std::error_code& ec) const noexcept = 0;
```

- **Effects:** Populate `out` with the local address the socket is bound to. If the socket is unbound or the operation fails, set `ec` accordingly.
- **Complexity:** Constant.

```cpp
virtual void getpeername(Address& out, std::error_code& ec) const noexcept = 0;
```

- **Effects:** Populate `out` with the remote peer address for connected sockets. If the socket is not connected or the operation fails, set `ec`.
- **Complexity:** Constant.

5. **Binding, listening, accepting**

```cpp
virtual void bind(const Address& local, std::error_code& ec) noexcept = 0;
```

- **Effects:** Perform the platform bind operation to associate the socket with `local`.
- **Complexity:** Constant.

```cpp
virtual void listen(int backlog, std::error_code& ec) noexcept = 0;
```

- **Effects:** Mark the socket as passive to accept incoming connection requests with the platform-specific `listen` call.
- **Parameters:** `backlog` — maximum queued connections; implementations must clamp negative backlog to 0.
- **Complexity:** Constant.

```cpp
virtual std::unique_ptr<SocketOps> accept(std::error_code& ec) noexcept = 0;
```

- **Effects:**
  - Accept a pending connection and return a new `SocketOps` instance that owns the accepted native handle.
  - On platforms that return additional information (remote address), the new instance should represent the accepted connection; the caller can extract `getpeername()` from it.
- **Returns:** On success: `unique_ptr<SocketOps>` owning the new connection and `ec` cleared. On failure: `nullptr` and `ec` set.
- **Remarks:** Implementation must not block indefinitely unless the underlying socket is in blocking mode; non-blocking semantics are inherited from the native socket.
- **Complexity:** Blocking or non-blocking depending on socket mode; complexity is constant per accepted connection.

6. **Connecting**

```cpp
virtual void connect(const Address& remote, std::error_code& ec) noexcept = 0;
```

- **Effects:** Initiate or complete a connection to `remote`. The method performs the raw platform operation (e.g., `connect(2)` / `WSAConnect`). Behavior in non-blocking mode should follow platform semantics: if the operation would block, set `ec` appropriately (e.g., `EINPROGRESS` on POSIX).
- **Remarks:** Higher layers may provide timeout logic and retries; `SocketOps` must not.
- **Complexity:** Potentially unbounded (depends on network and blocking behavior).

7. **Data transfer**

```cpp
virtual std::size_t send(const void* buf,
                         std::size_t len,
                         const Address* dest,
                         int flags,
                         std::error_code& ec) noexcept = 0;
```

- **Purpose:** Perform a single low-level send operation. If `dest` is non-null, send using connectionless semantics (e.g., `sendto`); otherwise use the socket's connected peer semantics (e.g., `send`).
- **Parameters:**
  - `buf` — pointer to data to send (caller ensures validity).
  - `len` — number of bytes to send.
  - `dest` — optional destination address for datagram sockets.
  - `flags` — platform-specific flags (e.g., `MSG_DONTWAIT`); may be ignored by some implementations.
  - `ec` — error out parameter.
- **Returns:** Number of bytes actually sent (> 0) on success. On failure, return value is `0` and `ec` is set.
- **Remarks:** Implementations must not attempt to loop until all bytes are sent. `sendall` semantics are provided by higher-level code.
- **Complexity:** At most `O(len)` for copying into kernel buffers; actual cost depends on platform and network.

```cpp
virtual std::pair<std::size_t, std::unique_ptr<Address>>
  recv(void* buf, std::size_t len, int flags, std::error_code& ec) noexcept = 0;
```

- **Purpose:** Perform a single low-level receive call. For datagram sockets, the returned `Address` points to the source of the datagram. For stream sockets the returned `Address` is `nullptr`.
- **Parameters:**
  - `buf` — pointer to receive buffer (caller ensures validity).
  - `len` — maximum number of bytes to read.
  - `flags` — platform-specific flags (e.g., `MSG_PEEK`).
  - `ec` — error out parameter.
- **Returns:** A pair `(nbytes, source)` where `nbytes` is the number of bytes placed into `buf` (0 <= `nbytes` <= `len`). If `nbytes == 0` and no error, semantics follow platform: on stream sockets, `0` indicates orderly EOF; on datagram sockets, `0` may indicate an empty datagram (platform-dependent). On failure returns `{0, nullptr}` and sets `ec`.
- **Remarks:** If the incoming message length exceeds `len` on datagram/seqpacket sockets, the implementation should return the first `len` bytes and discard the remainder (this mirrors `recvfrom` platform semantics). Implementations must not attempt to reassemble or collect more data than requested.
- **Complexity:** `O(nbytes)`.

8. **Shutdown**

```cpp
virtual void shutdown(ShutdownMode how, std::error_code& ec) noexcept = 0;
```

- **Effects:** Disable further send and/or receive operations on the socket according to `how`. Map `ShutdownMode::Receive/Send/Both` to platform-specific constants (POSIX `SHUT_RD/SHUT_WR/SHUT_RDWR`; Windows `SD_RECEIVE/SD_SEND/SD_BOTH`).
- **Remarks:** Implementations must not also close the socket unless explicitly requested by a higher layer; `shutdown()` only affects the data direction.
- **Complexity:** Constant.

9. **Socket options**

```cpp
virtual void setsockopt(int level, int optname, const void* optval, std::size_t optlen, std::error_code& ec) noexcept = 0;
```

- **Effects:** Set the socket option specified by `level` and `optname` to the value pointed to by `optval` of length `optlen`. Implementations must pass these parameters to the native `setsockopt`/`setsockopt` equivalent or map them appropriately on the platform.
- **Complexity:** Constant.

```cpp
virtual void getsockopt(int level, int optname, void* out, std::size_t& inout_len, std::error_code& ec) const noexcept = 0;
```

- **Effects:** Retrieve the socket option value. `inout_len` is the size of the `out` buffer on entry; on success it must be set to the number of bytes written. If the provided buffer is too small, implementations must set `ec` with an appropriate error and may set `inout_len` to the required size where the platform provides it.
- **Complexity:** Constant.

10. **Duplication**

```cpp
virtual std::unique_ptr<SocketOps> duplicate(std::error_code& ec) const noexcept = 0;
```

- **Effects:** Duplicate the underlying native handle using the appropriate platform API (e.g., `dup`/`dup2`/`WSADuplicateSocket`) and return a new `SocketOps` instance owning the duplicated handle.
- **Returns:** On success, a `unique_ptr<SocketOps>` owning a new implementation with a duplicated handle and `ec` cleared. On failure, `nullptr` and `ec` set.
- **Remarks:** This operation is optional on some platforms; if duplication is unsupported, implementations should set `ec` to an appropriate error (e.g., `ENOSYS` or `EOPNOTSUPP`).
- **Complexity:** Constant, platform-specific.

11. **Introspection**

```cpp
virtual AddressFamily family() const noexcept = 0;
virtual SocketKind kind() const noexcept = 0;
virtual int protocol() const noexcept = 0;
```

- **Returns:** Metadata about the socket: address family, socket kind (unknown/stream/datagram/seqpacket/raw), and protocol number.
- **Complexity:** Constant.

---

### `PosixSocketOps`

Implements the `SocketOps` interface using standard POSIX system calls. This class is used on Unix-like platforms (BSD, Linux, macOS).

---

### `Win32SocketOps`

Implements `SocketOps` using Win32 Sockets (Winsock2 API). This class is used on Windows platforms.

---

### Platform `SocketOps` Factory Functions

These functions provide a portable way to construct a platform-native `SocketOps` implementation without exposing platform-specific details to the library user. Factories are provided in throwing and non-throwing variants and support both creating a new native socket or taking ownership of an existing native handle.

#### Creating a new socket

```cpp
std::unique_ptr<SocketOps> make_socket_ops(AddressFamily family,
                                           SocketKind kind,
                                           int protocol);
std::unique_ptr<SocketOps> make_socket_ops(AddressFamily family,
                                           SocketKind kind,
                                           int protocol,
                                           std::error_code& ec) noexcept;
```

- **Effects:**
  - Maps `family` and `kind` to the platform-specific domain and type constants.
  - Invokes the native socket creation function (`socket()` on POSIX, `WSASocket()` on Windows) with the mapped values and the specified `protocol`.
  - Constructs a platform-specific `SocketOps` that takes ownership of the created native handle.
- **Returns:**
  - On success: returns a `std::unique_ptr<SocketOps>` owning a valid `SocketOps` instance.
  - On failure:
    - Throwing overload: throws `socket_error` with the platform error.
    - Non-throwing overload: returns `nullptr` and sets `ec` to the platform error.
- **Remarks:**
  - `protocol == 0` indicates the default protocol for the given `SocketKind`.
  - Factories are thread-safe.
- **Complexity:** Constant; platform-dependent socket creation cost.

#### Taking ownership of an existing native handle

```cpp
std::unique_ptr<SocketOps> make_socket_ops_from_native(SocketOps::native_handle_type native,
                                                       AddressFamily family,
                                                       SocketKind kind,
                                                       int protocol);
std::unique_ptr<SocketOps> make_socket_ops_from_native(native_handle_type native,
                                                       AddressFamily family,
                                                       SocketKind kind,
                                                       int protocol,
                                                       std::error_code& ec) noexcept;
```

- **Effects:**
  - Wraps the provided `native` handle in a platform-specific `SocketOps` object.
  - Validates the handle and metadata to the degree possible.
  - Ownership of the handle is transferred to the returned `SocketOps` on success.
- **Returns:**
  - On success: returns a `std::unique_ptr<SocketOps>` owning the `native` handle.
  - On failure:
    - Throwing overload: throws `socket_error` with the platform error.
    - Non-throwing overload: returns `nullptr` and sets `ec`; caller retains ownership of `native`.
- **Remarks:**
  - Caller is responsible for passing correct `family` and `kind`. Incorrect metadata may result in runtime errors.
  - Factories are thread-safe.
- **Complexity:** Platform-dependent handle validation cost.

---

### `Socket`

`Socket` is the library's type-erased, move-only, user-facing socket handle. It provides higher-level, safe and convenient operations built on top of the low-level `SocketOps` implementations. `Socket` delegates raw system interaction to its `ops` member and exposes both throwing and non-throwing (`std::error_code&`) overloads.

> [!IMPORTANT]
>
> - A `Socket` that does not own an implementation (`ops == nullptr`) is _empty_ / _invalid_; `operator bool()` returns `false` and most operations fail or throw `socket_error`.
> - Throwing overloads convert the `std::error_code` returned by underlying `SocketOps` into `socket_error`.
> - Non-throwing overloads propagate errors through a `std::error_code&` parameter and never throw.
> - `Socket` is move-only; moving transfers ownership of the underlying `SocketOps`.
> - Concurrent access to the same `Socket` object from multiple threads is undefined unless explicitly documented for a particular operation. Different `Socket` objects (including duplicates created via `duplicate`) may be used concurrently.
> - All sizes and counts are expressed in bytes unless otherwise noted.

#### Synopsis

```cpp
class Socket {
public:
  using duration            = std::chrono::milliseconds;
  using buffer_type         = Buffer;
  using buffer_view_type    = BufferView;
  using socket_kind_type    = SocketKind;
  using address_type        = Address;
  using address_family_type = AddressFamily;
  using native_handle_type  = SocketOps::native_handle_type;

  // Move-only semantics
  Socket(const Socket&)            = delete;
  Socket& operator=(const Socket&) = delete;
  Socket(Socket&& other) noexcept;
  Socket& operator=(Socket&& other) noexcept;

  // Constructors / Destructor
  Socket() noexcept;
  explicit Socket(std::unique_ptr<SocketOps> ops) noexcept;
  ~Socket();

  // Observers
  explicit operator bool() const noexcept;
  bool valid() const noexcept;
  address_family_type family() const noexcept;
  socket_kind_type kind() const noexcept;
  int protocol() const noexcept;

  // Lifecycle
  void close();
  void close(std::error_code& ec) noexcept;
  native_handle_type release() noexcept;
  native_handle_type native_handle() const noexcept;

  // Binding / listening / accepting
  void bind(const address_type& local);
  void bind(const address_type& local, std::error_code& ec) noexcept;
  void listen(int backlog);
  void listen(int backlog, std::error_code& ec) noexcept;
  Socket accept();
  Socket accept(std::error_code& ec) noexcept;

  // Connecting
  void connect(const address_type& remote);
  void connect(const address_type& remote, std::error_code& ec) noexcept;

  // I/O
  std::size_t send(buffer_view_type view, const address_type* dest = nullptr, int flags = 0);
  std::size_t
  send(buffer_view_type view, std::error_code& ec, const address_type* dest = nullptr, int flags = 0) noexcept;

  std::size_t sendall(buffer_view_type view, const address_type* dest = nullptr, int flags = 0);
  std::size_t
  sendall(buffer_view_type view, std::error_code& ec, const address_type* dest = nullptr, int flags = 0) noexcept;

  std::pair<buffer_type, std::unique_ptr<address_type>> recv(std::size_t count, int flags = 0);
  std::pair<buffer_type, std::unique_ptr<address_type>>
  recv(std::size_t count, std::error_code& ec, int flags = 0) noexcept;

  // File transfer
  std::size_t sendfile(const std::filesystem::path& file_path,
                       std::streampos offset    = 0,
                       std::size_t count        = 0,
                       const address_type* dest = nullptr,
                       int flags                = 0);
  std::size_t sendfile(std::ifstream& file,
                       std::streampos offset    = 0,
                       std::size_t count        = 0,
                       const address_type* dest = nullptr,
                       int flags                = 0);

  std::size_t sendfile(const std::filesystem::path& file_path,
                       std::error_code& ec,
                       std::streampos offset    = 0,
                       std::size_t count        = 0,
                       const address_type* dest = nullptr,
                       int flags                = 0) noexcept;
  std::size_t sendfile(std::ifstream& file,
                       std::error_code& ec,
                       std::streampos offset    = 0,
                       std::size_t count        = 0,
                       const address_type* dest = nullptr,
                       int flags                = 0) noexcept;

  // Shutdown
  void shutdown(ShutdownMode how);
  void shutdown(ShutdownMode how, std::error_code& ec) noexcept;

  // Socket options
  void setsockopt(int level, int optname, int value);
  void setsockopt(int level, int optname, int value, std::error_code& ec) noexcept;
  void setsockopt(int level, int optname, const buffer_type& option_value);
  void setsockopt(int level, int optname, const buffer_type& option_value, std::error_code& ec) noexcept;

  int getsockopt(int level, int optname) const;
  int getsockopt(int level, int optname, std::error_code& ec) const noexcept;
  buffer_type getsockopt(int level, int optname, std::size_t buflen) const;
  buffer_type getsockopt(int level, int optname, std::size_t buflen, std::error_code& ec) const noexcept;

  // Timeout / blocking
  void settimeout(duration timeout);
  void settimeout(duration timeout, std::error_code& ec) noexcept;
  std::optional<duration> gettimeout() const noexcept;

  void setblocking(bool would_block);
  void setblocking(bool would_block, std::error_code& ec) noexcept;
  bool getblocking() const noexcept;

  // Inheritable handle
  void setinheritable(bool inheritable);
  void setinheritable(bool inheritable, std::error_code& ec) noexcept;
  bool getinheritable() const noexcept;

  // Local / peer addresses
  std::unique_ptr<address_type> getsockname() const;
  std::unique_ptr<address_type> getsockname(std::error_code& ec) const noexcept;
  std::unique_ptr<address_type> getpeername() const;
  std::unique_ptr<address_type> getpeername(std::error_code& ec) const noexcept;

protected:
  std::unique_ptr<SocketOps> ops;
};
```

#### Member Functions

1. **Construction, move, destruction**

```cpp
Socket() noexcept;
```

- **Effects:** Constructs an empty, invalid `Socket` (`ops == nullptr`).
- **Complexity:** Constant.

```cpp
explicit Socket(std::unique_ptr<SocketOps> ops) noexcept;
```

- **Effects:** Constructs a `Socket` that takes ownership of `ops`. If `ops == nullptr`, behaves like the default constructor.
- **Complexity:** Constant.

```cpp
Socket(Socket&& other) noexcept;
Socket& operator=(Socket&& other) noexcept;
```

- **Effects:** Move-constructs or move-assigns. After move, `other` is left in an empty but valid state. No resources are leaked.
- **Complexity:** Constant.

```cpp
virtual ~Socket();
```

- **Effects:** Destroys the `Socket`. If the `Socket` still owns `ops`, the destructor calls `close()` and then destroys `ops`. The destructor must not throw; any errors from `close()` are ignored.
- **Complexity:** Constant.

2. **Observers**

```cpp
explicit operator bool() const noexcept;
```

- **Returns:** `true` if the `Socket` currently owns a valid `SocketOps` and that `SocketOps` reports a valid native handle; otherwise `false`.
- **Complexity:** Constant.

```cpp
bool valid() const noexcept;
```

- **Returns:** Equivalent to `static_cast<bool>(*this)`.
- **Complexity:** Constant.

```cpp
address_family_type family() const noexcept;
```

- **Effects:** Returns the address family of the underlying socket by delegating to `ops->family()`; if `!valid()` returns `AddressFamily::Unknown`.
- **Complexity:** Constant.

```cpp
socket_kind_type kind() const noexcept;
```

- **Effects:** Returns the socket kind as reported by `ops->kind()`. If the socket is invalid, returns `SocketKind::Unknown`.
- **Complexity:** Constant.

```cpp
int protocol() const noexcept;
```

- **Returns:** The protocol number reported by `ops->protocol()`. If the socket is invalid, returns `0`.
- **Complexity:** Constant.

3. **Lifecycle**

```cpp
void close();
```

- **Effects:** Close the socket. Equivalent to:
  - Call `close(std::error_code& ec)` and if it sets `ec`, throw `socket_error(ec)`.
- **Exceptions:** `socket_error` if underlying `SocketOps::close` fails.
- **Complexity:** Constant.

```cpp
void close(std::error_code& ec) noexcept;
```

- **Effects:** If `ops` is non-null, delegates to `ops->close(ec)` and then reset `ops` to `nullptr` regardless of success or failure. If `ops` is already null, `ec` is cleared.
- **Remarks:** Closing is idempotent at the `Socket` level; calling `close()` multiple times is well-formed.
- **Complexity:** Constant.

```cpp
native_handle_type release() noexcept;
```

- **Effects:** If `ops` is non-null, obtains the native handle by calling `ops->release()`, resets `ops` to `nullptr` and returns the native handle. If `ops` is null, returns the platform-specific invalid handle.
- **Returns:** The native handle previously owned by the `Socket` (or invalid handle if none).
- **Remarks:** After `release()` the caller owns the native handle and is responsible for closing it. The `Socket` becomes empty.
- **Complexity:** Constant.

```cpp
native_handle_type native_handle() const noexcept;
```

- **Returns:** The underlying native handle (via `ops->native_handle()`), or a platform-specific invalid handle if `!valid()`.
- **Complexity:** Constant.

4. **Binding, listening, accepting**

```cpp
void bind(const address_type& local);
void bind(const address_type& local, std::error_code& ec) noexcept;
```

- **Effects:** Delegate to `ops->bind(local, ec)`. On success `ec` cleared.
- **Exceptions:** `socket_error` (throwing overload) on failure.
- **Complexity:** Constant.

```cpp
void listen(int backlog);
void listen(int backlog, std::error_code& ec) noexcept;
```

- **Effects:** Clamp negative `backlog` to `0`. Delegate to `ops->listen(backlog, ec)`.
- **Exceptions:** `socket_error` (throwing overload) on failure.
- **Complexity:** Constant.

```cpp
Socket accept();
Socket accept(std::error_code& ec) noexcept;
```

- **Effects:** Delegate to `ops->accept(ec)`.
  - On success: `ops->accept` returns a `std::unique_ptr<SocketOps>` owning the accepted socket; `Socket::accept` constructs a new `Socket` from it and clears `ec`.
  - On failure: `ops->accept` returns `nullptr` and sets `ec`; the throwing overload throws `socket_error`.
- **Returns:** A new `Socket` owning the accepted connection on success.
- **Remarks:** Non-blocking behavior mirrors the underlying socket. The returned `Socket` exposes `getpeername()` to obtain remote address info.
- **Complexity:** Per-connection constant; blocking or non-blocking depending on the underlying native socket mode.

5. **Connecting**

```cpp
void connect(const address_type& remote);
void connect(const address_type& remote, std::error_code& ec) noexcept;
```

- **Effects:** Delegates to `ops->connect(remote, ec)`.
- **Exceptions:** `socket_error` (throwing overload) on failure.
- **Remarks:** For non-blocking sockets, a would-block result (e.g., `EINPROGRESS`) is reported via `ec` and converted to an exception by the throwing overload only if the caller expects it; callers may use `select`/`poll` to complete connection.
- **Complexity:** Potentially unbounded (depends on network and blocking semantics).

6. **Data transfer**

```cpp
std::size_t send(buffer_view_type view, const address_type* dest = nullptr, int flags = 0);
std::size_t send(buffer_view_type view, std::error_code& ec, const address_type* dest = nullptr, int flags = 0) noexcept;
```

- **Effects:** Calls `ops->send(view.data(), view.size(), dest, flags, ec)` and returns the number of bytes actually sent.
- **Returns:** Number of bytes sent (`> 0`) on success. On failure returns `0` and sets `ec`. Throwing overload throws `socket_error` on failure.
- **Complexity:** `O(n)` where `n` is number of bytes copied into kernel buffers; implementation-dependent.

```cpp
std::size_t sendall(buffer_view_type view, const address_type* dest = nullptr, int flags = 0);
std::size_t sendall(buffer_view_type view, std::error_code& ec, const address_type* dest = nullptr, int flags = 0) noexcept;
```

- **Purpose:** Ensure that all bytes in `view` are transmitted, looping calls to `ops->send` as necessary.
- **Effects:**
  - Repeatedly calls `ops->send(ptr + offset, remaining, dest, flags, ec)` until:
    - All bytes are sent (return total bytes sent, `ec` cleared); or
    - An error occurs (return bytes successfully sent so far, `ec` set); or
    - The underlying socket is non-blocking and a send would block — in this case the operation returns with `ec` set to the platform's would-block error (e.g., `EWOULDBLOCK`/`EAGAIN`) and the number of bytes transmitted so far.
- **Returns:** Total number of bytes successfully sent.
- **Remarks:** `sendall` is a convenience method that may block until completion on blocking sockets. Callers who require strict non-blocking semantics should use `send` and handle partial sends themselves.
- **Complexity:** `O(total bytes sent)` (may involve multiple syscalls).

```cpp
std::pair<buffer_type, std::unique_ptr<address_type>> recv(std::size_t count, int flags = 0);
std::pair<buffer_type, std::unique_ptr<address_type>> recv(std::size_t count, std::error_code& ec, int flags = 0) noexcept;
```

- **Effects:**
  - Allocates a `Buffer` of size `count` (or an implementation-defined maximum if `count == 0`), calls `ops->recv(buffer.data(), buffer.size(), flags, ec)`.
  - On success the returned `Buffer` is resized to `nbytes`. For stream sockets, the `unique_ptr<address_type>` is `nullptr`. For datagram/seqpacket sockets, the `unique_ptr<address_type>` points to the message source address if provided by the platform.
- **Returns:** `{buffer, source_address}` where `buffer.size()` equals the number of bytes read.
- **Remarks:** If `nbytes == 0` and `ec` is clear for stream sockets, this indicates orderly EOF. If the incoming datagram is larger than `count`, the buffer contains the first `count` bytes and the remainder is discarded.
- **Complexity:** `O(nbytes)`.

```cpp
std::size_t sendfile(const std::filesystem::path& file_path, std::streampos offset = 0, std::size_t count = 0, const address_type* dest = nullptr, int flags = 0);
std::size_t sendfile(std::ifstream& file, std::streampos offset = 0, std::size_t count = 0, const address_type* dest = nullptr, int flags = 0);
std::size_t sendfile(const std::filesystem::path& file_path, std::error_code& ec, std::streampos offset = 0, std::size_t count = 0, const address_type* dest = nullptr, int flags = 0) noexcept;
std::size_t sendfile(std::ifstream& file, std::error_code& ec, std::streampos offset = 0, std::size_t count = 0, const address_type* dest = nullptr, int flags = 0) noexcept;
```

- **Effects:** Transfer file data to the socket efficiently, using platform-specific zero-copy facilities where available (e.g., `sendfile`), falling back to read-then-send where necessary.
- **Parameters:**
  - `file_path` / `std::ifstream& file` — file to transfer.
  - `offset` — starting offset inside the file.
  - `count` — number of bytes to transfer; `0` means transfer until EOF.
  - `dest` — optional destination address for connectionless sockets; ignored for connected stream sockets.
  - `flags` — platform-specific flags forwarded to the underlying transfer primitive where applicable.
- **Effects:** Transfer up to `count` bytes starting at `offset`. On success return total bytes transferred and clear `ec`. On error return bytes transferred so far (possibly `0`) and set `ec`.
- **Behavior with non-blocking sockets:** If the socket is non-blocking and the underlying operation would block after a partial transfer, `sendfile` returns the number of bytes transferred so far and sets `ec` to the platform would-block error.
- **Remarks:** If `file_path` is used, `sendfile` opens the file; failures to open are reported via `ec` or exceptions. When given an `ifstream&`, the stream's position is used and must be valid for reading.
- **Complexity:** `O(bytes transferred)`; zero-copy variants provide improved CPU/memory efficiency.

7. **Shutdown**

```cpp
void shutdown(ShutdownMode how);
void shutdown(ShutdownMode how, std::error_code& ec) noexcept;
```

- **Effects:** Delegate to `ops->shutdown(how, ec)`.
- **Exceptions:** The throwing overload converts underlying `ec` into `socket_error`.
- **Remarks:** `shutdown()` only affects the data direction of the socket. It does not close the underlying descriptor/handle. After `shutdown(ShutdownMode::Both)` the socket may still be closed explicitly by calling `close()`.

8. **Socket options**

```cpp
void setsockopt(int level, int optname, int value);
void setsockopt(int level, int optname, int value, std::error_code& ec) noexcept;
void setsockopt(int level, int optname, buffer_view_type option_value);
void setsockopt(int level, int optname, buffer_view_type option_value, std::error_code& ec) noexcept;
```

- **Effects:** Convert `value` or `option_value` into raw pointer/size and delegate to `ops->setsockopt`.
- **Exceptions:** Throwing overloads throw `socket_error` on failure.
- **Complexity:** Constant.

```cpp
int getsockopt(int level, int optname) const;
int getsockopt(int level, int optname, std::error_code& ec) const noexcept;
buffer_type getsockopt(int level, int optname, std::size_t buflen) const;
buffer_type getsockopt(int level, int optname, std::size_t buflen, std::error_code& ec) const noexcept;
```

- **Effects:** Call `ops->getsockopt` with an appropriately-sized buffer and return the option value as `int` or `buffer_type`. On success adjust returned container length to the actual size written.
- **Complexity:** Constant.

9. **Timeout and blocking**

```cpp
void settimeout(duration timeout);
void settimeout(duration timeout, std::error_code& ec) noexcept;
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
std::optional<duration> gettimeout() const noexcept;
```

- **Returns:**
  - The current timeout for blocking operations, or `std::nullopt` if no timeout is set.

```cpp
void setblocking(bool would_block);
void setblocking(bool would_block, std::error_code& ec) noexcept;
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
bool getblocking() const noexcept;
```

- **Returns:**
  - `true` if the socket is in blocking mode, otherwise `false`.
- **Notes:** This is equivalent to checking `gettimeout() != 0`.
- **Complexity:** Constant

10. **Inheritable handle**

```cpp
void setinheritable(bool inheritable);
void setinheritable(bool inheritable, std::error_code& ec) noexcept;
bool getinheritable() const noexcept;
```

- **Effects:** Set or query whether the underlying native handle is inheritable by child processes (platform-dependent).
- **Remarks:** On platforms that do not support handle inheritance control, `setinheritable` shall return an error (e.g., `ENOSYS`) via `ec` or throw.
- **Complexity:** Constant.

11. **Local and peer addresses**

```cpp
std::unique_ptr<address_type> getsockname() const;
std::unique_ptr<address_type> getsockname(std::error_code& ec) const noexcept;
```

```cpp
std::unique_ptr<address_type> getpeername() const;
std::unique_ptr<address_type> getpeername(std::error_code& ec) const noexcept;
```

- **Effects:** Delegate to `ops->getsockname` or `ops->getpeername` and return a newly-allocated `Address` object on success. On failure set `ec` or throw `socket_error`.
- **Returns:** `unique_ptr<address_type>` owning the address on success; `nullptr` on failure (non-throwing overload) with `ec` set.
- **Complexity:** Constant.

---

### `StreamSocket`

The `StreamSocket` class provides a type-safe, high-level wrapper for sockets of kind `SocketKind::Stream`. It offers connection-oriented, reliable, sequenced byte stream communication, corresponding to POSIX `SOCK_STREAM` and typically backed by TCP/IP.

A `StreamSocket` is move-only, non-copyable, and provides throwing and non-throwing overloads for all operations. Most member functions delegate to the underlying `Socket` type-erased handle.

#### Synopsis

```cpp
class StreamSocket {
public:
  using duration           = std::chrono::milliseconds;
  using buffer_type        = Buffer;
  using buffer_view_type   = BufferView;
  using address_type       = Address;
  using native_handle_type = Socket::native_handle_type;

  // Move-only semantics
  StreamSocket(const StreamSocket&)            = delete;
  StreamSocket& operator=(const StreamSocket&) = delete;
  StreamSocket(StreamSocket&& other) noexcept;
  StreamSocket& operator=(StreamSocket&& other) noexcept;

  // Constructors / Destructor
  StreamSocket() noexcept;
  explicit StreamSocket(AddressFamily family, std::string_view protocol = "");
  explicit StreamSocket(Socket&& socket);
  ~StreamSocket();

  // Factory methods (non-throwing)
  static StreamSocket create(AddressFamily family, std::string_view protocol, std::error_code& ec) noexcept;
  static StreamSocket from_generic_socket(Socket&& socket, std::error_code& ec) noexcept;

  // Observers
  explicit operator bool() const noexcept;
  bool valid() const noexcept;
  AddressFamily family() const noexcept;
  int protocol() const noexcept;

  // Conversion
  Socket to_generic_socket() noexcept;

  // Lifecycle
  void close();
  void close(std::error_code& ec) noexcept;
  native_handle_type release() noexcept;
  native_handle_type native_handle() const noexcept;

  // Connection-oriented operations
  void bind(const address_type& local);
  void bind(const address_type& local, std::error_code& ec) noexcept;
  void listen(int backlog = /* platform-defined default */);
  void listen(int backlog, std::error_code& ec) noexcept;
  StreamSocket accept();
  StreamSocket accept(std::error_code& ec) noexcept;
  void connect(const address_type& remote);
  void connect(const address_type& remote, std::error_code& ec) noexcept;

  // Stream I/O operations
  std::size_t send(buffer_view_type data, int flags = 0);
  std::size_t send(buffer_view_type data, std::error_code& ec, int flags = 0) noexcept;
  std::size_t sendall(buffer_view_type data, int flags = 0);
  std::size_t sendall(buffer_view_type data, std::error_code& ec, int flags = 0) noexcept;

  std::pair<buffer_type, std::size_t> recv(std::size_t max_bytes, int flags = 0);
  std::pair<buffer_type, std::size_t> recv(std::size_t max_bytes, std::error_code& ec, int flags = 0) noexcept;
  std::size_t recv_into(buffer_type& buffer, int flags = 0);
  std::size_t recv_into(buffer_type& buffer, std::error_code& ec, int flags = 0) noexcept;

  // File transfer operations
  std::size_t
  sendfile(const std::filesystem::path& file_path, std::streampos offset = 0, std::size_t count = 0, int flags = 0);
  std::size_t sendfile(std::ifstream& file, std::streampos offset = 0, std::size_t count = 0, int flags = 0);
  std::size_t sendfile(const std::filesystem::path& file_path,
                       std::error_code& ec,
                       std::streampos offset = 0,
                       std::size_t count     = 0,
                       int flags             = 0) noexcept;
  std::size_t sendfile(std::ifstream& file,
                       std::error_code& ec,
                       std::streampos offset = 0,
                       std::size_t count     = 0,
                       int flags             = 0) noexcept;

  // Graceful shutdown
  void shutdown_read();
  void shutdown_read(std::error_code& ec) noexcept;
  void shutdown_write();
  void shutdown_write(std::error_code& ec) noexcept;
  void shutdown_both();
  void shutdown_both(std::error_code& ec) noexcept;

  // Connection state queries
  bool is_connected() const noexcept;
  bool is_listening() const noexcept;
  std::unique_ptr<address_type> local_address() const;
  std::unique_ptr<address_type> local_address(std::error_code& ec) const noexcept;
  std::unique_ptr<address_type> peer_address() const;
  std::unique_ptr<address_type> peer_address(std::error_code& ec) const noexcept;

  // Socket options and configuration
  void set_nodelay(bool enable = true);
  void set_nodelay(bool enable, std::error_code& ec) noexcept;
  bool get_nodelay() const;
  bool get_nodelay(std::error_code& ec) const noexcept;

  void set_keepalive(bool enable = true);
  void set_keepalive(bool enable, std::error_code& ec) noexcept;
  bool get_keepalive() const;
  bool get_keepalive(std::error_code& ec) const noexcept;

  void set_linger(bool enable, duration timeout = duration::zero());
  void set_linger(bool enable, duration timeout, std::error_code& ec) noexcept;
  std::pair<bool, duration> get_linger() const;
  std::pair<bool, duration> get_linger(std::error_code& ec) const noexcept;

  // Timeout / blocking
  void settimeout(duration timeout);
  void settimeout(duration timeout, std::error_code& ec) noexcept;
  std::optional<duration> gettimeout() const noexcept;

  void setblocking(bool would_block);
  void setblocking(bool would_block, std::error_code& ec) noexcept;
  bool getblocking() const noexcept;

  // Inheritable handle
  void setinheritable(bool inheritable);
  void setinheritable(bool inheritable, std::error_code& ec) noexcept;
  bool getinheritable() const noexcept;

private:
  Socket socket;
};
```

#### Additional Member Functions

1. **Constructors**

```cpp
StreamSocket() noexcept;
```

- **Effects:** Constructs an empty `StreamSocket` with no underlying socket.
- **Postconditions:** `valid() == false`.
- **Complexity:** Constant.

```cpp
explicit StreamSocket(AddressFamily family, std::string_view protocol = "");
```

- **Effects:**
  - Creates a new stream socket in the given address family using the specified protocol.
  - Internally, the constructor invokes the platform's socket creation function with domain corresponding to `family`, type corresponding to `SOCK_STREAM`, and protocol as determined below.
- **Protocol resolution:** The `protocol` argument is interpreted as follows:
  - If `protocol` is empty, the default protocol for stream sockets in the given address family shall be selected (typically TCP). Equivalent to passing `0` as protocol number.
  - If `protocol` is a decimal numeric string, it shall be parsed as an integer and used directly as the protocol number. If parsing fails or the number is out of range, the constructor fails with `errc::invalid_argument`.
  - Otherwise, `protocol` shall be treated as a protocol name and resolved through the system's protocol database:
    - If resolution succeeds, the resulting protocol number is used.
    - If resolution fails, the constructor fails with `errc::protocol_not_supported`.
- **Exceptions:** Throws `socket_error` on failure.
- **Complexity:** Constant, aside from at most one system protocol database lookup.

```cpp
static StreamSocket create(AddressFamily family, std::string_view protocol,
                           std::error_code& ec) noexcept;
```

- **Effects:** Same as above but reports errors through `ec` and never throws.
- **Returns:** Empty socket on failure.
- **Complexity:** Constant.

```cpp
explicit StreamSocket(Socket&& socket);
```

- **Effects:** Takes ownership of a generic socket.
- **Exceptions:** `socket_error` if the socket kind is not `SocketKind::Stream`.
- **Postconditions:** On success, `socket` is empty.
- **Complexity:** Constant.

```cpp
static StreamSocket from_generic_socket(Socket&& socket, std::error_code& ec) noexcept;
```

- **Effects:** Same as above but non-throwing.
- **Returns:** Empty `StreamSocket` if conversion fails.
- **Postconditions:** On success, `socket` is empty.
- **Complexity:** Constant.

2. **Conversion**

```cpp
Socket to_generic_socket() noexcept;
```

- **Effects:** Transfers ownership of the underlying `Socket` into a new `Socket` object.
- **Postconditions:** `*this` is empty (`valid() == false`).
- **Complexity:** Constant.

3. **I/O**

```cpp
std::size_t recv_into(buffer_type& buffer, int flags = 0);
std::size_t recv_into(buffer_type& buffer, std::error_code& ec, int flags = 0) noexcept;
```

- **Effects:** Receives up to `buffer.size()` bytes into `buffer`.
- **Returns:** Number of bytes actually read. A return value of `0` with no error indicates EOF.
- **Exceptions:** Throwing overload throws `socket_error` on failure.
- **Complexity:** `O(nbytes)`.

4. **Shutdown**

```cpp
void shutdown_read();
void shutdown_write();
void shutdown_both();
```

- **Effects:** Disable further reads, writes, or both, respectively.
- **Remarks:** Shutdown does not close the socket.
- **Exceptions:** `socket_error` on failure.
- **Complexity:** Constant.

5. **Connection state queries**

```cpp
bool is_connected() const noexcept;
```

- **Returns:** True if the socket is currently connected to a peer.
- **Remarks:** May return `false` if the socket was never connected, has been shutdown, or has been closed.

```cpp
bool is_listening() const noexcept;
```

- **Returns:** True if the socket has been marked for listening via `listen()`.

6. **Common Options**

```cpp
void set_nodelay(bool enable = true);
void set_nodelay(bool enable, std::error_code& ec) noexcept;
bool get_nodelay() const;
bool get_nodelay(std::error_code& ec) const noexcept;
```

- **Effects:** Sets or queries the `TCP_NODELAY` option on the underlying socket.
  - **When `enable == true`:** disables Nagle's algorithm, causing small writes to be sent immediately.
  - **When `enable == false`:** enables Nagle's algorithm, allowing coalescing of small writes.
- **Remarks:** On systems without `TCP_NODELAY`, the implementation shall provide an equivalent behavior if available, otherwise fail with `errc::not_supported`.
- **Complexity:** Constant.

```cpp
void set_keepalive(bool enable = true);
void set_keepalive(bool enable, std::error_code& ec) noexcept;
bool get_keepalive() const;
bool get_keepalive(std::error_code& ec) const noexcept;
```

- **Effects:** Sets or queries the `SO_KEEPALIVE` option on the underlying socket.
  - **When `enable == true`:** requests periodic transmission of keepalive probes on an otherwise idle connection.
  - **When `enable == false`:** disables such probes.
- **Remarks:** The frequency and timing of keepalive probes are system-dependent and may require additional socket options (not covered here) for fine control. On systems without `SO_KEEPALIVE`, the implementation shall fail with `errc::not_supported`.
- **Complexity:** Constant.

```cpp
void set_linger(bool enable, duration timeout = duration::zero());
void set_linger(bool enable, duration timeout, std::error_code& ec) noexcept;
std::pair<bool, duration> get_linger() const;
std::pair<bool, duration> get_linger(std::error_code& ec) const noexcept;
```

- **Effects:** Sets or queries the `SO_LINGER` option on the underlying socket.
  - **When `enable == false`:** disables linger; `close()` returns immediately and the system attempts to deliver unsent data.
  - **When `enable == true` with `timeout > 0`:** on `close()`, the system blocks until either all data is transmitted or `timeout` expires.
  - **When `enable == true` with `timeout == 0`:** the system discards unsent data and sends a TCP RST (abortive close).
- **Remarks:** Units of `timeout` are truncated to seconds on many platforms. A `timeout` greater than the maximum representable by the system shall be clamped.
- **Complexity:** Constant.

---

### `DatagramSocket`

A `DatagramSocket` represents an endpoint for message-oriented communication. It provides unreliable, connectionless delivery of discrete datagrams, with optional association to a single peer. Each `send` or `sendto` call transmits one datagram; each `recv` or `recvfrom` call returns at most one datagram. Broadcast and multicast operations are supported.

#### Synopsis

```cpp
class DatagramSocket {
public:
  using duration           = std::chrono::milliseconds;
  using buffer_type        = Buffer;
  using buffer_view_type   = BufferView;
  using address_type       = Address;
  using native_handle_type = Socket::native_handle_type;

  // Move-only semantics
  DatagramSocket(const DatagramSocket&)            = delete;
  DatagramSocket& operator=(const DatagramSocket&) = delete;
  DatagramSocket(DatagramSocket&& other) noexcept;
  DatagramSocket& operator=(DatagramSocket&& other) noexcept;

  // Constructors / Destructor
  DatagramSocket() noexcept;
  explicit DatagramSocket(AddressFamily family, std::string_view protocol = "");
  explicit DatagramSocket(Socket&& sock);
  ~DatagramSocket();

  // Factory methods
  static DatagramSocket create(AddressFamily family, std::string_view protocol, std::error_code& ec) noexcept;
  static DatagramSocket from_generic_socket(Socket&& sock, std::error_code& ec) noexcept;

  // Observers
  explicit operator bool() const noexcept;
  bool valid() const noexcept;
  AddressFamily family() const noexcept;
  int protocol() const noexcept;

  // Conversion
  Socket to_generic_socket() noexcept;

  // Lifecycle
  void close();
  void close(std::error_code& ec) noexcept;
  native_handle_type release() noexcept;
  native_handle_type native_handle() const noexcept;

  // Binding
  void bind(const address_type& local);
  void bind(const address_type& local, std::error_code& ec) noexcept;

  // Connectionless I/O
  std::size_t sendto(buffer_view_type data, const address_type& dest, int flags = 0);
  std::size_t sendto(buffer_view_type data, const address_type& dest, std::error_code& ec, int flags = 0) noexcept;

  std::pair<buffer_type, std::unique_ptr<address_type>> recvfrom(std::size_t max_bytes, int flags = 0);
  std::pair<buffer_type, std::unique_ptr<address_type>>
  recvfrom(std::size_t max_bytes, std::error_code& ec, int flags = 0) noexcept;

  std::pair<std::size_t, std::unique_ptr<address_type>> recvfrom_into(Buffer& buffer, int flags = 0);
  std::pair<std::size_t, std::unique_ptr<address_type>>
  recvfrom_into(Buffer& buffer, std::error_code& ec, int flags = 0) noexcept;

  // Connected datagram ops
  void connect(const address_type& remote);
  void connect(const address_type& remote, std::error_code& ec) noexcept;
  void disconnect();
  void disconnect(std::error_code& ec) noexcept;

  std::size_t send(buffer_view_type data, int flags = 0);
  std::size_t send(buffer_view_type data, std::error_code& ec, int flags = 0) noexcept;

  buffer_type recv(std::size_t max_bytes, int flags = 0);
  buffer_type recv(std::size_t max_bytes, std::error_code& ec, int flags = 0) noexcept;
  std::size_t recv_into(buffer_type& buffer, int flags = 0);
  std::size_t recv_into(buffer_type& buffer, std::error_code& ec, int flags = 0) noexcept;

  // Broadcast
  void set_broadcast(bool enable = true);
  void set_broadcast(bool enable, std::error_code& ec) noexcept;
  bool get_broadcast() const;
  bool get_broadcast(std::error_code& ec) const noexcept;

  // Multicast (IPv4/IPv6)
  void join_multicast_group(const address_type& group);
  void join_multicast_group(const address_type& group, std::error_code& ec) noexcept;
  void leave_multicast_group(const address_type& group);
  void leave_multicast_group(const address_type& group, std::error_code& ec) noexcept;

  void set_multicast_ttl(int ttl);
  void set_multicast_ttl(int ttl, std::error_code& ec) noexcept;
  int get_multicast_ttl() const;
  int get_multicast_ttl(std::error_code& ec) const noexcept;

  // Timeout / blocking
  void settimeout(duration timeout);
  void settimeout(duration timeout, std::error_code& ec) noexcept;
  std::optional<duration> gettimeout() const noexcept;

  void setblocking(bool would_block);
  void setblocking(bool would_block, std::error_code& ec) noexcept;
  bool getblocking() const noexcept;

  // Inheritable handle
  void setinheritable(bool inheritable);
  void setinheritable(bool inheritable, std::error_code& ec) noexcept;
  bool getinheritable() const noexcept;

  // Datagram size queries
  std::size_t max_datagram_size() const;
  std::size_t max_datagram_size(std::error_code& ec) const noexcept;

  // Connection state
  bool is_connected() const noexcept;
  std::unique_ptr<address_type> local_address() const;
  std::unique_ptr<address_type> local_address(std::error_code& ec) const noexcept;
  std::unique_ptr<address_type> peer_address() const;
  std::unique_ptr<address_type> peer_address(std::error_code& ec) const noexcept;

private:
  Socket socket;
  bool connected{false};
};
```

#### Additional Member Functions

1. **Constructors**

```cpp
explicit DatagramSocket(AddressFamily family, std::string_view protocol = "");
```

- **Effects:**
  Creates a new datagram socket in the given address family with type `SOCK_DGRAM`.
- **Protocol resolution:**
  - If `protocol` is empty: use the system default protocol for datagram sockets (`0`, typically UDP).
  - If `protocol` is a decimal numeric string: parse it as an integer protocol number. If parsing fails or is out of range, the constructor fails with `errc::invalid_argument`.
  - Otherwise: resolve `protocol` as a name via the system protocol database. If not found, the constructor fails with `errc::protocol_not_supported`.
- **Exceptions:** `socket_error` on failure.
- **Remarks:** Equivalent to calling the system `socket()` function with domain determined by `family`, type `SOCK_DGRAM`, and the resolved protocol number.
- **Complexity:** Constant time, except for a possible single database lookup.

```cpp
static DatagramSocket create(AddressFamily family,
                             std::string_view protocol,
                             std::error_code& ec) noexcept;
```

- **Effects:** Equivalent to the above constructor, but sets `ec` instead of throwing and returns an empty socket on failure.
- **Complexity:** Same as the above constructor.

```cpp
explicit DatagramSocket(Socket&& sock);
```

- **Effects:** Takes ownership of a generic socket.
- **Exceptions:** `socket_error` if the socket kind is not `SocketKind::Datagram`.
- **Postconditions:** On success, `sock` is empty.
- **Complexity:** Constant.

```cpp
static DatagramSocket from_generic_socket(Socket&& sock, std::error_code& ec) noexcept;
```

- **Effects:** Equivalent to the above constructor, but sets `ec` instead of throwing and returns an empty socket on failure.
- **Complexity:** Same as the above constructor.

```cpp
Socket to_generic_socket() noexcept;
```

- **Effects:** Returns a `Socket` representing the same underlying handle. After the call, the `DatagramSocket` is empty.

2. **I/O Operations**

```cpp
std::size_t sendto(buffer_view_type data, const address_type& dest, int flags = 0);
```

- **Effects:** Sends one datagram to `dest`.
- **Returns:** Number of bytes sent. Does not retry partial sends.
- **Exceptions:** `socket_error` on failure.

```cpp
std::pair<buffer_type, std::unique_ptr<address_type>> recvfrom(std::size_t max_bytes, int flags = 0);
```

- **Effects:** Receives one datagram into a new `Buffer` of size at most `max_bytes`. Returns also the source address.
- **Exceptions:** `socket_error` on failure.

```cpp
std::pair<std::size_t, std::unique_ptr<address_type>>
recvfrom_into(Buffer& buffer, int flags = 0);
```

- **Effects:** Receives one datagram into the provided `Buffer`.
- **Returns:** A pair `(nbytes, source_address)` where `nbytes` is the number of bytes stored in `buffer`.
- **Exceptions:** `socket_error` on failure.
- **Remarks:** If the incoming datagram is larger than `buffer.size()`, excess bytes are discarded.
- **Complexity:** Linear in `buffer.size()`.

3. **Connected Datagram Operations**

```cpp
void connect(const address_type& remote);
```

- **Effects:** Associates the datagram socket with the given peer `remote`.
- **Remarks:** After a successful call, `send` and `recv` may be used without specifying addresses.
- **Exceptions:** `socket_error` on failure.

```cpp
void disconnect();
```

- **Effects:** Breaks the association with any connected peer. On POSIX, equivalent to calling `connect()` with an `AF_UNSPEC` address. On Windows, equivalent to connecting to an address of family `AF_UNSPEC` with zero fields.
- **Exceptions:** `socket_error` on failure.

4. **Socket Options**

```cpp
void set_broadcast(bool enable = true);
```

- **Effects:** Sets `SO_BROADCAST` (or equivalent) option on the socket.

```cpp
bool get_broadcast() const;
```

- **Returns:** `true` if `SO_BROADCAST` is enabled.

```cpp
void join_multicast_group(const address_type& group);
void leave_multicast_group(const address_type& group);
```

- **Effects:** Manipulates IP multicast group membership:
  - IPv4: `IP_ADD_MEMBERSHIP` / `IP_DROP_MEMBERSHIP`.
  - IPv6: `IPV6_JOIN_GROUP` / `IPV6_LEAVE_GROUP`.

```cpp
void set_multicast_ttl(int ttl);
```

- **Effects:** Sets TTL for outgoing multicast datagrams:
  - IPv4: `IP_MULTICAST_TTL`.
  - IPv6: `IPV6_MULTICAST_HOPS`.

5. **Datagram Size Queries**

```cpp
std::size_t max_datagram_size() const;
```

- **Effects:** Returns maximum size of a single datagram supported by the platform for this socket.

---

### `SeqPacketSocket`

The `SeqPacketSocket` class is a high-level, move-only, user-facing handle for sequenced, reliable, connection-oriented message sockets. It provides a convenient and type-safe interface over the underlying type-erased `Socket`, enforcing message boundary semantics. Unlike `StreamSocket`, operations on `SeqPacketSocket` respect message boundaries: each call to `send_message` corresponds to exactly one message, and each call to `recv_message` retrieves exactly one message (subject to truncation).

> [!NOTE]
> Not all platforms support sequenced packet sockets. Therefore, this class is currently unimplemented.

#### Synopsis

```cpp
class SeqPacketSocket {
public:
  using duration           = std::chrono::milliseconds;
  using buffer_type        = Buffer;
  using buffer_view_type   = BufferView;
  using address_type       = Address;
  using native_handle_type = Socket::native_handle_type;

  // Move-only semantics
  SeqPacketSocket(const SeqPacketSocket&) = delete;
  SeqPacketSocket& operator=(const SeqPacketSocket&) = delete;
  SeqPacketSocket(SeqPacketSocket&& other) noexcept;
  SeqPacketSocket& operator=(SeqPacketSocket&& other) noexcept;

  // Constructors / Destructor
  SeqPacketSocket() noexcept;
  explicit SeqPacketSocket(AddressFamily family, std::string_view protocol = "");
  explicit SeqPacketSocket(Socket&& socket);
  ~SeqPacketSocket();

  // Factories
  static SeqPacketSocket create(AddressFamily family, std::string_view protocol,
                                std::error_code& ec) noexcept;
  static SeqPacketSocket from_generic_socket(Socket&& socket, std::error_code& ec) noexcept;

  // Observers
  explicit operator bool() const noexcept;
  bool valid() const noexcept;
  AddressFamily family() const noexcept;
  int protocol() const noexcept;

  // Conversion
  Socket to_generic_socket() noexcept;

  // Lifecycle
  void close();
  void close(std::error_code& ec) noexcept;
  native_handle_type release() noexcept;
  native_handle_type native_handle() const noexcept;

  // Connection-oriented setup
  void bind(const address_type& local);
  void bind(const address_type& local, std::error_code& ec) noexcept;
  void listen(int backlog = /* implementation-defined default */);
  void listen(int backlog, std::error_code& ec) noexcept;
  SeqPacketSocket accept();
  SeqPacketSocket accept(std::error_code& ec) noexcept;
  void connect(const address_type& remote);
  void connect(const address_type& remote, std::error_code& ec) noexcept;

  // Message-oriented I/O
  std::size_t send_message(buffer_view_type message, int flags = 0);
  std::size_t send_message(buffer_view_type message, std::error_code& ec,
                           int flags = 0) noexcept;

  buffer_type recv_message(std::size_t max_bytes, int flags = 0);
  buffer_type recv_message(std::size_t max_bytes, std::error_code& ec,
                           int flags = 0) noexcept;

  std::size_t recv_message_into(Buffer& buffer, int flags = 0);
  std::size_t recv_message_into(Buffer& buffer, std::error_code& ec,
                                int flags = 0) noexcept;

  // Message boundary queries
  std::size_t max_message_size() const;
  std::size_t max_message_size(std::error_code& ec) const noexcept;
  bool message_truncated() const noexcept;

  // Shutdown operations
  void shutdown_read();
  void shutdown_read(std::error_code& ec) noexcept;
  void shutdown_write();
  void shutdown_write(std::error_code& ec) noexcept;
  void shutdown_both();
  void shutdown_both(std::error_code& ec) noexcept;

  // Timeout / blocking
  void settimeout(duration timeout);
  void settimeout(duration timeout, std::error_code& ec) noexcept;
  std::optional<duration> gettimeout() const noexcept;

  void setblocking(bool would_block);
  void setblocking(bool would_block, std::error_code& ec) noexcept;
  bool getblocking() const noexcept;

  // Inheritable handle
  void setinheritable(bool inheritable);
  void setinheritable(bool inheritable, std::error_code& ec) noexcept;
  bool getinheritable() const noexcept;

  // Connection state queries
  bool is_connected() const noexcept;
  bool is_listening() const noexcept;
  std::unique_ptr<address_type> local_address() const;
  std::unique_ptr<address_type> local_address(std::error_code& ec) const noexcept;
  std::unique_ptr<address_type> peer_address() const;
  std::unique_ptr<address_type> peer_address(std::error_code& ec) const noexcept;

private:
  Socket socket;
};
```

---

### `RawSocket`

The `RawSocket` class provides a strongly typed interface for raw sockets (`SocketKind::Raw`). It exposes low-level datagram-oriented access to underlying protocols (e.g., ICMP, IP-in-IP).

Unlike `StreamSocket`, it does not provide connection semantics; each `sendto` and `recvfrom` carries explicit addressing. `RawSocket` is move-only and non-copyable.

#### Synopsis

```cpp
class RawSocket {
public:
  using duration           = std::chrono::milliseconds;
  using buffer_type        = Buffer;
  using buffer_view_type   = BufferView;
  using address_type       = Address;
  using native_handle_type = Socket::native_handle_type;

  // Move-only semantics
  RawSocket(const RawSocket&)            = delete;
  RawSocket& operator=(const RawSocket&) = delete;
  RawSocket(RawSocket&& other) noexcept;
  RawSocket& operator=(RawSocket&& other) noexcept;

  // Constructors / Destructor
  RawSocket() noexcept; // creates empty/invalid socket
  ~RawSocket();

  // Factory methods
  explicit RawSocket(AddressFamily family, std::string_view protocol);
  static RawSocket create(AddressFamily family, std::string_view protocol, std::error_code& ec) noexcept;

  explicit RawSocket(Socket&& socket);
  static RawSocket from_generic_socket(Socket&& socket, std::error_code& ec) noexcept;

  // Observers
  explicit operator bool() const noexcept;
  bool valid() const noexcept;
  AddressFamily family() const noexcept;
  int protocol() const noexcept;

  // Conversion
  Socket to_generic_socket() noexcept;

  // Lifecycle
  void close();
  void close(std::error_code& ec) noexcept;
  native_handle_type release() noexcept;
  native_handle_type native_handle() const noexcept;

  // Binding
  void bind(const address_type& local);
  void bind(const address_type& local, std::error_code& ec) noexcept;

  // Raw protocol I/O
  std::size_t sendto(buffer_view_type data, const address_type& dest, int flags = 0);
  std::size_t sendto(buffer_view_type data, const address_type& dest, std::error_code& ec, int flags = 0) noexcept;

  std::pair<buffer_type, std::unique_ptr<address_type>> recvfrom(std::size_t max_bytes, int flags = 0);
  std::pair<buffer_type, std::unique_ptr<address_type>>
  recvfrom(std::size_t max_bytes, std::error_code& ec, int flags = 0) noexcept;

  std::pair<std::size_t, std::unique_ptr<address_type>> recvfrom_into(Buffer& buffer, int flags = 0);
  std::pair<std::size_t, std::unique_ptr<address_type>>
  recvfrom_into(Buffer& buffer, std::error_code& ec, int flags = 0) noexcept;

  // Connected raw ops
  void connect(const address_type& remote);
  void connect(const address_type& remote, std::error_code& ec) noexcept;
  void disconnect();
  void disconnect(std::error_code& ec) noexcept;

  std::size_t send(buffer_view_type data, int flags = 0);
  std::size_t send(buffer_view_type data, std::error_code& ec, int flags = 0) noexcept;

  buffer_type recv(std::size_t max_bytes, int flags = 0);
  buffer_type recv(std::size_t max_bytes, std::error_code& ec, int flags = 0) noexcept;
  std::size_t recv_into(buffer_type& buffer, int flags = 0);
  std::size_t recv_into(buffer_type& buffer, std::error_code& ec, int flags = 0) noexcept;

  // Header inclusion control
  void set_header_included(bool include = true);
  void set_header_included(bool include, std::error_code& ec) noexcept;
  bool get_header_included() const;
  bool get_header_included(std::error_code& ec) const noexcept;

  // Shutdown operations
  void shutdown_read();
  void shutdown_read(std::error_code& ec) noexcept;
  void shutdown_write();
  void shutdown_write(std::error_code& ec) noexcept;
  void shutdown_both();
  void shutdown_both(std::error_code& ec) noexcept;

  // Timeout / blocking
  void settimeout(duration timeout);
  void settimeout(duration timeout, std::error_code& ec) noexcept;
  std::optional<duration> gettimeout() const noexcept;

  void setblocking(bool would_block);
  void setblocking(bool would_block, std::error_code& ec) noexcept;
  bool getblocking() const noexcept;

  // Inheritable handle
  void setinheritable(bool inheritable);
  void setinheritable(bool inheritable, std::error_code& ec) noexcept;
  bool getinheritable() const noexcept;

  // Protocol filtering (platform-specific)
  void set_protocol_filter(int protocol);
  void set_protocol_filter(int protocol, std::error_code& ec) noexcept;
  int get_protocol_filter() const;
  int get_protocol_filter(std::error_code& ec) const noexcept;

  // Connection state
  bool is_connected() const noexcept;
  std::unique_ptr<address_type> local_address() const;
  std::unique_ptr<address_type> local_address(std::error_code& ec) const noexcept;
  std::unique_ptr<address_type> peer_address() const;
  std::unique_ptr<address_type> peer_address(std::error_code& ec) const noexcept;

  // Raw socket options
  void set_raw_sockopt(int level, int optname, const buffer_type& value);
  void set_raw_sockopt(int level, int optname, const buffer_type& value, std::error_code& ec) noexcept;
  buffer_type get_raw_sockopt(int level, int optname, std::size_t max_size) const;
  buffer_type get_raw_sockopt(int level, int optname, std::size_t max_size, std::error_code& ec) const noexcept;

private:
  Socket socket;
};
```

#### Notes

- Unless otherwise specified, member functions have semantics identical to their `StreamSocket` counterparts (e.g., `close()`, `bind()`), adapted for `SOCK_RAW`.
- `set_header_included()` corresponds to `IP_HDRINCL` (IPv4) or equivalent platform option, controlling whether the caller supplies its own protocol headers.
- `set_protocol_filter()` is platform-specific.
- `sendto()` and `recvfrom()` directly expose raw packet transmission/reception. Higher-level framing or reliability must be implemented by the user.
