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
	#pragma comment (lib, "Ws2_32.lib") // link against winsock libraries
	#define sleep(milliSeconds) Sleep((milliSeconds))
	#define inwin32(...) __VA_ARGS__
#else // UNIX
	#include <time.h>               // clock_gettime
	#include <unistd.h>             // close()
	//#include <pthread.h>          // POSIX threads
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
	static struct wsa { // static funcvar init on first entry to func
		wsa()  { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); }
		~wsa() { WSACleanup(); }
	} wsa_raii_handle; // this raii handle will be automatically unloaded
}
#endif

static int Socket_NewSocket(AddressFamily af, SocketType st, IPProtocol ipp)
{
	inwin32(Socket_Initialize());

	int family = Socket_AFToInt(af);
	int type   = Socket_STToInt(st);
	int proto  = Socket_IPPToInt(ipp);

	return socket(family, type, proto);
}

void Socket_Close(int& socket)
{
	if (socket != -1)
	{
		closesocket(socket);
		socket = -1;
	}
}

std::string Socket_LastErrStr()
{
	char buffer[1024];
	int err = WSAGetLastError();
	int len = sprintf(buffer, "Error %d: ", err);
	len += FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, 
						  err, 0, buffer + len, sizeof buffer - len, 0);
	return std::string(buffer, len);
}



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
		case WSAEWOULDBLOCK: return 0; // no data available right now
		case WSAECONNRESET:  
			Socket_Close(socket); 
			return -1; // connection reset...
		}
	#else // unix
		switch (errno) {
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



AddressFamily Socket_IntToAF(int af)
{
	switch (af) { // AF is nonlinear
	default:
	case AF_UNSPEC: return AF_DontCare;
	case AF_INET:   return AF_IPv4;
	case AF_INET6:  return AF_IPv6;
	case AF_BTH:    return AF_Bth;
	}
}
SocketType Socket_IntToST(int sock)
{
	if (sock < 0 || sock > SOCK_SEQPACKET)
		return ST_Unspecified; // default fallback
	return SocketType(sock);
}
IPProtocol Socket_IntToIPP(int ipproto)
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
int Socket_AFToInt(AddressFamily addressFamily)
{
	static int families[] = { AF_UNSPEC, AF_INET, AF_INET6, AF_BTH };
	return families[addressFamily];
}
int Socket_STToInt(SocketType sockType)
{
	return int(sockType); // SOCK is linear
}
int Socket_IPPToInt(IPProtocol ipProtocol)
{
	static int protos[] = { 0, IPPROTO_ICMP, IPPROTO_IGMP, 3, IPPROTO_TCP, IPPROTO_UDP, IPPROTO_ICMPV6, IPPROTO_PGM };
	return protos[ipProtocol];
}



SocketType Socket_SocketType(int socket)
{
	return Socket_IntToST(Socket_Getopt(socket, SOL_SOCKET, SO_TYPE));
}

ProtocolInfo Socket_ProtocolInfo(int socket)
{
	WSAPROTOCOL_INFO winf = { 0 };
	int len = sizeof(winf);
	getsockopt(socket, SOL_SOCKET, SO_PROTOCOL_INFO, (char*)&winf, &len);
	ProtocolInfo info = {
		winf.iProtocol,
		Socket_IntToAF(winf.iAddressFamily),
		Socket_IntToST(winf.iSocketType),
		Socket_IntToIPP(winf.iProtocol),
	};
	return info;
}
bool Socket_IsConnected(int& socket)
{
	char c;
	int ret = recv(socket, &c, 1, MSG_PEEK);
	return Socket_HandleRecv(socket, ret) != -1;
}



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



int Socket_ListenTo(int port)
{
	inwin32(Socket_Initialize());

	// set up local server listener
	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(port);            // host to network short
	addr.sin_addr.s_addr = htonl(INADDR_ANY); // host to network long

	// create TCP stream socket
	int server = Socket_NewSocket(AF_IPv4, ST_Stream, IPP_TCP);
	if (server == -1)
		return -1;

	Socket_NoBlockNoDelay(server);
	bind(server, (struct sockaddr*)&addr, sizeof(addr));   // setup as listening socket
	listen(server, SOMAXCONN);                             // start listening for new clients
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


bool Socket_AcceptAsync(int listener, Socket_AsyncCallback callback, int timeoutMillis)
{
	return false;
}






int Socket_ConnectTo(const char* hostname, int port, AddressFamily af)
{
	inwin32(Socket_Initialize());

	struct sockaddr_in addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(port); // host to network short
	#if _MSC_VER || __linux // MSVC++ or linux gcc
		inet_pton(AF_INET, hostname, &addr.sin_addr); // text to network address lookup
	#else // __MINGW32__
		addr.sin_addr.s_addr = inet_addr(hostname);
	#endif

	int remote = Socket_NewSocket(af, ST_Stream, IPP_TCP);
	if (remote == -1)
		return -1;

	if (connect(remote, (struct sockaddr*)&addr, sizeof addr)) { // did connect fail?
		closesocket(remote);
		return -1;
	}

	Socket_NoBlockNoDelay(remote);
	return remote;
}

int Socket_ConnectTo(const char* host, int port, AddressFamily af, int timeoutMillis)
{
	double timeout = timeoutMillis / 1000.0;
	double start = timer_Time();
	do
	{
		int remote = Socket_ConnectTo(host, port, af);
		if (remote != -1)
			return remote;
		sleep(1); // suspend until next timeslice
	} while (timeout > 0.0 && (timer_Time() - start) < timeout);
	return -1;
}

/////////////////////////////////////////////////////////////////////////////

