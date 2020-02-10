#pragma once
/**
 * Simple and efficient wrapper around POSIX sockets, Copyright (c) 2014-2018, Jorma Rebane
 * Distributed under MIT Software License
 */
#if _MSC_VER
#  pragma warning(disable: 4251) // class 'std::*' needs to have dll-interface to be used by clients of struct 'rpp::*'
#endif
#include <string>   // std::string, std::wstring
#include <vector>   // std::vector
#include "config.h"

namespace rpp
{
    using std::string;
    using std::wstring;
    using std::basic_string;
    using std::vector;

    enum RPPAPI address_family
    {
        AF_DontCare,    // unspecified AddressFamily, service provider will choose most appropriate
        AF_IPv4,        // The Internet Protocol version 4 (IPv4) address family
        AF_IPv6,        // The Internet Protocol version 6 (IPv6) address family
        AF_Bth,         // Bluetooth address family, supported since WinXP SP2
    };

    enum RPPAPI socket_type
    {
        ST_Unspecified, // unspecified socket type (invalid socket)
        ST_Stream,      // TCP only, byte stream for IPv4 or IPv6 protocols
        ST_Datagram,    // UDP only, byte datagrams for IPv4 or IPv6 protocols
        ST_Raw,         // application provides IPv4 or IPv6 headers
        ST_RDM,         // reliable message datagram for PGM
        ST_SeqPacket,   // TCP based, similar to ST_Stream but slower, however packet boundaries are respected "TCP datagrams"
    };

    enum RPPAPI ip_protocol
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
        SO_Blocking  = (1<<1),  // request a blocking socket instead of the default non-blocking
        // Enables socket load balancing and buffering Nagle algorithm.
        // Will cause delays. Only applies to TCP sockets.
        SO_Nagle     = (1<<2),  
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


    // Sleeps for specified milliseconds duration
    void RPPAPI thread_sleep(int milliseconds) noexcept;

    // Spawns a new thread, thread handles are automatically closed
    void RPPAPI spawn_thread(void(*thread_func)(void* arg), void* arg) noexcept;

    // Measure highest accuracy time in seconds for both Windows and Linux
    double RPPAPI timer_time() noexcept;


    // Basic IP-Address abstraction
    struct RPPAPI ipaddress
    {
        address_family Family;     // IPv4 or IPv6
        unsigned short Port;       // port in host byte order
        #if _MSC_VER
        #  pragma warning(disable:4201)
        #endif
        union {
            union {
                unsigned long Addr4; // IPv4 Address
                unsigned char Addr4Parts[4];
            };
            struct {
                unsigned char Addr6[16]; // IPv6 Address
                unsigned long FlowInfo;
                unsigned long ScopeId;
            };
        };

        ipaddress() noexcept; // memset 0 ipaddress
        ipaddress(address_family af) noexcept;
        ipaddress(address_family af, int port) noexcept;
        ipaddress(address_family af, const char* hostname, int port) noexcept;
        ipaddress(address_family af, const string& hostname, int port) noexcept;
        ipaddress(int socket) noexcept;
        ipaddress(address_family af, const string& ipAddressAndPort) noexcept;

        bool resolve_addr(const char* hostname) noexcept;

        // True if current ADDRRESS part has been resolved (or properly constructed)
        bool is_resolved() const noexcept;

        // @brief Fills buffer dst[maxCount] with addr string
        int name(char* dst, int maxCount) const noexcept;
        string name() const noexcept;
        const char* cname() const noexcept;

        void clear() noexcept;
        int port() const noexcept;
    };
    struct RPPAPI ipaddress4 : public ipaddress
    {
        ipaddress4() noexcept : ipaddress(AF_IPv4) {}
        ipaddress4(int port) noexcept : ipaddress(AF_IPv4, port) {}
        ipaddress4(const char* hostname, int port) noexcept : ipaddress(AF_IPv4, hostname, port) {}
        ipaddress4(const string& ipAddressAndPort) noexcept : ipaddress(AF_IPv4, ipAddressAndPort) {}
    };
    struct RPPAPI ipaddress6 : public ipaddress
    {
        ipaddress6() noexcept : ipaddress(AF_IPv6) {}
        ipaddress6(int port) noexcept : ipaddress(AF_IPv6, port) {}
        ipaddress6(const char* hostname, int port) noexcept : ipaddress(AF_IPv6, hostname, port) {}
        ipaddress6(const string& ipAddressAndPort) noexcept : ipaddress(AF_IPv6, ipAddressAndPort) {}
    };

    ////////////////////////////////////////////////////////////////////////////////

    struct RPPAPI ipinterface
    {
        string    name; // friendly name
        ipaddress addr; // address of the IP interface
        string    addrname;

        ipinterface() = default;
        ipinterface(string&& name, const ipaddress& addr, string&& addrname) noexcept
            : name(move(name)), addr(addr), addrname(move(addrname)) {}

        static vector<ipinterface> get_interfaces(address_family af = AF_IPv4);
    };

    ////////////////////////////////////////////////////////////////////////////////


    /**
     * A lightweight C++ Socket object with basic error handling and resource safety
     */
    class RPPAPI socket
    {
    protected:

        // mostly for debugging
        enum category : unsigned char {
            SC_Unknown,
            SC_Listen,  // this socket is a LISTEN server socket via socket::listen
            SC_Accept,  // this socket was accepted vie socket::accept as a server side client
            SC_Client,  // this is a Client side connection socket via socket::connect
        };

        static constexpr int INVALID = -1;
        int       Sock;   // Socket handle
        ipaddress Addr;   // remote addr
        bool      Shared; // if true, Socket is shared and dtor won't call closesocket()
        bool      Blocking; // only relevant on windows, if TRUE, the socket is blocking
        category  Category;

    public:

        // Create a socket with an initialized handle
        // WARNING: socket will take ownership of the handle, 
        //          unless you set shared=true, which is equivalent of
        //          set_shared(true) OR calling release_noclose()
        // @param shared If TRUE, then this socket is not closed in socket destructor
        // @param blocking Whether the socket is configured as blocking or not.
        //                 On Windows, there is no way to query this from socket handle
        socket(int handle, const ipaddress& addr, bool shared=false, bool blocking=true) noexcept;

        // Creates a default socket object
        socket() noexcept;
        // Creates a listener socket, can be TCP or UDP
        explicit socket(int port, address_family af = AF_IPv4, ip_protocol ipp = IPP_TCP, socket_option opt = SO_None) noexcept;
        // Creates a connection to the specified remote address, always TCP!
        socket(const ipaddress& addr, socket_option opt = SO_None) noexcept;
        // Tries to connect to the specified remote address with timeout, always TCP!
        socket(const ipaddress& addr, int millis, socket_option opt = SO_None) noexcept;
        // Connects to hostname:port, always TCP! 
        socket(const char* hostname, int port, address_family af = AF_IPv4, socket_option opt = SO_None) noexcept;
        // Connects to hostname:port with timeout, always TCP! 
        socket(const char* hostname, int port, int millis, address_family af = AF_IPv4, socket_option opt = SO_None) noexcept;
        ~socket() noexcept;

        // Closes the connection (if any) and returns this socket to a default state
        void close() noexcept;

        // Releases the socket handle from this socket:: but does not call closesocket() !
        // The socket handle itself is returned
        int release_noclose() noexcept;

        // Set this socket as shared, which means closesocket() won't be called
        void set_shared(bool shared=true) noexcept { Shared = shared; }
        bool is_shared() const noexcept { return Shared != 0; }

        explicit operator bool() const noexcept { return Sock != INVALID; }
        // @return TRUE if Sock handle is currently VALID
        bool good() const noexcept { return Sock != INVALID; }
        // @return TRUE if Sock handle is currently INVALID
        bool bad()  const noexcept { return Sock == INVALID; }
        // @return OS socket handle. We are generous.
        int oshandle() const noexcept { return Sock; }

        socket(const socket&)            = delete; // no copy construct
        socket& operator=(const socket&) = delete; // no copy assign
        socket(socket&& fwd)            noexcept;  // move construct allowed
        socket& operator=(socket&& fwd) noexcept;  // move assign allowed

        // @return Current ipaddress
        const ipaddress& address() const noexcept { return Addr; }
        int port() const noexcept { return Addr.Port; }
        // @return String representation of ipaddress
        string name() const noexcept { return Addr.name(); }
        const char* cname() const noexcept { return Addr.cname(); }

        /**
         * @return A human readable description string of the last occurred socket error in errno.
         */
        static string last_err(int err=0) noexcept;

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
        int send(const vector<uint8_t>& bytes) noexcept {
            return send(bytes.data(), static_cast<int>(bytes.size()));
        }
        // Send a C++ string
        template<class T> int send(const basic_string<T>& str) noexcept { 
            return send(str.data(), int(sizeof(T) * str.size())); 
        }

        /**
         * UDP only. Sends a datagram to the specified ipaddress
         * Return a number of characters sent or -1 if socket closed
         * Automatically closes socket during critical failure
         */
        int sendto(const ipaddress& to, const void* buffer, int numBytes) noexcept;
        // Send a null delimited C string
        int sendto(const ipaddress& to, const char* str) noexcept;
        // Send a null delimited WIDE string (platform unsafe)
        int sendto(const ipaddress& to, const wchar_t* str) noexcept;
        // Send a byte buffer
        int sendto(const ipaddress& to, const vector<uint8_t>& bytes) noexcept {
            return sendto(to, bytes.data(), static_cast<int>(bytes.size()));
        }
        // Send a C++ string
        template<class T> int sendto(const ipaddress& to, const basic_string<T>& str) noexcept {
            return sendto(to, str.data(), int(sizeof(T) * str.size()));
        }

        // Forces the socket object to flush any data in its buffers
        NOINLINE void flush() noexcept;

        /**
         * Peeks the socket for currently available bytes to read
         * Automatically closes socket during critical failure and returns -1
         * If there is no data in recv buffer, this function returns 0
         */
        NOINLINE int available() const noexcept;

        /**
         * Receive data from remote socket, return number of bytes received
         * Automatically closes socket during critical failure and returns -1
         * If there is no data to receive, this function returns 0
         */
        NOINLINE int recv(void* buffer, int maxBytes) noexcept;

        /**
         * Peek bytes from remote socket, return number of bytes peeked
         * Automatically closes socket during critical failure and returns -1
         * If there is no data to peek, this function returns 0
         */
        NOINLINE int peek(void* buffer, int numBytes) noexcept;

        /**
         * Skips a number of bytes from the read stream
         * This is a controlled flush of the OS socket read buffer
         */
        NOINLINE void skip(int n) noexcept;

        /**
         * UDP only. Receives up to maxBytes from some ipaddress.
         * The application must handle the ipaddress themselves as usual for UDP.
         * If there is no data available, this function returns 0
         */
        NOINLINE int recvfrom(ipaddress& from, void* buffer, int maxBytes) noexcept;

        /**
         * Peeks available() bytes, resizes outBuffer and reads some data into it
         * returns TRUE if outBuffer was written to, FALSE if no data or socket error
         */
        NOINLINE bool recv(vector<uint8_t>& outBuffer);
        
        /**
         * UDP only. Peeks available() bytes, resizes outBuffer and reads a single packet into it
         * returns TRUE if outBuffer was written to, FALSE if no data or socket error
         */
        NOINLINE bool recvfrom(ipaddress& from, vector<uint8_t>& outBuffer);
        

    private: 
        int handle_errno(int err=0) noexcept;
        int handle_txres(long ret) noexcept;

        template<class Action> static bool try_for_period(int millis, Action action) noexcept {
            const double timeout = millis / 1000.0; // millis to seconds
            const double start = timer_time();
            do {
                if (action())
                    return true; // success
                thread_sleep(1); // suspend until next timeslice
            } while (timeout > 0.0 && (timer_time() - start) < timeout);
            return false; // timeout
        }
        template<class Ret> Ret try_recv(Ret (socket::*recv)(int max), int millis) noexcept {
            Ret res;
            try_for_period(millis, [&]() {
                return available() != 0 ? (res = (this->*recv)(0x7fffffff)),true : false;
            });
            return res;
        }
        template<class Ret> Ret try_recvfrom(Ret (socket::*recvfrom)(ipaddress& from, int max), ipaddress& from, int millis) noexcept {
            Ret res;
            try_for_period(millis, [&]() {
                return available() != 0 ? (res = (this->*recvfrom)(from, 0x7fffffff)),true : false;
            });
            return res;
        }
    public:

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
        template<class T> NOINLINE T recv_gen(int maxCount = 0x7fffffff) noexcept {
            int count = available() / sizeof(typename T::value_type);
            if (count <= 0) return T{};
            int n = count < maxCount ? count : maxCount;
            T cont;
            cont.resize(n);
            int received = recv((void*)cont.data(), n * sizeof(typename T::value_type));
            // even though we had N+ bytes available, a single packet might be smaller
            if (received >= 0 && n != received)
                cont.resize(received); // truncate
            return cont;
        }
        string          recv_str (int maxChars = 0x7fffffff) noexcept { return recv_gen<string>(maxChars); }
        wstring         recv_wstr(int maxChars = 0x7fffffff) noexcept { return recv_gen<wstring>(maxChars); }
        vector<uint8_t> recv_data(int maxCount = 0x7fffffff) noexcept { return recv_gen<vector<uint8_t>>(maxCount); }


        /**
         * Waits up to timeout millis for data from remote end.
         * If no data available or critical failure, an empty vector is returned
         */
        template<class T> NOINLINE T wait_recv(int millis) noexcept
        {
            return try_recv(&socket::recv_gen<T>, millis);
        }
        string          wait_recv_str(int millis)  noexcept { return wait_recv<string>(millis); }
        wstring         wait_recv_wstr(int millis) noexcept { return wait_recv<wstring>(millis); }
        vector<uint8_t> wait_recv_data(int millis) noexcept { return wait_recv<vector<uint8_t>>(millis); }


        /**
         * Waits up to timeoutMillis for available data from remote end.
         * @return TRUE if there's data available, FALSE otherwise.
         */
        bool wait_available(int millis = 1000) noexcept;


        // Sends a request and waits until an answer is returned
        template<class T, class U> T request(const U& request, int millis = 1000) noexcept {
            return send(request) <= 0 ? T{} : wait_recv<T>(millis);
        }
        template<class U> auto request_str(const U& req, int millis = 1000)  noexcept { return request<string>(req, millis); }
        template<class U> auto request_wstr(const U& req, int millis = 1000) noexcept { return request<wstring>(req, millis); }
        template<class U> auto request_data(const U& req, int millis = 1000) noexcept { return request<vector<uint8_t>>(req, millis); }

        /**
         * UDP version of recv_gen
         */
        template<class T> NOINLINE T recvfrom_gen(ipaddress& from, int maxCount = 0x7fffffff) noexcept {
            int count = available() / sizeof(typename T::value_type);
            if (count <= 0) return T{};
            int n = count < maxCount ? count : maxCount;
            T cont;
            cont.resize(n);
            recvfrom(from, (void*)cont.data(), n * sizeof(typename T::value_type));
            return cont;
        }
        string          recvfrom_str (ipaddress& from, int maxChars = 0x7fffffff) noexcept { return recvfrom_gen<string>(from, maxChars); }
        wstring         recvfrom_wstr(ipaddress& from, int maxChars = 0x7fffffff) noexcept { return recvfrom_gen<wstring>(from, maxChars); }
        vector<uint8_t> recvfrom_data(ipaddress& from, int maxCount = 0x7fffffff) noexcept { return recvfrom_gen<vector<uint8_t>>(from, maxCount); }


        /**
         * UDP version of wait_recv. Waits up to timeout millis for data from remote end.
         * If no data available or critical failure, an empty vector is returned
         */
        template<class T> NOINLINE T wait_recvfrom(ipaddress& from, int millis) noexcept
        {
            return try_recvfrom(&socket::recvfrom_gen<T>, from, millis);
        }
        string          wait_recvfrom_str (ipaddress& from, int millis) noexcept { return wait_recvfrom<string>(from,millis); }
        wstring         wait_recvfrom_wstr(ipaddress& from, int millis) noexcept { return wait_recvfrom<wstring>(from,millis); }
        vector<uint8_t> wait_recvfrom_data(ipaddress& from, int millis) noexcept { return wait_recvfrom<vector<uint8_t>>(from,millis); }



        /**
         * Get socket option from the specified optlevel with the specified opt key SO_xxxx
         * @note Reference Socket options: https://msdn.microsoft.com/en-us/library/windows/desktop/ms740525%28v=vs.85%29.aspx
         * @return getsockopt result for the specified socketopt SO_xxxx or < 0 if error occurred (or check errno)
         */
        int get_opt(int optlevel, int socketopt) const noexcept;
        /**
         * Tries to set a socket option in the optlevel for the specified socketopt SO_xxxx
         * @note Reference Socket options: https://msdn.microsoft.com/en-us/library/windows/desktop/ms740525%28v=vs.85%29.aspx
         * @param optlevel Socket option level such as IPPROTO_IP or IPPROTO_TCP.
         * @return 0 on success, error code otherwise (or check errno)
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
         * @return 0 on success, error code otherwise (or check errno)
         */
        int set_ioctl(int iocmd, int value) noexcept;

        /**
         * Configure socket settings: non-blocking I/O,
         * Nagle disabled (TCP_NODELAY=TRUE), only applies to TCP sockets
         */
        void set_noblock_nodelay() noexcept;
        /**
         * Configure socket settings: I/O blocking mode off(0) or on(1)
         * @param socketsBlock (FALSE = NOBLOCK, TRUE = BLOCK)
         * @return TRUE if set succeded, check socket::last_err() for error message
         */
        bool set_blocking(bool socketsBlock) noexcept;
        // @return TRUE if socket is set to blocking: set_blocking(true);, check socket::last_err() for error message
        bool is_blocking() const noexcept;

        /**
         * Configure socket settings: Nagle off (TCP_NODELAY) 
         * or on (Nagle bandwidth opt.)
         * @note This only applies for TCP sockets
         * @param enableNagle (FALSE = TCP_NODELAY, TRUE = Nagle enabled)
         * @return TRUE if set succeeded, check socket::last_err() for error message
         */
        bool set_nagle(bool enableNagle) noexcept;
        // @return TRUE if socket is set to nodelay: set_nagle(false);, check socket::last_err() for error message
        bool is_nodelay() const noexcept;

        // Sets the receive buffer size
        // @return TRUE if set size was successful, check socket::last_err() for error message
        bool set_rcv_buf_size(size_t size) noexcept;
        // @return Receive buffer size
        int get_rcv_buf_size() const noexcept;

        // Sets the send buffer size
        // @return TRUE if set size was successful, check socket::last_err() for error message
        bool set_snd_buf_size(size_t size) noexcept;
        // @return Send buffer size
        int get_snd_buf_size() const noexcept;

        ////////////////////////////////////////////////////////////////////////////

        // @return The SocketType of the socket
        socket_type type() const noexcept;
        // @return The AddressFamily of the socket
        address_family family() const noexcept;
        // @return The IP Protocol of the socket
        ip_protocol ipproto() const noexcept;
        // @return ProtocolInfo of the socket
        protocol_info protocol() const noexcept;
        /**
         * Checks if the socket is still valid and connected. If connection has
         * reset or remote end has closed the connection, the socket will be closed
         * @return TRUE if the socket is still valid and connected
         */
        bool connected() noexcept;

        ////////////////////////////////////////////////////////////////////////////

        // Creates a new socket without binding or connecting anything
        bool create(address_family af = AF_IPv4, ip_protocol ipp = IPP_TCP, socket_option opt = SO_None) noexcept;
        
        // Binds this socket to some address:port -- as a TCP listener OR a general UDP socket
        // If you bind an UDP socket to a remote address, you can use send() to send datagrams without specifying remote address
        bool bind(const ipaddress& address) noexcept;

        // Start listening for new clients
        bool listen() noexcept;


        enum SelectFlag
        {
            SF_Read   = (1<<0), // select if characters available for reading, i.e. read() won't block
            SF_Write  = (1<<1), // select if a send() will not block or a connection was established
            SF_Except = (1<<2), // select for any exceptional IO conditions, such as MSG_OOB
            SF_ReadWrite = SF_Read|SF_Write, //
        };

        // Tries to select this socket for conditions
        // Select suspends the thread until this Socket file descriptor is signaled
        // Return false on timeout or error. (check socket::last_err())
        bool select(int timeoutMillis, SelectFlag selectFlags = SF_ReadWrite) noexcept;

        ////////////////////////////////////////////////////////////////////////////

        /**
         * Creates a new listener type socket used for accepting new connections
         * @return TRUE if the socket is successfully listening to addr
         */
        bool listen(const ipaddress& localAddr, ip_protocol ipp = IPP_TCP, socket_option opt = SO_None) noexcept;
        /**
         * Creates a new listener type socket used for accepting new connections
         * @return TRUE if the socket is successfully listening to addr
         */
        bool listen(int localPort, address_family af = AF_IPv4, ip_protocol ipp = IPP_TCP, socket_option opt = SO_None) noexcept;
        /**
         * STATIC
         * Creates a new listener type socket used for accepting new connections
         */
        static socket listen_to(const ipaddress& localAddr, ip_protocol ipp = IPP_TCP, socket_option opt = SO_None) noexcept;
        /**
         * STATIC
         * Creates a new listener type socket used for accepting new connections
         */
        static socket listen_to(int localPort, address_family af = AF_IPv4, ip_protocol ipp = IPP_TCP, socket_option opt = SO_None) noexcept;


        /**
         * STATIC
         * Creates a new generic socket for sending and receiving UDP datagrams
         */
        static socket make_udp(const ipaddress& localAddr, socket_option opt = SO_None) noexcept {
            return listen_to(localAddr, IPP_UDP, opt);
        }
        /**
         * STATIC
         * Creates a new generic socket for sending and receiving UDP datagrams
         */
        static socket make_udp(int localPort, address_family af = AF_IPv4, socket_option opt = SO_None) noexcept {
            return listen_to(localPort, af, IPP_UDP, opt);
        }


        /**
         * Try accepting a new connection from THIS listening socket. Accepted socket set to noblock nodelay.
         * If there are no pending connections this function returns invalid socket
         * To wait for a new connection in a blocking way use accept(timeoutMillis)
         * To wait for a new connection asynchronously use AcceptAsync()
         * @return Invalid socket if there are no pending connections, otherwise a valid socket handle
         */
        socket accept() const noexcept;
        /**
         * Blocks until a new connection arrives or the specified timeout is reached.
         * To block forever, set timeoutMillis to -1
         * This function will fail automatically if the socket is closed
         */
        socket accept(int millis) const noexcept;


        /**
         * Connects to a remote socket and sets the socket as nonblocking and tcp nodelay
         * @param remoteAddr Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
         * @return TRUE: valid socket, false: error (check socket::last_err())
         */
        bool connect(const ipaddress& remoteAddr, socket_option opt) noexcept;
        /**
         * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
         * @param af Address family to use, most commonly AF_IPv4 or AF_IPv6
         * @return TRUE: valid socket, false: error (check socket::last_err())
         */
        bool connect(const char* hostname, int port, address_family af = AF_IPv4, socket_option opt = SO_None) noexcept;
        /**
         * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
         * @param remoteAddr Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
         * @param millis Timeout value if the server takes too long to respond
         * @return TRUE: valid socket, false: error (check socket::last_err())
         */
        bool connect(const ipaddress& remoteAddr, int millis, socket_option opt = SO_None) noexcept;
        /**
         * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
         * @param af Address family to use, most commonly AF_IPv4 or AF_IPv6
         * @param millis Timeout value if the server takes too long to respond
         * @return TRUE: valid socket, false: error (check socket::last_err())
         */
        bool connect(const char* hostname, int port, int millis, address_family af = AF_IPv4, socket_option opt = SO_None) noexcept;


        /**
         * Connects to a remote socket and sets the socket as nonblocking and tcp nodelay
         * @param remoteAddr Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
         * @return If successful, an initialized Socket object
         */
        static socket connect_to(const ipaddress& remoteAddr, socket_option opt = SO_None) noexcept;
        /**
         * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
         * @param af Address family to use, most commonly AF_IPv4 or AF_IPv6
         * @return If successful, an initialized Socket object
         */
        static socket connect_to(const char* hostname, int port, address_family af = AF_IPv4, socket_option opt = SO_None) noexcept;
        /**
         * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
         * @param remoteAddr Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
         * @param millis Timeout value if the server takes too long to respond
         * @return If successful, an initialized Socket object
         */
        static socket connect_to(const ipaddress& remoteAddr, int millis, socket_option opt = SO_None) noexcept;
        /**
         * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
         * @param af Address family to use, most commonly AF_IPv4 or AF_IPv6
         * @param millis Timeout value if the server takes too long to respond
         * @return If successful, an initialized Socket object
         */
        static socket connect_to(const char* hostname, int port, int millis, address_family af = AF_IPv4, socket_option opt = SO_None) noexcept;


        /**
         * Starts an Async IO operation to accept a new connection until a connection
         * arrives or the specified timeout is reached
         * To pend asynchronously forever, set timeoutMillis to -1
         * The callback(socket s) receives accepted Socket object
         */
        template<class Func> void accept_async(Func&& func, int millis = -1) noexcept
        {
            struct async {
                socket* listener;
                Func callback;
                int millis;
                static void accept(async* arg) {
                    socket* listener = arg->listener;
                    auto    callback = arg->callback;
                    int     timeout  = arg->millis;
                    delete arg; // delete before callback (in case of exception)
                    callback(std::move(listener->accept(timeout)));
                }
            };
            if (auto* arg = new async{ this, std::forward<Func>(func), millis })
                spawn_thread((void(*)(void*))&async::accept, arg);
        }
        /**
         * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
         * @param remoteAddr Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
         * @param func The callback(socket s) receives accepted Socket object
         * @param millis Timeout value if the server takes too long to respond
         */
        template<class Func> void connect_async(const ipaddress& remoteAddr, Func&& func, int millis, socket_option opt = SO_None) noexcept
        {
            struct async {
                ipaddress remoteAddr;
                Func callback;
                int millis;
                socket_option opt;
                static void connect(async* arg) {
                    async a { std::move(*arg) };
                    delete arg; // delete before callback (in case of an exception)
                    a.callback(std::move(socket::connect_to(a.remoteAddr, a.millis, a.opt)));
                }
            };
            close();
            Addr = remoteAddr;
            if (auto* arg = new async{ remoteAddr, std::forward<Func>(func), millis, opt })
                spawn_thread((void(*)(void*))&async::connect, arg);
        }
    };

    ////////////////////////////////////////////////////////////////////////////////

    /**
     * Makes an UDP socket with a random port
     */
    RPPAPI socket make_udp_randomport(socket_option opt = SO_None);

    /**
     * Creates a loopback (127.0.0.1) TCP Listener with a random port number
     */
    RPPAPI socket make_tcp_randomport(socket_option opt = SO_None);

    ////////////////////////////////////////////////////////////////////////////////

} // namespace rpp
