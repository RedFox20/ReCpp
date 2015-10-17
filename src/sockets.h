#pragma once
#include <string> // std::string

enum address_family
{
	AF_DontCare,    // unspecified AddressFamily, service provider will choose most appropriate
	AF_IPv4,        // The Internet Protocol version 4 (IPv4) address family
	AF_IPv6,        // The Internet Protocol version 6 (IPv6) address family
	AF_Bth,         // Bluetooth address family, supported since WinXP SP2
};

enum socket_type
{
	ST_Unspecified, // unspecified socket type (invalid socket)
	ST_Stream,      // TCP only, byte stream for IPv4 or IPv6 protocols
	ST_Datagram,    // UDP only, byte datagrams for IPv4 or IPv6 protocols
	ST_Raw,         // application provides IPv4 or IPv6 headers
	ST_RDM,         // reliable message datagram for PGM
	ST_SeqPacket,   // TCP based, similar to ST_Stream but slower, however packet boundaries are respected "TCP datagrams"
};

enum ip_protocol
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

struct protocol_info
{
	int            ProtoVersion; // protocol version identifier
	address_family AddrFamily;
	socket_type    SockType;
	ip_protocol    Protocol;
};

enum async_result
{
	AR_Failed,
	AR_Success,
};

/**
 * A generic Async callback function for socket IO routines
 * @param socket The socket relevant
 * @param result Describes the result of the Async operation, such as AR_Failed or AR_Success
 */
typedef void (*Socket_AsyncCallback)(int socket, async_result result);


/**
 * Closes the specified socket. The socket is passed as a pointer
 * because Socket_Close will set it to -1
 */
void Socket_Close(int& socket);



/**
 * @return A human readable description string of the last occurred socket error in errno.
 */
std::string Socket_LastErrStr();



/**
 * Send data to remote socket, return number of bytes sent or -1 if socket closed
 * Automatically closes socket during critical failure
 */
int Socket_Send(int& socket, const void* buffer, int numBytes);
inline int Socket_SendStr(int& socket, const char* str)
{
	return Socket_Send(socket, str, strlen(str));
}

/**
 * Peeks the socket for currently available bytes to read
 * Automatically closes socket during critical failure and returns -1
 * If there is no data in recv buffer, this function returns 0
 */
int Socket_Available(int& socket);

/**
 * Recv data from remote socket, return number of bytes received
 * Automatically closes socket during critical failure and returns -1
 * If there is no data to receive, this function returns 0
 */
int Socket_Recv(int& socket, void* buffer, int maxBytes);

/**
 * Peeks the recv buffer with Socket_Available and then builds
 * and std::string of the available data.
 * If no data available or critical failure, then an empty string is returned
 */
std::string Socket_RecvStr(int& socket);

/**
 * Peeks the recv buffer with Socket_Available and then builds
 * and std::wstring of the available data.
 * If no data available or critical failure, then an empty string is returned
 */
std::wstring Socket_RecvWStr(int& socket);



/**
 * Get socket option from the specified optlevel with the specified opt key SO_xxxx
 * @note Reference Socket options: https://msdn.microsoft.com/en-us/library/windows/desktop/ms740525%28v=vs.85%29.aspx
 * @return getsockopt result for the specified socketopt SO_xxxx or < 0 if error occurred (or check errno)
 */
int Socket_Getopt(int socket, int optlevel, int socketopt);
/**
 * Tries to set a socket option in the optlevel for the specified socketopt SO_xxxx
 * @note Reference Socket options: https://msdn.microsoft.com/en-us/library/windows/desktop/ms740525%28v=vs.85%29.aspx
 * @param optlevel Socket option level such as IPPROTO_IP or IPPROTO_TCP. 
 * @return 0 on success, error code otherwise (or check errno)
 */
int Socket_Setopt(int socket, int optlevel, int socketopt, int value);
/**
 * Control IO handle with the specified IO command
 * @note This function designed for iocmd-s that SET values
 * @note Reference ioctlsocket: https://msdn.microsoft.com/en-us/library/windows/desktop/ms738573%28v=vs.85%29.aspx
 * @return 0 on success, error code otherwise (or check errno)
 */
int Socket_SetIoctl(int socket, int iocmd, int value);
/**
 * Get values from IO handle with the specified IO command
 * @note This function designed for iocmd-s that GET values
 * @note Reference ioctlsocket: https://msdn.microsoft.com/en-us/library/windows/desktop/ms738573%28v=vs.85%29.aspx
 * @return 0 on success, error code otherwise (or check errno)
 */
int Socket_GetIoctl(int socket, int iocmd, int& outValue);



/** @return UNIX af => AddressFamily conversion */
address_family Socket_IntToAF(int af);
/** @return UNIX sock => SocketType conversion */
socket_type Socket_IntToST(int sock);
/** @return UNIX ipproto => AddressFamily conversion */
ip_protocol Socket_IntToIPP(int ipproto);

/** @return AddressFamily => UNIX af conversion */
int Socket_AFToInt(address_family addressFamily);
/** @return SocketType => UNIX sock conversion */
int Socket_STToInt(socket_type sockType);
/** @return AddressFamily => UNIX ipproto conversion */
int Socket_IPPToInt(ip_protocol ipProtocol);


/**
 * @return The SocketType of the socket
 */
socket_type Socket_SocketType(int socket);
/**
 * @return ProtocolInfo of the socket
 */
protocol_info Socket_ProtocolInfo(int socket);
/**
 * Checks if the socket is still valid and connected. If connection has
 * reset or remote end has closed the connection, the socket will be closed
 * @return TRUE if the socket is still valid and connected
 */
bool Socket_IsConnected(int& socket);





/**
 * Configure socket settings: non-blocking I/O, Nagle disabled (TCP_NODELAY=TRUE)
 */
void Socket_NoBlockNoDelay(int socket);
/**
 * Configure socket settings: I/O blocking mode off(0) or on(1)
 * @param socketsBlock (0 = NOBLOCK, 1 = BLOCK)
 */
void Socket_SetBlocking(int socket, int socketsBlock = 0);
/**
 * Configure socket settings: Nagle off (TCP_NODELAY) or on (Nagle bandwidth opt.)
 * @param enableNagle (0 = TCP_NODELAY, 1 = Nagle enabled)
 */
void Socket_SetNagle(int socket, int enableNagle = 0);




struct ipaddress
{
	address_family AddrFamily; // IPv4 or IPv6
	unsigned short Port; // port in host byte order
	union {
		struct {
			unsigned long Addr4; // IPv4 Address
		};
		struct {
			unsigned char Addr6[16];        // IPv6 Address
			unsigned long FlowInfo;
			unsigned long ScopeId;
		};
	};

	ipaddress(address_family af);
	ipaddress(address_family af, int port);
	ipaddress(address_family af, const char* hostname, int port);

	std::string name() const;
	void clear();
	int port() const;
};
struct ipaddress4 : public ipaddress
{
	ipaddress4() : ipaddress(AF_IPv4) {}
	ipaddress4(int port) : ipaddress(AF_IPv4, port) {}
	ipaddress4(const char* hostname, int port) : ipaddress(AF_IPv4, hostname, port) {}
};
struct ipaddress6 : public ipaddress
{
	ipaddress6() : ipaddress(AF_IPv6) {}
	ipaddress6(int port) : ipaddress(AF_IPv6, port) {}
	ipaddress6(const char* hostname, int port) : ipaddress(AF_IPv6, hostname, port) {}
};


/**
 * @return Address associated with this socket
 */
ipaddress Socket_GetAddr(int socket);



/**
 * Creates a new listener type socket used for accepting new connections
 */
int Socket_ListenTo(const ipaddress& address);
inline int Socket_ListenTo(int port, address_family af = AF_IPv4)
{
	return Socket_ListenTo(ipaddress(af, port));
}


/**
 * Try accepting a new connection from a listening socket. Accepted socket set to noblock nodelay
 * If there are no pending connections this function returns -1
 * To wait for a new connection in a blocking way use Socket_AcceptWait()
 * To wait for a new connection asynchronously use Socket_AcceptAsync()
 * @return -1 if there are no pending connections, otherwise a valid socket handle
 */
int Socket_Accept(int listener);
/**
 * Blocks until a new connection arrives or the specified timeout is reached.
 * To block forever, set timeoutMillis to -1
 * This function will fail automatically if the socket is closed
 */
int Socket_Accept(int listener, int timeoutMillis);
/**
 * Starts an Async IO operation to accept a new connection until a connection 
 * arrives or the specified timeout is reached
 * To pend asynchronously forever, set timeoutMillis to -1
 * The callback receives accepted socket with AR_Success or -1 for AR_Failed
 */
void Socket_AcceptAsync(int listener, Socket_AsyncCallback callback, int timeoutMillis = -1);


/**
 * Connects to a remote socket and sets the socket as nonblocking and tcp nodelay
 * @param address Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
 * @return >= 0: valid socket, -1: error (check errno)
 */
int Socket_ConnectTo(const ipaddress& address);

/**
 * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
 * @param af Address family to use, most commonly AF_IPv4 or AF_IPv6
 * @return >= 0: valid socket, -1: error (check errno)
 */
inline int Socket_ConnectTo(const char* hostname, int port, address_family af = AF_IPv4)
{
	return Socket_ConnectTo(ipaddress(af, hostname, port));
}

/**
 * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
 * @param address Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
 * @param timeoutMillis Timeout value if the server takes too long to respond
 * @return >= 0: valid socket, -1: error (check errno)
 */
int Socket_ConnectTo(const ipaddress& address, int timeoutMillis);

/**
 * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
 * @param af Address family to use, most commonly AF_IPv4 or AF_IPv6
 * @param timeoutMillis Timeout value if the server takes too long to respond
 * @return >= 0: valid socket, -1: error (check errno)
 */
inline int Socket_ConnectTo(const char* hostname, int port,
                     int timeoutMillis, address_family af = AF_IPv4)
{
	return Socket_ConnectTo(ipaddress(af, hostname, port), timeoutMillis);
}

/**
 * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
 * @param address Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
 * @param callback The callback receives accepted socket with AR_Success or -1 for AR_Failed
 * @param timeoutMillis Timeout value if the server takes too long to respond
 */
void Socket_ConnectToAsync(const ipaddress& address, 
                           Socket_AsyncCallback callback, int timeoutMillis);




namespace rpp
{
	/**
	 * A lightweight C++ Socket object
	 */
	class socket
	{
		int       Sock; // Socket handle
		ipaddress Addr; // remote addr

		socket(int handle, const ipaddress& addr);

	public:
		/**
		 * @brief Creates a default socket object
		 */
		socket();
		/**
		 * @brief Creates a listener socket
		 */
		explicit socket(int port, address_family af = AF_IPv4);
		/**
		 * @brief Creates a connection to the specified remote address
		 */
		socket(const ipaddress& address);
		/**
		 * @brief Tries to connect to the specified remote address with timeout
		 */
		socket(const ipaddress& address, int timeoutMillis);
		/**
		 * @brief Connects to hostname:port
		 */
		socket(const char* hostname, int port, address_family af = AF_IPv4);
		/**
		 * @brief Connects to hostname:port with timeout
		 */
		socket(const char* hostname, int port, int timeoutMillis, address_family af = AF_IPv4);

		~socket();

		/**
		 * @brief Closes the connection (if any) and returns this socket to a default state
		 */
		void close();

		inline operator bool() const { return Sock != -1; }
		inline bool good() const { return Sock != -1; }
		inline bool bad()  const { return Sock == -1; }

		socket(const socket&) = delete; // no copy construct
		socket& operator=(const socket&) = delete; // no copy

		socket(socket&& fwd);
		socket& operator=(socket&& fwd);


		/**
		 * @return Current ipaddress
		 */
		inline const ipaddress& address() const { return Addr; }
		/**
		 * @return String representation of ipaddress
		 */
		inline std::string name() const { return Addr.name(); }


		/**
		 * @return A human readable description string of the last occurred socket error in errno.
		 */
		static std::string last_err();


		/**
		 * Send data to remote socket, return number of bytes sent or -1 if socket closed
		 * Automatically closes socket during critical failure
		 */
		int send(const void* buffer, int numBytes);
		/** Send a null delimited C string */
		int send(const char* str);
		/** Send a null delimited C string */
		int send(const wchar_t* str);
		/** Send a C++ string */
		template<class T> inline int send(const std::basic_string<T>& str)
		{
			return Socket_Send(Sock, str.data(), sizeof(T)*str.length());
		}


		/**
		 * Peeks the socket for currently available bytes to read
		 * Automatically closes socket during critical failure and returns -1
		 * If there is no data in recv buffer, this function returns 0
		 */
		int available();


		/**
		 * Recv data from remote socket, return number of bytes received
		 * Automatically closes socket during critical failure and returns -1
		 * If there is no data to receive, this function returns 0
		 */
		int recv(void* buffer, int maxBytes);
		/**
		 * Peeks the recv buffer with Socket_Available and then builds
		 * and std::string of the available data.
		 * If no data available or critical failure, then an empty string is returned
		 */
		std::string recv_str();
		/**
		* Peeks the recv buffer with Socket_Available and then builds
		* and std::wstring of the available data.
		* If no data available or critical failure, then an empty string is returned
		*/
		std::wstring recv_wstr();


		/**
		 * Get socket option from the specified optlevel with the specified opt key SO_xxxx
		 * @note Reference Socket options: https://msdn.microsoft.com/en-us/library/windows/desktop/ms740525%28v=vs.85%29.aspx
		 * @return getsockopt result for the specified socketopt SO_xxxx or < 0 if error occurred (or check errno)
		 */
		int get_opt(int optlevel, int socketopt);
		/**
		 * Tries to set a socket option in the optlevel for the specified socketopt SO_xxxx
		 * @note Reference Socket options: https://msdn.microsoft.com/en-us/library/windows/desktop/ms740525%28v=vs.85%29.aspx
		 * @param optlevel Socket option level such as IPPROTO_IP or IPPROTO_TCP.
		 * @return 0 on success, error code otherwise (or check errno)
		 */
		int set_opt(int optlevel, int socketopt, int value);
		/**
		 * Get values from IO handle with the specified IO command
		 * @note This function designed for iocmd-s that GET values
		 * @note Reference ioctlsocket: https://msdn.microsoft.com/en-us/library/windows/desktop/ms738573%28v=vs.85%29.aspx
		 * @return 0 on success, error code otherwise (or check errno)
		 */
		int get_ioctl(int iocmd, int& outValue);
		/**
		 * Control IO handle with the specified IO command
		 * @note This function designed for iocmd-s that SET values
		 * @note Reference ioctlsocket: https://msdn.microsoft.com/en-us/library/windows/desktop/ms738573%28v=vs.85%29.aspx
		 * @return 0 on success, error code otherwise (or check errno)
		 */
		int set_ioctl(int iocmd, int value);


		/**
		 * @return The SocketType of the socket
		 */
		socket_type type() const;
		/**
		 * @return ProtocolInfo of the socket
		 */
		protocol_info protocol() const;
		/**
		 * Checks if the socket is still valid and connected. If connection has
		 * reset or remote end has closed the connection, the socket will be closed
		 * @return TRUE if the socket is still valid and connected
		 */
		bool connected();


		/**
		 * Configure socket settings: non-blocking I/O, Nagle disabled (TCP_NODELAY=TRUE)
		 */
		void set_noblock_nodelay();
		/**
		 * Configure socket settings: I/O blocking mode off(0) or on(1)
		 * @param socketsBlock (0 = NOBLOCK, 1 = BLOCK)
		 */
		void set_blocking(int socketsBlock = 0);
		/**
		 * Configure socket settings: Nagle off (TCP_NODELAY) or on (Nagle bandwidth opt.)
		 * @param enableNagle (0 = TCP_NODELAY, 1 = Nagle enabled)
		 */
		void set_nagle(int enableNagle = 0);




		/**
		 * Creates a new listener type socket used for accepting new connections
		 */
		void listen(const ipaddress& address);
		/**
		 * Creates a new listener type socket used for accepting new connections
		 */
		void listen(int port, address_family af = AF_IPv4);
		/**
		 * Creates a new listener type socket used for accepting new connections
		 */
		static socket listen_to(const ipaddress& address);
		/**
		 * Creates a new listener type socket used for accepting new connections
		 */
		static socket listen_to(int port, address_family af = AF_IPv4);




		/**
		 * Try accepting a new connection from THIS listening socket. Accepted socket set to noblock nodelay.
		 * If there are no pending connections this function returns invalid socket
		 * To wait for a new connection in a blocking way use Accept(timeoutMillis)
		 * To wait for a new connection asynchronously use AcceptAsync()
		 * @return Invalid socket if there are no pending connections, otherwise a valid socket handle
		 */
		socket accept() const;
		/**
		 * Blocks until a new connection arrives or the specified timeout is reached.
		 * To block forever, set timeoutMillis to -1
		 * This function will fail automatically if the socket is closed
		 */
		socket accept(int timeoutMillis) const;




		/**
		 * Connects to a remote socket and sets the socket as nonblocking and tcp nodelay
		 * @param address Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
		 * @return TRUE: valid socket, false: error (check errno)
		 */
		bool connect(const ipaddress& address);
		/**
		 * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
		 * @param af Address family to use, most commonly AF_IPv4 or AF_IPv6
		 * @return TRUE: valid socket, false: error (check errno)
		 */
		bool connect(const char* hostname, int port, address_family af = AF_IPv4);
		/**
		 * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
		 * @param address Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
		 * @param timeoutMillis Timeout value if the server takes too long to respond
		 * @return TRUE: valid socket, false: error (check errno)
		 */
		bool connect(const ipaddress& address, int timeoutMillis);
		/**
		 * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
		 * @param af Address family to use, most commonly AF_IPv4 or AF_IPv6
		 * @param timeoutMillis Timeout value if the server takes too long to respond
		 * @return TRUE: valid socket, false: error (check errno)
		 */
		bool connect(const char* hostname, int port,
			int timeoutMillis, address_family af = AF_IPv4);




		/**
		 * Connects to a remote socket and sets the socket as nonblocking and tcp nodelay
		 * @param address Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
		 * @return If successful, an initialized Socket object
		 */
		static socket connect_to(const ipaddress& address);
		/**
		 * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
		 * @param af Address family to use, most commonly AF_IPv4 or AF_IPv6
		 * @return If successful, an initialized Socket object
		 */
		static socket connect_to(const char* hostname, int port, address_family af = AF_IPv4);
		/**
		 * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
		 * @param address Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
		 * @param timeoutMillis Timeout value if the server takes too long to respond
		 * @return If successful, an initialized Socket object
		 */
		static socket connect_to(const ipaddress& address, int timeoutMillis);
		/**
		 * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
		 * @param af Address family to use, most commonly AF_IPv4 or AF_IPv6
		 * @param timeoutMillis Timeout value if the server takes too long to respond
		 * @return If successful, an initialized Socket object
		 */
		static socket connect_to(const char* hostname, int port,
			int timeoutMillis, address_family af = AF_IPv4);




		/**
		 * Starts an Async IO operation to accept a new connection until a connection
		 * arrives or the specified timeout is reached
		 * To pend asynchronously forever, set timeoutMillis to -1
		 * The callback receives accepted Socket object
		 */
		template<class Func> void accept_async(Func callback, int timeoutMillis = -1)
		{
			struct async
			{
				socket& listener;
				Func callback;
				int timeoutMillis;
				static void accept(accept_async* arg)
				{
					socket& listener = arg->listener;
					auto callback = arg->callback;
					int timeout = arg->timeoutMillis;
					delete arg; // delete before callback (in case of exception)
					callback(listener.accept(timeout));
				}
			};

			auto* arg = new async{ listener, callback, timeoutMillis };
			spawn_thread((void(*)(void*))&async::accept, arg);
		}
		/**
		 * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
		 * @param address Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
		 * @param callback The callback receives accepted Socket object
		 * @param timeoutMillis Timeout value if the server takes too long to respond
		 */
		template<class Func> void connect_async(const ipaddress& address,
			Func callback, int timeoutMillis)
		{
			struct async
			{
				ipaddress addr;
				Func callback;
				int timeoutMillis;
				static void connect(async* arg)
				{
					ipaddress addr = arg->addr;
					auto callback = arg->callback;
					int timeout = arg->timeoutMillis;
					delete arg; // delete before callback (in case of exception)
					callback(socket::connect_to(addr, timeout));
				}
			};

			Socket_Close(Sock);
			Addr = address;

			auto* arg = new async{ address, callback, timeoutMillis };
			spawn_thread((void(*)(void*))&async::connect, arg);
		}
	};

} // namespace rpp