#include "sockets.h"
/**
 * Uses C99 dialect, so compile with -std=gnu99 or -std=c99
 * or via IDE-s Visual Studio 2013 (and higher) / DevC++5.7 (and higher)
 *
 * Visual C/C++:   Create an empty project, compile and run.
 *
 * DevC++ (MinGW): Project Options... -> Parameters -> Linker
 *                                    -> Add Library or Object:  libws2_32.a
 *
 * GCC (linux):    gcc -std=gnu99 -pthread sockets.c -o sockets
 */
#include <stdlib.h>    // malloc
#include <stdio.h>     // printf
#include <string.h>    // memcpy,memset

/**
 * Cross-platform compatibility definitions and helper functions
 */
#ifdef _WIN32 // MSVC and MinGW
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
	#include <WinSock2.h>
	#include <ws2tcpip.h>           // winsock2 and TCP/IP functions
	#include <process.h>            // _beginthreadex
	#ifdef _MSC_VER
		#pragma comment(lib, "Ws2_32.lib") // link against winsock libraries
	#endif
	#define sleep(milliSeconds) Sleep((milliSeconds))
	#define inwin32(...) __VA_ARGS__
#ifdef __MINGW32__
	extern "C" __declspec(dllimport) int WSAAPI inet_pton(int af, const char* src, void* dst);
	extern "C" const char* WSAAPI inet_ntop(int af, void* pAddr, char* pStringBuf, size_t StringBufSize);
#endif
#else // UNIX
	#include <time.h>               // clock_gettime
	#include <unistd.h>             // close()
	#include <pthread.h>            // POSIX threads
	#include <sys/types.h>          // required type definitions
	#include <sys/socket.h>         // LINUX sockets
	#include <netinet/in.h>         // sockaddr_in
	#include <netinet/tcp.h>        // TCP_NODELAY
	#include <errno.h>              // last error number
	#include <sys/ioctl.h>          // ioctl()
	#include <sys/fcntl.h>          // fcntl()
	#include <arpa/inet.h>          // inet_addr, inet_ntoa
	#define sleep(milliSeconds) usleep((milliSeconds) * 1000)
	#define inwin32(...) /*nothing*/ 
	 // map linux socket calls to winsock calls via macros
	#define closesocket(fd) close(fd)
	#define ioctlsocket(fd, request, arg) ioctl(fd, request, arg)
#endif

/////////////////////////////////////////////////////////////////////////////

namespace rpp
{
	using namespace std;

	/////////////////////////////////////////////////////////////////////////////
	// sleeps for specified milliseconds duration
	void thread_sleep(int milliseconds)
	{
		sleep(milliseconds);
	}
	// spawns a new thread, thread handles are automatically closed 
	void spawn_thread(void(*thread_func)(void* arg), void* arg)
	{
	#if _WIN32
		_beginthreadex(0, 0, (unsigned(_stdcall*)(void*))thread_func, arg, 0, 0);
	#else // Linux
		pthread_t threadHandle;
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		pthread_create(&threadHandle, &attr, (void*(*)(void*))thread_func, arg);
		pthread_attr_destroy(&attr);
	#endif
	}
	// measure highest accuracy time in seconds for both Windows and Linux
	double timer_time()
	{
	#if _WIN32
		static double timer_freq = 0.0;
		LARGE_INTEGER t;
		if (timer_freq == 0.0) // initialize high perf timer frequency
		{
			QueryPerformanceFrequency(&t);
			timer_freq = (double)t.QuadPart;
		}
		QueryPerformanceCounter(&t);
		return (double)t.QuadPart / timer_freq;
	#else
		struct timespec tm;
		clock_gettime(CLOCK_REALTIME, &tm);
		return tm.tv_sec + (double)tm.tv_nsec / 1000000000.0;
	#endif
	}
	////////////////////////////////////////////////////////////////////





	/////////////////////////// SOCKETS API ////////////////////////////

#if _WIN32
	static inline void Socket_Initialize()
	{
		static struct wsa { // static var init on first entry to func
			wsa()  { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); }
			~wsa() { WSACleanup(); }
		} wsa_raii_handle; // this RAII handle will be automatically unloaded
	}
#endif

	static inline int Socket_NewSocket(address_family af, socket_type st, ip_protocol ipp)
	{
		inwin32(Socket_Initialize());

		int family = addrfamily_int(af);
		int type = socktype_int(st);
		int proto = ipproto_int(ipp);
		return (int)::socket(family, type, proto);
	}


	//////////////////// ENUM conversions /////////////////////

	address_family to_addrfamily(int af)
	{
		switch (af) { // AF is nonlinear
		default:
		case AF_UNSPEC:   return AF_DontCare;
		case AF_INET:     return AF_IPv4;
		case AF_INET6:    return AF_IPv6;
		case 32/*AF_BTH*/:return AF_Bth;
		}
	}
	socket_type to_socktype(int sock)
	{
		return socket_type(sock < 0 || sock > SOCK_SEQPACKET ? 0 : sock);
	}
	ip_protocol to_ipproto(int ipproto)
	{
		switch (ipproto) { // IPPROTO is nonlinear
		default:                return IPP_DontCare;
		case IPPROTO_ICMP:      return IPP_ICMP;
		case IPPROTO_IGMP:      return IPP_IGMP;
		case 3/*IPPROTO_GGP*/:  return IPP_BTH;
		case IPPROTO_TCP:       return IPP_TCP;
		case IPPROTO_UDP:       return IPP_UDP;
		case IPPROTO_ICMPV6:    return IPP_ICMPV6;
		case 113/*IPPROTO_PGM*/:return IPP_PGM;
		}
	}
	int addrfamily_int(address_family addressFamily)
	{
		static int families[] = { AF_UNSPEC, AF_INET, AF_INET6, 32/*AF_BTH*/ };
		return families[addressFamily];
	}
	int socktype_int(socket_type sockType)
	{
		return (int)sockType; // SOCK is linear
	}
	int ipproto_int(ip_protocol ipProtocol)
	{
		static int protos[] = { 0, IPPROTO_ICMP, IPPROTO_IGMP, 3/*IPPROTO_GGP*/, IPPROTO_TCP, IPPROTO_UDP, IPPROTO_ICMPV6, 113/*IPPROTO_PGM*/ };
		return protos[ipProtocol];
	}


	union saddr {
		sockaddr     sa;
		sockaddr_in  sa4;
		sockaddr_in6 sa6;
		sockaddr_storage sas;
		operator const sockaddr*() const { return &sa; }
		int size() const {
			switch (sa.sa_family) {
				default:       return sizeof sa;
				case AF_INET:  return sizeof sa4;
				case AF_INET6: return sizeof sa6;
			}
		}
	};

	ipaddress::ipaddress(address_family af) 
		: Family(af), Port(0), FlowInfo(0), ScopeId(0)
	{
		if (af == AF_IPv4) Addr4 = INADDR_ANY;
		else memset(Addr6, 0, sizeof Addr6);
	}
	ipaddress::ipaddress(address_family af, int port) 
		: Family(af), Port(ushort(port)), FlowInfo(0), ScopeId(0)
	{
		if (af == AF_IPv4) Addr4 = INADDR_ANY;
		else memset(Addr6, 0, sizeof Addr6);
	}
	ipaddress::ipaddress(address_family af, const char* hostname, int port)
		: Family(af), Port(ushort(port)), FlowInfo(0), ScopeId(0)
	{
		void* addr = af == AF_IPv4 ? (void*)&Addr4 : (void*)&Addr6;
		int family = af == AF_IPv4 ? AF_INET : AF_INET6;
		inet_pton(family, hostname, addr); // text to netaddr lookup
	}
	ipaddress::ipaddress(int socket)
	{
		saddr a;
		socklen_t len = sizeof(a);
		if (getsockname(socket, &a.sa, &len)) {
			Family=AF_IPv4, Port=0, FlowInfo=0, ScopeId=0;
			return; // quiet fail on error/invalid socket
		}

		Family = to_addrfamily(a.sa4.sin_family);
		Port = ntohs(a.sa4.sin_port);
		if (Family == AF_IPv4) {
			Addr4 = a.sa4.sin_addr.s_addr;
			FlowInfo = 0, ScopeId = 0;
		}
		else if (Family == AF_IPv6) { // AF_IPv6
			memcpy(Addr6, &a.sa6.sin6_addr, sizeof Addr6);
			FlowInfo = a.sa6.sin6_flowinfo;
			ScopeId  = a.sa6.sin6_scope_id;
		}
	}
	int ipaddress::name(char* dst, int maxCount) const
	{
		if (inet_ntop(addrfamily_int(Family), (void*)&Addr4, dst, maxCount))
			return sprintf(dst, "%s:%d", dst, Port);
		return 0;
	}
	string ipaddress::name() const
	{
		char buf[128];
		return string(buf, name(buf, sizeof buf));
	}
	ostream& ipaddress::operator<<(ostream& os) const
	{
		char buf[128];
		return os.write(buf, name(buf, sizeof buf));
	}
	void ipaddress::clear()
	{
		Family = AF_IPv4;
		Port     = 0;
		FlowInfo = 0;
		ScopeId  = 0;
		Addr4    = INADDR_ANY;
	}
	int ipaddress::port() const
	{
		return ntohs(Port);
	}


	static saddr to_saddr(const ipaddress& ipa)
	{
		saddr a;
		a.sa4.sin_family = (ushort)addrfamily_int(ipa.Family);
		a.sa4.sin_port   = htons(ipa.Port);
		if (ipa.Family == AF_IPv4) {
			a.sa4.sin_addr.s_addr = ipa.Addr4;
			memset(a.sa4.sin_zero, 0, sizeof a.sa4.sin_zero);
		}
		else { // AF_IPv6
			memcpy(&a.sa6.sin6_addr, ipa.Addr6, sizeof ipa.Addr6);
			a.sa6.sin6_flowinfo = ipa.FlowInfo;
			a.sa6.sin6_scope_id = ipa.ScopeId;
		}
		return a;
	}




	///////////////////////////////////////////////////////////////////////////
	/////////        socket
	///////////////////////////////////////////////////////////////////////////

	socket::socket(int handle, const ipaddress& addr) : Sock(handle), Addr(addr) {}
	socket::socket() : Sock(-1), Addr(AF_IPv4) {}
	socket::socket(int port, address_family af) : Sock(-1), Addr(af, port) {
		listen(Addr);
	}
	socket::socket(const ipaddress& address) : Sock(-1), Addr(address) {
		connect(Addr);
	}
	socket::socket(const ipaddress& address, int timeoutMillis) : Sock(-1), Addr(address) {
		connect(Addr, timeoutMillis);
	}
	socket::socket(const char* hostname, int port, address_family af) : Sock(-1), Addr(af, hostname, port) {
		connect(Addr);
	}
	socket::socket(const char* hostname, int port, int timeoutMillis, address_family af) : Sock(-1), Addr(af, hostname, port) {
		connect(Addr, timeoutMillis);
	}
	socket::socket(socket&& fwd) : Sock(fwd.Sock), Addr(fwd.Addr) {
		fwd.Sock = -1;
		fwd.Addr.clear();
	}
	socket& socket::operator=(socket&& fwd) {
		if (this != &fwd) {
			if (Sock != -1) closesocket(Sock);
			Sock = fwd.Sock;
			Addr = fwd.Addr;
			fwd.Sock = -1;
			fwd.Addr.clear();
		}
		return *this;
	}
	socket::~socket() {
		if (Sock != -1) closesocket(Sock);
	}


	void socket::close() 
	{
		if (Sock != -1) closesocket(Sock), Sock = -1;
		Addr.clear();
	}


	string socket::last_err()
	{
		char buf[2048];
		return string(buf, sprintf(buf, "Error %d: %s", errno, strerror(errno)));
	}


	int socket::send(const void* buffer, int numBytes) {
		int ret = ::send(Sock, (const char*)buffer, numBytes, 0);
		if (ret == -1) // socket error?
			close(); 
		return ret;
	}
	int socket::send(const char* str)    { return send(str, (int)strlen(str)); }
	int socket::send(const wchar_t* str) { return send(str, int(sizeof(wchar_t) * wcslen(str))); }


	void socket::flush()
	{
		// flush write buffer:
		bool nodelay = is_nodelay();
		if (!nodelay) set_nodelay(true); // force only if needed
		set_nodelay(nodelay); // this must be called at least once

		skip(available()); // flush read buffer
	}

	int socket::available()
	{
		int bytesAvail;
		return get_ioctl(FIONREAD, bytesAvail) ? -1 : bytesAvail;
	}

	int socket::recv(void* buffer, int maxBytes)
	{
		return handle_recv(::recv(Sock, (char*)buffer, maxBytes, 0));
	}

	int socket::peek(void* buffer, int numBytes)
	{
		return handle_recv(::recv(Sock, (char*)buffer, numBytes, MSG_PEEK));
	}

	void socket::skip(int count)
	{
		char dump[128];
		for (int i = 0; i < count;) {
			int n = recv(dump, sizeof dump);
			if (n <= 0) break;
			i += n;
		}
	}

	// properly handles the crazy responses given by the recv() function
	// returns -1 on critical failure, otherwise it returns bytesAvailable (0...N)
	int socket::handle_recv(int ret)
	{
		if (ret == 0) // socket closed gracefully
		{
			close();
			return -1;
		}
		else if (ret == -1) // socket error?
		{
		#if _WIN32
			//auto str = Socket_LastErrStr(); // enable for debugging
			switch (WSAGetLastError()) 
			{
			default: 
				fprintf(stderr, "Recv unhandled %s\n", socket::last_err().c_str());
				return 0;
			case WSAENOTCONN:    return 0; // this socket is not connection oriented
			case WSAEWOULDBLOCK: return 0; // no data available right now
			case WSAECONNRESET:  
				close();
				return -1; // connection reset...
			}
		#else // unix
			switch (errno) {
			default:
				fprintf(stderr, "Recv unhandled %s\n", socket::last_err().c_str());
				return 0;
			case ENOTCONN:    return 0; // this socket is not connection oriented
			case EWOULDBLOCK: return 0; // no data available right now
			case ENETRESET:
			case ECONNABORTED:
			case ECONNRESET: 
				close();
				return -1; // connection reset...
			}
		#endif
		}
		return ret; // return as bytesAvailable
	}
	
	bool socket::wait_available(int millis)
	{
		try_for_period(millis, [this]() { return available() != 0; });
		return available() > 0;
	}

	int socket::get_opt(int optlevel, int socketopt) const {
		int value; socklen_t len = sizeof(int);
		return getsockopt(Sock, optlevel, socketopt, (char*)&value, &len) ? errno : value;
	}
	int socket::set_opt(int optlevel, int socketopt, int value) {
		return setsockopt(Sock, optlevel, socketopt, (char*)&value, 4) ? errno : 0;
	}
	int socket::get_ioctl(int iocmd, int& outValue) const {
		return ioctlsocket(Sock, iocmd, (u_long*)&outValue) ? errno : 0;
	}
	int socket::set_ioctl(int iocmd, int value) {
		return ioctlsocket(Sock, iocmd, (u_long*)&value) ? errno : 0;
	}


	socket_type socket::type() const {
		return to_socktype(get_opt(SOL_SOCKET, SO_TYPE));
	}
	address_family socket::family() const {
		return Addr.Family;
	}
	ip_protocol socket::ipproto() const {
		return protocol().Protocol;
	}


	protocol_info socket::protocol() const
	{
	#ifdef _WIN32
		WSAPROTOCOL_INFO winf = { 0 };
		int len = sizeof(winf);
		getsockopt(Sock, SOL_SOCKET, SO_PROTOCOL_INFO, (char*)&winf, &len);
		return protocol_info {
			winf.iProtocol,
			to_addrfamily(winf.iAddressFamily),
			to_socktype(winf.iSocketType),
			to_ipproto(winf.iProtocol),
		};
	#else
		return protocol_info();
	#endif
	}


	bool socket::connected() {
		char c;
		return Sock != -1 && peek(&c, 1) >= 0;
	}


	void socket::set_noblock_nodelay() {
		set_blocking(false); // blocking: false
		set_nodelay(false);  // nagle:    false
	}
	void socket::set_blocking(bool socketsBlock /*= 0*/) {
		set_ioctl(FIONBIO, !socketsBlock); // FIONBIO: 1 nonblock, 0 block
	}
	void socket::set_nodelay(bool enableNagle /*= 0*/) {
		set_opt(IPPROTO_TCP, TCP_NODELAY, !enableNagle); // TCP_NODELAY: 1 nodelay, 0 nagle enabled
	}
	bool socket::is_blocking() const {
		int value; get_ioctl(FIONBIO, value);
		return !!value;
	}
	bool socket::is_nodelay() const {
		return !!get_opt(IPPROTO_TCP, TCP_NODELAY);
	}

	void socket::listen(const ipaddress& addr)
	{
		inwin32(Socket_Initialize());
		if (Sock != -1) closesocket(Sock);

		// create TCP stream socket
		Sock = Socket_NewSocket(addr.Family, ST_Stream, IPP_TCP);
		if (Sock == -1)
			return;

		set_noblock_nodelay();

		auto sa = to_saddr(addr);
		::bind(Sock, sa, sa.size()); // setup as listening socket
		::listen(Sock, SOMAXCONN);   // start listening for new clients
		
		Addr = addr;
	}
	void socket::listen(int port, address_family af /*= AF_IPv4*/) {
		listen(ipaddress(af, port));
	}
	socket socket::listen_to(const ipaddress& addr) {
		socket s; s.listen(addr); return s;
	}
	socket socket::listen_to(int port, address_family af /*= AF_IPv4*/) {
		return listen_to(ipaddress(af, port));
	}


	socket socket::accept() const
	{
		// assume the listener socket is already non-blocking
		socket client { (int)::accept(Sock, NULL, NULL), ipaddress(AF_IPv4) };
		if (client.Sock != -1) { // do we have a client?
			new (&client.Addr) ipaddress(client.Sock); // update ipaddress
			// set the client socket as non-blocking, since socket options are not inherited
			client.set_noblock_nodelay();
		}
		return client;
	}
	socket socket::accept(int millis) const
	{
		socket client;
		try_for_period(millis, [this, &client]() {
			return (client = accept()).good();
		});
		return client;
	}


	bool socket::connect(const ipaddress& addr)
	{
		inwin32(Socket_Initialize());
		if (Sock != -1) closesocket(Sock);

		Sock = Socket_NewSocket(addr.Family, ST_Stream, IPP_TCP);
		if (Sock == -1)
			return false;

		auto sa = to_saddr(addr);
		if (::connect(Sock, sa, sa.size())) { // did connect fail?
			closesocket(Sock), Sock = -1;
			return false;
		}

		Addr = addr;
		set_noblock_nodelay();
		return Sock != -1;
	}	
	bool socket::connect(const ipaddress& addr, int millis)
	{
		try_for_period(millis, [this, &addr]() {
			return connect(addr);
		});
		return Sock != -1;
	}
	bool socket::connect(const char* hostname, int port, address_family af /*= AF_IPv4*/) {
		return connect(ipaddress(af, hostname, port));
	}
	bool socket::connect(const char* hostname, int port, int millis, address_family af /*= AF_IPv4*/) {
		return connect(ipaddress(af, hostname, port), millis);
	}


	socket socket::connect_to(const ipaddress& addr) { return socket(addr); }
	socket socket::connect_to(const char* hostname, int port, address_family af /*= AF_IPv4*/) {
		return connect_to(ipaddress(af, hostname, port));
	}
	socket socket::connect_to(const ipaddress& addr, int millis) { return socket(addr, millis); }
	socket socket::connect_to(const char* hostname, int port, int millis, address_family af /*= AF_IPv4*/) {
		return connect_to(ipaddress(af, hostname, port), millis);
	}

} // namespace rpp

