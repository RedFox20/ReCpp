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
#include <string.h>    // strcmp

/////////////////////////////////////////////////////////////////////////////
/**
 * Cross-platform compatibility definitions and helper functions
 */
#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
	#include <ws2tcpip.h>               // winsock2 and TCP/IP functions
	#include <process.h>                // _beginthreadex
	#pragma comment (lib, "Ws2_32.lib") // link against winsock libraries
	#define sleep(milliSeconds) Sleep((milliSeconds))
	#define inwin32(...) __VA_ARGS__
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
// spawns a new thread, thread handles are automatically closed 
void spawn_thread(void(*thread_func)(void* arg), void* arg)
{
#if _WIN32
	_beginthreadex(0,0,(unsigned(_stdcall*)(void*))thread_func,arg,0,0);
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
double timer_Time()
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

static int Socket_NewSocket(address_family af, socket_type st, ip_protocol ipp)
{
	inwin32(Socket_Initialize());

	int family = Socket_AFToInt(af);
	int type   = Socket_STToInt(st);
	int proto  = Socket_IPPToInt(ipp);

	return socket(family, type, proto);
}

void Socket_Close(int& s)
{
	if (s != -1)
	{
		closesocket(s);
		s = -1;
	}
}

std::string Socket_LastErrStr()
{
	char buffer[2048];
#ifdef _WIN32
	int err = WSAGetLastError();
	int len = sprintf(buffer, "Error %d: ", err);
	len += FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, 
						  err, 0, buffer + len, sizeof buffer - len, 0);
#else
	int err = errno;
	int len = sprintf(buffer, "Error %d: %s", err, strerror(err));
#endif
	return std::string(buffer, len);
}


//////////////////// SEND & RECV /////////////////////

int Socket_Send(int& socket, const void* buffer, int numBytes)
{
	int ret = send(socket, (const char*)buffer, numBytes, 0);
	if (ret == -1) // socket error?
		Socket_Close(socket);
	return ret;
}


// properly handles the crazy responses given by the recv() function
// returns -1 on critical failure, otherwise it returns bytesAvailable (0...N)
static int Socket_HandleRecv(int& socket, int ret)
{
	if (ret == 0) // socket closed gracefully
	{
		Socket_Close(socket);
		return -1;
	}
	else if (ret == -1) // socket error?
	{
	#if _WIN32
		//auto str = Socket_LastErrStr(); // enable for debugging
		switch (int err = WSAGetLastError()) 
		{
		default: 
			fprintf(stderr, "Recv unhandled %s\n", Socket_LastErrStr().c_str());
			return 0;
		case WSAENOTCONN:    return 0; // this socket is not connection oriented
		case WSAEWOULDBLOCK: return 0; // no data available right now
		case WSAECONNRESET:  
			Socket_Close(socket); 
			return -1; // connection reset...
		}
	#else // unix
		switch (errno) {
		default:
			fprintf(stderr, "Recv unhandled %s\n", Socket_LastErrStr().c_str());
			return 0;
		case ENOTCONN:    return 0; // this socket is not connection oriented
		case EWOULDBLOCK: return 0; // no data available right now
		case ENETRESET:
		case ECONNABORTED:
		case ECONNRESET: 
			Socket_Close(socket); 
			return -1; // connection reset...
		}
	#endif
	}
	return ret; // return as bytesAvailable
}

int Socket_Available(int& socket)
{
	int bytesAvail;
	return Socket_GetIoctl(socket, FIONREAD, bytesAvail) ? -1 : bytesAvail;
}

int Socket_Recv(int& socket, void* buffer, int maxBytes)
{
	int ret = recv(socket, (char*)buffer, maxBytes, 0);
	return Socket_HandleRecv(socket, ret);
}

std::string Socket_RecvStr(int& socket)
{
	int available = Socket_Available(socket);
	if (available <= 0) return std::string();

	std::string str(available, '\0');
	Socket_Recv(socket, (char*)str.data(), available);
	return str;
}

std::wstring Socket_RecvWStr(int& socket)
{
	int available = Socket_Available(socket);
	if (available <= 0) return std::wstring();

	std::wstring str(available/sizeof(wchar_t), '\0');
	Socket_Recv(socket, (char*)str.data(), available);
	return str;
}



//////////////////// sockopt & ioctl /////////////////////

int Socket_Getopt(int socket, int optlevel, int socketopt)
{
	int value, len = sizeof(int);
	return getsockopt(socket, optlevel, socketopt, (char*)&value, &len) ? errno : value;
}

int Socket_Setopt(int socket, int optlevel, int socketopt, int value)
{
	return setsockopt(socket, optlevel, socketopt, (char*)&value, 4) ? errno : 0;
}

int Socket_SetIoctl(int socket, int iocmd, int value)
{
	return ioctlsocket(socket, iocmd, (u_long*)&value) ? errno : 0;
}

int Socket_GetIoctl(int socket, int iocmd, int& outValue)
{
	return ioctlsocket(socket, iocmd, (u_long*)&outValue) ? errno : 0;
}



//////////////////// ENUM conversions /////////////////////

address_family Socket_IntToAF(int af)
{
	switch (af) { // AF is nonlinear
	default:
	case AF_UNSPEC: return AF_DontCare;
	case AF_INET:   return AF_IPv4;
	case AF_INET6:  return AF_IPv6;
	case AF_BTH:    return AF_Bth;
	}
}
socket_type Socket_IntToST(int sock)
{
	if (sock < 0 || sock > SOCK_SEQPACKET)
		return ST_Unspecified; // default fallback
	return socket_type(sock);
}
ip_protocol Socket_IntToIPP(int ipproto)
{
	switch (ipproto) { // IPPROTO is nonlinear
	default:            return IPP_DontCare;
	case IPPROTO_ICMP:  return IPP_ICMP;
	case IPPROTO_IGMP:  return IPP_IGMP;
	case IPPROTO_GGP:   return IPP_BTH;
	case IPPROTO_TCP:   return IPP_TCP;
	case IPPROTO_UDP:   return IPP_UDP;
	case IPPROTO_ICMPV6:return IPP_ICMPV6;
	case IPPROTO_PGM:   return IPP_PGM;
	}
}
int Socket_AFToInt(address_family addressFamily)
{
	static int families[] = { AF_UNSPEC, AF_INET, AF_INET6, AF_BTH };
	return families[addressFamily];
}
int Socket_STToInt(socket_type sockType)
{
	return int(sockType); // SOCK is linear
}
int Socket_IPPToInt(ip_protocol ipProtocol)
{
	static int protos[] = { 0, IPPROTO_ICMP, IPPROTO_IGMP, 3, IPPROTO_TCP, IPPROTO_UDP, IPPROTO_ICMPV6, IPPROTO_PGM };
	return protos[ipProtocol];
}



//////////////////// Socket Basic INFO /////////////////////

socket_type Socket_SocketType(int socket)
{
	return Socket_IntToST(Socket_Getopt(socket, SOL_SOCKET, SO_TYPE));
}

protocol_info Socket_ProtocolInfo(int socket)
{
#ifdef _WIN32
	WSAPROTOCOL_INFO winf = { 0 };
	int len = sizeof(winf);
	getsockopt(socket, SOL_SOCKET, SO_PROTOCOL_INFO, (char*)&winf, &len);
	protocol_info info = {
		winf.iProtocol,
		Socket_IntToAF(winf.iAddressFamily),
		Socket_IntToST(winf.iSocketType),
		Socket_IntToIPP(winf.iProtocol),
	};
	return info;
#else
	return protocol_info();
#endif
}
bool Socket_IsConnected(int& socket)
{
	char c;
	int ret = recv(socket, &c, 1, MSG_PEEK);
	return Socket_HandleRecv(socket, ret) != -1;
}



//////////////////// Socket Behavior Control /////////////////////

void Socket_NoBlockNoDelay(int socket)
{
	Socket_SetBlocking(socket, 0); // blocking: false
	Socket_SetNagle(socket,    0); // nagle:    false
}
void Socket_SetBlocking(int socket, int socketsBlock)
{
	// FIONBIO: 1 nonblock, 0 block
	Socket_SetIoctl(socket, FIONBIO, !socketsBlock);
}
void Socket_SetNagle(int socket, int enableNagle)
{
	// TCP_NODELAY: 1 nodelay, 0 nagle enabled
	Socket_Setopt(socket, IPPROTO_TCP, TCP_NODELAY, !enableNagle);
}



//////////////////// SockAddr initializations /////////////////////

ipaddress::ipaddress(address_family af) 
	: AddrFamily(af), Port(0), FlowInfo(0), ScopeId(0)
{
	if (af == AF_IPv4) Addr4 = INADDR_ANY;
	else memset(Addr6, 0, sizeof Addr6);
}
ipaddress::ipaddress(address_family af, int port) 
	: AddrFamily(af), Port(port), FlowInfo(0), ScopeId(0)
{
	if (af == AF_IPv4) Addr4 = INADDR_ANY;
	else memset(Addr6, 0, sizeof Addr6);
}
ipaddress::ipaddress(address_family af, const char* hostname, int port)
	: AddrFamily(af), Port(port), FlowInfo(0), ScopeId(0)
{
	if (af == AF_IPv4)
	{
	#if _MSC_VER || __linux // MSVC++ or linux gcc
		inet_pton(AF_INET, hostname, &Addr4); // text to network address lookup
	#else // __MINGW32__
		Addr4.s_addr = inet_addr(hostname);
	#endif
	}
	else
	{
	#if _MSC_VER || __linux // MSVC++ or linux gcc
	#else // __MINGW32__
		#warning MINGW32 does not have inet_pton
	#endif
		inet_pton(AF_INET6, hostname, &Addr6);
	}
}
std::string ipaddress::name() const
{
	char buf[128];
	auto* str = inet_ntop(Socket_AFToInt(AddrFamily), (void*)&Addr4, buf, sizeof buf);
	if (str == nullptr)
		return "";
	int len = sprintf(buf, "%s:%d", buf, Port);
	return std::string(buf, len);
}
void ipaddress::clear()
{
	AddrFamily = AF_IPv4;
	Port     = 0;
	FlowInfo = 0;
	ScopeId  = 0;
	Addr4    = INADDR_ANY;
}
int ipaddress::port() const
{
	return ntohs(Port);
}



union saddr
{
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

saddr to_saddr(const ipaddress& ipa)
{
	saddr a;
	a.sa4.sin_family = Socket_AFToInt(ipa.AddrFamily);
	a.sa4.sin_port   = htons(ipa.Port);
	if (ipa.AddrFamily == AF_IPv4)
	{
		a.sa4.sin_addr.s_addr = ipa.Addr4;
		memset(a.sa4.sin_zero, 0, sizeof a.sa4.sin_zero);
	}
	else // AF_IPv6
	{
		memcpy(&a.sa6.sin6_addr, ipa.Addr6, sizeof ipa.Addr6);
		a.sa6.sin6_flowinfo = ipa.FlowInfo;
		a.sa6.sin6_scope_id = ipa.ScopeId;
		a.sa6.sin6_scope_struct.Value = 0;
	}
	return a;
}

ipaddress Socket_GetAddr(int socket)
{
	saddr a;
	int len = sizeof(a);
	if (getsockname(socket, &a.sa, &len))
		return ipaddress(AF_IPv4); // quiet fail on error/invalid socket

	ipaddress ipa(Socket_IntToAF(a.sa4.sin_family));
	ipa.Port = ntohs(a.sa4.sin_port);
	if (ipa.AddrFamily == AF_IPv4)
	{
		ipa.Addr4 = a.sa4.sin_addr.s_addr;
	}
	else if (ipa.AddrFamily == AF_IPv6) // AF_IPv6
	{
		memcpy(ipa.Addr6, &a.sa6.sin6_addr, sizeof ipa.Addr6);
		ipa.FlowInfo = a.sa6.sin6_flowinfo;
		ipa.ScopeId  = a.sa6.sin6_scope_id;
	}
	return ipa;
}


//////////////////// Core of the API /////////////////////

int Socket_ListenTo(const ipaddress& address)
{
	inwin32(Socket_Initialize());

	// create TCP stream socket
	int server = Socket_NewSocket(address.AddrFamily, ST_Stream, IPP_TCP);
	if (server == -1)
		return -1;

	Socket_NoBlockNoDelay(server);
	
	auto addr = to_saddr(address);
	bind(server, addr, addr.size()); // setup as listening socket
	listen(server, SOMAXCONN);         // start listening for new clients
	return server;
}


int Socket_Accept(int listener)
{
	// assume the listener socket is already non-blocking
	int client = accept(listener, NULL, NULL);
	if (client != -1) // do we have a client?
	{
		// set the client socket as non-blocking, since socket options are not inherited
		Socket_NoBlockNoDelay(client);
		return client;
	}
	return client;
}
int Socket_Accept(int listener, int timeoutMillis)
{
	double timeout = timeoutMillis / 1000.0;
	double start = timer_Time();
	do
	{
		int client = Socket_Accept(listener);
		if (client != -1)
			return client;
		sleep(1); // suspend until next timeslice
	}
	while (timeout > 0.0 && (timer_Time() - start) < timeout);
	return -1;
}


struct AcceptAsync
{
	int listener;
	Socket_AsyncCallback callback;
	int timeoutMillis;
};
static void Socket_WaitAcceptAsync(AcceptAsync* arg)
{
	int listener  = arg->listener;
	auto callback = arg->callback;
	int timeout   = arg->timeoutMillis;
	delete arg; // delete before callback (in case of exception)
	int socket = Socket_Accept(listener, timeout);
	callback(socket, socket != -1 ? AR_Success : AR_Failed);
}
void Socket_AcceptAsync(int listener, Socket_AsyncCallback callback, int timeoutMillis)
{
	auto* arg = new AcceptAsync{ listener, callback, timeoutMillis };
	spawn_thread((void(*)(void*))&Socket_WaitAcceptAsync, arg);
}




int Socket_ConnectTo(const ipaddress& address)
{
	inwin32(Socket_Initialize());

	int remote = Socket_NewSocket(address.AddrFamily, ST_Stream, IPP_TCP);
	if (remote == -1)
		return -1;

	auto addr = to_saddr(address);
	if (connect(remote, addr, addr.size())) { // did connect fail?
		closesocket(remote);
		return -1;
	}

	Socket_NoBlockNoDelay(remote);
	return remote;
}

int Socket_ConnectTo(const ipaddress& address, int timeoutMillis)
{
	double timeout = timeoutMillis / 1000.0;
	double start = timer_Time();
	do
	{
		int remote = Socket_ConnectTo(address);
		if (remote != -1)
			return remote;
		sleep(1); // suspend until next timeslice
	} while (timeout > 0.0 && (timer_Time() - start) < timeout);
	return -1;
}

struct ConnectToAsync
{
	ipaddress addr;
	Socket_AsyncCallback callback;
	int timeoutMillis;
};
static void Socket_WaitConnectToAsync(ConnectToAsync* arg)
{
	ipaddress addr = arg->addr;
	auto callback = arg->callback;
	int timeout   = arg->timeoutMillis;
	delete arg; // delete before callback (in case of exception)
	int socket = Socket_ConnectTo(addr, timeout);
	callback(socket, socket != -1 ? AR_Success : AR_Failed);
}

void Socket_ConnectToAsync(const ipaddress& address,
                           Socket_AsyncCallback callback, int timeoutMillis)
{
	auto* arg = new ConnectToAsync{ address, callback, timeoutMillis };
	spawn_thread((void(*)(void*))&Socket_WaitConnectToAsync, arg);
}

/////////////////////////////////////////////////////////////////////////////
namespace rpp
{
	socket::socket() : Sock(-1), Addr(AF_IPv4)
	{
	}

	socket::socket(int handle, const ipaddress& addr) : Sock(handle), Addr(addr)
	{
	}

	socket::socket(socket&& fwd) : Sock(fwd.Sock), Addr(fwd.Addr)
	{
		fwd.Sock = -1;
		fwd.Addr.clear();
	}

	socket& socket::operator=(socket&& fwd)
	{
		if (this != &fwd)
		{
			Socket_Close(Sock);
			Sock = fwd.Sock;
			Addr = fwd.Addr;
			fwd.Sock = -1;
			fwd.Addr.clear();
		}
		return *this;
	}

	socket::~socket()
	{
		Socket_Close(Sock);
	}

	void socket::close()
	{
		Socket_Close(Sock);
		Addr.clear();
	}

	std::string socket::last_err()
	{
		return Socket_LastErrStr();
	}

	int socket::send(const void* buffer, int numBytes)
	{
		return Socket_Send(Sock, buffer, numBytes);
	}

	int socket::send(const char* str)
	{
		return Socket_Send(Sock, str, strlen(str));
	}

	int socket::send(const wchar_t* str)
	{
		return Socket_Send(Sock, str, sizeof(wchar_t) * wcslen(str));
	}

	int socket::available()
	{
		return Socket_Available(Sock);
	}

	int socket::recv(void* buffer, int maxBytes)
	{
		return Socket_Recv(Sock, buffer, maxBytes);
	}

	std::string socket::recv_str()
	{
		return Socket_RecvStr(Sock);
	}

	std::wstring socket::recv_wstr()
	{
		return Socket_RecvWStr(Sock);
	}

	int socket::get_opt(int optlevel, int socketopt)
	{
		return Socket_Getopt(Sock, optlevel, socketopt);
	}

	int socket::set_opt(int optlevel, int socketopt, int value)
	{
		return Socket_Setopt(Sock, optlevel, socketopt, value);
	}

	int socket::get_ioctl(int iocmd, int& outValue)
	{
		return Socket_GetIoctl(Sock, iocmd, outValue);
	}

	int socket::set_ioctl(int iocmd, int value)
	{
		return Socket_GetIoctl(Sock, iocmd, value);
	}

	socket_type socket::type() const
	{
		return Socket_SocketType(Sock);
	}

	protocol_info socket::protocol() const
	{
		return Socket_ProtocolInfo(Sock);
	}

	bool socket::connected()
	{
		return Socket_IsConnected(Sock);
	}

	void socket::set_noblock_nodelay()
	{
		Socket_NoBlockNoDelay(Sock);
	}

	void socket::set_blocking(int socketsBlock /*= 0*/)
	{
		Socket_SetBlocking(Sock, socketsBlock);
	}

	void socket::set_nagle(int enableNagle /*= 0*/)
	{
		Socket_SetNagle(enableNagle);
	}



	void socket::listen(const ipaddress& address)
	{
		Socket_Close(Sock);
		Sock = Socket_ListenTo(address);
		Addr = address;
	}

	void socket::listen(int port, address_family af /*= AF_IPv4*/)
	{
		listen(ipaddress(af, port));
	}

	socket socket::listen_to(const ipaddress& address)
	{
		socket s;
		s.Sock = Socket_ListenTo(address);
		s.Addr = address;
		return std::move(s);
	}

	socket socket::listen_to(int port, address_family af /*= AF_IPv4*/)
	{
		return listen_to(ipaddress(af, port));
	}

	socket socket::accept() const
	{
		int s = Socket_Accept(Sock);
		return socket{ s, Socket_GetAddr(s) };
	}

	socket socket::accept(int timeoutMillis) const
	{
		int s = Socket_Accept(Sock, timeoutMillis);
		return socket{ s, Socket_GetAddr(s) };
	}

	bool socket::connect(const ipaddress& address)
	{
		Socket_Close(Sock);
		Sock = Socket_ConnectTo(address);
		Addr = address;
		return Sock != -1;
	}

	bool socket::connect(const char* hostname, int port, address_family af /*= AF_IPv4*/)
	{
		return connect(ipaddress(af, hostname, port));
	}

	bool socket::connect(const ipaddress& address, int timeoutMillis)
	{
		Socket_Close(Sock);
		Sock = Socket_ConnectTo(address, timeoutMillis);
		Addr = address;
		return Sock != -1;
	}

	bool socket::connect(const char* hostname, int port, int timeoutMillis, address_family af /*= AF_IPv4*/)
	{
		return connect(ipaddress(af, hostname, port), timeoutMillis);
	}

	socket socket::connect_to(const ipaddress& address)
	{
		return socket{ Socket_ConnectTo(address), address };
	}

	socket socket::connect_to(const char* hostname, int port, address_family af /*= AF_IPv4*/)
	{
		return connect_to(ipaddress(af, hostname, port));
	}

	socket socket::connect_to(const ipaddress& address, int timeoutMillis)
	{
		return socket{ Socket_ConnectTo(address, timeoutMillis), address };
	}

	socket socket::connect_to(const char* hostname, int port, int timeoutMillis, address_family af /*= AF_IPv4*/)
	{
		return connect_to(ipaddress(af, hostname, port), timeoutMillis);
	}

} // namespace rpp