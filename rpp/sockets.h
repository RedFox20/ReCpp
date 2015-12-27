#pragma once
#include <string>   // std::string, std::wstring
#include <vector>   // std::vector
#include <ostream>  // ostream& operator<<
#include <string.h> // strlen

namespace rpp
{
	using namespace std;

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


	/** @return UNIX af => AddressFamily conversion */
	address_family to_addrfamily(int af);
	/** @return UNIX sock => SocketType conversion */
	socket_type to_socktype(int sock);
	/** @return UNIX ipproto => AddressFamily conversion */
	ip_protocol to_ipproto(int ipproto);
	/** @return AddressFamily => UNIX af conversion */
	int addrfamily_int(address_family addressFamily);
	/** @return SocketType => UNIX sock conversion */
	int socktype_int(socket_type sockType);
	/** @return AddressFamily => UNIX ipproto conversion */
	int ipproto_int(ip_protocol ipProtocol);


	struct protocol_info
	{
		int            ProtoVersion; // protocol version identifier
		address_family Family;
		socket_type    SockType;
		ip_protocol    Protocol;

		int family_int() const { return addrfamily_int(Family); }
		int type_int()   const { return socktype_int(SockType); }
		int proto_int()  const { return ipproto_int(Protocol); }
	};


	/** @brief sleeps for specified milliseconds duration */
	void thread_sleep(int milliseconds);

	/** @brief spawns a new thread, thread handles are automatically closed */
	void spawn_thread(void(*thread_func)(void* arg), void* arg);

	/** @brief measure highest accuracy time in seconds for both Windows and Linux */
	double timer_time();


	struct ipaddress
	{
		address_family Family; // IPv4 or IPv6
		unsigned short Port;       // port in host byte order
		union {
			struct {
				unsigned long Addr4; // IPv4 Address
			};
			struct {
				unsigned char Addr6[16]; // IPv6 Address
				unsigned long FlowInfo;
				unsigned long ScopeId;
			};
		};

		ipaddress(address_family af);
		ipaddress(address_family af, int port);
		ipaddress(address_family af, const char* hostname, int port);
		ipaddress(int socket);

		// @brief Fills buffer dst[maxCount] with addr string
		int name(char* dst, int maxCount) const;
		string name() const;
		ostream& operator<<(ostream& os) const;

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

	// allow os << ipaddress();
	inline ostream& operator<<(ostream& os, const ipaddress& addr) {
		return addr.operator<<(os);
	}


	/**
	 * A lightweight C++ Socket object
	 */
	class socket
	{
	protected:
		static const int INVALID = -1;
		int       Sock; // Socket handle
		ipaddress Addr; // remote addr

		/** @brief Create a socket with an initialized handle */
		socket(int handle, const ipaddress& addr);

	public:
		/** @brief Creates a default socket object */
		socket();
		/** @brief Creates a listener socket */
		explicit socket(int port, address_family af = AF_IPv4);
		/** @brief Creates a connection to the specified remote address */
		socket(const ipaddress& addr);
		/** @brief Tries to connect to the specified remote address with timeout */
		socket(const ipaddress& addr, int millis);
		/** @brief Connects to hostname:port */
		socket(const char* hostname, int port, address_family af = AF_IPv4);
		/** @brief Connects to hostname:port with timeout */
		socket(const char* hostname, int port, int millis, address_family af = AF_IPv4);
		~socket();

		/** @brief Closes the connection (if any) and returns this socket to a default state */
		void close();

		operator bool() const { return Sock != INVALID; }
		/** @return TRUE if Sock handle is currently VALID */
		bool good() const { return Sock != INVALID; }
		/** @return TRUE if Sock handle is currently INVALID */
		bool bad()  const { return Sock == INVALID; }
		/** @return OS socket handle. We are generous. */
		int oshandle() const { return Sock; }

		socket(const socket&)            = delete; // no copy construct
		socket& operator=(const socket&) = delete; // no copy assign
		socket(socket&& fwd);                      // move construct allowed
		socket& operator=(socket&& fwd);           // move assign allowed

		/** @return Current ipaddress */
		const ipaddress& address() const { return Addr; }
		/** @return String representation of ipaddress */
		string name() const { return Addr.name(); }

		/**
		 * @return A human readable description string of the last occurred socket error in errno.
		 */
		static string last_err();

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
		template<class T> int send(const basic_string<T>& str) { return send(str.data(), int(sizeof(T) * str.length())); }

		/** @brief Forces the socket object to flush any data in its buffers */
		void flush();

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
		 * Peek bytes from remote socket, return number of bytes peeked
		 * Automatically closes socket during critical failure and returns -1
		 * If there is no data to peek, this function returns 0
		 */
		int peek(void* buffer, int numBytes);

		/**
		 * Skips a number of bytes from the read stream
		 * This is a controlled flush of the OS socket read buffer
		 */
		void skip(int n);

	private: 
		int handle_recv(int ret);

		template<class Action> static void try_for_period(int millis, Action action) {
			const double timeout = millis / 1000.0; // millis to seconds
			const double start = timer_time();
			do {
				if (action())
					break;
				thread_sleep(1); // suspend until next timeslice
			} while (timeout > 0.0 && (timer_time() - start) < timeout);
		}
		template<class Ret> Ret try_recv(Ret (socket::*recv)(int max), int millis) {
			Ret res;
			try_for_period(millis, [&]() {
				return available() != 0 ? (res = (this->*recv)(0x7fffffff)),true : false;
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
		template<class T> T recv_gen(int maxCount = 0x7fffffff) {
			int count = available() / sizeof(typename T::value_type);
			if (count <= 0) return T();
			int n = count < maxCount ? count : maxCount;
			T cont;
			cont.resize(n);
			recv((void*)cont.data(), n * sizeof(typename T::value_type));
			return cont;
		}
		string          recv_str(int maxChars = 0x7fffffff)  { return recv_gen<string>(maxChars); }
		wstring         recv_wstr(int maxChars = 0x7fffffff) { return recv_gen<wstring>(maxChars); }
		vector<uint8_t> recv_data(int maxCount = 0x7fffffff) { return recv_gen<vector<uint8_t>>(maxCount); }

		
		/**
		 * Waits up to timeout millis for data from remote end.
		 * If no data available or critical failure, an empty vector is returned
		 */
		template<class T> T wait_recv(int millis)
		{
			return try_recv(&socket::recv_gen<T>, millis);
		}
		string          wait_recv_str(int millis)  { return wait_recv<string>(millis); }
		wstring         wait_recv_wstr(int millis) { return wait_recv<wstring>(millis); }
		vector<uint8_t> wait_recv_data(int millis) { return wait_recv<vector<uint8_t>>(millis); }


		/**
		 * Waits up to timeoutMillis for available data from remote end.
		 * @return TRUE if there's data available, FALSE otherwise.
		 */
		bool wait_available(int millis = 1000);


		/** Sends a request and waits until an answer is returned */
		template<class T, class U> T request(const U& request, int millis = 1000) {
			return send(request) <= 0 ? T() : wait_recv<T>(millis);
		}
		template<class U> string          request_str(const U& req, int millis = 1000)  { return request<string>(req, millis); }
		template<class U> wstring         request_wstr(const U& req, int millis = 1000) { return request<wstring>(req, millis); }
		template<class U> vector<uint8_t> request_data(const U& req, int millis = 1000) { return request<vector<uint8_t>>(req, millis); }


		/**
		 * Get socket option from the specified optlevel with the specified opt key SO_xxxx
		 * @note Reference Socket options: https://msdn.microsoft.com/en-us/library/windows/desktop/ms740525%28v=vs.85%29.aspx
		 * @return getsockopt result for the specified socketopt SO_xxxx or < 0 if error occurred (or check errno)
		 */
		int get_opt(int optlevel, int socketopt) const;
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
		int get_ioctl(int iocmd, int& outValue) const;
		/**
		 * Control IO handle with the specified IO command
		 * @note This function designed for iocmd-s that SET values
		 * @note Reference ioctlsocket: https://msdn.microsoft.com/en-us/library/windows/desktop/ms738573%28v=vs.85%29.aspx
		 * @return 0 on success, error code otherwise (or check errno)
		 */
		int set_ioctl(int iocmd, int value);


		/** @return The SocketType of the socket */
		socket_type type() const;
		/** @return The AddressFamily of the socket */
		address_family family() const;
		/** @return The IP Protocol of the socket */
		ip_protocol ipproto() const;
		/** @return ProtocolInfo of the socket */
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
		 * @param socketsBlock (FALSE = NOBLOCK, TRUE = BLOCK)
		 */
		void set_blocking(bool socketsBlock = false);
		/**
		 * Configure socket settings: Nagle off (TCP_NODELAY) or on (Nagle bandwidth opt.)
		 * @param enableNagle (FALSE = TCP_NODELAY, TRUE = Nagle enabled)
		 */
		void set_nodelay(bool enableNagle = false);
		/** @return TRUE if socket is set to blocking: set_blocking(true); */
		bool is_blocking() const;
		/** @return TRUE if socket is set to nodelay: set_nodelay(true); */
		bool is_nodelay() const;


		/**
		 * Creates a new listener type socket used for accepting new connections
		 */
		void listen(const ipaddress& addr);
		/**
		 * Creates a new listener type socket used for accepting new connections
		 */
		void listen(int port, address_family af = AF_IPv4);
		/**
		 * STATIC
		 * Creates a new listener type socket used for accepting new connections
		 */
		static socket listen_to(const ipaddress& addr);
		/**
		 * STATIC
		 * Creates a new listener type socket used for accepting new connections
		 */
		static socket listen_to(int port, address_family af = AF_IPv4);


		/**
		 * Try accepting a new connection from THIS listening socket. Accepted socket set to noblock nodelay.
		 * If there are no pending connections this function returns invalid socket
		 * To wait for a new connection in a blocking way use accept(timeoutMillis)
		 * To wait for a new connection asynchronously use AcceptAsync()
		 * @return Invalid socket if there are no pending connections, otherwise a valid socket handle
		 */
		socket accept() const;
		/**
		 * Blocks until a new connection arrives or the specified timeout is reached.
		 * To block forever, set timeoutMillis to -1
		 * This function will fail automatically if the socket is closed
		 */
		socket accept(int millis) const;


		/**
		 * Connects to a remote socket and sets the socket as nonblocking and tcp nodelay
		 * @param address Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
		 * @return TRUE: valid socket, false: error (check errno)
		 */
		bool connect(const ipaddress& addr);
		/**
		 * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
		 * @param af Address family to use, most commonly AF_IPv4 or AF_IPv6
		 * @return TRUE: valid socket, false: error (check errno)
		 */
		bool connect(const char* hostname, int port, address_family af = AF_IPv4);
		/**
		 * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
		 * @param address Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
		 * @param millis Timeout value if the server takes too long to respond
		 * @return TRUE: valid socket, false: error (check errno)
		 */
		bool connect(const ipaddress& addr, int millis);
		/**
		 * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
		 * @param af Address family to use, most commonly AF_IPv4 or AF_IPv6
		 * @param millis Timeout value if the server takes too long to respond
		 * @return TRUE: valid socket, false: error (check errno)
		 */
		bool connect(const char* hostname, int port, int millis, address_family af = AF_IPv4);


		/**
		 * Connects to a remote socket and sets the socket as nonblocking and tcp nodelay
		 * @param address Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
		 * @return If successful, an initialized Socket object
		 */
		static socket connect_to(const ipaddress& addr);
		/**
		 * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
		 * @param af Address family to use, most commonly AF_IPv4 or AF_IPv6
		 * @return If successful, an initialized Socket object
		 */
		static socket connect_to(const char* hostname, int port, address_family af = AF_IPv4);
		/**
		 * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
		 * @param address Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
		 * @param millis Timeout value if the server takes too long to respond
		 * @return If successful, an initialized Socket object
		 */
		static socket connect_to(const ipaddress& addr, int millis);
		/**
		 * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
		 * @param af Address family to use, most commonly AF_IPv4 or AF_IPv6
		 * @param millis Timeout value if the server takes too long to respond
		 * @return If successful, an initialized Socket object
		 */
		static socket connect_to(const char* hostname, int port, int millis, address_family af = AF_IPv4);


		/**
		 * Starts an Async IO operation to accept a new connection until a connection
		 * arrives or the specified timeout is reached
		 * To pend asynchronously forever, set timeoutMillis to -1
		 * The callback(socket s) receives accepted Socket object
		 */
		template<class Func> void accept_async(Func func, int millis = -1)
		{
			struct async {
				socket& listener;
				Func callback;
				int millis;
				static void accept(async* arg) {
					socket& listener = arg->listener;
					auto    callback = arg->callback;
					int     timeout  = arg->millis;
					delete arg; // delete before callback (in case of exception)
					callback(move(listener.accept(timeout)));
				}
			};
			if (auto* arg = new async{ *this, func, millis })
				spawn_thread((void(*)(void*))&async::accept, arg);
		}
		/**
		 * Connects to a remote socket and sets the socket as nonblocking and TCP nodelay
		 * @param addr Initialized SockAddr4 (IPv4) or SockAddr6 (IPv6) network address
		 * @param func The callback(socket s) receives accepted Socket object
		 * @param millis Timeout value if the server takes too long to respond
		 */
		template<class Func> void connect_async(const ipaddress& addr, Func func, int millis)
		{
			struct async {
				ipaddress addr;
				Func callback;
				int millis;
				static void connect(async* arg) {
					ipaddress addr = arg->addr;
					auto  callback = arg->callback;
					int   timeout  = arg->millis;
					delete arg; // delete before callback (in case of exception)
					callback(move(socket::connect_to(addr, timeout)));
				}
			};
			close();
			Addr = addr;
			if (auto* arg = new async{ addr, func, millis })
				spawn_thread((void(*)(void*))&async::connect, arg);
		}
	};

} // namespace rpp