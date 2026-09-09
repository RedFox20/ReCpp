// C++20 module interface unit for <rpp/sockets.h>.
module;

// global module fragment: the header stays here, so an importer and an includer share one entity
#include "sockets.h"

export module rpp.sockets;

// GENERATED EXPORTS BEGIN, tools/gen_module_exports.py owns this block

export namespace rpp {
    using rpp::address_family;
    using rpp::AF_DontCare;
    using rpp::AF_IPv4;
    using rpp::AF_IPv6;
    using rpp::AF_Bth;
    using rpp::socket_type;
    using rpp::ST_Unspecified;
    using rpp::ST_Stream;
    using rpp::ST_Datagram;
    using rpp::ST_Raw;
    using rpp::ST_RDM;
    using rpp::ST_SeqPacket;
    using rpp::socket_category;
    using rpp::SC_Unknown;
    using rpp::SC_Listen;
    using rpp::SC_Accept;
    using rpp::SC_Client;
    using rpp::ip_protocol;
    using rpp::IPP_DontCare;
    using rpp::IPP_ICMP;
    using rpp::IPP_IGMP;
    using rpp::IPP_BTH;
    using rpp::IPP_TCP;
    using rpp::IPP_UDP;
    using rpp::IPP_ICMPV6;
    using rpp::IPP_PGM;
    using rpp::socket_option;
    using rpp::SO_None;
    using rpp::SO_ReuseAddr;
    using rpp::SO_Blocking;
    using rpp::SO_NonBlock;
    using rpp::SO_Nagle;
    using rpp::to_addrfamily;
    using rpp::to_socktype;
    using rpp::to_ipproto;
    using rpp::addrfamily_int;
    using rpp::socktype_int;
    using rpp::ipproto_int;
    using rpp::protocol_info;
    using rpp::raw_address;
    using rpp::ipaddress;
    using rpp::ipaddress4;
    using rpp::ipaddress6;
    using rpp::ipinterface;
    using rpp::socket;
    using rpp::make_udp_randomport;
    using rpp::make_tcp_randomport;
    using rpp::get_ip_interface;
    using rpp::get_system_ip;
    using rpp::get_broadcast_ip;
    using rpp::get_network_handle;
}
// GENERATED EXPORTS END
