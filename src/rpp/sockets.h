#pragma once
/**
 * Simple and efficient wrapper around POSIX sockets, Copyright (c) 2014-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
#if _MSC_VER
#  pragma warning(disable: 4251) // class 'std::*' needs to have dll-interface to be used by clients of struct 'rpp::*'
#endif
#include "config.h"
#include "strview.h" // rpp::strview, std::string
#include "load_balancer.h" // rpp::load_balancer
#include <vector>   // std::vector

namespace rpp
{
    enum RPPAPI address_family : uint8_t
    {
        AF_DontCare,    // unspecified AddressFamily, service provider will choose most appropriate
        AF_IPv4,        // The Internet Protocol version 4 (IPv4) address family
        AF_IPv6,        // The Internet Protocol version 6 (IPv6) address family
        AF_Bth,         // Bluetooth address family, supported since WinXP SP2
    };

    enum RPPAPI socket_type : uint8_t
    {
        ST_Unspecified, // unspecified socket type (invalid socket)
        ST_Stream,      // TCP only, byte stream for IPv4 or IPv6 protocols
        ST_Datagram,    // UDP only, byte datagrams for IPv4 or IPv6 protocols
        ST_Raw,         // application provides IPv4 or IPv6 headers
        ST_RDM,         // reliable message datagram for PGM
        ST_SeqPacket,   // TCP based, similar to ST_Stream but slower, however packet boundaries are respected "TCP datagrams"
    };

    enum RPPAPI socket_category : uint8_t
    {
        SC_Unknown, // UDP or other unknown socket 
        SC_Listen,  // this socket is a LISTEN server socket via socket::listen()
        SC_Accept,  // this socket was accepted via socket::accept() as a server side client
        SC_Client,  // this is a Client side connection socket via socket::connect()
    };

    enum RPPAPI ip_protocol : uint8_t
    {
        IPP_DontCare,   // generic IP based protocol, service provider will choose most appropriate
        IPP_ICMP,       // only supported with ST_Raw on IPv4 or IPv6
        IPP_IGMP,       // only supported with ST_Raw on IPv4 or IPv6
        IPP_BTH,        // bluetooth RFCOMM protocol for AF_Bth
        IPP_TCP,        // TCP for ST_Stream on IPv4 or IPv6
        IPP_UDP,        // UDP for ST_Datagram on IPv4 or IPv6
        IPP_ICMPV6,     // only supported with ST_Raw on IPv4 or IPv6
        IPP_PGM,        // PGM - only supported with ST_RDM on IPv4
    };

    enum RPPAPI socket_option
    {
        SO_None,        // --
        SO_ReuseAddr = (1<<0),  // allows multiple sockets to bind to the same address
        SO_Blocking  = (1<<1),  // request a blocking socket instead of socket default
        SO_NonBlock  = (1<<2),  // request a non-blocking socket instead of socket default, takes priority over SO_Blocking
        // Enables socket load balancing and buffering Nagle algorithm.
        // Will cause delays. Only applies to TCP sockets.
        SO_Nagle     = (1<<3),
    };

    // @return UNIX af => AddressFamily conversion
    address_family to_addrfamily(int af) noexcept;
    // @return UNIX sock => SocketType conversion
    socket_type to_socktype(int sock) noexcept;
    // @return Default mapping of ip_protocol socket types. Ex: ST_Datagram on IPP_UDP*/
    socket_type to_socktype(ip_protocol ipp) noexcept;
    // @return UNIX ipproto => AddressFamily conversion
    ip_protocol to_ipproto(int ipproto) noexcept;
    // @return AddressFamily => UNIX af conversion
    int addrfamily_int(address_family addressFamily) noexcept;
    // @return SocketType => UNIX sock conversion
    int socktype_int(socket_type sockType) noexcept;
    // @return AddressFamily => UNIX ipproto conversion
    int ipproto_int(ip_protocol ipProtocol) noexcept;


    struct RPPAPI protocol_info
    {
        int            ProtoVersion; // protocol version identifier (ip_protocol OR SO_TYPE)
        address_family Family;
        socket_type    SockType;
        ip_protocol    Protocol;

        int family_int() const noexcept { return addrfamily_int(Family); }
        int type_int()   const noexcept { return socktype_int(SockType); }
        int proto_int()  const noexcept { return ipproto_int(Protocol); }
    };

    /**
     * @brief IP address abstraction, without port
     */
    struct RPPAPI raw_address
    {
        /**
         * @brief Current Address family, supported values AF_IPv4 or AF_IPv6, anything else is invalid
         */
        address_family Family; // IPv4 or IPv6
        #if _MSC_VER
        #  pragma warning(disable:4201)
        #endif
        union {
            union {
                unsigned long Addr4; // IPv4 Address in NETWORK byte order (!)
                unsigned char Addr4Parts[4];
            };
            struct {
                unsigned char Addr6[16]; // IPv6 Address in NETWORK byte order (!)
                unsigned long FlowInfo;
                unsigned long ScopeId; // The network prefix for IPv6
            };
        };

        raw_address() noexcept; // memset 0 raw_address
        explicit raw_address(address_family af) noexcept;

        // manually initialize address from an IPv4 integer address
        raw_address(address_family af, uint32_t ipv4) noexcept;

        // manually initialize an IPv6 address
        raw_address(address_family af, const void* ipv6, unsigned long flowInfo, unsigned long scopeId) noexcept;

        // resolves the ip address from a string
        raw_address(address_family af, const char* ip_address) noexcept;
        raw_address(address_family af, const std::string& ip_address) noexcept
            : raw_address(af, ip_address.c_str()) {}

        /** @returns rpp::address_family of this raw_address */
        address_family family() const noexcept { return Family; }

        /** @returns true if this raw_address is IPv4 */
        bool is_ipv4() const noexcept { return Family == AF_IPv4; }

        /** @returns true if this raw_address is IPv6 */
        bool is_ipv6() const noexcept { return Family == AF_IPv6; }

        /** @returns IPv4 address as a 32-bit integer */
        uint32_t ipv4_address() const noexcept { return Addr4; }

        /** Resets this raw_address to a default state */
        void reset() noexcept;

        /**
         * @returns AF_IPv4 or AF_IPv6 depending on the syntax of this ip_and_port string.
         *          AF_DontCare if the string is invalid
         */
        static address_family get_address_family(const char* ip_and_port) noexcept;
        static address_family get_address_family(const std::string& ip_and_port) noexcept
        {
            return get_address_family(ip_and_port.c_str());
        }

        /**
         * Initializes this ipaddress to a resolved hostname
         * @param af Address family, such as AF_IPv4 or AF_IPv6
         * @param hostname Such as "www.google.com" or "192.168.1.0"
         * @param port Optional port number that can be used to resolve the hostname
         * @returns true if hostname was resolved successfully
         */
        bool resolve_addr(address_family af, const char* hostname, int port = 0) noexcept;
        bool resolve_addr(address_family af, const std::string& hostname, int port = 0) noexcept
        {
            return resolve_addr(af, hostname.c_str(), port);
        }

        bool equals(const raw_address& addr) const noexcept;
        int compare(const raw_address& addr) const noexcept;
        bool operator==(const raw_address& addr) const noexcept { return equals(addr); }
        bool operator!=(const raw_address& addr) const noexcept { return !equals(addr); }

        /**
         * @returns true if this address family has not been set
         * A zero address is valid for listener sockets, so this only validates the Family
         */
        bool is_empty() const noexcept { return Family == 0; }
        explicit operator bool() const noexcept { return Family != 0; }

        /**
         * @returns true if this raw_address has an ip address sequence set. returns false for listener ports
        */
        bool has_address() const noexcept;

        /** 
         * @brief Returns the IP address as a string. Example result: "192.168.1.110"
         */
        std::string str() const noexcept;

        /**
         * @brief Returns the IP address as a string. Example result: "192.168.1.110"
         * WARNING: uses a hidden global buffer, so this is safe to use only once per line
         */
        char* c_str() const noexcept;

        /** 
         * @brief Formats the IP into `buf` and returns the length of the string
         * Example result: "192.168.1.110"
         */
        int to_cstr(char* buf, int max) const noexcept;
    };


    /**
     * @brief IP address and port abstraction for both IPv4 and IPv6
     */
    struct RPPAPI ipaddress
    {
        raw_address Address;
        unsigned short Port; // port in host byte order

        /** @brief Creates a default address that is_empty() */
        ipaddress() noexcept : Address{}, Port{0} {}

        /** @brief Copies the address, but changes the port */
        ipaddress(const ipaddress& ip, int port) noexcept
            : Address{ip.Address}, Port{static_cast<uint16_t>(port)} {}

        /** @brief Copies the address, but changes the port */
        ipaddress(const raw_address& addr, int port) noexcept
            : Address{addr}, Port{static_cast<uint16_t>(port)} {}

        /**
         * Initializes a new IP address from port int
         * This can be used for listener sockets
         */
        ipaddress(address_family af, int port) noexcept
            : Address{af}, Port{static_cast<uint16_t>(port)} {}

        /**
         * Initializes a new IP address from hostname and port int
         * Example: hostname="192.168.1.110" port=14550
         * If you don't know AF, use `ipaddress::get_address_family(hostname)` to detect it
         */
        ipaddress(address_family af, const char* hostname, int port) noexcept;
        ipaddress(address_family af, const std::string& hostname, int port) noexcept
            : ipaddress{af, hostname.c_str(), port} {}

        /**
         * Initializes a new IP address from ip and port string
         * Example: "192.168.1.110:14550"
         * If you don't know AF, use `ipaddress::get_address_family(ip_and_port)` to detect it
         */
        ipaddress(address_family af, const char* ip_and_port) noexcept;
        ipaddress(address_family af, const std::string& ip_and_port) noexcept
            : ipaddress{af, ip_and_port.c_str()} {}

        /**
         * Initializes a new IP address from ip and port string
         * Example: "192.168.1.110:14550"
         * Detects IPv4 or IPv6 automatically
         */
        ipaddress(const char* ip_and_port) noexcept
            : ipaddress{get_address_family(ip_and_port), ip_and_port} {}
        ipaddress(const std::string& ip_and_port) noexcept
            : ipaddress{get_address_family(ip_and_port), ip_and_port} {}

        /**
         * Initializes a new IP address from string hostname and port
         * hostname:"192.168.1.110", port:"14550"
         * Detects IPv4 or IPv6 automatically
         */
        ipaddress(const char* hostname, const char* port) noexcept;
        ipaddress(const std::string& hostname, const std::string& port) noexcept
            : ipaddress{hostname.c_str(), port.c_str()} {}

        /**
         * Initializes a new IP address from string hostname and port
         * hostname:"192.168.1.110", port:"14550"
         * Detects IPv4 or IPv6 automatically
         */
        ipaddress(const char* hostname, int port) noexcept
            : ipaddress{get_address_family(hostname), hostname, port} {}
        ipaddress(const std::string& hostname, int port) noexcept
            : ipaddress{get_address_family(hostname), hostname.c_str(), port} {}

        /**
         * Initializes directly from a socket handle, 
         * figures out adddress family from the socket
         */
        explicit ipaddress(int socket) noexcept;

        /**
         * @returns AF_IPv4 or AF_IPv6 depending on the syntax of this ip_and_port string.
         *          AF_DontCare if the string is invalid
         */
        static address_family get_address_family(const char* ip_and_port) noexcept
        {
            return raw_address::get_address_family(ip_and_port);
        }
        static address_family get_address_family(const std::string& ip_and_port) noexcept
        {
            return raw_address::get_address_family(ip_and_port.c_str());
        }

        /**
         * Initializes this ipaddress to a resolved hostname
         * @param af Address family, such as AF_IPv4 or AF_IPv6
         * @param hostname Such as "www.google.com" or "192.168.1.0"
         * @param port Optional port number that can be used to resolve the hostname
         * @returns true if hostname was resolved successfully
         */
        bool resolve_addr(address_family af, const char* hostname, int port = 0) noexcept
        {
            return Address.resolve_addr(af, hostname, port);
        }
        bool resolve_addr(address_family af, const std::string& hostname, int port = 0) noexcept
        {
            return Address.resolve_addr(af, hostname.c_str(), port);
        }

        /** Resets this ipaddress to a default state */
        void reset() noexcept;

        /** @returns IP address part of this socket */
        const raw_address& address() const noexcept { return Address; }

        /** @returns The port number of this ipaddress */
        int port() const noexcept { return Port; }

        /**
         * @returns true if current ADDRESS part has been resolved to an IP address (or properly constructed).
         *          false if this is a listener port or an uninitialized struct
         */
        bool has_address() const noexcept { return Address.has_address(); }

        /**
         * @returns true if this address is not even a listener port, but just an uninitialized struct
         */
        bool is_empty() const noexcept { return Port == 0; }

        /**
         * @returns true if this address has at least a listener port
         **/
        bool is_valid() const noexcept { return Address.Family && Port != 0; }
        explicit operator bool() const noexcept { return Address.Family && Port != 0; }

        /** @returns Address string with port, eg "192.168.1.1:14550" "*/
        std::string str() const noexcept;
        const char* cstr() const noexcept;
        /** @brief Fills buffer dst[maxCount] with addr string */
        int to_cstr(char* dst, int maxCount) const noexcept;

        // BACKWARDS COMPATIBILITY
        std::string name() const noexcept { return str(); }
        const char* cname() const noexcept { return cstr(); }
        int name(char* dst, int maxCount) const noexcept { return to_cstr(dst, maxCount); }

        /** @returns true if this IP address family, address, port all equal */
        bool equals(const ipaddress& ip) const noexcept;
        bool operator==(const ipaddress& ip) const noexcept { return this->equals(ip); }
        bool operator!=(const ipaddress& ip) const noexcept { return !this->equals(ip); }

        /** @brief Compares two IP addresses by their family, address and then by port */
        int compare(const ipaddress& ip) const noexcept;
    };

    /**
     * @brief Wrapper class to construct IPv4 addresses
     */
    struct RPPAPI ipaddress4 : public ipaddress
    {
        /** @brief Creates a default address that is_empty() */
        ipaddress4() noexcept = default;

        /** @brief Copies the address, but changes the port */
        ipaddress4(const ipaddress& ip, int port) noexcept : ipaddress{ip, port} {}

        /** @brief Copies the address, but changes the port */
        ipaddress4(const raw_address& addr, int port) noexcept : ipaddress{addr, port} {}

        /**
         * Initializes a new IP address from port int
         * This can be used for listener sockets
         */
        explicit ipaddress4(int port) noexcept : ipaddress{AF_IPv4, port} {}

        /**
         * Initializes a new IP address from hostname and port int
         * Example: hostname="192.168.1.110" port=14550
         */
        ipaddress4(const char* hostname, int port) noexcept : ipaddress{AF_IPv4, hostname, port} {}
        ipaddress4(const std::string& hostname, int port) noexcept : ipaddress{AF_IPv4, hostname, port} {}

        /**
         * Initializes a new IP address from ip and port string
         * Example: "192.168.1.110:14550"
         */
        explicit ipaddress4(const char* ip_and_port) noexcept : ipaddress{AF_IPv4, ip_and_port} {}
        explicit ipaddress4(const std::string& ip_and_port) noexcept : ipaddress{AF_IPv4, ip_and_port} {}
    };

    /**
     * @brief Wrapper class to construct IPv6 addresses
     */
    struct RPPAPI ipaddress6 : public ipaddress
    {
        /** @brief Creates a default address that is_empty() */
        ipaddress6() noexcept = default;

        /** @brief Copies the address, but changes the port */
        ipaddress6(const ipaddress& ip, int port) noexcept : ipaddress{ip, port} {}

        /** @brief Copies the address, but changes the port */
        ipaddress6(const raw_address& addr, int port) noexcept : ipaddress{addr, port} {}

        /**
         * Initializes a new IP address from port int
         * This can be used for listener sockets
         */
        explicit ipaddress6(int port) noexcept : ipaddress(AF_IPv6, port) {}

        /**
         * Initializes a new IP address from hostname and port int
         */
        ipaddress6(const char* hostname, int port) noexcept : ipaddress(AF_IPv6, hostname, port) {}
        ipaddress6(const std::string& hostname, int port) noexcept : ipaddress{AF_IPv6, hostname, port} {}

        /**
         * Initializes a new IP address from ip and port string
         */
        explicit ipaddress6(const char* ip_and_port) noexcept : ipaddress{AF_IPv6, ip_and_port} {}
        explicit ipaddress6(const std::string& ip_and_port) noexcept : ipaddress(AF_IPv6, ip_and_port) {}
    };

    ////////////////////////////////////////////////////////////////////////////////

    struct RPPAPI ipinterface
    {
        std::string name; // friendly name
        ipaddress   addr; // address of the IP interface
        ipaddress   netmask; // subnet mask
        ipaddress   broadcast; // broadcast address
        ipaddress   gateway; // gateway address

        ipinterface() noexcept = default;

        /**
         * @returns All interfaces sorted by IP address
         */
        static std::vector<ipinterface> get_interfaces(address_family af = AF_IPv4) noexcept;

        /**
         * Gets interfaces sorted by name match
         */
        static std::vector<ipinterface> get_interfaces(const std::string& name_match, address_family af = AF_IPv4) noexcept;

        /**
         * @brief Default comparison function for ipinterfaces, sorts by `gateway` then by `addr`
         */
        static bool compare(const ipinterface& a, const ipinterface& b) noexcept;
    };

    ////////////////////////////////////////////////////////////////////////////////


    /**
     * A lightweight C++ Socket object with basic error handling and resource safety
     */
    class RPPAPI socket
    {
    public:

        static constexpr int INVALID = -1;

        // if true, Socket is shared and dtor won't call closesocket()
        static constexpr bool DEFAULT_SHARED = false;

        // if TRUE, the socket is blocking
        static constexpr bool DEFAULT_BLOCKING = true;

        // Automatically closes the socket on disconnect events.
        // This is TRUE for sockets created via accept() and connect()/connect_to().
        static constexpr bool DEFAULT_AUTOCLOSE = false;
        static constexpr bool DEFAULT_AUTOCLOSE_CLIENT_SOCKETS = false;
    
        // automatically sets nodelay (nagle disabled) on TCP sockets
        static constexpr bool DEFAULT_NODELAY = true;

    protected:

        int Sock;            // Socket handle
        ipaddress Addr;      // remote addr
        mutable int LastErr; // last error code
        bool Shared;         // if true, Socket is shared and dtor won't call closesocket() [@see socket::DEFAULT_SHARED]
        bool Blocking;       // if TRUE, the socket is blocking [@see socket::DEFAULT_BLOCKING], will be updated during create()/connect()
        bool AutoClose;      // automatically closes the socket on disconnect events [@see socket::DEFAULT_AUTOCLOSE]
        bool Connected;      // for connection oriented sockets [SC_Client, SC_Accept] whether the connection is still valid
        socket_category Category;
        socket_type Type;

    public:

        /**
         * Create a socket with an initialized handle
         * WARNING: socket will take ownership of the handle, 
         *          unless you set shared=true, which is equivalent of
         *          set_shared(true) OR calling release_noclose()
         * @note THROWS is handle is invalid
         * @param shared If TRUE, then this socket is not closed in socket destructor
         * @param blocking Whether the socket is configured as blocking or not.
         *                 On Windows, there is no way to query this from socket handle
         */
        static socket from_os_handle(int handle, const ipaddress& addr,
                                     bool shared=false, bool blocking=true);

        /**
         * @brief Creates an invalid socket object with the last_err() set to the given error code
         * @param last_err Last error code to set
         * @param addr[optional] Address information of the failure
         */
        static socket from_err_code(int last_err, const ipaddress& addr = {}) noexcept;

        /* Creates a default socket object */
        socket() noexcept;
        ~socket() noexcept;
        socket(socket&& s) noexcept;             // move construct allowed
        socket& operator=(socket&& s) noexcept;  // move assign allowed

        socket(const socket&) = delete;            // no copy construct
        socket& operator=(const socket&) = delete; // no copy assign

        /** Closes the connection (if any) and returns this socket to a default state */
        void close() noexcept;

        // Releases the socket handle from this socket:: but does not call closesocket() !
        // The socket handle itself is returned
        int release_noclose() noexcept;

        /** Set this socket as shared, which means closesocket() won't be called */
        void set_shared(bool shared=true) noexcept { Shared = shared; }
        bool is_shared() const noexcept { return Shared != 0; }

        /** Sets whether this socket automatically closes on disconnect events */
        void set_autoclosing(bool autoclose=true) noexcept { AutoClose = autoclose; }

        /** @returns true if socket is autoclosing, this is true by default
         *           for sockets created via accept() and connect() */
        bool is_autoclosing() const noexcept { return AutoClose; }

        explicit operator bool() const noexcept { return Sock != INVALID; }
        /** @return TRUE if Sock handle is currently VALID */
        bool good() const noexcept { return Sock != INVALID; }
        /** @return TRUE if Sock handle is currently INVALID */
        bool bad()  const noexcept { return Sock == INVALID; }

        /** @return OS socket handle. We are generous. */
        int os_handle() const noexcept { return Sock; }
        int oshandle() const noexcept { return Sock; }

        /** @returns Current ipaddress */
        const ipaddress& address() const noexcept { return Addr; }
        /** @returns Port of the current ipaddress */
        int port() const noexcept { return Addr.Port; }

        /** @returns String representation of ipaddress */
        std::string str() const noexcept { return Addr.str(); }
        const char* cstr() const noexcept { return Addr.cstr(); }
        std::string name() const noexcept { return Addr.str(); }
        const char* cname() const noexcept { return Addr.cstr(); }

        /** @brief Common platform independent socket errors */
        enum error : int
        {
            SE_NONE = 0, // no error
            SE_UNKNOWN = -1, // unknown error (not a common error)
            SE_NETRESET = 1, // The network has dropped the connection on reset
            SE_MSGSIZE = 2, // The message is too large to be sent all at once, as the socket requires
            SE_INPROGRESS = 3, // A blocking socket is in progress or the service provider is still processing a callback function
            SE_AGAIN = 4, // The socket is marked as nonblocking and the receive operation would block, use poll()
            SE_NOTCONN = 5, // this Socket is not Connection oriented! (aka LISTEN SOCKET)
            SE_ADDRNOTAVAIL = 6, // The remote address is not a valid address (such as ADDR_ANY)
            SE_ADDRINUSE = 7, // The specified address is already in use
            SE_CONNRESET = 8, // The remote host reset the connection request
            SE_CONNREFUSED = 9, // The remote host is actively refusing the connection
            SE_CONNABORTED = 10, // The connection was aborted by the .NET Framework or the underlying socket provider
            SE_TIMEDOUT = 11, // The connection attempt timed out, or the connected host has failed to respond
            SE_HOSTUNREACH = 12, // The remote host cannot be reached from this host at this time
            SE_NETUNREACH = 13, // The network cannot be reached from this host at this time
            SE_BADSOCKET = 14, // invalid socket
            SE_ALREADYCONN = 15, // socket is already connected and cannot connect again
            SE_ADDRFAULT = 16, // the addr argument is not in a valid part of the user address space
            SE_INTERRUPTED = 17, // The blocking call was canceled via a signal
            SE_SOCKTYPE = 18, // The socket family and protocol do not match
            SE_SOCKFAMILY = 19, // The address family is not supported by OS implementation
            SE_SHUTDOWN = 20, // The socket has been shut down
        };

        /** @return The last OS specific error code. 
         *          The user is responsible for manually parsing this depending on platform. */
        int last_errno() const noexcept;

    private:
        void set_errno() const noexcept;
        int handle_errno(int err=0) noexcept;
        int handle_txres(long ret) noexcept;
    public:

        /** @return A human readable description string of the last occurred socket error. */
        std::string last_err() const noexcept;

        /** @return The last generalized error type */
        error last_err_type() const noexcept;

        /** @returns SOL_SOCKET SO_ERROR (OS defined value) */
        int get_socket_level_error() const noexcept;

        /** @return A human readable description string of the last occurred OS socket error. */
        static std::string last_os_socket_err(int err=0) noexcept;

        /** @return A cross platform socket::error enumeration */
        static error last_os_socket_err_type(int err=0) noexcept;

        /** @return A human readable description string of a socket::error */
        static std::string to_string(error socket_error) noexcept;

        /**
         * Send data to remote socket, return number of bytes sent or -1 if socket closed
         * Automatically closes socket during critical failure
         */
        NOINLINE int send(const void* buffer, int numBytes) noexcept;

        // Send a null delimited C string
        int send(const char* str) noexcept;
        // Send a null delimited WIDE string (platform unsafe)
        int send(const wchar_t* str) noexcept;
        // Send a byte buffer
        int send(const std::vector<uint8_t>& bytes) noexcept {
            return send(bytes.data(), static_cast<int>(bytes.size()));
        }
        // Send a C++ string
        template<class T> int send(const std::basic_string<T>& str) noexcept { 
            return this->send(str.data(), static_cast<int>(sizeof(T) * str.size())); 
        }
        // Send a string view
        int send(rpp::strview str) noexcept {
            return this->send(str.data(), str.size());
        }

        /**
         * UDP only. Sends a datagram to the specified ipaddress
         * Return a number of characters sent or -1 if socket closed
         * Automatically closes socket during critical failure
         */
        int sendto(const ipaddress& to, const void* buffer, int numBytes) noexcept;

        /** Send a null delimited C string */
        int sendto(const ipaddress& to, const char* str) noexcept;
        /** Send a null delimited WIDE string (platform unsafe) */
        int sendto(const ipaddress& to, const wchar_t* str) noexcept;
        /** Send a byte buffer */
        int sendto(const ipaddress& to, const std::vector<uint8_t>& bytes) noexcept {
            return sendto(to, bytes.data(), static_cast<int>(bytes.size()));
        }
        /** Send a byte buffer */
        int sendto(const ipaddress& to, const std::vector<char>& bytes) noexcept {
            return sendto(to, bytes.data(), static_cast<int>(bytes.size()));
        }
        /** Send a C++ string */
        template<class T> int sendto(const ipaddress& to, const std::basic_string<T>& str) noexcept {
            return this->sendto(to, str.data(), static_cast<int>(sizeof(T) * str.size()));
        }
        // Send a string view
        int sendto(const ipaddress& to, rpp::strview str) noexcept {
            return this->sendto(to, str.data(), str.size());
        }

        /**
         * Forces the socket flush both the recv and send buffers
         * @warning Send buffer can only be flushed on TCP sockets, for UDP this is a no-op
         */
        NOINLINE void flush() noexcept;

        /**
         * Flushes only the socket send buffer
         * @warning Send buffer can only be flushed on TCP sockets, for UDP this is a no-op
         */
        NOINLINE void flush_send_buf() noexcept;

        /**
         * Flushes the socket receive buffer. Until available() reports 0.
         */
        NOINLINE void flush_recv_buf() noexcept;

        /**
         * Skips a number of bytes from the recv buffer
         * This is a controlled flush of the OS socket read buffer
         * @note This MAY block until bytesToSkip is achieved
         * @warning `skip(available());` should be used with caution, since the
         *          behaviour is different on WINSOCK vs LINUX.
         *          If you want to flush recv buffer, call flush_recv_buf()
         * @param bytesToSkip Number of bytes we want to skip
         * @return Number of bytes actually skipped.
         */
        NOINLINE int skip(int bytesToSkip) noexcept;

        /**
         * Peeks the socket for currently available bytes to read
         * Automatically closes socket during critical failure and returns -1
         * If there is no data in recv buffer, this function returns 0
         *
         * @note There is a critical difference between WINSOCK and LINUX sockets.
         *       WINSOCK UDP: reports total # of bytes in receive buffer
         *       LINUX UDP: report size of the next datagram only
         */
        NOINLINE int available() const noexcept;

        /**
         * Attempts to only peek the size of a single datagram.
         * On LINUX this is equivalent to available()
         * On WINSOCK this is will peek up to 4096 byte-sized datagrams
         * @warning The downside is that this will cause bytes to be read twice
         *          from the recv buffer, so use at your own risk
         */
        NOINLINE int peek_datagram_size() noexcept;

        /**
         * Receive data from remote socket, return number of bytes received
         * Automatically closes socket during critical failure and returns -1
         * If there is no data to receive, this function returns 0
         */
        NOINLINE int recv(void* buffer, int maxBytes) noexcept;

        /**
         * @brief Waits up to timeout until data is available and then calls recv() or returns 0
         * @param timeout Timeout millis
         */
        int recv_timeout(void* buffer, int maxBytes, int timeout) noexcept
        {
            return wait_available(timeout) ? recv(buffer, maxBytes) : 0;
        }

        /**
         * Peek bytes from remote socket, return number of bytes peeked.
         * Automatically closes TCP socket during critical failure and returns -1
         * If there is no data to peek, this function returns 0
         * 
         * If this socket::is_blocking(), then poll(0ms, PF_Read) will be used to
         * avoid blocking.
         */
        NOINLINE int peek(void* buffer, int numBytes) noexcept;

        /**
         * UDP only. Receives up to maxBytes from some ipaddress.
         * The application must handle the ipaddress themselves as usual for UDP.
         * If there is no data available, this function returns 0
         */
        NOINLINE int recvfrom(ipaddress& from, void* buffer, int maxBytes) noexcept;

        /**
         * @brief Waits up to timeout until data is available and then calls recvfrom() or returns 0
         * @param timeout Timeout millis
         */
        int recvfrom_timeout(ipaddress& from, void* buffer, int maxBytes, int timeout) noexcept
        {
            return wait_available(timeout) ? recvfrom(from, buffer, maxBytes) : 0;
        }

        /**
         * Peeks available() bytes, resizes outBuffer and reads some data into it
         * returns TRUE if outBuffer was written to, FALSE if no data or socket error
         */
        NOINLINE bool recv(std::vector<uint8_t>& outBuffer);
        
        /**
         * UDP only. Peeks available() bytes, resizes outBuffer and reads a single packet into it
         * returns TRUE if outBuffer was written to, FALSE if no data or socket error
         */
        NOINLINE bool recvfrom(ipaddress& from, std::vector<uint8_t>& outBuffer);

        /**
         * Peeks the recv buffer with Socket_Available and then builds
         * a class T container of the available data.
         * Only maxCount elems or less will be read - default maxCount is INT_MAX
         * If no data available or critical failure, an empty container is returned
         * Class T must implement:
         *     void resize(numElements);
         *     value_type* data();
         *     typedef ... value_type;
         */
        template<class T> NOINLINE T recv_gen(int maxCount = 0x7fffffff) noexcept
        {
            int count = available() / sizeof(typename T::value_type);
            int n = count < maxCount ? count : maxCount;
            if (n <= 0) return T{};

            T cont; cont.resize(n);
            int received = recv((void*)cont.data(), n * sizeof(typename T::value_type));
            if (received <= 0) return T{};

            // even though we had N+ bytes available, a single packet might be smaller
            if (received < n) cont.resize(received); // truncate
            return cont;
        }

        /** UDP version of recv_gen */
        template<class T> NOINLINE T recvfrom_gen(ipaddress& from, int maxCount = 0x7fffffff) noexcept
        {
            int count = available() / sizeof(typename T::value_type);
            int n = count < maxCount ? count : maxCount;
            if (n <= 0) return T{};

            T cont; cont.resize(n);
            int received = recvfrom(from, (void*)cont.data(), n * sizeof(typename T::value_type));
            if (received <= 0) return T{};

            // even though we had N+ bytes available, a single packet might be smaller
            if (received < n) cont.resize(received); // truncate
            return cont;
        }

        std::string recv_str(int maxChars = 0x7fffffff) noexcept { return recv_gen<std::string>(maxChars); }
        std::wstring recv_wstr(int maxChars = 0x7fffffff) noexcept { return recv_gen<std::wstring>(maxChars); }
        std::vector<uint8_t> recv_data(int maxCount = 0x7fffffff) noexcept { return recv_gen<std::vector<uint8_t>>(maxCount); }

        std::string recvfrom_str(ipaddress& from, int maxChars = 0x7fffffff) noexcept { return recvfrom_gen<std::string>(from, maxChars); }
        std::wstring recvfrom_wstr(ipaddress& from, int maxChars = 0x7fffffff) noexcept { return recvfrom_gen<std::wstring>(from, maxChars); }
        std::vector<uint8_t> recvfrom_data(ipaddress& from, int maxCount = 0x7fffffff) noexcept { return recvfrom_gen<std::vector<uint8_t>>(from, maxCount); }

        /**
         * Peeks the recv buffer for a single string, up to maxCount chars
         */
        std::string peek_str(int maxCount = 0x7fffffff) noexcept;

        /**
         * Waits up to timeout millis for data from remote end.
         * If no data available or critical failure, an empty vector is returned
         */
        template<class T> NOINLINE T wait_recv(int millis) noexcept
        {
            return wait_available(millis) ? recv_gen<T>() : T{};
        }
        std::string wait_recv_str(int millis) noexcept { return wait_recv<std::string>(millis); }
        std::wstring wait_recv_wstr(int millis) noexcept { return wait_recv<std::wstring>(millis); }
        std::vector<uint8_t> wait_recv_data(int millis) noexcept { return wait_recv<std::vector<uint8_t>>(millis); }


        /**
         * Waits up to timeoutMillis for available data from remote end.
         * @return TRUE if there's data available, FALSE otherwise.
         */
        bool wait_available(int millis = 1000) noexcept;


        // Sends a request and waits until an answer is returned
        template<class T, class U> T request(const U& request, int millis = 1000) noexcept
        {
            return send(request) <= 0 ? T{} : wait_recv<T>(millis);
        }
        template<class U> auto request_str(const U& req, int millis = 1000) noexcept { return request<std::string>(req, millis); }
        template<class U> auto request_wstr(const U& req, int millis = 1000) noexcept { return request<std::wstring>(req, millis); }
        template<class U> auto request_data(const U& req, int millis = 1000) noexcept { return request<std::vector<uint8_t>>(req, millis); }


        /**
         * UDP version of wait_recv. Waits up to timeout millis for data from remote end.
         * If no data available or critical failure, an empty vector is returned
         */
        template<class T> NOINLINE T wait_recvfrom(ipaddress& from, int millis) noexcept
        {
            return wait_available(millis) ? recvfrom_gen<T>(from) : T{};
        }
        std::string wait_recvfrom_str(ipaddress& from, int millis) noexcept { return wait_recvfrom<std::string>(from,millis); }
        std::wstring wait_recvfrom_wstr(ipaddress& from, int millis) noexcept { return wait_recvfrom<std::wstring>(from,millis); }
        std::vector<uint8_t> wait_recvfrom_data(ipaddress& from, int millis) noexcept { return wait_recvfrom<std::vector<uint8_t>>(from,millis); }



        /**
         * Get socket option from the specified optlevel with the specified opt key SO_xxxx
         * @note Reference Socket options: https://msdn.microsoft.com/en-us/library/windows/desktop/ms740525%28v=vs.85%29.aspx
         * @return getsockopt result for the specified socketopt SO_xxxx or -1 if error occurred (check last_err())
         */
        int get_opt(int optlevel, int socketopt) const noexcept;

        /**
         * Tries to set a socket option in the optlevel for the specified socketopt SO_xxxx
         * @note Reference Socket options: https://msdn.microsoft.com/en-us/library/windows/desktop/ms740525%28v=vs.85%29.aspx
         * @param optlevel Socket option level such as IPPROTO_IP or IPPROTO_TCP.
         * @return 0 on success, error code otherwise (or check last_err())
         */
        int set_opt(int optlevel, int socketopt, int value) noexcept;

        /**
         * Get values from IO handle with the specified IO command
         * @note This function designed for iocmd-s that GET values
         * @note Reference ioctlsocket: https://msdn.microsoft.com/en-us/library/windows/desktop/ms738573%28v=vs.85%29.aspx
         * @return 0 on success, error code otherwise (or check errno)
         */
        int get_ioctl(int iocmd, int& outValue) const noexcept;

        /**
         * Control IO handle with the specified IO command
         * @note This function designed for iocmd-s that SET values
         * @note Reference ioctlsocket: https://msdn.microsoft.com/en-us/library/windows/desktop/ms738573%28v=vs.85%29.aspx
         * @return 0 on success, error code otherwise (or check last_err())
         */
        int set_ioctl(int iocmd, int value) noexcept;

        /**
         * Enables broadcast on UDP sockets, returns true on success
         */
        bool enable_broadcast() noexcept;

        /**
         * Configure socket settings: non-blocking I/O,
         * Nagle disabled (TCP_NODELAY=TRUE), only applies to TCP sockets
         */
        void set_noblock_nodelay() noexcept;

        /**
         * Configure socket settings: I/O blocking mode off(false) or on(true)
         * @param socketsBlock (TRUE = BLOCK, FALSE = NOBLOCK)
         * @return TRUE if set succeded, check socket::last_err() for error message
         */
        bool set_blocking(bool socketsBlock) noexcept;

        /**
         * @return TRUE if socket is set to blocking: set_blocking(true);,
         *         check socket::last_err() for error message
         **/
        bool is_blocking() const noexcept;

        /**
         * Configure socket settings: Nagle off (TCP_NODELAY) 
         * or on (Nagle bandwidth opt.)
         * @note This only applies for TCP sockets
         * @param enableNagle (FALSE = TCP_NODELAY, TRUE = Nagle enabled)
         * @return TRUE if set succeeded, check socket::last_err() for error message
         */
        bool set_nagle(bool enableNagle) noexcept;

        /** @see set_nagle() */
        bool set_nodelay(bool nodelay) noexcept { return set_nagle(!nodelay); }

        /**
         * @returns true if socket is set to nodelay: set_nagle(false);, 
         *          check socket::last_err() for error message
         */
        bool is_nodelay() const noexcept;

        enum buffer_option
        {
            BO_Recv, // SO_RCVBUF
            BO_Send, // SO_SNDBUF
        };

        /**
         * Sets the receive/send buffer size
         * @param size Size in bytes
         * @param force On UNIX platforms, uses SO_RCVBUFFORCE/SO_SNDBUFFORCE to force the size
         * @return TRUE if set size was successful, check socket::last_err() for error message
         */
        bool set_buf_size(buffer_option opt, size_t size, bool force = false) noexcept;
        /**
         * Sets the receive buffer size
         * @param size Size in bytes
         * @param force On UNIX platforms, uses SO_RCVBUFFORCE to force the size
         * @return TRUE if set size was successful, check socket::last_err() for error message
         */
        bool set_rcv_buf_size(size_t size, bool force = false) noexcept { return set_buf_size(BO_Recv, size, force); }
        /**
         * Sets the send buffer size
         * @param size Size in bytes
         * @param force On UNIX platforms, uses SO_SNDBUFFORCE to force the size
         * @return true if set size was successful, check socket::last_err() for error message
         */
        bool set_snd_buf_size(size_t size, bool force = false) noexcept { return set_buf_size(BO_Send, size, force); }

        /** @return Receiver/Send buffer size */
        int get_buf_size(buffer_option opt) const noexcept;
        /** @return Receive buffer size */
        int get_rcv_buf_size() const noexcept { return get_buf_size(BO_Recv); }
        /** @return Send buffer size */
        int get_snd_buf_size() const noexcept { return get_buf_size(BO_Send); }


        /** @return Remaining space in the socket send buffer. 0 if no space. -1 on error. */
        int get_send_buffer_remaining() const noexcept;

        /**
         * @brief Sets the socket SO_LINGER option.
         * When enabled, a close() will not return until all queued messages for the
         * socket have been successfully sent or the linger timeout has been reached. Otherwise,
         * the call returns immediately and the closing is done in the background.
         * @param active TRUE to enable linger option, FALSE to disable
         * @param seconds The linger timeout in seconds (0 = immediate close with ENETRESET)
         */
        bool set_linger(bool active, int seconds) noexcept;

        ////////////////////////////////////////////////////////////////////////////

        /**
         * @returns The socket category, describing how this socket was first initialized.
         *          SC_Listen: this socket is a LISTEN server socket via socket::listen().
         *          SC_Accept: this socket was accepted via socket::accept() as a server side client.
         *          SC_Client: this is a Client side connection socket via socket::connect().
         */
        socket_category category() const noexcept { return Category; }

        /** @return The SocketType of the socket */
        socket_type type() const noexcept { return Type; }

        /** @brief Updates the internal socket Type */
        socket_type update_socket_type() noexcept;

        /** @return The AddressFamily of the socket */
        address_family family() const noexcept;

        /** @return The IP Protocol of the socket */
        ip_protocol ipproto() const noexcept;

        /** @return ProtocolInfo of the socket */
        protocol_info protocol() const noexcept;

        /**
         * Checks if the socket is still valid and connected. If connection has
         * reset or remote end has closed the connection, the socket will be closed
         * @return TRUE if the socket is still valid and connected
         */
        bool connected() noexcept;

        ////////////////////////////////////////////////////////////////////////////

        /**
         * @brief Creates a new socket without binding or connecting anything
         * @warning This will always close() the socket if it was already created, assuming the new settings are different.
         * @param af Address family of the socket
         * @param ipp IP protocol of the socket
         * @param opt Socket options to set. @see socket_option and @see socket::DEFAULT_BLOCKING
         */
        bool create(address_family af = AF_IPv4,
                    ip_protocol ipp = IPP_TCP,
                    socket_option opt = SO_None) noexcept;
        
        // Binds this socket to some address:port -- as a TCP listener OR a general UDP socket
        // If you bind an UDP socket to a remote address, you can use send() to send datagrams without specifying remote address
        bool bind(const ipaddress& address) noexcept;

        /**
         * @brief For TCP sockets, starts listening for new clients, returns true on success
         */
        bool listen() noexcept;


        enum SelectFlag
        {
            SF_Read   = (1<<0), // select if characters available for reading, i.e. read() won't block
            SF_Write  = (1<<1), // select if a send() will not block or a connection was established
            SF_Except = (1<<2), // select for any exceptional IO conditions, such as MSG_OOB
            SF_ReadWrite = SF_Read|SF_Write, //
        };

        /**
         * @brief Tries to select this socket for conditions
         * Select suspends the thread until this Socket file descriptor is signaled
         * @param timeoutMillis The maximum time to wait for the socket to be signaled
         * @param selectFlags Which condition should trigger a return? Read, Write, ReadWrite, Except?
         * @returns true on condition success, false on timeout or error. (check socket::last_err())
         */
        bool select(int timeoutMillis, SelectFlag selectFlags = SF_ReadWrite) noexcept;


        enum PollFlag
        {
            PF_Read = (1<<0), // poll if socket is ready for reading
            PF_Write = (1<<1), // poll if socket is ready for writing
            PF_ReadWrite = PF_Read|PF_Write, // poll if socket is ready for reading or writing
        };

        /**
         * @brief Calls poll() or WSAPoll() on this socket
         * This is faster than select()
         * @warning After calling poll(), you MUST read all available data from the socket
         *          before calling poll again.
         *          poll() will only react if there is NEW data available
         * @param timeoutMillis Timeout in milliseconds to suspend until the socket is signaled
         * @param pollFlags [optional] Poll flags to use, default is [PF_Read]
         * @returns true if the socket is signaled, false on timeout or error (check socket::last_err())
         */
        bool poll(int timeoutMillis, PollFlag pollFlags = PF_Read) noexcept;

        /**
         * @brief Enables polling multiple sockets for READ readiness
         * @param in List of sockets to poll
         * @param ready Indexes of sockets that are ready for reading
         * @param timeoutMillis Maximum time to wait for a socket to be ready
         * @param pollFlags [optional] Poll flags to use, default is [PF_Read]
         * @returns true if at least one socket is ready for reading, false on timeout or error
         *          To handle errors, you must check each socket last_err() individually
         */
        static bool poll(const std::vector<socket*>& in, std::vector<int>& ready,
                         int timeoutMillis, PollFlag pollFlags = PF_Read) noexcept;

    private:
        bool on_poll_result(int revents, PollFlag pollFlags) noexcept;

    public:
        ////////////////////////////////////////////////////////////////////////////

        /**
         * Creates a new listener type socket used for accepting new connections
         * @return TRUE if the socket is successfully listening to addr
         */
        bool listen(const ipaddress& localAddr,
                    ip_protocol ipp = IPP_TCP,
                    socket_option opt = SO_None) noexcept;

        /**
         * STATIC
         * Creates a new listener type socket used for accepting new connections
         * @returns valid socket if successful, otherwise a default socket{} object
         */
        static socket listen_to(const ipaddress& localAddr,
                                ip_protocol ipp = IPP_TCP,
                                socket_option opt = SO_None) noexcept;

        /**
         * STATIC
         * Creates a new generic socket for sending and receiving UDP datagrams
         * @returns valid socket if successful, otherwise a default socket{} object
         */
        static socket listen_to_udp(const ipaddress& localAddr, socket_option opt = SO_None) noexcept
        {
            return listen_to(localAddr, IPP_UDP, opt);
        }

        /**
         * STATIC
         * Creates a new generic socket for sending and receiving UDP datagrams
         * @returns valid socket if successful, otherwise a default socket{} object
         */
        static socket listen_to_udp(int localPort, socket_option opt = SO_None) noexcept
        {
            return listen_to(ipaddress{AF_IPv4, localPort}, IPP_UDP, opt);
        }

        /**
         * STATIC
         * Creates a new generic socket for sending and receiving UDP datagrams
         * @returns valid socket if successful, otherwise a default socket{} object
         */
        static socket make_udp(const ipaddress& localAddr, socket_option opt = SO_None) noexcept
        {
            return listen_to(localAddr, IPP_UDP, opt);
        }

        /**
         * Try accepting a new connection from THIS listening socket.
         * If a fatal error happens, and AutoClosing is enabled, this can close the socket.
         * 
         * @param timeoutMillis if > 0, polls the socket until timeoutMillis is reached
         *                      if == 0, tests for a pending connection and returns immediately
         *                      if < 0, waits forever until a connection is established
         * 
         * @return Invalid socket if there are no pending connections, otherwise a valid socket handle
         */
        socket accept(int timeoutMillis = 0) noexcept;

        /**
         * Connects to a remote socket and sets the socket as nonblocking and tcp nodelay
         * @param remoteAddr Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
         * @param opt Socket options to set, use SO_Blocking if you want blocking sockets
         * @return TRUE: valid socket, false: error (check socket::last_err())
         */
        bool connect(const ipaddress& remoteAddr, socket_option opt) noexcept;

        /**
         * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
         * @param remoteAddr Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
         * @param millis Timeout value if the server takes too long to respond
         * @param opt Socket options to set, use SO_Blocking if you want blocking sockets
         * @return TRUE: valid socket, false: error (check socket::last_err())
         */
        bool connect(const ipaddress& remoteAddr, int millis,
                     socket_option opt = SO_None) noexcept;

    private:
        void configure_connected_client(socket_option opt) noexcept;

    public:

        /**
         * Connects to a remote socket and sets the socket as nonblocking and tcp nodelay
         * @param remoteAddr Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
         * @param opt Socket options to set, use SO_Blocking if you want blocking sockets
         * @return If successful, an initialized Socket object
         */
        static socket connect_to(const ipaddress& remoteAddr,
                                 socket_option opt = SO_None) noexcept;

        /**
         * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
         * @param remoteAddr Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
         * @param millis Timeout value if the server takes too long to respond
         * @param opt Socket options to set, use SO_Blocking if you want blocking sockets
         * @return If successful, an initialized Socket object
         */
        static socket connect_to(const ipaddress& remoteAddr, int millis,
                                 socket_option opt = SO_None) noexcept;


        /**
         * Starts an Async IO operation to accept a new connection until a connection
         * arrives or the specified timeout is reached
         * To pend asynchronously forever, set timeoutMillis to -1
         * The callback(socket s) receives accepted Socket object
         */
        //template<class Func> void accept_async(Func&& func, int millis = -1) noexcept
        //{
        //    // TODO: rewrite using new rpp coroutines
        //    //struct async {
        //    //    socket* listener;
        //    //    Func callback;
        //    //    int millis;
        //    //    static void accept(async* arg) {
        //    //        socket* listener = arg->listener;
        //    //        auto    callback = arg->callback;
        //    //        int     timeout  = arg->millis;
        //    //        delete arg; // delete before callback (in case of exception)
        //    //        callback(std::move(listener->accept(timeout)));
        //    //    }
        //    //};
        //    //if (auto* arg = new async{ this, std::forward<Func>(func), millis })
        //    //    spawn_thread((void(*)(void*))&async::accept, arg);
        //}
        /**
         * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
         * @param remoteAddr Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
         * @param func The callback(socket s) receives accepted Socket object
         * @param millis Timeout value if the server takes too long to respond
         */
        //template<class Func> void connect_async(const ipaddress& remoteAddr, Func&& func, int millis, socket_option opt = SO_None) noexcept
        //{
        //    // TODO: rewrite using new rpp coroutines
        //    //struct async {
        //    //    ipaddress remoteAddr;
        //    //    Func callback;
        //    //    int millis;
        //    //    socket_option opt;
        //    //    static void connect(async* arg) {
        //    //        async a { std::move(*arg) };
        //    //        delete arg; // delete before callback (in case of an exception)
        //    //        a.callback(std::move(socket::connect_to(a.remoteAddr, a.millis, a.opt)));
        //    //    }
        //    //};
        //    //close();
        //    //Addr = remoteAddr;
        //    //if (auto* arg = new async{ remoteAddr, std::forward<Func>(func), millis, opt })
        //    //    spawn_thread((void(*)(void*))&async::connect, arg);
        //}
    };

    ////////////////////////////////////////////////////////////////////////////////

    /**
     * Creates am INADDR_ANY UDP socket with a random port
     * @param bind_address Address to bind to, default is INADDR_ANY
     */
    RPPAPI socket make_udp_randomport(socket_option opt = SO_None, raw_address bind_address = raw_address{AF_IPv4}) noexcept;

    /**
     * Creates an INADDR_ANY TCP Listener with a random port number
     * @param bind_address Address to bind to, default is INADDR_ANY
     */
    RPPAPI socket make_tcp_randomport(socket_option opt = SO_None, raw_address bind_address = raw_address{AF_IPv4}) noexcept;

    /**
     * @returns Main interface of the current system
     * @param interface ["eth|lan|wlan"] Main interface name substring patterns to match
     * @param af Address family to use, most commonly AF_IPv4 or AF_IPv6
     */
    rpp::ipinterface get_ip_interface(const std::string& network_interface = "eth|lan|wlan", address_family af = AF_IPv4);

    /**
     * @returns Main interface IP address of the current system
     * @param interface ["eth|lan|wlan"] Main interface name substring patterns to match
     */
    std::string get_system_ip(const std::string& network_interface = "eth|lan|wlan", address_family af = AF_IPv4);

    /**
     * @returns Main interface Broadcast IP address of the current system
     * @param interface ["eth|lan|wlan"] Main interface name substring patterns to match
     */
    std::string get_broadcast_ip(const std::string& network_interface = "eth|lan|wlan", address_family af = AF_IPv4);

    ////////////////////////////////////////////////////////////////////////////////

} // namespace rpp
