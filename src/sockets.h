#pragma once
#include <string> // std::string

enum AddressFamily
{
	AF_DontCare,    // unspecified AddressFamily, service provider will choose most appropriate
	AF_IPv4,        // The Internet Protocol version 4 (IPv4) address family
	AF_IPv6,        // The Internet Protocol version 6 (IPv6) address family
	AF_Bth,         // Bluetooth address family, supported since WinXP SP2
};

enum SocketType
{
	ST_Unspecified, // unspecified socket type (invalid socket)
	ST_Stream,      // TCP only, byte stream for IPv4 or IPv6 protocols
	ST_Datagram,    // UDP only, byte datagrams for IPv4 or IPv6 protocols
	ST_Raw,         // application provides IPv4 or IPv6 headers
	ST_RDM,         // reliable message datagram for PGM
	ST_SeqPacket,   // TCP based, similar to ST_Stream but slower, however packet boundaries are respected "TCP datagrams"
};

enum IPProtocol
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

struct ProtocolInfo
{
	int ProtoVersion; // protocol version identifier
	AddressFamily AddrFamily;
	SocketType    SockType;
	IPProtocol    Protocol;
};

enum AsyncResult
{
	AR_Failed,
	AR_Success,
};

/**
 * A generic Async callback function for socket IO routines
 * @param socket The socket relevant
 * @param result Describes the result of the Async operation, such as AR_Failed or AR_Success
 */
typedef void (*Socket_AsyncCallback)(int socket, AsyncResult result);


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
AddressFamily Socket_IntToAF(int af);
/** @return UNIX sock => SocketType conversion */
SocketType Socket_IntToST(int sock);
/** @return UNIX ipproto => AddressFamily conversion */
IPProtocol Socket_IntToIPP(int ipproto);

/** @return AddressFamily => UNIX af conversion */
int Socket_AFToInt(AddressFamily addressFamily);
/** @return SocketType => UNIX sock conversion */
int Socket_STToInt(SocketType sockType);
/** @return AddressFamily => UNIX ipproto conversion */
int Socket_IPPToInt(IPProtocol ipProtocol);


/**
 * @return The SocketType of the socket
 */
SocketType Socket_SocketType(int socket);
/**
 * @return ProtocolInfo of the socket
 */
ProtocolInfo Socket_ProtocolInfo(int socket);
/**
 * Checks if the socket is still valid and connected. If connection has
 * reset or remote end has closed the connection, the socket will be closed
 * @return TRUE if the socket is still valid and connected
 */
bool Socket_IsConnected(int& socket);



/**
 * Configure socket settings: non-blocking I/O, nagle disabled (TCP_NODELAY=TRUE)
 */
void Socket_NoBlockNoDelay(int socket);
/**
 * Configure socket settings: I/O blocking mode off(0) or on(1)
 * @param socketsBlock (0 = NOBLOCK, 1 = BLOCK)
 */
void Socket_SetBlocking(int socket, int socketsBlock = 0);
/**
 * Configure socket settings: nagle off (TCP_NODELAY) or on (Nagle bandwidth opt.)
 * @param enableNagle (0 = TCP_NODELAY, 1 = Nagle enabled)
 */
void Socket_SetNagle(int socket, int enableNagle = 0);



/**
 * Creates a new listener type socket used for accepting new connections
 */
int Socket_ListenTo(int port);



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
 * The completionRoutine receives accepted socket with AR_Success or -1 for AR_Failed
 * @return TRUE if async IO operation was started
 */
bool Socket_AcceptAsync(int listener, Socket_AsyncCallback callback, int timeoutMillis = -1);



/**
 * Connects to a remote socket and sets the socket as nonblocking and tcp nodelay
 * @param af Address family to use, most commonly AF_IPv4 or AF_IPv6
 * @return >= 0: valid socket, -1: error (check errno)
 */
int Socket_ConnectTo(const char* hostname, int port, AddressFamily af);
/**
 * Connects to a remote socket and sets the socket as nonblocking and tcp nodelay
 */
inline int Socket_ConnectToIPv4(const char* hostname, int port)
{
	return Socket_ConnectTo(hostname, port, AF_IPv4);
}
/**
 * Connects to a remote socket and sets the socket as nonblocking and tcp nodelay
 */
inline int Socket_ConnectToIPv6(const char* hostname, int port)
{
	return Socket_ConnectTo(hostname, port, AF_IPv6);
}
/**
* Connects to a remote socket and sets the socket as nonblocking and tcp nodelay
* @param af Address family to use, most commonly AF_IPv4 or AF_IPv6
* @param timeoutMillis Timeout value if the server takes too long to respond
* @return >= 0: valid socket, -1: error (check errno)
*/
int Socket_ConnectTo(const char* hostname, int port, AddressFamily af, int timeoutMillis);