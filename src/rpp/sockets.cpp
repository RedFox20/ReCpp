#include "sockets.h"
#include "debugging.h"
#include "strview.h" // _tostring
#include "sort.h"
#include "./math.h" // rpp::min
#include "endian.h" // rpp::readLEU16, rpp::writeLEU16 (Windows AF_Unix framing)
#include <cstdlib>    // malloc
#include <cstdio>     // printf
#include <cstring>    // memcpy,memset,strlen
#include <cstddef>    // offsetof
#include <cassert>
#include <memory> // std::unique_ptr
#include <thread> // std::this_thread::yield()

#if DEBUG || _DEBUG || RPP_DEBUG
#  define RPP_SOCKETS_DBG 1
#else
#  define RPP_SOCKETS_DBG 0
#endif

/**
 * Cross-platform compatibility definitions and helper functions
 */
#if (_WIN32 || _WIN64) // MSVC and MinGW
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
    #include <WinSock2.h>
    #include <ws2tcpip.h>           // winsock2 and TCP/IP functions
    #include <afunix.h>             // sockaddr_un (AF_UNIX, Win10 1803+)
    #include <mstcpip.h>            // WSAIoctl Options
    #include <process.h>            // _beginthreadex
    #include <Iphlpapi.h>
    #ifdef _MSC_VER
        #pragma comment(lib, "Ws2_32.lib") // link against winsock libraries
        #pragma comment(lib, "Iphlpapi.lib")
    #endif
    #define sleep(milliSeconds) Sleep((milliSeconds))
    #define inwin32(...) __VA_ARGS__
    #define inunix(...) /*do nothing*/
    #ifdef __MINGW32__
        extern "C" __declspec(dllimport) int WSAAPI inet_pton(int af, const char* src, void* dst);
        extern "C" const char* WSAAPI inet_ntop(int af, void* pAddr, char* pStringBuf, size_t StringBufSize);
    #endif
#else // UNIX
    #include <unistd.h>             // close()
    #include <pthread.h>            // POSIX threads
    #include <sys/types.h>          // required type definitions
    #include <sys/socket.h>         // LINUX sockets
    #include <netdb.h>              // addrinfo, freeaddrinfo...
    #include <netinet/in.h>         // sockaddr_in
    #include <netinet/tcp.h>        // TCP_NODELAY
    #include <sys/un.h>             // sockaddr_un (AF_UNIX)
    #include <cerrno>               // last error number
    #include <sys/ioctl.h>          // ioctl()
    #if __has_include(<fcntl.h>)
        #include <fcntl.h>          // fcntl()
    #else
        #include <sys/fcntl.h>      // fcntl()
    #endif
    #include <poll.h> // poll()
    #include <linux/sockios.h>      // SIOCOUTQ (get send queue size)
    #include <arpa/inet.h>          // inet_addr, inet_ntoa
    // on Android this requires API level 24
    #if RPP_ANDROID && __ANDROID_API__ < 24
        #error "<ifaddrs.h> requires ANDROID_API level 24"
    #else
        #include <ifaddrs.h>
    #endif
    #if RPP_ANDROID
        #include "jni_cpp.h"
        #include <android/log.h>
        #include <android/multinetwork.h> // android_setsocknetwork
    #endif
    #define sleep(milliSeconds) ::usleep((milliSeconds) * 1000)
    #define inwin32(...) /*nothing*/
    #define inunix(...) __VA_ARGS__
     // map linux socket calls to winsock calls via macros
    #define closesocket(fd) ::close(fd)
#endif

// For MSVC we don't need to flush logs, but for LINUX, it's necessary for atomic-ish logging
#if _MSC_VER
#  define logflush(...)
#else
#  define logflush(file) fflush(file)
#endif

// AF_Unix message sockets: Windows (framed SOCK_STREAM) and Linux
// (SOCK_SEQPACKET in the abstract namespace). Everything else fails cleanly --
// the SOCK_SEQPACKET + abstract-namespace + SCM_RIGHTS combination is
// Linux-specific, so we refuse to pretend support we can't deliver.
#ifndef RPP_UNIX_SOCKET_SUPPORTED
#  if _WIN32 || defined(__linux__)
#    define RPP_UNIX_SOCKET_SUPPORTED 1
#  else
#    define RPP_UNIX_SOCKET_SUPPORTED 0
#  endif
#endif

#ifndef MSG_NOSIGNAL
#  define MSG_NOSIGNAL 0
#endif

// Maps a bare errno macro to its platform-specific socket error constant
// (WSA-prefixed on Windows). Used throughout for cross-platform error handling.
#if _WIN32
#  define ESOCK(errmacro) WSA ## errmacro
#else
#  define ESOCK(errmacro) errmacro
#endif

#if RPP_SOCKETS_DBG
    #define indebug(...) __VA_ARGS__
    // logs the error only if previous error was different
    #define logerronce(err, fmt, ...) do { \
        static int prev_failure; \
        if ((err) != prev_failure) { \
            fprintf(stderr, __log_format(fmt"\n", __FILE__, __LINE__, __FUNCTION__), ##__VA_ARGS__); \
            logflush(stderr); \
        } \
    } while (0)
    // log into stderr
    #define logerror(fmt, ...) do { \
        fprintf(stderr, __log_format(fmt"\n", __FILE__, __LINE__, __FUNCTION__), ##__VA_ARGS__); \
        logflush(stderr); \
    } while (0)
    // log into stdout
    #define logdebug(fmt, ...) do { \
        fprintf(stdout, __log_format(fmt"\n", __FILE__, __LINE__, __FUNCTION__), ##__VA_ARGS__); \
        logflush(stdout); \
    } while (0)
#else
    #define indebug(...) /*NOP in release*/
    #define logerronce(err, fmt, ...) /*NOP in release*/
    #define logerror(fmt, ...) /*NOP in release*/
    #define logdebug(fmt, ...) /*NOP in release*/
#endif

/////////////////////////////////////////////////////////////////////////////

namespace rpp
{
    /////////////////////////////////////////////////////////////////////////////

#if _WIN32
    static void InitWinSock() noexcept
    {
        static struct wsa { // static var init on first entry to func
            wsa() { WSADATA w; (void)WSAStartup(MAKEWORD(2, 2), &w); }
            ~wsa() { WSACleanup(); }
        } wsa_raii_handle; // this RAII handle will be automatically unloaded
    }
#endif


    //////////////////// ENUM conversions /////////////////////

    address_family to_addrfamily(int af) noexcept
    {
        switch (af) { // AF is nonlinear
        default:
        case AF_UNSPEC:   return AF_DontCare;
        case AF_INET:     return AF_IPv4;
        case AF_INET6:    return AF_IPv6;
        case 32/*AF_BTH*/:return AF_Bth;
        case AF_UNIX:     return AF_Unix;
        }
    }
    socket_type to_socktype(int sock) noexcept
    {
        switch (sock) {
        default:             return ST_Unspecified;
        case SOCK_STREAM:    return ST_Stream;
        case SOCK_DGRAM:     return ST_Datagram;
        case SOCK_RAW:       return ST_Raw;
        case SOCK_RDM:       return ST_RDM;
        case SOCK_SEQPACKET: return ST_SeqPacket;
        }
    }
    socket_type to_socktype(ip_protocol ipp) noexcept
    {
        static socket_type defaults[] = {
            ST_Unspecified, ST_Raw, ST_Raw, ST_Stream, ST_Stream, ST_Datagram, ST_Raw, ST_RDM
        };
        return defaults[ipp];
    }
    ip_protocol to_ipproto(int ipproto) noexcept
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
    int addrfamily_int(address_family addressFamily) noexcept
    {
        static int families[] = { AF_UNSPEC, AF_INET, AF_INET6, 32/*AF_BTH*/, AF_UNIX };
        return families[addressFamily];
    }
    int socktype_int(socket_type sockType) noexcept
    {
        static int types[] = { 0, SOCK_STREAM, SOCK_DGRAM, SOCK_RAW, SOCK_RDM, SOCK_SEQPACKET };
        return types[sockType];
    }
    int ipproto_int(ip_protocol ipProtocol) noexcept
    {
        static int protos[] = { 0, IPPROTO_ICMP, IPPROTO_IGMP, 3/*IPPROTO_GGP*/, IPPROTO_TCP, IPPROTO_UDP, IPPROTO_ICMPV6, 113/*IPPROTO_PGM*/ };
        return protos[ipProtocol];
    }

    ///////////////////////////////////////////////////////////////////////////
    ////////        IP Address
    ///////////////////////////////////////////////////////////////////////////

    // Wraps the various sockaddr families plus an explicit address length.
    // The length is REQUIRED for AF_UNIX in the Linux abstract namespace, whose
    // sockaddr length is offsetof(sun_path)+1+namelen and CANNOT be derived from
    // sa_family. to_saddr() sets Len for every family; raw OS sockaddrs that are
    // reinterpret_cast to saddr (e.g. from getifaddrs) never read Len/size().
    #if _MSC_VER
    #  pragma warning(push)
    #  pragma warning(disable:4201) // nonstandard anonymous struct/union
    #endif
    struct saddr {
        union {
            sockaddr     sa;
            sockaddr_in  sa4;
            sockaddr_in6 sa6;
            sockaddr_un  sun;
        };
        int Len = 0; // explicit address length; 0 => derive from sa_family
        operator const sockaddr*() const noexcept { return &sa; }
        int size() const noexcept {
            if (Len > 0) return Len;
            switch (sa.sa_family) {
                default:       return sizeof(sa);
                case AF_INET:  return sizeof(sa4);
                case AF_INET6: return sizeof(sa6);
            }
        }
    };
    #if _MSC_VER
    #  pragma warning(pop)
    #endif

    // --------------------------------------------------------------------------------

    raw_address::raw_address() noexcept
        : Family{AF_DontCare}, FlowInfo{0}, ScopeId{0}
    {
        memset(Addr6, 0, sizeof(Addr6)); // zero the address union (covers Addr4)
    }

    raw_address::raw_address(address_family af) noexcept
        : Family{af}, FlowInfo{0}, ScopeId{0}
    {
        memset(Addr6, 0, sizeof(Addr6)); // zero the address union (covers Addr4)
    }

    raw_address::raw_address(address_family af, uint32_t ipv4) noexcept : raw_address{af}
    {
        Addr4 = ipv4;
    }

    raw_address::raw_address(address_family af, const void* ipv6,
                             unsigned long flowInfo, unsigned long scopeId) noexcept : raw_address{af}
    {
        memcpy(Addr6, ipv6, sizeof(Addr6));
        FlowInfo = flowInfo;
        ScopeId = scopeId;
    }

    raw_address::raw_address(address_family af, const char* ip_address) noexcept : raw_address{af}
    {
        resolve_addr(af, ip_address);
    }

    void raw_address::reset() noexcept
    {
        Family = AF_DontCare;
        memset(Addr6, 0, sizeof(Addr6)); // zero the address union (covers Addr4)
        FlowInfo = 0;
        ScopeId = 0;
    }

    address_family raw_address::get_address_family(const char* ip_and_port) noexcept
    {
        if (!ip_and_port || *ip_and_port == '\0')
            return AF_IPv4;

        if (*ip_and_port == '[') // IPv6 "[2001:db8::1]:8080"
            return AF_IPv6;

        bool was_colon = false;
        for (const char* p = ip_and_port; *p != '\0'; ++p)
        {
            bool is_colon = *p == ':';
            if (is_colon && was_colon)
                return AF_IPv6; // IPv6 "::1" or "2001:db8::1"
            was_colon = is_colon;
        }
        // this is a hostname like "www.kratt.codefox.ee", or ip "192.168.1.1:8912", or ":8080"
        return AF_IPv4;
    }

    bool raw_address::resolve_addr(address_family af, const char* hostname, int port) noexcept
    {
        Family = af;
        void* addr = af == AF_IPv4 ? (void*)&Addr4 : (void*)&Addr6;
        //int addrLen = Family == AF_IPv4 ? sizeof Addr4 : sizeof Addr6;
        int family = af == AF_IPv4 ? AF_INET : AF_INET6;
        memset(addr, 0, sizeof(Addr6));

        if (af == AF_IPv4 && (!hostname || *hostname == '\0'))
            return true; // listener socket IP addresss { "", 8080 }

        // TODO: add parsing for scopeid "::1/64" and "::1%eth0"
        if (af == AF_IPv6 && strcmp(hostname, "::1") == 0) // IPv6 loopback address
        {
            Addr6[15] = 1;
            return true;
        }

        addrinfo hint = {}; // must be nulled
        hint.ai_family = family; // only filter by family

        char port_str[32] = "";
        if (port > 0)
            _tostring(port_str, port);

        if (isdigit(hostname[0]))
            hint.ai_flags = AI_NUMERICHOST;

        inwin32(InitWinSock());

        addrinfo* infos = nullptr;
        bool success = false;
        if (!getaddrinfo(hostname, (port > 0 ? port_str : nullptr), &hint, &infos))
        {
            for (addrinfo* info = infos; info != nullptr; info = info->ai_next)
            {
                if (info->ai_family == family)
                {
                    sockaddr_in* sin = (sockaddr_in*)info->ai_addr;
                    if (family == AF_INET)
                    {
                        Addr4 = sin->sin_addr.s_addr;
                    }
                    else
                    {
                        sockaddr_in6* sin6 = (sockaddr_in6*)sin;
                        memcpy(Addr6, &sin6->sin6_addr, sizeof(Addr6));
                        FlowInfo = sin6->sin6_flowinfo;
                        ScopeId  = sin6->sin6_scope_id;
                    }
                    success = true;
                    break;
                }
            }
            freeaddrinfo(infos);
        }
        else
        {
            logerror("getaddrinfo failed: %s", socket::last_os_socket_err().c_str());
        }
        return success;
    }

    bool raw_address::equals(const raw_address& addr) const noexcept
    {
        if (Family == addr.Family)
        {
            if (Family == AF_IPv4)
                return Addr4 == addr.Addr4;
            if (Family == AF_Unix)
                return true; // nameless marker: same family => equal (name lives on unix_socket)
            return FlowInfo == addr.FlowInfo && ScopeId == addr.ScopeId
                && memcmp(Addr6, addr.Addr6, sizeof(Addr6)) == 0;
        }
        return false;
    }

    int raw_address::compare(const raw_address& addr) const noexcept
    {
        if (Family < addr.Family) return -1;
        if (Family > addr.Family) return +1;

        // families are equal, compare by addr
        if (Family == AF_IPv4)
            return memcmp(&Addr4, &addr.Addr4, sizeof(Addr4));
        else if (Family == AF_Unix)
            return 0; // nameless marker: same family => equal (name lives on unix_socket)
        else
            return memcmp(&Addr6, &addr.Addr6, sizeof(Addr6));
    }

    bool raw_address::has_address() const noexcept
    {
        if (Family == AF_IPv4)
            return Addr4 != INADDR_ANY;
        if (Family == AF_Unix)
            return false;
        for (unsigned char i : Addr6) // NOLINT(readability-use-anyofallof)
            if (i != 0) return true;
        return false;
    }

    std::string raw_address::str() const noexcept
    {
        char buf[128];
        return std::string{buf, buf+to_cstr(buf, 128)};
    }

    char* raw_address::c_str() const noexcept
    {
        static thread_local char buf[128];
        (void)to_cstr(buf, 128);
        return buf;
    }

    int raw_address::to_cstr(char* buf, int max) const noexcept
    {
        if (Family == AF_DontCare) {
            if (max > 0) buf[0] = '\0';
            return 0;
        }
        if (Family == AF_Unix) {
            // The local name is not stored in the address (see rpp::unix_socket);
            // emit a stable family marker for logging.
            int n = snprintf(buf, max, "AF_Unix");
            return n < 0 ? 0 : n;
        }
        inwin32(InitWinSock());
        if (inet_ntop(addrfamily_int(Family), (void*)&Addr4, buf, max))
            return (int)strlen(buf);
        return snprintf(buf, max, "%u.%u.%u.%u",
                        Addr4Parts[0], Addr4Parts[1], Addr4Parts[2], Addr4Parts[3]);
    }

    // --------------------------------------------------------------------------------

    ipaddress::ipaddress(address_family af, const char* hostname, int port) noexcept
        : Address{af}, Port{static_cast<uint16_t>(port)}
    {
        Address.resolve_addr(af, hostname, port);
    }

    ipaddress::ipaddress(address_family af, const char* ip_and_port) noexcept
        : ipaddress{af, 0}
    {
        if (ip_and_port && *ip_and_port != '\0')
        {
            if (af == AF_IPv6)
            {
                if (*ip_and_port == '[') // IPv6 "[2001:db8::1]:8080"
                {
                    if (const char* end = strchr(ip_and_port, ']'))
                    {
                        int ip_len = static_cast<int>(end - ip_and_port - 1);
                        char ip_part[128] = "";
                        memcpy(ip_part, ip_and_port + 1, ip_len);
                        ip_part[ip_len] = '\0';

                        Port = atoi(end + 2);
                        resolve_addr(af, ip_part, Port);
                    }
                }
                else // only IP, port 0
                {
                    resolve_addr(af, ip_and_port, 0);
                }
            }
            else if (const char* port = strchr(ip_and_port, ':')) // got port?
            {
                int ip_len = static_cast<int>(port - ip_and_port);
                char ip_part[128] = "";
                memcpy(ip_part, ip_and_port, ip_len);
                ip_part[ip_len] = '\0';

                Port = atoi(port + 1);
                resolve_addr(af, ip_part, Port);
            }
            else // only IP, port 0
            {
                resolve_addr(af, ip_and_port, 0);
            }
        }
    }

    ipaddress::ipaddress(const char* hostname, const char* port) noexcept
        : ipaddress{get_address_family(hostname), hostname, atoi(port)} {}

    ipaddress::ipaddress(int socket) noexcept
    {
        inwin32(InitWinSock());
        saddr a;
        socklen_t len = sizeof(a);
        if (getsockname(socket, reinterpret_cast<sockaddr*>(&a), &len) != 0) {
            Address.Family=AF_DontCare, Port=0, Address.FlowInfo=0, Address.ScopeId=0;
            return; // quiet fail on error/invalid socket
        }

        Address.Family = to_addrfamily(a.sa.sa_family);
        if (Address.Family == AF_Unix) {
            // AF_UNIX has no port and the address carries no name (it lives on
            // rpp::unix_socket); just record the family marker.
            Port = 0;
            Address.FlowInfo = 0, Address.ScopeId = 0;
            return;
        }
        Port = ntohs(a.sa4.sin_port);
        if (Address.Family == AF_IPv4) {
            Address.Addr4 = a.sa4.sin_addr.s_addr;
            Address.FlowInfo = 0, Address.ScopeId = 0;
        }
        else if (Address.Family == AF_IPv6) { // AF_IPv6
            memcpy(Address.Addr6, &a.sa6.sin6_addr, sizeof(Address.Addr6));
            Address.FlowInfo = a.sa6.sin6_flowinfo;
            Address.ScopeId  = a.sa6.sin6_scope_id;
        }
    }

    // --------------------------------------------------------------------------------

    void ipaddress::reset() noexcept
    {
        Address.reset();
        Port = 0;
    }

    int ipaddress::to_cstr(char* dst, int maxCount) const noexcept
    {
        if (!Port) // default case is easy, just return address string with no port
            return Address.to_cstr(dst, maxCount);

        int addrLen = Address.to_cstr(dst, maxCount);
        if (addrLen > 0)
        {
            int len;
            // by default use IPv4 format for all output
            if (Address.Family != AF_IPv6) // "127.0.0.1:port"
            {
                len = Address.to_cstr(dst, maxCount);
            }
            else // AF_IPv6 [2001:db8::1]:8080
            {
                len = 0;
                dst[len++] = '[';
                len += Address.to_cstr(dst+len, maxCount-len);
                dst[len++] = ']';
            }
            dst[len++] = ':';
            len += _tostring(dst+len, Port);
            return len;
        }
        return 0;
    }

    std::string ipaddress::str() const noexcept
    {
        char buf[128];
        return std::string{buf, buf+to_cstr(buf, 128)};
    }

    const char* ipaddress::cstr() const noexcept
    {
        static char buf[128];
        (void)to_cstr(buf, 128);
        return buf;
    }

    bool ipaddress::equals(const ipaddress& ip) const noexcept
    {
        return Port == ip.Port && Address == ip.Address;
    }

    int ipaddress::compare(const ipaddress& ip) const noexcept
    {
        int c = Address.compare(ip.Address);
        if (c != 0) return c;
        // addresses are equal, compare by port
        if (Port < ip.Port) return -1;
        if (Port > ip.Port) return +1;
        return 0;
    }

    // Fills the AF_UNIX sockaddr_un from a unix name. Sets a.Len, or leaves it 0
    // (=> invalid/zero-length address) if the name doesn't fit, so callers fail
    // cleanly.
    static void fill_unix_addr(saddr& a, const char* name) noexcept
    {
        memset(&a.sun, 0, sizeof(a.sun));
        a.sun.sun_family = AF_UNIX;
        a.Len = 0; // assume failure
        if (!name || *name == '\0')
            return;
#if _WIN32
        // Windows: "%TEMP%\<name>.sock" (per-user temp dir, so no uid keying).
        char tmp[MAX_PATH];
        const DWORD n = GetTempPathA(sizeof(tmp), tmp); // ends with '\'
        if (n == 0 || n >= sizeof(tmp))
            return;
        const int len = snprintf(a.sun.sun_path, sizeof(a.sun.sun_path), "%s%s.sock", tmp, name);
        if (len < 0 || len >= static_cast<int>(sizeof(a.sun.sun_path)))
            return;
        a.Len = static_cast<int>(sizeof(a.sun));
#elif RPP_UNIX_SOCKET_SUPPORTED
        // Linux abstract namespace: leading NUL, no trailing terminator counted.
        const size_t nlen = strlen(name);
        if (nlen > sizeof(a.sun.sun_path) - 1) // leading NUL occupies sun_path[0]
            return; // name too long; never truncate
        memcpy(a.sun.sun_path + 1, name, nlen);
        a.Len = static_cast<int>(offsetof(sockaddr_un, sun_path) + 1 + nlen);
#else
        (void)name; // unsupported platform: leave Len == 0
#endif
    }

    static saddr to_saddr(const ipaddress& ipa) noexcept
    {
        saddr a;
        if (ipa.Address.Family == AF_Unix) {
            // The unix name is not stored in the address; AF_Unix sockets bind/
            // connect via unix_socket's own path. Return a zero-length sockaddr.
            fill_unix_addr(a, nullptr);
            return a;
        }
        a.sa4.sin_family = (uint16_t)addrfamily_int(ipa.Address.Family);
        a.sa4.sin_port = htons(ipa.Port);
        if (ipa.Address.Family == AF_IPv4) {
            a.sa4.sin_addr.s_addr = ipa.Address.Addr4;
            memset(a.sa4.sin_zero, 0, sizeof(a.sa4.sin_zero));
            a.Len = sizeof(a.sa4);
        } else { // AF_IPv6
            memcpy(&a.sa6.sin6_addr, ipa.Address.Addr6, sizeof(ipa.Address.Addr6));
            a.sa6.sin6_flowinfo = ipa.Address.FlowInfo;
            a.sa6.sin6_scope_id = ipa.Address.ScopeId;
            a.Len = sizeof(a.sa6);
        }
        return a;
    }

    static ipaddress to_ipaddress(const saddr& a) noexcept
    {
        ipaddress ipa { to_addrfamily(a.sa4.sin_family), (int)ntohs(a.sa4.sin_port) };
        if (ipa.Address.Family == AF_IPv4) {
            ipa.Address.Addr4 = a.sa4.sin_addr.s_addr;
        } else { // AF_IPv6
            memcpy(ipa.Address.Addr6, &a.sa6.sin6_addr, sizeof(ipa.Address.Addr6));
            ipa.Address.FlowInfo = a.sa6.sin6_flowinfo;
            ipa.Address.ScopeId  = a.sa6.sin6_scope_id;
        }
        return ipa;
    }


    ///////////////////////////////////////////////////////////////////////////
    ////////        IP Interfaces
    ///////////////////////////////////////////////////////////////////////////

    template<class ADAPTER_ADDRESS_T>
    ADAPTER_ADDRESS_T* get_first_address(int family, ADAPTER_ADDRESS_T* firstAddress) noexcept
    {
        for (auto addr = firstAddress; addr != nullptr; addr = addr->Next)
            if (!family || addr->Address.lpSockaddr->sa_family == family)
                return addr;
        return nullptr;
    }

    std::vector<ipinterface> ipinterface::get_interfaces(address_family af) noexcept
    {
        int family = addrfamily_int(af);
        std::vector<ipinterface> out;

    #if _WIN32
        InitWinSock();

        ULONG flags = GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS;
        ULONG bufLen = 0;
        GetAdaptersAddresses(family, flags, nullptr, nullptr, &bufLen);
        IP_ADAPTER_ADDRESSES* ipa_addrs = (IP_ADAPTER_ADDRESSES*)_malloca(bufLen);

        if (!GetAdaptersAddresses(family, flags, nullptr, ipa_addrs, &bufLen))
        {
            int count = 0;
            for (auto ipaa = ipa_addrs; ipaa != nullptr; ipaa = ipaa->Next)
                ++count;
            out.reserve(count);

            for (auto ipaa = ipa_addrs; ipaa != nullptr; ipaa = ipaa->Next)
            {
                out.emplace_back();
                ipinterface& in = out.back();
                in.name = to_string(ipaa->FriendlyName);

                // prepend DNS suffix to the name, eg "lan"
                std::string suffix = to_string(ipaa->DnsSuffix);
                if (!suffix.empty())
                    in.name = suffix + " " + in.name;

                if (IP_ADAPTER_UNICAST_ADDRESS* unicast = get_first_address(family, ipaa->FirstUnicastAddress))
                {
                    in.addr = to_ipaddress(*(saddr*)unicast->Address.lpSockaddr);
                    if (family == AF_INET6 && unicast->Address.lpSockaddr->sa_family == AF_INET6)
                    {
                        // TODO: ipv6 scopeid
                        // in.addr.Address.ScopeId = unicast->OnLinkPrefixLength;
                    }
                    else if (family == AF_INET || (!family && unicast->Address.lpSockaddr->sa_family == AF_INET))
                    {
                        in.netmask = in.addr;
                        ConvertLengthToIpv4Mask(unicast->OnLinkPrefixLength, &in.netmask.Address.Addr4);

                        // calculate broadcast address using the subnet mask
                        in.broadcast = in.addr;
                        in.broadcast.Address.Addr4 = (in.addr.Address.Addr4 & in.netmask.Address.Addr4) | ~in.netmask.Address.Addr4;
                    }
                }

                if (family == AF_INET6)
                {
                    // Multicast addresses are address groups identified by a single IP address
                    // This is used by IPv6 for broadcasting
                    if (auto* multicast = get_first_address(family, ipaa->FirstMulticastAddress))
                    {
                        in.broadcast = to_ipaddress(*(saddr*)multicast->Address.lpSockaddr);
                    }
                }

                if (ipaa->Flags & IP_ADAPTER_ADDRESS_DNS_ELIGIBLE)
                {
                    if (auto* gateway = get_first_address(family, ipaa->FirstGatewayAddress))
                    {
                        in.gateway = to_ipaddress(*(saddr*)gateway->Address.lpSockaddr);
                    }
                }
            }
        }
        _freea(ipa_addrs); // free the allocated memory
    #else
        ifaddrs* if_addrs;
        if (!getifaddrs(&if_addrs))
        {
            int count = 0;
            for (auto* ifa = if_addrs; (ifa && ifa->ifa_addr); ifa = ifa->ifa_next) {
                if (!family || ifa->ifa_addr->sa_family == family) ++count;
            }
            out.reserve(count);

            for (auto* ifa = if_addrs; (ifa && ifa->ifa_addr); ifa = ifa->ifa_next)
            {
                if (!family || ifa->ifa_addr->sa_family == family)
                {
                    ipinterface& in = out.emplace_back();
                    in.name = std::string{ifa->ifa_name};
                    in.addr = to_ipaddress(*(saddr*)ifa->ifa_addr);
                    if (ifa->ifa_netmask)
                        in.netmask = to_ipaddress(*(saddr*)ifa->ifa_netmask);
                    if ((ifa->ifa_flags & 0x2/*IFF_BROADCAST*/) != 0 && ifa->ifa_ifu.ifu_broadaddr)
                    {
                        in.broadcast = to_ipaddress(*(saddr*)ifa->ifa_ifu.ifu_broadaddr);
                    }
                    // TODO: gateway
                }
            }
            freeifaddrs(if_addrs);
        }
    #endif

        rpp::insertion_sort(out.data(), out.size(), &ipinterface::compare);
        return out;
    }

    bool ipinterface::compare(const ipinterface& a, const ipinterface& b) noexcept
    {
        // gateway should always be first
        if (a.gateway.has_address() && !b.gateway.has_address()) return true;
        if (b.gateway.has_address() && !a.gateway.has_address()) return false;
        return a.addr.compare(b.addr) < 0;
    }

    // does a simple pattern match by splitting on certain characters like "|"
    static int pattern_match(rpp::strview haystack, rpp::strview patterns)
    {
        rpp::strview pattern;
        // TODO: support more patterns like specific wildcards etc
        while (patterns.next(pattern, '|'))
        {
            if (const char* match = haystack.find(pattern))
            {
                int index = int(match - haystack.begin());
                return index;
            }
        }
        return -1; // no match
    }

    std::vector<ipinterface> ipinterface::get_interfaces(const std::string& name_match, address_family af) noexcept
    {
        std::vector<ipinterface> out = get_interfaces(af);
        if (!name_match.empty())
        {
            rpp::insertion_sort(out.data(), out.size(), [&](const ipinterface& a, const ipinterface& b) noexcept
            {
                int apos = pattern_match(a.name, name_match);
                int bpos = pattern_match(b.name, name_match);
                if (apos != -1 && bpos == -1) return true; // match is always first
                if (bpos != -1 && apos == -1) {
                    return false; // non match is always last
                }
                // both match, or none match, use default sorting rule:
                return ipinterface::compare(a, b);
            });
        }
        return out;
    }

    ///////////////////////////////////////////////////////////////////////////
    /////////        socket
    ///////////////////////////////////////////////////////////////////////////

    socket::socket() noexcept
        : Sock{INVALID}
        , LastErr{0}
        , Shared{DEFAULT_SHARED}
        , Blocking{DEFAULT_BLOCKING}
        , AutoClose{DEFAULT_AUTOCLOSE}
        , Connected{false}
        , Category{SC_Unknown}
        , Type{ST_Unspecified}
    {
    }
    socket socket::from_os_handle(int handle, const ipaddress& addr, bool shared, bool blocking) noexcept
    {
        socket s{}; // set the defaults
        s.set_os_handle_unsafe(handle);
        s.Addr = addr;
        s.Shared = shared;
        s.Blocking = blocking;
        s.Category = SC_Unknown;
        return s;
    }
    socket socket::from_err_code(int last_err, const ipaddress& addr) noexcept
    {
        socket s{};
        s.Addr = addr;
        s.set_errno_unlocked(last_err);
        return s;
    }
    socket::socket(socket&& s) noexcept : socket{}/*create with defaults*/
    {
        this->operator=(std::move(s)); // reuse move operator
    }
    socket& socket::operator=(socket&& s) noexcept
    {
        // lock both mutexes, but do not swap them
        std::scoped_lock lock { Mtx, s.Mtx };
        std::swap(Sock, s.Sock);
        std::swap(Addr, s.Addr);
        std::swap(LastErr, s.LastErr);
        std::swap(Shared, s.Shared);
        std::swap(Blocking, s.Blocking);
        std::swap(AutoClose, s.AutoClose);
        std::swap(Category, s.Category);
        std::swap(Type, s.Type);
        std::swap(Connected, s.Connected);
        return *this;
    }
    socket::~socket() noexcept
    {
        close();
    }

    void socket::close() noexcept
    {
        std::unique_lock lock { Mtx };
        int sock = os_handle_unsafe();
        if (sock != -1)
        {
            // BUGFIX: need to reset the socket state immediately, otherwise
            //         ::shutdown() can wake up a blocking thread and try to
            //         also call close()+shutdown()
            set_os_handle_unsafe(-1);
            Type = ST_Unspecified;
            Category = SC_Unknown;
            Connected = false;
            bool shared = Shared;

            // Release the lock here, to unblock any other threads that might
            // be woken by shutdown()/closesocket() and try to call socket::close()
            lock.unlock();

            if (!shared)
            {
                ::shutdown(sock, /*SHUT_RDWR*/2); // send FIN
                closesocket(sock);
            }
        }
        // dont clear the address, so we have info on what we just closed
    }

    int socket::release_noclose() noexcept
    {
        std::scoped_lock lock { Mtx };
        int sock = os_handle_unsafe();
        set_os_handle_unsafe(-1);
        return sock;
    }

    int socket::send(const void* buffer, int numBytes) noexcept
    {
        if (numBytes <= 0) // important! ignore 0-byte I/O, handle_txres cant handle it
            return 0;

        if (is_af_unix())
        {
            // AF_Unix is message-oriented: reject oversize messages on every
            // platform (the Windows length prefix is 16-bit, so an oversize
            // frame would wrap and desync the peer). On Windows AF_Unix sockets
            // are unix_socket instances whose override frames the payload; this
            // base path is the POSIX SEQPACKET case.
            if (numBytes > MAX_UNIX_MESSAGE_SIZE)
            {
                set_errno(ESOCK(EMSGSIZE));
                return -1;
            }
        }

        int flags = 0;
        #if __linux__
            // Linux triggers SIGPIPE on write to a closed socket, which terminates the application
            // Since we're running in a multithreaded environment, we MUST ignore it
            flags = MSG_NOSIGNAL;
        #endif
        std::unique_lock lock = rpp::spin_lock(Mtx);
        return handle_txres(::send(os_handle_unsafe(), (const char*)buffer, numBytes, flags));
    }
    int socket::send(const char* str)    noexcept { return send(str, (int)strlen(str)); }
    int socket::send(const wchar_t* str) noexcept { return send(str, int(sizeof(wchar_t) * wcslen(str))); }


    int socket::sendto(const ipaddress& to, const void* buffer, int numBytes) noexcept
    {
        Assert(type() == ST_Datagram, "sendto only works on UDP sockets");

        if (numBytes <= 0) // important! ignore 0-byte I/O, handle_txres cant handle it
            return 0;

        saddr a = to_saddr(to);
        socklen_t len = sizeof(a);
        std::unique_lock lock = rpp::spin_lock(Mtx);
        return handle_txres(::sendto(os_handle_unsafe(), (const char*)buffer, numBytes, 0, &a.sa, len));
    }
    int socket::sendto(const ipaddress& to, const char* str)    noexcept { return sendto(to, str, (int)strlen(str)); }
    int socket::sendto(const ipaddress& to, const wchar_t* str) noexcept { return sendto(to, str, int(sizeof(wchar_t) * wcslen(str))); }


    void socket::flush() noexcept
    {
        std::unique_lock lock = rpp::spin_lock(Mtx);
        flush_send_buf();
        flush_recv_buf();
    }

    void socket::flush_send_buf() noexcept
    {
        if (type() == ST_Stream && !is_af_unix()) // TCP only, never AF_Unix
        {
            std::unique_lock lock = rpp::spin_lock(Mtx);
            // flush write buffer (hack only available for TCP sockets)
            bool nagleEnabled = !is_nodelay();
            if (nagleEnabled)
                set_nagle(false); // disable nagle
            set_nagle(nagleEnabled); // this must be called at least once
        }
    }

    void socket::flush_recv_buf() noexcept
    {
        std::unique_lock lock = rpp::spin_lock(Mtx);
        if (type() == ST_Stream)
        {
            skip(available());
        }
        else
        {
        #if _WIN32
            // On WINSOCK, this skips the total available bytes in recv buffer
            skip(available());
        #else
            // On LINUX we need to dump all datagrams one-by-one
            const int MAX_DATAGRAMS = 1000; // limit number of datagrams to flush
            for (int i = 0; i < MAX_DATAGRAMS; ++i)
            {
                if (skip(available()) <= 0)
                    break;
            }
        #endif
        }
    }

    int socket::skip(int bytesToSkip) noexcept
    {
        if (bytesToSkip <= 0)
            return 0;

        std::unique_lock lock = rpp::spin_lock(Mtx);
        int remaining = bytesToSkip;
        int skipped = 0;
        char dump[4096];

        if (type() == ST_Stream)
        {
            while (skipped < bytesToSkip)
            {
                int len = recv(dump, rpp::min<int>(sizeof(dump), remaining));
                if (len <= 0)
                    break;
                skipped += len;
                remaining -= len;
            }
        }
        else
        {
            ipaddress from;
            while (skipped < bytesToSkip)
            {
                // datagram flush is much more complicated because WINSOCK and LINUX
                // sockets report available() differently
                // WINSOCK: total number of available bytes in recv buffer
                // LINUX: current datagram size
                int avail = available();
                if (avail <= 0)
                    break;

                //logdebug("CHECK available %d", avail);

                int max = rpp::min<int>(sizeof(dump), remaining);
                int len = recvfrom(from, dump, max);
                if (len < 0)
                {
                    //logdebug("EOF skipped %d/%d", skipped, bytesToSkip);
                    break;
                }

                if (len == 0) // UDP packet was probably truncated, get available() again
                {
                #if _WIN32 // on WINSOCK, we see the total available bytes
                    int after = available();
                    if (after <= 0) // at this point we can't really know the exact N
                    {
                        //logdebug("TRUNC EOF skipped %d/%d", skipped+max, bytesToSkip);
                        skipped += max;
                        break;
                    }

                    len = rpp::max<int>(0, avail - after);
                #else
                    len = avail; // on LINUX, sockets report available() per datagram
                #endif
                    //logdebug("TRUNC max: skipped %d/%d", skipped+len, bytesToSkip);
                    skipped   += len;
                    remaining -= len;
                }
                else
                {
                    //logdebug("OK %d skipped %d/%d", len, skipped+len, bytesToSkip);
                    skipped   += len;
                    remaining -= len;
                }
            }
        }
        return skipped;
    }

    int socket::available() const noexcept
    {
        int bytesAvail;
    #if MIPS
        // TODO: this is a hack to get available() working on MIPS
        //       later on it should be fixed in the kernel
        return get_ioctl(0x467F, bytesAvail) ? -1 : bytesAvail;
    #else
        return get_ioctl(FIONREAD, bytesAvail) ? -1 : bytesAvail;
    #endif
    }

    int socket::peek_datagram_size() noexcept // NOLINT: readability-make-member-function-const
    {
    #if !_WIN32
        return available();
    #else
        char buffer[4096];
        return peek(buffer, sizeof(buffer));
    #endif
    }

    int socket::recv(void* buffer, int maxBytes) noexcept
    {
        if (maxBytes <= 0) // important! ignore 0-byte I/O, handle_txres cant handle it
            return 0;
        std::unique_lock lock { Mtx };
        int sock = os_handle_unsafe();
        // we can't keep the mutex locked if we're entering a blocking operation
        if (Blocking) lock.unlock();
        return handle_txres(::recv(sock, (char*)buffer, maxBytes, 0)); // NOLINT(clang-analyzer-unix.BlockInCriticalSection)
    }

    int socket::peek(void* buffer, int numBytes) noexcept
    {
        if (numBytes <= 0) // important! ignore 0-byte I/O, handle_txres cant handle it
            return 0;

        std::unique_lock lock = rpp::spin_lock(Mtx);
        // if the socket is blocking, then MSG_PEEK can cause a blocking operation
        // which goes against rpp::socket::peek() API specification.
        // So we use poll() to detect whether the socket is readable first
        if (Blocking && !poll(0, PF_Read))
            return 0; // socket is not readable

        if (type() == ST_Stream)
        {
            // NOTE: if ::recv() returns 0, then the socket was closed gracefully
            return handle_txres(::recv(os_handle_unsafe(), (char*)buffer, numBytes, MSG_PEEK));
        }
        else
        {
            saddr a;
            socklen_t len = sizeof(a);
            return handle_txres(::recvfrom(os_handle_unsafe(), (char*)buffer, numBytes, MSG_PEEK, &a.sa, &len));
        }
    }

    NOINLINE int socket::recvfrom(ipaddress& from, void* buffer, int maxBytes) noexcept
    {
        lock_t lock = rpp::spin_lock(Mtx);
        return recvfrom(lock, from, buffer, maxBytes);
    }

    NOINLINE int socket::recvfrom(lock_t& lock, ipaddress& from, void* buffer, int maxBytes) noexcept
    {
        Assert(type() == ST_Datagram, "recvfrom only works on UDP sockets");
        Assert(lock.owns_lock(), "recvfrom requires a locked mutex");
        Assert(lock.mutex() == &Mtx, "recvfrom requires a lock on the actual socket mutex");

        if (maxBytes <= 0) // important! ignore 0-byte I/O, handle_txres cant handle it
            return 0;

        saddr a;
        socklen_t len = sizeof(a);
        int sock = os_handle_unsafe();
        // we can't keep the mutex locked if we're entering a blocking operation
        bool unlocked = false;
        if (Blocking)
        {
            lock.unlock();
            unlocked = true;
        }
        int res = handle_txres(::recvfrom(sock, (char*)buffer, maxBytes, 0, &a.sa, &len));
        if (unlocked) lock.lock(); // re-lock the mutex if we unlocked it before
        if (res > 0) from = to_ipaddress(a);
        return res;
    }

    bool socket::recv(std::vector<uint8_t>& outBuffer)
    {
        int count = available();
        if (count <= 0)
            return false;
        outBuffer.resize(count);
        int n = this->recv(outBuffer.data(), (int)outBuffer.size());
        if (n >= 0 && n != count)
            outBuffer.resize(n);
        return n > 0;
    }

    bool socket::recvfrom(ipaddress& from, std::vector<uint8_t>& outBuffer)
    {
        int count = available();
        if (count <= 0)
            return false;
        outBuffer.resize(count);
        int n = this->recvfrom(from, outBuffer.data(), (int)outBuffer.size());
        if (n >= 0 && n != count)
            outBuffer.resize(n);
        return n > 0;
    }

    static void os_setsockerr(int err) noexcept
    {
        #if _WIN32
            WSASetLastError(err);
        #else
            errno = err;
        #endif
    }

    static int os_getsockerr() noexcept
    {
        #if _WIN32
            return WSAGetLastError();
        #else
            return errno;
        #endif
    }

    // PUBLIC -- for external use if regular error API is not enough
    int socket::last_errno() const noexcept
    {
        std::unique_lock lock = rpp::spin_lock(Mtx);
        int err = get_errno_unlocked();
        return err ? err : set_errno_unlocked(os_getsockerr());
    }
    void socket::set_errno(int err) const noexcept
    {
        std::unique_lock lock = rpp::spin_lock(Mtx);
        LastErr = err;
    }
    int socket::get_errno() const noexcept
    {
        std::unique_lock lock = rpp::spin_lock(Mtx);
        return LastErr;
    }

    // properly handles the crazy responses given by the recv() and send() functions
    // returns -1 on critical failure, otherwise it returns bytesAvailable (0...N)
    int socket::handle_txres(long ret) noexcept
    {
        if (ret == 0) // socket closed gracefully
        {
            set_errno(os_getsockerr());
            if (Type == ST_Stream || Type == ST_SeqPacket)
            {
                logdebug("socket closed gracefully");
                close();
            }
            return -1;
        }
        else if (ret == -1) // socket error?
        {
            return handle_errno();
        }
        set_errno(0); // clear any errors
        return (int)ret; // return as bytesAvailable
    }

    int socket::handle_errno(int err) noexcept
    {
        std::unique_lock lock = rpp::spin_lock(Mtx); // mtx lock does not affect errno
        int errcode = set_errno_unlocked(err ? err : os_getsockerr());
        switch (errcode) {
            case 0: return 0; // no error
            default: {
                logerror("socket fh:%d %s", os_handle_unsafe(), last_os_socket_err(errcode).c_str());
                if (AutoClose) close(); else Connected = false;
                os_setsockerr(errcode); // store the errcode after close() so that application can inspect it
                return -1;
            }
            // The connection has been broken due to keep-alive activity detecting a failure while the operation was in progress.
            case ESOCK(ENETRESET):     // ignore net reset errors
            case ESOCK(EMSGSIZE):      // message too large to fit into buffer and was truncated
            case ESOCK(EINPROGRESS):   // request is in progress, you should call wait
            case ESOCK(EWOULDBLOCK):   // no data available right now
            #if EAGAIN != ESOCK(EWOULDBLOCK)
            case EAGAIN:               // no data available right now
            #endif
            case ESOCK(ENOTCONN):      // this Socket is not Connection oriented! (aka LISTEN SOCKET)
            case ESOCK(EADDRNOTAVAIL): // address doesn't exist
            case ESOCK(ENETUNREACH):   // network is unreachable
            case ESOCK(EISCONN):       // socket is already connected
            case ESOCK(EINTR):         // the operation was interrupted
                return 0; // all of these are not fatal errors, so we return 0
            // Cannot send after transport endpoint shutdown
            // but do not close the socket, it may have data available to read
            case ESOCK(ESHUTDOWN): {
                Connected = false;
                return 0;
            }
            case ESOCK(EADDRINUSE): {
                logerror("socket fh:%d EADDRINUSE %s", os_handle_unsafe(), last_os_socket_err(errcode).c_str());
                if (AutoClose) close(); else Connected = false;
                os_setsockerr(errcode); // store the errcode after close() so that application can inspect it
                return -1;
            }
            case ESOCK(EBADF):         // invalid socket
            case ESOCK(EFAULT):        // invalid address
            case ESOCK(EPROTOTYPE):    // invalid socket arguments
            case ESOCK(EPROTONOSUPPORT): // invalid protocol
            case ESOCK(EAFNOSUPPORT):  // invalid address family
            case ESOCK(ECONNRESET):    // connection lost
            case ESOCK(ECONNREFUSED):  // connect failed
            case ESOCK(ECONNABORTED):  // connection closed
            case ESOCK(ETIMEDOUT):     // remote end did not respond
            case ESOCK(EHOSTUNREACH):  // no route to host
                if (AutoClose) close(); else Connected = false;
                os_setsockerr(errcode); // store the errcode after close() so that application can inspect it
                return -1;
        }
    }

    std::string socket::last_err() const noexcept
    {
        return last_os_socket_err(get_errno());
    }

    socket::error socket::last_err_type() const noexcept
    {
        return last_os_socket_err_type(get_errno());
    }

    int socket::get_socket_level_error() const noexcept
    {
        return get_opt(SOL_SOCKET, SO_ERROR);
    }

    std::string socket::last_os_socket_err(int err) noexcept
    {
        char buf[2048];
        int errcode = err ? err : os_getsockerr();
        if (errcode == 0)
            return {};

        // if it's a known error, use a common shorter message:
        socket::error errtype = last_os_socket_err_type(errcode);
        if (errtype > 0)
            return to_string(errtype);

    // otherwise use a more detailed and localized error message from the OS:
    #if _WIN32
        char* msg = buf + 1024;
        int len = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, errcode, 0, msg, 1024, nullptr);
        if (msg[len - 2] == '\r') msg[len -= 2] = '\0';
    #else
        char* msg = strerror(errcode);
    #endif

        int errlen = snprintf(buf, sizeof(buf), "OSError %d: %s", errcode, msg);
        if (errlen < 0) errlen = (int)strlen(buf);
        return std::string{ buf, buf + errlen };
    }

    socket::error socket::last_os_socket_err_type(int err) noexcept
    {
        // NOLINTBEGIN(bugprone-branch-clone)
        int errcode = err ? err : os_getsockerr();
        switch (errcode) {
            case 0: return SE_NONE;
            default: return SE_UNKNOWN; // unknown error, not in the list below
            case ESOCK(ENETRESET):     return SE_NETRESET;
            case ESOCK(EMSGSIZE):      return SE_MSGSIZE;
            case ESOCK(EINPROGRESS):   return SE_INPROGRESS;
            case ESOCK(EWOULDBLOCK):   return SE_AGAIN;
            #if EAGAIN != ESOCK(EWOULDBLOCK) // legacy alias to EWOULDBLOCK
            case EAGAIN:               return SE_AGAIN;
            #endif
            case ESOCK(ENOTCONN):      return SE_NOTCONN;
            case ESOCK(EADDRNOTAVAIL): return SE_ADDRNOTAVAIL;
            case ESOCK(EADDRINUSE):    return SE_ADDRINUSE;
            case ESOCK(ECONNRESET):    return SE_CONNRESET;
            case ESOCK(ECONNREFUSED):  return SE_CONNREFUSED;
            case ESOCK(ECONNABORTED):  return SE_CONNABORTED;
            case ESOCK(ETIMEDOUT):     return SE_TIMEDOUT;
            case ESOCK(EHOSTUNREACH):  return SE_HOSTUNREACH;
            case ESOCK(ENETUNREACH):   return SE_NETUNREACH;
            case ESOCK(EBADF):         return SE_BADSOCKET;
            case ESOCK(ENOTSOCK):      return SE_BADSOCKET;
            case ESOCK(EISCONN):       return SE_ALREADYCONN;
            case ESOCK(EFAULT):        return SE_ADDRFAULT;
            case ESOCK(EINTR):         return SE_INTERRUPTED;
            case ESOCK(EPROTOTYPE):    return SE_SOCKTYPE;
            case ESOCK(EPROTONOSUPPORT): return SE_SOCKTYPE;
            case ESOCK(EAFNOSUPPORT):  return SE_SOCKFAMILY;
            case ESOCK(ESHUTDOWN):     return SE_SHUTDOWN;
        }
        // NOLINTEND(bugprone-branch-clone)
    }

    std::string socket::to_string(socket::error socket_error) noexcept
    {
        switch (socket_error) {
            default: return "unknown error (" + std::to_string((int)socket_error) + ")";
            case SE_UNKNOWN:      return "unknown error";
            case SE_NONE:         return "no error";
            case SE_NETRESET:     return "network reset (ENETRESET)";
            case SE_MSGSIZE:      return "message too large (EMSGSIZE)";
            case SE_INPROGRESS:   return "operation in progress (EINPROGRESS)";
            case SE_AGAIN:        return "no data available (EAGAIN)";
            case SE_NOTCONN:      return "not connected (ENOTCONN)";
            case SE_ADDRNOTAVAIL: return "address not available (EADDRNOTAVAIL)";
            case SE_ADDRINUSE:    return "address in use (EADDRINUSE)";
            case SE_CONNRESET:    return "connection reset (ECONNRESET)";
            case SE_CONNREFUSED:  return "connection refused (ECONNREFUSED)";
            case SE_CONNABORTED:  return "connection aborted (ECONNABORTED)";
            case SE_TIMEDOUT:     return "timed out (ETIMEDOUT)";
            case SE_HOSTUNREACH:  return "host unreachable (EHOSTUNREACH)";
            case SE_NETUNREACH:   return "network unreachable (ENETUNREACH)";
            case SE_BADSOCKET:    return "bad socket (EBADF)";
            case SE_ALREADYCONN:  return "already connected (EISCONN)";
            case SE_ADDRFAULT:    return "invalid address (EFAULT)";
            case SE_INTERRUPTED:  return "operation interrupted (EINTR)";
            case SE_SOCKTYPE:     return "invalid socket family and proto (EPROTOTYPE)";
            case SE_SOCKFAMILY:   return "unsupported socket family (EAFNOSUPPORT)";
            case SE_SHUTDOWN:     return "socket was shut down (ESHUTDOWN)";
        }
    }

    std::string socket::peek_str(int maxCount) noexcept
    {
        int count = available();
        int n = count < maxCount ? count : maxCount;
        if (n <= 0) return {};

        std::string cont; cont.resize(n);
        int received = peek((void*)cont.data(), n);
        if (received <= 0) return {};

        // even though we had N+ bytes available, a single packet might be smaller
        if (received < n) cont.resize(received); // truncate
        return cont;
    }

    bool socket::wait_available(int millis) noexcept
    {
        if (!connected() || !poll(millis, PF_Read))
            return false;
        return available() > 0;
    }

    int socket::get_opt(int optlevel, int socketopt) const noexcept
    {
        int value = 0; socklen_t len = sizeof(int);
        std::unique_lock lock = rpp::spin_lock(Mtx);
        bool ok = getsockopt(os_handle_unsafe(), optlevel, socketopt, (char*)&value, &len) == 0;
        set_errno_unlocked(ok ? 0 : os_getsockerr());
        return ok ? value : -1;
    }
    int socket::set_opt(int optlevel, int socketopt, int value) noexcept
    {
        std::unique_lock lock = rpp::spin_lock(Mtx);
        bool ok = setsockopt(os_handle_unsafe(), optlevel, socketopt, (char*)&value, sizeof(int)) == 0;
        return set_errno_unlocked(ok ? 0 : os_getsockerr());
    }
    int socket::set_opt(int optlevel, int socketopt, void* value, int value_size) noexcept
    {
        std::unique_lock lock = rpp::spin_lock(Mtx);
        bool ok = setsockopt(os_handle_unsafe(), optlevel, socketopt, (char*)value, value_size) == 0;
        return set_errno_unlocked(ok ? 0 : os_getsockerr());
    }

#if RPP_SOCKETS_DBG
    static const char* ioctl_string(int iocmd)
    {
        switch (iocmd)
        {
            case FIONREAD: return "FIONREAD";
            case FIONBIO:  return "FIONBIO";
            case FIOASYNC: return "FIOASYNC";
            default: {
                static char buf[32];
                snprintf(buf, sizeof(buf), "%d", iocmd);
                return buf;
            }
        }
    }
#endif

    int socket::get_ioctl(int iocmd, int& outValue) const noexcept
    {
        std::unique_lock lock = rpp::spin_lock(Mtx);
        set_errno_unlocked(0);
    #if _WIN32 // on win32, ioctlsocket SETS FIONBIO, so we need this little helper
        if (iocmd == FIONBIO)
        {
            outValue = Blocking ? 0 : 1; // FIONBIO: !=0 nonblock, 0 block
            return 0;
        }
        u_long val = 0;
        if (ioctlsocket(os_handle_unsafe(), iocmd, &val) == 0)
        {
            outValue = (int)val;
            return 0;
        }
    #else
        if (::ioctl(os_handle_unsafe(), iocmd, &outValue) == 0)
            return 0;
    #endif
        int err = set_errno_unlocked(os_getsockerr());
        logerronce(err, "(%s) failed: %s", ioctl_string(iocmd), last_err().c_str());
        return err;
    }

    int socket::set_ioctl(int iocmd, int value) noexcept
    {
        std::unique_lock lock = rpp::spin_lock(Mtx);
        set_errno_unlocked(0);
    #if _WIN32
        u_long val = value;
        if (ioctlsocket(os_handle_unsafe(), iocmd, &val) == 0)
            return 0;
    #else
        if (::ioctl(os_handle_unsafe(), iocmd, &value) == 0)
            return 0;
    #endif
        return set_errno_unlocked(os_getsockerr());
    }

    bool socket::enable_broadcast() noexcept
    {
        bool success = set_opt(SOL_SOCKET, SO_BROADCAST, true) == 0;
        if (!success)
            LogError("setsockopt SO_BROADCAST TRUE failed: %s\n", last_err());
        return success;
    }

    bool socket::enable_multicast(rpp::ipaddress4 multicast_group, int ttl) noexcept
    {
        if (Type != ST_Datagram)
            return false; // multicast only works on datagram sockets

        struct ip_mreq group = {};
        group.imr_multiaddr = to_saddr(multicast_group).sa4.sin_addr;
        group.imr_interface.s_addr = htonl(INADDR_ANY); // bind to all interfaces

        bool success = set_opt(IPPROTO_IP, IP_ADD_MEMBERSHIP, &group, sizeof(group)) == 0;
        if (!success)
        {
            LogError("setsockopt IP_ADD_MEMBERSHIP failed: %s\n", last_err());
            return false;
        }
        success = set_opt(IPPROTO_IP, IP_MULTICAST_TTL, ttl) == 0;
        if (!success)
        {
            LogError("setsockopt IP_MULTICAST_TTL failed: %s\n", last_err());
            return false;
        }
        return true;
    }

    void socket::set_noblock_nodelay() noexcept
    {
        set_blocking(false); // blocking: false
        if (type() == ST_Stream)
            set_nagle(false);    // nagle:    false
    }

    bool socket::set_blocking(bool socketsBlock) noexcept
    {
        std::unique_lock lock = rpp::spin_lock(Mtx);
    #if _WIN32
        u_long val = socketsBlock?0:1; // FIONBIO: !=0 nonblock, 0 block
        if (ioctlsocket(os_handle_unsafe(), FIONBIO, &val) == 0)
        {
            Blocking = socketsBlock; // On Windows, there is no way to GET FIONBIO without setting it
            return true;
        }
    #else
        int flags = fcntl(os_handle_unsafe(), F_GETFL, 0);
        if (flags < 0) flags = 0;

        flags = socketsBlock ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
        if (fcntl(os_handle_unsafe(), F_SETFL, flags) == 0)
        {
            Blocking = socketsBlock;
            return true;
        }
    #endif
        logerror("set_blocking(%s) failed: %s", socketsBlock?"true":"false", last_err().c_str());
        return false;
    }

    bool socket::is_blocking() const noexcept
    {
        std::unique_lock lock = rpp::spin_lock(Mtx);
    #if _WIN32
        return Blocking; // On Windows, there is no way to GET FIONBIO without setting it
    #else
        int flags = fcntl(os_handle_unsafe(), F_GETFL, 0);
        if (flags < 0) return false;
        bool nonBlocking = (flags & O_NONBLOCK) != 0;
        return !nonBlocking;
    #endif
    }

    bool socket::set_nagle(bool enableNagle) noexcept
    {
        if (type() != ST_Stream || is_af_unix())
            return false; // cannot set nagle (TCP only, never AF_Unix)

        // TCP_NODELAY: 1 nodelay, 0 nagle enabled
        if (set_opt(IPPROTO_TCP, TCP_NODELAY, enableNagle?0:1) == 0)
            return true;
        logerror("set_nagle(%s) failed: %s", enableNagle?"true":"false", last_err().c_str());
        return false;
    }
    bool socket::is_nodelay() const noexcept
    {
        if (type() != ST_Stream || is_af_unix())
            return true; // non-TCP (incl AF_Unix) are assumed to have nagle off

        int result = get_opt(IPPROTO_TCP, TCP_NODELAY);
        if (result < 0)
            return false;
        return result == 1; // TCP_NODELAY: 1 nodelay, 0 nagle enabled
    }

    bool socket::set_buf_size(buffer_option opt, size_t size, [[maybe_unused]] bool force) noexcept
    {
        int which = (opt == BO_Recv ? SO_RCVBUF : SO_SNDBUF);
        int command = which;
        #if __linux__
            // NOTE: on linux the kernel doubles buffsize for internal bookkeeping
            //       so to keep things consistent between platforms, we divide by 2 on linux:
            int size_cmd = static_cast<int>(size / 2);
            if (force) command = (opt == BO_Recv ? SO_RCVBUFFORCE : SO_SNDBUFFORCE);
        #else
            int size_cmd = static_cast<int>(size);
        #endif
        std::unique_lock lock = rpp::spin_lock(Mtx);
        if (set_opt(SOL_SOCKET, command, size_cmd) != 0)
            return false;
        return get_opt(SOL_SOCKET, which) == static_cast<int>(size);
    }

    /** @return Receiver/Send buffer size */
    int socket::get_buf_size(buffer_option opt) const noexcept
    {
        int n = get_opt(SOL_SOCKET, opt == BO_Recv ? SO_RCVBUF : SO_SNDBUF);
        return n >= 0 ? n : 0;
    }

    int socket::get_send_buffer_remaining() const noexcept
    {
        #ifdef SIOCOUTQ
            int outQueueSize; // (not sent + not acked)
            if (get_ioctl(SIOCOUTQ, outQueueSize) == 0/*OK*/)
            {
                int sndBufSize = get_snd_buf_size();
                return (sndBufSize > outQueueSize) ? (sndBufSize - outQueueSize) : 0;
            }
        #endif
        return -1; // unknown
    }

    bool socket::set_linger(bool active, int seconds) noexcept
    {
        struct linger l;
        l.l_onoff = active ? 1 : 0;
        l.l_linger = seconds;
        std::unique_lock lock = rpp::spin_lock(Mtx);
        bool ok = setsockopt(os_handle_unsafe(), SOL_SOCKET, SO_LINGER, (char*)&l, sizeof(l)) == 0;
        set_errno_unlocked(ok ? 0 : os_getsockerr());
        return ok;
    }

    ////////////////////////////////////////////////////////////////////////////////

    socket_type socket::update_socket_type() noexcept // called during construction from handle
    {
        int so_type = get_opt(SOL_SOCKET, SO_TYPE);
        Type = to_socktype(so_type);
        if (Type == ST_Unspecified) {
            logerror("socket fh:%d SOL_SOCKET SO_TYPE:%d lasterr:%s\n",
                      os_handle_unsafe(), so_type, last_err().c_str());
        }
        return Type;
    }
    address_family socket::family() const noexcept {
        return Addr.Address.Family;
    }
    ip_protocol socket::ipproto() const noexcept
    {
        #ifdef _WIN32
            WSAPROTOCOL_INFOW winf = { 0 };
            int len = sizeof(winf);
            std::unique_lock lock = rpp::spin_lock(Mtx);
            bool ok = getsockopt(os_handle_unsafe(), SOL_SOCKET, SO_PROTOCOL_INFOW, (char*)&winf, &len) == 0;
            set_errno_unlocked(ok ? 0 : os_getsockerr());
            return to_ipproto(winf.iProtocol);
        #else // this implementation is incomplete:
            switch (get_opt(SOL_SOCKET, SO_TYPE)) {
            default:          return IPP_DontCare;
            case SOCK_STREAM: return IPP_TCP; // assume TCP... might actually be something else
            case SOCK_DGRAM:  return IPP_UDP;
            }
        #endif
    }


    protocol_info socket::protocol() const noexcept
    {
        #ifdef _WIN32
            WSAPROTOCOL_INFOW winf = { 0 };
            int len = sizeof(winf);
            std::unique_lock lock = rpp::spin_lock(Mtx);
            bool ok = getsockopt(os_handle_unsafe(), SOL_SOCKET, SO_PROTOCOL_INFOW, (char*)&winf, &len) == 0;
            set_errno_unlocked(ok ? 0 : os_getsockerr());
            return protocol_info {
                winf.iProtocol,
                to_addrfamily(winf.iAddressFamily),
                to_socktype(winf.iSocketType),
                to_ipproto(winf.iProtocol),
            };
        #else // this implementation is incomplete:
            int t = get_opt(SOL_SOCKET, SO_TYPE);
            return protocol_info { t, family(), type(), ipproto() };
        #endif
    }

    ////////////////////////////////////////////////////////////////////////////////

    bool socket::connected() noexcept
    {
        // try to lock, but don't deadlock if another thread is doing an atomic
        // recv on the socket.
        std::unique_lock lock1 = rpp::spin_lock(Mtx);
        int sock = os_handle_unsafe();
        if (lock1.owns_lock())
            lock1.unlock(); // suppress warning
        if (sock == INVALID)
            return false;

        // this only catches the most severe errors on the socket
        int err = get_socket_level_error();
        if (err != 0)
        {
            if (handle_errno(err > 0 ? err : 0) == 0)
            {
                // After some of the errors socket might still be considered valid,
                // but the connection is lost
                return Connected;
            }
            return false; // it was a fatal error
        }

        if (Type == ST_Datagram)
            return true; // UDP is always connected (no connection state to check)

        if (Category == SC_Listen)
            return true; // Listening sockets are always connected

        // Checking for connection status only makes sense for connection oriented sockets.
        // This only applies for Accepted or Client sockets.
        // To check if a TCP socket is still connected we use a nonblocking read.
        // In the case of Blocking sockets, an additional poll() is done.
        if (Category == SC_Accept || Category == SC_Client) // Accept & Client implies TCP
        {
            // if it was already marked as disconnected, it cannot recover again
            // without a new connection being made
            if (!Connected)
                return false;

            // if the socket is blocking, then MSG_PEEK can cause a blocking operation.
            // So we use poll() to detect whether the socket is readable first.
            // A subsequent call to `recv()` will return 0, indicating a graceful close.
        #if _WIN32 || _WIN64
            struct pollfd pfd = { SOCKET(sock), POLLRDNORM, 0 };
            int poll_r = WSAPoll(&pfd, 1, 0);
        #else
            struct pollfd pfd = { sock, POLLRDNORM, 0 };
            int poll_r = ::poll(&pfd, 1, 0);
        #endif
            if (poll_r > 0) // data is available to read
            {
                char c;
                // handle_txres() should take care of the status
                // and set the Connected flag properly
                std::unique_lock lock2 = rpp::spin_lock(Mtx);
                handle_txres(::recv(sock, &c, 1, MSG_PEEK));
            }
            // if poll doesn't trigger, we rely on the Connected flag
            return Connected;
        }

        // an unknown socket category, assume it's NOT connection oriented
        return false;
    }

    ////////////////////////////////////////////////////////////////////////////////

    bool socket::create(address_family af, ip_protocol ipp, socket_option opt) noexcept
    {
        std::unique_lock lock = rpp::spin_lock(Mtx);
        inwin32(InitWinSock());
        close();

        int family = addrfamily_int(af);
        int type = 0;
        int proto = 0;
        socket_type stype = ST_Unspecified;

        if (af == AF_Unix)
        {
        #if RPP_UNIX_SOCKET_SUPPORTED
            // Linux: SOCK_SEQPACKET (message boundaries). Windows: SOCK_STREAM
            // (no SEQPACKET; messages are framed in software). proto 0.
        #if _WIN32
            type = SOCK_STREAM;  stype = ST_Stream;
        #else
            type = SOCK_SEQPACKET; stype = ST_SeqPacket;
        #endif
            proto = 0;
        #else
            // Unsupported platform: refuse to pretend support we can't deliver.
            handle_errno(ESOCK(EAFNOSUPPORT));
            return false;
        #endif
        }
        else
        {
            stype = to_socktype(ipp);
            type  = socktype_int(stype);
            proto = ipproto_int(ipp);
        }

        set_os_handle_unsafe((int)::socket(family, type, proto)); // NOLINT(readability-redundant-casting)
        if (os_handle_unsafe() == INVALID)
        {
            handle_errno();
            return false;
        }

        Type = stype;
        // remember the family so family()/is_af_unix() work before bind/connect
        Addr.Address.Family = af;

        // TCP_NODELAY only applies to genuine TCP streams, never to AF_Unix
        // (on Windows a unix socket is ST_Stream but has no IPPROTO_TCP options).
        if (stype == ST_Stream && af != AF_Unix)
        {
            if (opt & SO_Nagle) set_nagle(true);
            else                set_nodelay(DEFAULT_NODELAY);
        }

        // configure the blocking mode as required
        if      (opt & SO_NonBlock) set_blocking(/*socketsBlock:*/false);
        else if (opt & SO_Blocking) set_blocking(/*socketsBlock:*/true);
        else                        set_blocking(/*socketsBlock:*/DEFAULT_BLOCKING);

        if (opt & SO_ReuseAddr)
        {
            if (!enable_reuse_address(true))
                return false;
        }
        return true;
    }

    bool socket::enable_reuse_address(bool enable) noexcept
    {
        if (!good())
            return false;
        int reuse = enable ? 1 : 0;
        if (set_opt(SOL_SOCKET, SO_REUSEADDR, reuse)) {
            return handle_errno(get_errno_unlocked()) == 0;
        }
        // NOTE: SO_REUSEPORT is intentionally NOT set here.
        // SO_REUSEADDR allows binding to a recently closed port (TIME_WAIT).
        // SO_REUSEPORT allows multiple sockets to bind to the same port and
        // enables kernel load-balancing of incoming packets, which causes
        // UDP packets to be delivered to the wrong socket in localhost scenarios.
        return true; // SO_REUSEADDR is set
    }

    bool socket::bind(const ipaddress& addr, socket_option opt) noexcept
    {
        if (addr.Address.Family == AF_Unix)
        {
            set_errno(ESOCK(EINVAL));
            return false;
        }
        auto sa = to_saddr(addr);
        std::unique_lock lock = rpp::spin_lock(Mtx);

        if (opt & SO_ReuseAddr)
        {
            if (!enable_reuse_address(true))
                return false; // failed to enable SO_REUSEADDR, dont bind
        }

        if (::bind(os_handle_unsafe(), sa, sa.size()) == 0)
        {
            Addr = addr;
            return true;
        }
        handle_errno();
        return false;
    }

    bool socket::listen() noexcept
    {
        Assert(type() != socket_type::ST_Datagram, "Cannot use socket::listen() on UDP sockets");

        std::unique_lock lock = rpp::spin_lock(Mtx);
        if (::listen(os_handle_unsafe(), SOMAXCONN) == 0) // start listening for new clients
        {
            Category = SC_Listen;
            return true;
        }
        handle_errno();
        return false;
    }

    bool socket::select(int millis, SelectFlag selectFlags) noexcept
    {
        int sock = os_handle();
        if (sock == -1) // cannot select on an invalid socket
            return false;

        fd_set set;
        FD_ZERO(&set);
        FD_SET(sock, &set);
        timeval timeout;
        timeout.tv_sec  = (millis / 1000);
        timeout.tv_usec = (millis % 1000) * 1000;

        fd_set* readfds   = (selectFlags & SF_Read)   ? &set : nullptr;
        fd_set* writefds  = (selectFlags & SF_Write)  ? &set : nullptr;
        fd_set* exceptfds = (selectFlags & SF_Except) ? &set : nullptr;
        errno = 0;
        int rescode = ::select(sock+1, readfds, writefds, exceptfds, &timeout);
        int err = get_socket_level_error();
        if (err != 0)
        {
            // select failed somehow
            handle_errno(err > 0 ? err : 0);
            return false;
        }

        int errcode = os_getsockerr();
        if (rescode == -1 || errcode != 0)
        {
            if (handle_errno(errcode) != 0)
            {
                logerronce(errcode, "select() failed: %s", last_err().c_str());
                return false;
            }
        }
        return rescode > 0; // success: > 0, timeout == 0
    }

    bool socket::poll(int timeoutMillis, PollFlag pollFlags) noexcept
    {
        const short events = short(
            ((pollFlags & PF_Read) ? POLLIN : 0) |
            ((pollFlags & PF_Write) ? POLLOUT : 0)
        );

    #if _WIN32 || _WIN64
        struct pollfd pfd = { SOCKET(os_handle()), events, 0 };
        int r = WSAPoll(&pfd, 1, timeoutMillis);
    #else
        struct pollfd pfd = { os_handle(), events, 0 };
        int r = ::poll(&pfd, 1, timeoutMillis);
    #endif
        if (r < 0)
        {
            handle_errno();
            return false;
        }
        return on_poll_result(pfd.revents, pollFlags);
    }


#if RPP_HAS_CXX20
    int socket::poll(std::span<socket* const> in, std::vector<int>& ready,
                     int timeoutMillis, PollFlag pollFlags) noexcept
#else
    int socket::poll(const std::vector<socket*>& in, std::vector<int>& ready,
                     int timeoutMillis, PollFlag pollFlags) noexcept
#endif
    {
        ready.resize(in.size());

        int n = poll(in.data(), static_cast<int>(in.size()),
                     ready.data(), static_cast<int>(ready.size()),
                     timeoutMillis, pollFlags);

        if (n > 0) ready.resize(n);
        return n;
    }

    int socket::poll(socket* const* in, int inCount,
                     int* outReadyIndexes, int outMaxCount,
                     int timeoutMillis, PollFlag pollFlags) noexcept
    {
        // in 99.9% of cases only 2-4 sockets are polled
        // 32 sockets takes roughly 256 bytes of stack space
        constexpr int MAX_LOCAL_FDS = 32;

        struct pollfd local_pfd[MAX_LOCAL_FDS];
        struct pollfd* pfd = local_pfd;
        if (inCount > MAX_LOCAL_FDS) // too many fds, allocate dynamically
            pfd = (struct pollfd*)malloc(sizeof(struct pollfd) * inCount);

        memset(pfd, 0, sizeof(struct pollfd) * inCount);

        const short events = short(
            ((pollFlags & PF_Read) ? POLLIN : 0) |
            ((pollFlags & PF_Write) ? POLLOUT : 0)
        );

        for (int i = 0; i < inCount; ++i)
        {
            pfd[i].fd = in[i]->os_handle();
            pfd[i].events = events;
            pfd[i].revents = 0;
        }

    #if _WIN32 || _WIN64
        int r = WSAPoll(pfd, inCount, timeoutMillis);
    #else
        int r = ::poll(pfd, inCount, timeoutMillis);
    #endif

        int readyCount = 0;
        if (r >= 0)
        {
            // BUGFIX: we don't trust the poll() return value, we double-check all of the sockets
            for (int i = 0; i < inCount; ++i)
            {
                if (in[i]->on_poll_result(pfd[i].revents, pollFlags))
                {
                    // null is allowed, and outMaxCount can be smaller than inCount
                    if (outReadyIndexes && readyCount < outMaxCount)
                        outReadyIndexes[readyCount] = i;
                    ++readyCount;
                }
            }
        }

        if (pfd != local_pfd)
            free(pfd); // free the dynamically allocated pollfd array
        return readyCount;
    }

    bool socket::on_poll_result(int revents, PollFlag pollFlags) noexcept
    {
        if ((revents & POLLNVAL) != 0) // dead socket
        {
            set_errno(ESOCK(EBADF));
            return false;
        }

        if ((revents & POLLHUP) != 0) // TCP FIN
        {
            // the other side has GRACEFULLY closed the connection
            // this is not an error, but the socket is no longer connected
            handle_errno(ESOCK(ESHUTDOWN));

            // BUGFIX: if TCP socket is SHUTDOWN, sending is no longer possible
            //         and polling makes no sense, however there might be data to read
            return (pollFlags & PF_Read) != 0 && ((revents & POLLIN) != 0 || available() > 0);
        }

        if ((revents & POLLERR) != 0) // some other socket error
        {
            handle_errno(get_socket_level_error());
            return false;
        }

        set_errno(0); // clear errors

        // BUGFIX: it's possible for poll() to miss some READ events
        //         so double-check with available() to be sure
        if ((pollFlags & PF_Read) != 0 && ((revents & POLLIN) != 0 || available() > 0))
            return true;

        if ((revents & POLLOUT) != 0 && (pollFlags & PF_Write) != 0)
            return true;

        // timeout, do not consider this as an error
        return false;
    }

    ////////////////////////////////////////////////////////////////////////////////

    bool socket::listen(const ipaddress& localAddr, ip_protocol ipp, socket_option opt) noexcept
    {
        std::unique_lock lock = rpp::spin_lock(Mtx);

        // SO_ReuseAddr is handled by bind(), remove it from the options
        socket_option creationOpts = socket_option(opt & (~SO_ReuseAddr));
        if (!create(localAddr.Address.Family, ipp, creationOpts))
            return false;

        // bind with binding options (SO_ReuseAddr)
        if (!bind(localAddr, opt))
            return false;

        if (ipp != IPP_UDP && !listen()) // start listening for new clients
            return false;
        return true;
    }

    socket socket::listen_to(const ipaddress& localAddr, ip_protocol ipp, socket_option opt) noexcept
    {
        socket s;
        if (s.listen(localAddr, ipp, opt))
            return s;
        return socket::from_err_code(s.get_errno_unlocked(), localAddr);
    }


    socket socket::accept(int timeoutMillis) noexcept
    {
        if (!good())
        {
            LogError("Cannot use socket::accept() on closed sockets");
            return socket::from_err_code(ESOCK(EBADF));
        }
        // accept() works on connection-oriented sockets: TCP streams and
        // AF_Unix SEQPACKET (Linux) / framed SOCK_STREAM (Windows).
        if (type() != socket_type::ST_Stream && type() != socket_type::ST_SeqPacket)
        {
            LogError("Cannot use socket::accept() on non-TCP sockets, use recvfrom instead");
            return socket::from_err_code(ESOCK(EPROTOTYPE));
        }

        if (!this->poll(timeoutMillis, PF_Read)) // poll will handle any errors
            return socket::from_err_code(get_errno());

        saddr saddr;
        socklen_t len = sizeof(saddr);
        std::unique_lock lock = rpp::spin_lock(Mtx);
        int handle = (int)::accept(os_handle_unsafe(), &saddr.sa, &len); // NOLINT(readability-redundant-casting)
        if (handle == -1)
        {
            handle_errno();
            return socket::from_err_code(get_errno_unlocked());
        }

        socket client = socket::from_os_handle(handle, ipaddress{handle});
        if (client.update_socket_type() == ST_Unspecified) // validate the socket handle
        {
            LogError("socket::from_os_handle(int): invalid handle %s", client.last_err());
            return socket::from_err_code(get_errno_unlocked());
        }

        const bool unix_client = client.is_af_unix();

        // set the accepted socket with same options as the listener
        if (is_nodelay()) client.set_nagle(false); // no-op / skipped for AF_Unix
        client.set_blocking(is_blocking());
        // AF_Unix sockets self-close on fatal I/O errors (old unix_socket contract)
        client.set_autoclosing(unix_client ? true : DEFAULT_AUTOCLOSE_CLIENT_SOCKETS);
        client.Connected = true;
        client.Category = SC_Accept;
        return client;
    }

    bool socket::connect(const ipaddress& remoteAddr, socket_option opt) noexcept
    {
        std::unique_lock lock { Mtx };

        // disallow connecting to an unspecified address
        if (!remoteAddr.has_address())
        {
            set_errno_unlocked(ESOCK(EINVAL));
            return false;
        }

        if (!good())
        {
            // need to use SO_Blocking for infinite wait
            if (!create(remoteAddr.Address.Family, IPP_TCP, socket_option(opt|SO_Blocking)))
                return false;
        }

        Addr = remoteAddr;
        auto sa = to_saddr(remoteAddr);
        int sock = os_handle_unsafe();
        lock.unlock(); // release lock before a blocking connect() call

        if (::connect(sock, sa, sa.size()) != 0)
        {
            // connection unsuccessful, the user has to retry or handle error
            handle_errno();
            return false;
        }

        lock.lock(); // relock again
        configure_connected_client(opt);
        return true;
    }

    bool socket::connect(const ipaddress& remoteAddr, int millis, socket_option opt) noexcept
    {
        std::unique_lock lock { Mtx };

        // disallow connecting to an unspecified address
        if (!remoteAddr.has_address())
        {
            set_errno_unlocked(ESOCK(EINVAL));
            return false;
        }

        if (!good())
        {
            // needs to be a non-blocking socket to do connect() + poll()
            if (!create(remoteAddr.Address.Family, IPP_TCP, socket_option(opt & SO_NonBlock)))
                return false;
        }

        // needs to be a non-blocking socket to do connect() + poll()
        if (is_blocking())
            set_blocking(false);

        Addr = remoteAddr;
        auto sa = to_saddr(remoteAddr);
        int sock = os_handle_unsafe();

        set_errno_unlocked(0); // clear any errors

        // this will return immediately because socket is in non-blocking mode and we hold the mutex
        if (::connect(sock, sa, sa.size()) == 0)
        {
            configure_connected_client(opt);
            return true;
        }

        int err = os_getsockerr(); // read errno
        // EALREADY: nonblocking connect is already in progress, second connect has no effect
        // EINPROGRESS|EWOULDBLOCK: nonblocking connect is in progress, use poll() to wait for completion
        if (err == ESOCK(EALREADY) || err == ESOCK(EINPROGRESS) || err == ESOCK(EWOULDBLOCK))
        {
            lock.unlock(); // unlock before entering very slow poll()
            if (poll(millis, PF_Write))
            {
                // the socket is writable, but according to connect() manual,
                // SO_ERROR needs to be checked for the final status
                lock.lock(); // relock again
                int so_err = get_socket_level_error();
                if (so_err == 0)
                {
                    configure_connected_client(opt);
                    return true;
                }
                handle_errno(so_err);
                return false;
            }
            else
            {
                lock.lock(); // relock again
                // if poll timed out (no last errno), then use the EINPROGRESS / EALREADY error codes
                if (!get_errno_unlocked())
                    set_errno_unlocked(err);
                return false;
            }
        }

        logerror("socket fh:%d async connect error: %s",
                  os_handle_unsafe(), last_os_socket_err(err).c_str());
        handle_errno(err);
        return false;
    }

    void socket::configure_connected_client(socket_option opt) noexcept
    {
        Category = SC_Client;
        // AF_Unix sockets self-close on fatal I/O errors (old unix_socket contract)
        set_autoclosing(is_af_unix() ? true : DEFAULT_AUTOCLOSE_CLIENT_SOCKETS);
        Connected = true;

        // TCP_NODELAY is meaningless for AF_Unix (and on Windows would issue a
        // bogus IPPROTO_TCP setsockopt on an ST_Stream unix socket).
        if (!is_af_unix())
        {
            if (opt & SO_Nagle) set_nagle(true);
            else                set_nodelay(DEFAULT_NODELAY);
        }

        // configure blocking flags
        if      (opt & SO_NonBlock) set_blocking(false);
        else if (opt & SO_Blocking) set_blocking(true);
        else                        set_blocking(Blocking); // use server socket default
    }

    socket socket::connect_to(const ipaddress& remoteAddr, socket_option opt) noexcept
    {
        socket s;
        if (s.connect(remoteAddr, opt))
            return s;
        return socket::from_err_code(s.get_errno_unlocked(), remoteAddr);
    }

    socket socket::connect_to(const ipaddress& remoteAddr, int millis, socket_option opt) noexcept
    {
        socket s;
        if (s.connect(remoteAddr, millis, opt))
            return s;
        return socket::from_err_code(s.get_errno_unlocked(), remoteAddr);
    }

    ////////////////////////////////////////////////////////////////////////////////
    ///////        AF_Unix (local) sockets
    ////////////////////////////////////////////////////////////////////////////////

#if RPP_UNIX_SOCKET_SUPPORTED && !_WIN32
    // Closes every SCM_RIGHTS fd attached to a received message, except `keep`.
    static void close_received_fds(msghdr& msg, int keep) noexcept
    {
        for (cmsghdr* c = CMSG_FIRSTHDR(&msg); c != nullptr; c = CMSG_NXTHDR(&msg, c))
        {
            if (c->cmsg_level != SOL_SOCKET || c->cmsg_type != SCM_RIGHTS)
                continue;
            const size_t count = (c->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            for (size_t i = 0; i < count; ++i)
            {
                int fd = -1;
                memcpy(&fd, CMSG_DATA(c) + i * sizeof(int), sizeof(fd));
                if (fd >= 0 && fd != keep)
                    ::close(fd);
            }
        }
    }
#endif

#if _WIN32 && RPP_UNIX_SOCKET_SUPPORTED
    // Drains any unsent frame tail; the stream must stay in sync. Returns true
    // once the tx buffer is empty, false if it would block (tail still pending)
    // -- the socket self-closes on a fatal error. Called from send()/recv().
    bool unix_socket::try_flush_unix() noexcept
    {
        std::unique_lock lock = rpp::spin_lock(Mtx);
        unix_framing* f = &Framing;
        while (f->tx_off < f->tx.size())
        {
            const int r = ::send(static_cast<SOCKET>(os_handle_unsafe()),
                                 reinterpret_cast<const char*>(f->tx.data()) + f->tx_off,
                                 static_cast<int>(f->tx.size() - f->tx_off), 0);
            if (r > 0)
            {
                f->tx_off += static_cast<size_t>(r);
                continue;
            }
            if (r < 0 && WSAGetLastError() == WSAEWOULDBLOCK)
                return false; // tail still stuck
            lock.unlock();
            close(); // peer gone
            return false;
        }
        f->tx.clear();
        f->tx_off = 0;
        return true;
    }

    int unix_socket::send_unix_framed(const void* data, int len) noexcept
    {
        std::unique_lock lock = rpp::spin_lock(Mtx);
        unix_framing* f = &Framing;
        if (os_handle_unsafe() == INVALID)
            return -1;

        // Finish any partially-sent frame first; the stream must stay in sync.
        lock.unlock();
        if (!try_flush_unix())
        {
            // tail still stuck (would block) or peer gone (socket self-closed)
            if (os_handle_unsafe() == INVALID)
                return -1;
            set_errno(WSAEWOULDBLOCK);
            return 0; // would-block: not sent, socket stays valid
        }
        lock.lock();
        if (os_handle_unsafe() == INVALID)
            return -1;

        // Frame: 2-byte LE length prefix + payload.
        std::vector<uint8_t> frame;
        frame.reserve(static_cast<size_t>(2 + len));
        frame.resize(2);
        writeLEU16(frame.data(), static_cast<uint16_t>(len));
        const uint8_t* src = static_cast<const uint8_t*>(data);
        frame.insert(frame.end(), src, src + len);
        const int total = static_cast<int>(frame.size());

        const int r = ::send(static_cast<SOCKET>(os_handle_unsafe()),
                             reinterpret_cast<const char*>(frame.data()), total, 0);
        if (r == total)
            return len; // whole frame committed
        if (r >= 0)
        {
            // Partial: the frame is committed, stash the tail to keep the stream
            // in sync. Delivered by a later send()/recv() via try_flush_unix().
            f->tx.assign(frame.begin() + r, frame.end());
            f->tx_off = 0;
            return len;
        }
        if (WSAGetLastError() == WSAEWOULDBLOCK)
        {
            set_errno(WSAEWOULDBLOCK);
            return 0; // send buffer full
        }
        lock.unlock();
        close(); // peer gone
        return -1;
    }

    int unix_socket::recv_unix_framed(void* buf, int cap) noexcept
    {
        // Opportunistically push out any frame tail left by a prior partial
        // send(); a recv()-only peer otherwise leaves the wire half-framed.
        try_flush_unix();
        if (os_handle_unsafe() == INVALID)
            return -1;

        std::unique_lock lock { Mtx };
        unix_framing* f = &Framing;

        // Threshold past which we compact rx instead of leaving consumed bytes
        // before rx_off. Bounds the memmove cost to O(backlog) amortized.
        constexpr size_t compact_threshold = 64 * 1024;
        for (;;)
        {
            const size_t avail = f->rx.size() - f->rx_off;
            if (avail >= 2) // a complete frame already buffered?
            {
                const uint8_t* p = f->rx.data() + f->rx_off;
                const int len = readLEU16(p);
                if (avail >= static_cast<size_t>(2 + len))
                {
                    const int n = len < cap ? len : cap; // truncate like SEQPACKET
                    memcpy(buf, p + 2, static_cast<size_t>(n));
                    f->rx_off += static_cast<size_t>(2 + len);
                    if (f->rx_off == f->rx.size())
                    {
                        f->rx.clear();
                        f->rx_off = 0;
                    }
                    else if (f->rx_off >= compact_threshold)
                    {
                        f->rx.erase(f->rx.begin(), f->rx.begin() + static_cast<std::ptrdiff_t>(f->rx_off));
                        f->rx_off = 0;
                    }
                    return n;
                }
            }
            char tmp[4096];
            int sock = os_handle_unsafe();
            const int r = ::recv(static_cast<SOCKET>(sock), tmp, sizeof(tmp), 0);
            if (r > 0)
            {
                f->rx.insert(f->rx.end(), tmp, tmp + r);
                continue;
            }
            if (r < 0 && WSAGetLastError() == WSAEWOULDBLOCK)
                return 0; // nothing ready (non-blocking)
            lock.unlock();
            close(); // EOF or fatal
            return -1;
        }
    }

    int unix_socket::send(const void* buffer, int numBytes) noexcept
    {
        if (numBytes <= 0) // ignore 0-byte I/O, handle_txres can't handle it
            return 0;
        // AF_Unix is message-oriented: the Windows length prefix is 16-bit, so
        // an oversize frame would wrap and desync the peer.
        if (numBytes > MAX_UNIX_MESSAGE_SIZE)
        {
            set_errno(ESOCK(EMSGSIZE));
            return -1;
        }
        return send_unix_framed(buffer, numBytes); // software framing
    }

    int unix_socket::recv(void* buffer, int maxBytes) noexcept
    {
        if (maxBytes <= 0) // ignore 0-byte I/O, handle_txres can't handle it
            return 0;
        return recv_unix_framed(buffer, maxBytes); // software de-framing
    }

    void unix_socket::close() noexcept
    {
        // Drop any half-assembled frames before releasing the handle, so a
        // reused unix_socket (close() + reconnect) starts from a clean slate.
        // Hold Mtx while mutating Framing: a peer-gone self-close on one thread
        // can race a send/recv framing op on another (both take Mtx).
        {
            lock_t lock = rpp::spin_lock(Mtx);
            Framing = unix_framing{};
        }
        socket::close();
    }
#endif // _WIN32 && RPP_UNIX_SOCKET_SUPPORTED

    unix_socket::unix_socket(unix_socket&& s) noexcept
    {
        *this = std::move(s); // delegate to the locked move-assign below
    }

    unix_socket& unix_socket::operator=(unix_socket&& s) noexcept
    {
        if (this == &s) return *this;
        std::scoped_lock lock { mutex(), s.mutex() };
        std::swap(SockName, s.SockName);
    #if _WIN32 && RPP_UNIX_SOCKET_SUPPORTED
        std::swap(Framing, s.Framing);
    #endif
        socket::operator=(std::move(s));
        return *this;
    }

    // Builds an invalid unix_socket carrying a last error, mirroring
    // socket::from_err_code() but preserving the unix_socket type.
    unix_socket unix_socket::from_err(int err) noexcept
    {
        unix_socket s;
        s.Addr.Address.Family = AF_Unix;
        s.set_errno_unlocked(err);
        return s;
    }

    // Binds this AF_Unix socket to SockName. The name is not stored in the
    // socket address, so we build the sockaddr_un here from SockName directly.
    bool unix_socket::bind_unix(socket_option opt) noexcept
    {
        saddr sa = {};
        fill_unix_addr(sa, SockName.c_str());
        if (sa.Len == 0) // empty / too-long name (fill_unix_addr left Len == 0)
        {
            set_errno(ESOCK(EINVAL));
            return false;
        }
        std::unique_lock lock = rpp::spin_lock(Mtx);
        if (opt & SO_ReuseAddr)
        {
            if (!enable_reuse_address(true))
                return false; // failed to enable SO_REUSEADDR, dont bind
        }
    #if _WIN32 && RPP_UNIX_SOCKET_SUPPORTED
        // Windows AF_Unix is a real filesystem object; remove a stale socket
        // file left over from a past run before binding.
        ::DeleteFileA(sa.sun.sun_path);
    #endif
        if (::bind(os_handle_unsafe(), sa, sa.size()) == 0)
        {
            Addr.Address.Family = AF_Unix; // nameless family marker
            return true;
        }
        handle_errno();
        return false;
    }

    // Connects this AF_Unix socket to SockName (blocking connect, mirrors the
    // 2-arg socket::connect() but builds the sockaddr_un from SockName).
    bool unix_socket::connect_unix_to(socket_option opt) noexcept
    {
        saddr sa = {};
        fill_unix_addr(sa, SockName.c_str());
        if (sa.Len == 0) // empty / too-long name (fill_unix_addr left Len == 0)
        {
            set_errno(ESOCK(EINVAL));
            return false;
        }
        std::unique_lock lock = rpp::spin_lock(Mtx);
        Addr.Address.Family = AF_Unix; // nameless family marker
        int sock = os_handle_unsafe();
        lock.unlock(); // release lock before a blocking connect() call

        if (::connect(sock, sa, sa.size()) != 0)
        {
            handle_errno();
            return false;
        }
        lock.lock();
        configure_connected_client(opt);
        return true;
    }

    unix_socket unix_socket::listen_unix(strview name, int backlog, socket_option opt) noexcept
    {
        if (name.empty())
            return from_err(ESOCK(EINVAL));

        unix_socket s;
        s.SockName = std::string{name.data(), static_cast<size_t>(name.size())};
        // SO_ReuseAddr is handled by bind_unix(), remove it from the creation options
        socket_option creationOpts = socket_option(opt & ~SO_ReuseAddr);
        if (!s.create(AF_Unix, IPP_DontCare, creationOpts))
            return from_err(s.get_errno_unlocked());
        if (!s.bind_unix(opt))
            return from_err(s.get_errno_unlocked());
        if (::listen(s.os_handle(), backlog) != 0)
        {
            s.handle_errno();
            return from_err(s.get_errno_unlocked());
        }
        s.Category = SC_Listen;
        return s;
    }

    unix_socket unix_socket::connect_unix(strview name, socket_option opt) noexcept
    {
        if (name.empty())
            return from_err(ESOCK(EINVAL));

        unix_socket s;
        s.SockName = std::string{name.data(), static_cast<size_t>(name.size())};
        // create the AF_Unix socket up front (connect() would default to IPP_TCP)
        if (!s.create(AF_Unix, IPP_DontCare, socket_option(opt | SO_Blocking)))
            return from_err(s.get_errno_unlocked());
        if (s.connect_unix_to(opt))
            return s;
        return from_err(s.get_errno_unlocked());
    }

    unix_socket unix_socket::accept(int timeoutMillis) noexcept
    {
        unix_socket client { socket::accept(timeoutMillis) };
        client.SockName = SockName; // inherit the listener's name (best effort)
        return client;
    }

    bool unix_socket::pair(unix_socket& a, unix_socket& b) noexcept
    {
#if RPP_UNIX_SOCKET_SUPPORTED && !_WIN32 // Linux only (SCM_RIGHTS pair semantics)
        int sv[2];
        if (::socketpair(AF_UNIX, SOCK_SEQPACKET, 0, sv) != 0)
            return false;
        a = unix_socket{ socket::from_os_handle(sv[0], ipaddress::unix_addr()) };
        b = unix_socket{ socket::from_os_handle(sv[1], ipaddress::unix_addr()) };
        a.Type = b.Type = ST_SeqPacket;
        a.Connected = b.Connected = true;
        a.Category = b.Category = SC_Client;
        a.SockName = b.SockName = "pair";
        // self-close on fatal I/O errors, matching connect/accept unix sockets
        a.set_autoclosing(true);
        b.set_autoclosing(true);
        return true;
#else
        (void)a, (void)b;
        return false;
#endif
    }

    bool unix_socket::send_fd(uint32_t tag, int fd) noexcept
    {
#if RPP_UNIX_SOCKET_SUPPORTED && !_WIN32 // SCM_RIGHTS is Linux only
        std::unique_lock lock = rpp::spin_lock(Mtx);
        if (os_handle_unsafe() == INVALID || fd < 0)
            return false;

        iovec iov{&tag, sizeof(tag)};
        union {
            char buf[CMSG_SPACE(sizeof(int))];
            cmsghdr align;
        } control;
        memset(&control, 0, sizeof(control));

        msghdr msg{};
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = control.buf;
        msg.msg_controllen = sizeof(control.buf);

        cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));

        int sock = os_handle_unsafe();
        ssize_t n;
        do {
            n = ::sendmsg(sock, &msg, MSG_DONTWAIT | MSG_NOSIGNAL);
        } while (n < 0 && errno == EINTR);

        if (n == static_cast<ssize_t>(sizeof(tag)))
            return true; // all-or-nothing like send()
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return false; // send buffer full, socket stays valid
        lock.unlock();
        close(); // peer gone (or partial/short -- unrecoverable)
        return false;
#else
        (void)tag, (void)fd;
        return false; // no SCM_RIGHTS here
#endif
    }

    int unix_socket::recv_fd(uint32_t& out_tag, int& out_fd) noexcept
    {
#if RPP_UNIX_SOCKET_SUPPORTED && !_WIN32 // SCM_RIGHTS is Linux only
        std::unique_lock lock { Mtx };
        if (os_handle_unsafe() == INVALID)
            return -1;

        uint32_t tag = 0;
        iovec iov{&tag, sizeof(tag)};
        union {
            char buf[CMSG_SPACE(sizeof(int))];
            cmsghdr align;
        } control;

        msghdr msg{};
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = control.buf;
        msg.msg_controllen = sizeof(control.buf);

        int sock = os_handle_unsafe();
        ssize_t n;
        do {
#ifdef MSG_CMSG_CLOEXEC
            n = ::recvmsg(sock, &msg, MSG_CMSG_CLOEXEC);
#else
            n = ::recvmsg(sock, &msg, 0);
#endif
        } while (n < 0 && errno == EINTR);

        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            return 0; // nothing ready (non-blocking mode), socket stays valid
        if (n <= 0)
        {
            lock.unlock();
            close();
            return -1; // EOF or fatal
        }
        if (n != static_cast<ssize_t>(sizeof(tag)))
        {
            close_received_fds(msg, -1);
            return 0; // malformed -- skip
        }

        cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        if (cmsg == nullptr || cmsg->cmsg_level != SOL_SOCKET || cmsg->cmsg_type != SCM_RIGHTS ||
            cmsg->cmsg_len != CMSG_LEN(sizeof(int)))
        {
            close_received_fds(msg, -1);
            return 0; // no (single) fd attached -- skip
        }

        int fd = -1;
        memcpy(&fd, CMSG_DATA(cmsg), sizeof(fd));
        close_received_fds(msg, fd); // defensive: drop any extra piggybacked fds
        out_tag = tag;
        out_fd = fd;
        return 1;
#else
        (void)out_tag, (void)out_fd;
        return -1; // no SCM_RIGHTS here
#endif
    }

    bool socket::bind_to_interface([[maybe_unused]] uint64_t network_handle) noexcept
    {
        std::unique_lock lock = rpp::spin_lock(Mtx);
        if (!good())
            return false;

    #if RPP_ANDROID && __ANDROID_API__ >= 23
        if (android_setsocknetwork((net_handle_t)network_handle, os_handle_unsafe()) != 0)
        {
            set_errno_unlocked(os_getsockerr());
            logerror("Failed to bind socket to network handle: %s", last_err().c_str());
            return false;
        }
        return true;
    #else
        // TODO: implement for other platforms
        return false;
    #endif
    }

    bool socket::bind_to_interface(const std::string& interface) noexcept
    {
        if (!good() || interface.empty())
            return false;

        std::optional<uint64_t> network_handle = get_network_handle(interface);
        if (!network_handle)
        {
            logerror("Failed to get network handle for interface: %s", interface.c_str());
            return false;
        }

        return bind_to_interface(*network_handle);
    }

    void socket::unbind_interface() noexcept
    {
    #if RPP_ANDROID && __ANDROID_API__ >= 23
        bind_to_interface(0); // 0=NETWORK_UNSPECIFIED
    #else
        // TODO: implement for other platforms
    #endif
    }

    ////////////////////////////////////////////////////////////////////////////////

    socket make_udp_randomport(socket_option opt, raw_address bind_address) noexcept
    {
        socket s;
        for (int i = 0; i < 100; ++i) {
            int port = (rand() % (65536 - 8000));
            s = socket::make_udp(ipaddress{bind_address, port}, opt);
            if (s.good()) return s;
        }
        return s; // keeps the error code and address of the last attempt
    }

    socket make_tcp_randomport(socket_option opt, raw_address bind_address) noexcept
    {
        socket s;
        for (int i = 0; i < 100; ++i) {
            int port = (rand() % (65536 - 8000));
            s = socket::listen_to(ipaddress{bind_address, port}, IPP_TCP, opt);
            if (s.good()) return s;
        }
        return s; // keeps the error code and address of the last attempt
    }

    ipinterface get_ip_interface(const std::string& network_interface, address_family af)
    {
        std::vector<ipinterface> interfaces = ipinterface::get_interfaces(network_interface, af);
        if (interfaces.empty())
            return {};
        for (const ipinterface& ip : interfaces)
            if (pattern_match(ip.name, network_interface) != -1)
                return ip;
        return interfaces.front();
    }

    std::string get_system_ip(const std::string& network_interface, address_family af)
    {
        return get_ip_interface(network_interface, af).addr.str();
    }

    std::string get_broadcast_ip(const std::string& network_interface, address_family af)
    {
        return get_ip_interface(network_interface, af).broadcast.str();
    }

    std::optional<uint64_t> get_network_handle(const std::string& network_interface) noexcept
    {
    #if RPP_ANDROID && __ANDROID_API__ >= 21
        using namespace rpp::jni;

        try
        {
            auto* mainActivity = getMainActivity();
            if (!mainActivity)
            {
                logerror("get_network_handle failed: mainActivity uninitialized");
                return std::nullopt;
            }

            static Class Activity{"android/app/Activity"};
            static Class ConnectivityManager{"android/net/ConnectivityManager"};
            static Class LinkProperties{"android/net/LinkProperties"};
            static Class Network{"android/net/Network"};

            static Method getSystemService = Activity.method("getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
            static Method getAllNetworks = ConnectivityManager.method("getAllNetworks", "()[Landroid/net/Network;");
            static Method getLinkProperties = ConnectivityManager.method("getLinkProperties", "(Landroid/net/Network;)Landroid/net/LinkProperties;");
            static Method getInterfaceName = LinkProperties.method("getInterfaceName", "()Ljava/lang/String;");
            static Method getNetworkHandle = Network.method("getNetworkHandle", "()J");
            static Ref<jobject> connectivityManager = getSystemService.globalObjectF(mainActivity, JString::from("connectivity"));

            JArray networks = getAllNetworks.arrayF(JniType::Object, connectivityManager);
            jsize length = networks.getLength();

            for (jsize i = 0; i < length; ++i)
            {
                Ref<jobject> network { networks.getObjectAt(i) };
                Ref<jobject> linkProperties = getLinkProperties.objectF(connectivityManager, network);

                if (linkProperties)
                {
                    std::string interfaceName = getInterfaceName.stringF(linkProperties).str();
                    if (network_interface == interfaceName)
                    {
                        return static_cast<uint64_t>(getNetworkHandle.longF(network));
                    }
                }
            }
            return std::nullopt;
        }
        catch (const std::exception& e)
        {
            logerror("get_network_handle %s failed: %s", network_interface.c_str(), e.what());
            return std::nullopt;
        }
    #else
        // TODO: implement for other platforms
        (void)network_interface;
        return std::nullopt;
    #endif
    }

    ////////////////////////////////////////////////////////////////////////////////

    void wait_readable(std::initializer_list<int> fds, int timeout_ms) noexcept
    {
    #if _WIN32
        WSAPOLLFD pfds[8];
        ULONG n = 0;
        for (int fd : fds)
            if (fd >= 0 && n < 8) // POLLRDNORM: WSAPoll rejects combos with POLLPRI
                pfds[n++] = WSAPOLLFD{static_cast<SOCKET>(fd), POLLRDNORM, 0};
        if (n > 0)
            ::WSAPoll(pfds, n, timeout_ms);
    #else
        pollfd pfds[8];
        nfds_t n = 0;
        for (int fd : fds)
            if (fd >= 0 && n < 8)
                pfds[n++] = pollfd{fd, POLLIN, 0};
        if (n > 0)
            ::poll(pfds, n, timeout_ms);
    #endif
    }

    ////////////////////////////////////////////////////////////////////////////////

} // namespace rpp

