#include <rpp/sockets.h>
#include <rpp/timer.h>
#include <rpp/tests.h>
#include <thread>

using namespace rpp;

TestImpl(test_sockets)
{
    //////////////////////////////////////////////////////////////////

    TestInit(test_sockets) {}

    TestCase(ipaddress_doesnt_smash_stack)
    {
        uint8_t before[4] = { 0xBB,0xBB,0xBB,0xBB };
        ipaddress addr;
        uint8_t after[4] = { 0xAA,0xAA,0xAA,0xAA };

        addr = ipaddress4{"192.168.1.1", 1234};
        AssertTrue(!addr.is_empty());
        AssertTrue(addr.is_valid());
        AssertTrue(addr.has_address());
        for (int i = 0; i < 4; ++i) AssertEqual(before[i], 0xBB);
        for (int i = 0; i < 4; ++i) AssertEqual(after[i], 0xAA);
    }

    TestCase(socket_doesnt_smash_stack)
    {
        uint8_t before[4] = { 0xBB,0xBB,0xBB,0xBB };
        socket s;
        uint8_t after[4] = { 0xAA,0xAA,0xAA,0xAA };

        s = rpp::make_udp_randomport();
        AssertTrue(s.good());
        AssertTrue(!s.address().is_empty());
        AssertTrue(s.address().is_valid());
        for (int i = 0; i < 4; ++i) AssertEqual(before[i], 0xBB);
        for (int i = 0; i < 4; ++i) AssertEqual(after[i], 0xAA);
    }

    TestCase(init_ipv4)
    {
        ipaddress4 a;
        AssertTrue(a.is_empty());
        AssertFalse(a.is_valid());
        AssertTrue(a.address().is_empty());
        AssertFalse(a.address().has_address());
        AssertEqual(a.port(), 0);
        AssertEqual(a.address().str(), "");

        ipaddress4 b { 1234 };
        AssertFalse(b.is_empty());
        AssertTrue(b.is_valid());
        AssertFalse(b.address().is_empty());
        AssertFalse(b.address().has_address());
        AssertEqual(b.port(), 1234);
        AssertEqual(b.address().str(), "0.0.0.0");

        ipaddress4 c { "127.0.0.1", 12345 };
        AssertFalse(c.is_empty());
        AssertTrue(c.is_valid());
        AssertFalse(c.address().is_empty());
        AssertTrue(c.address().has_address());
        AssertEqual(c.port(), 12345);
        AssertEqual(c.address().str(), "127.0.0.1");

        ipaddress4 d { "127.0.0.1:12345" };
        AssertFalse(d.is_empty());
        AssertTrue(d.is_valid());
        AssertFalse(d.address().is_empty());
        AssertTrue(d.address().has_address());
        AssertEqual(d.port(), 12345);
        AssertEqual(d.address().str(), "127.0.0.1");
    }

    TestCase(init_ipv6)
    {
        ipaddress6 a;
        AssertTrue(a.is_empty());
        AssertFalse(a.is_valid());
        AssertTrue(a.address().is_empty());
        AssertFalse(a.address().has_address());
        AssertEqual(a.port(), 0);
        AssertEqual(a.address().str(), "");

        ipaddress6 b { 1234 };
        AssertFalse(b.is_empty());
        AssertTrue(b.is_valid());
        AssertFalse(b.address().is_empty());
        AssertFalse(b.address().has_address());
        AssertEqual(b.port(), 1234);
        AssertEqual(b.address().str(), "::");

        ipaddress6 c { "::1", 12345 };
        AssertFalse(c.is_empty());
        AssertTrue(c.is_valid());
        AssertFalse(c.address().is_empty());
        AssertTrue(c.address().has_address());
        AssertEqual(c.port(), 12345);
        AssertEqual(c.address().str(), "::1");

        ipaddress6 d { "[2001:db8:1::ab9:C0A8:102]:12345" };
        AssertFalse(d.is_empty());
        AssertTrue(d.is_valid());
        AssertFalse(d.address().is_empty());
        AssertTrue(d.address().has_address());
        AssertEqual(d.port(), 12345);
        AssertEqual(d.address().str(), "2001:db8:1::ab9:c0a8:102");
    }

    TestCase(ipaddress_from_ip_and_port)
    {
        std::string system_ip = get_system_ip("eth|lan|wlan");
        print_info("system_ip: %s\n", system_ip.c_str());

        rpp::ipaddress ip { system_ip + ":14550" };
        print_info("ipaddress: %s\n", ip.cname());
        AssertTrue(ip.is_valid());
        AssertEqual(ip.port(), 14550);
        AssertEqual(ip.address().str(), system_ip);
    }

    TestCase(ipaddress_from_unknown_subnet)
    {
        rpp::ipaddress ip { "172.23.0.3:14560" };
        print_info("ipaddress: %s\n", ip.cname());
        AssertTrue(ip.is_valid());
        AssertEqual(ip.port(), 14560);
        AssertEqual(ip.address().str(), "172.23.0.3");
        AssertEqual(ip.str(), "172.23.0.3:14560");
    }

    TestCase(ipaddress_for_listener_port)
    {
        rpp::ipaddress ip { "", "14550" };
        print_info("ipaddress: %s\n", ip.cname());
        AssertTrue(ip.is_valid());
        AssertEqual(ip.port(), 14550);
        AssertEqual(ip.address().str(), "0.0.0.0");
        AssertEqual(ip.str(), "0.0.0.0:14550");
    }

    TestCase(ipaddress_for_listener_port_single_arg)
    {
        rpp::ipaddress ip { ":14550" };
        print_info("ipaddress: %s\n", ip.cname());
        AssertTrue(ip.is_valid());
        AssertEqual(ip.port(), 14550);
        AssertEqual(ip.address().str(), "0.0.0.0");
        AssertEqual(ip.str(), "0.0.0.0:14550");
    }

    TestCase(broadcast)
    {
        rpp::ipinterface iface = rpp::get_ip_interface("eth|lan|wlan");
        std::string system_ip = iface.addr.str();
        std::string broadcast_ip = iface.broadcast.str();
        print_info("system_ip: %s\n", system_ip.c_str());
        print_info("broadcast_ip: %s\n", broadcast_ip.c_str());

        rpp::socket listener;
        AssertTrue(listener.create(rpp::AF_IPv4, rpp::IPP_UDP, rpp::SO_Blocking));
        AssertMsg(listener.bind(rpp::ipaddress{ ":12550" }), "bind failed: %s", listener.last_err().c_str());
        AssertTrue(listener.enable_broadcast());

        rpp::socket listener2;
        AssertTrue(listener2.create(rpp::AF_IPv4, rpp::IPP_UDP, rpp::SO_Blocking));
        AssertTrue(listener2.bind(rpp::ipaddress{ ":15550" }));

        rpp::ipaddress broadcast_addr { broadcast_ip + ":15550" };
        AssertGreater(listener.sendto(broadcast_addr, "hello\0", 6), 0);

        char message[256] = "no-mesages-received";
        rpp::ipaddress from;
        AssertGreater(listener2.recvfrom_timeout(from, message, sizeof(message), /*timeout_ms*/500), 0);
        AssertEqual(rpp::strview{message}, "hello");
        AssertEqual(from.str(), system_ip + ":12550");
    }

    TestCase(list_interfaces_ipv4)
    {
        std::vector<ipinterface> ifaces = ipinterface::get_interfaces("eth|lan|wlan", AF_IPv4);
        AssertNotEqual(ifaces.size(), 0);
    #if _MSC_VER
        AssertTrue(ifaces[0].gateway.has_address()); // the very first interface should have a gateway (the lan interface)
    #endif
        for (const auto& iface : ifaces)
        {
            print_info("ipinterface  %-32s  addr:%-15s  netmask:%-15s  broadcast:%-15s  gateway:%-15s\n",
                iface.name.c_str(),
                iface.addr.str().c_str(),
                iface.netmask.str().c_str(),
                iface.broadcast.str().c_str(),
                iface.gateway.str().c_str()
            );
            AssertNotEqual(iface.name, "");
            AssertTrue(iface.addr.has_address());
            AssertTrue(iface.netmask.has_address());
        #if _MSC_VER
            AssertTrue(iface.broadcast.has_address());
        #endif
            // AssertTrue(iface.gateway.has_address()); // disabled because virtual interfaces don't have a gateway
        }
    }

    TestCase(list_interfaces_ipv6)
    {
        std::vector<ipinterface> ifaces = ipinterface::get_interfaces("eth|lan|wlan", AF_IPv6);
    #if _MSC_VER
        AssertNotEqual(ifaces.size(), 0);
        AssertTrue(ifaces[0].gateway.has_address()); // the very first interface should have a gateway (the lan interface)
    #endif
        for (const auto& iface : ifaces)
        {
            print_info("ipinterface  %-32s  addr=%-15s  broadcast=%-15s  gateway=%-15s\n",
                iface.name.c_str(),
                iface.addr.str().c_str(),
                iface.broadcast.str().c_str(),
                iface.gateway.str().c_str()
            );
            AssertNotEqual(iface.name, "");
            AssertTrue(iface.addr.has_address());
            // AssertTrue(iface.broadcast.has_address());
        }
    }

    TestCase(udp_socket_options)
    {
        socket sock = rpp::make_udp_randomport();
        AssertFalse(sock.is_blocking()); // should be false by default
        AssertTrue(sock.is_nodelay()); // should be nodelay by default

        Assert(sock.set_blocking(true));
        Assert(sock.is_blocking());

        AssertFalse(sock.set_nagle(true)); // cannot set nagle on UDP socket
        AssertTrue(sock.is_nodelay()); // UDP is always nodelay

        print_info("default UDP SO_RCVBUF: %d\n", sock.get_rcv_buf_size());
        print_info("default UDP SO_SNDBUF: %d\n", sock.get_snd_buf_size());

        // NOTE: if there is a mismatch here, then some unix-like kernel didn't double the buffer
        //       which is expected behaviour on non-windows platforms
        Assert(sock.set_snd_buf_size(16384));
        AssertThat(sock.get_snd_buf_size(), 16384);

        Assert(sock.set_rcv_buf_size(32768));
        AssertThat(sock.get_rcv_buf_size(), 32768);

        // check UDP noblock delay, it cannot affect NAGLE
        sock.set_noblock_nodelay();
        AssertFalse(sock.is_blocking());
        AssertTrue(sock.is_nodelay());
    }

    TestCase(tcp_socket_options)
    {
        socket sock = rpp::make_tcp_randomport();
        AssertFalse(sock.is_blocking()); // should be false by default
        AssertTrue(sock.is_nodelay()); // should be nodelay by default

        Assert(sock.set_blocking(true));
        Assert(sock.is_blocking());

        Assert(sock.set_nagle(true));
        AssertFalse(sock.is_nodelay());

        print_info("default TCP SO_RCVBUF: %d\n", sock.get_rcv_buf_size());
        print_info("default TCP SO_SNDBUF: %d\n", sock.get_snd_buf_size());

        // NOTE: if there is a mismatch here, then some unix-like kernel didn't double the buffer
        //       which is expected behaviour on non-windows platforms
        Assert(sock.set_snd_buf_size(16384));
        AssertThat(sock.get_snd_buf_size(), 16384);

        Assert(sock.set_rcv_buf_size(32768));
        AssertThat(sock.get_rcv_buf_size(), 32768);

        // check TCP noblock delay, it cannot affect NAGLE
        sock.set_noblock_nodelay();
        AssertFalse(sock.is_blocking());
        AssertTrue(sock.is_nodelay());
    }

    
    TestCase(socket_udp_send_receive)
    {
        std::vector<uint8_t> msg(4000, 'x');
        socket send = rpp::make_udp_randomport();
        socket recv = rpp::make_udp_randomport();
        AssertFalse(send.is_blocking()); // should be false by default
        AssertFalse(recv.is_blocking()); // should be false by default

        auto recv_addr = ipaddress(AF_IPv4, "127.0.0.1", recv.port());
        AssertThat(send.sendto(recv_addr, msg), (int)msg.size());

        std::vector<uint8_t> buf;
        Assert(recv.recv(buf));

        AssertThat(send.sendto(recv_addr, msg), (int)msg.size());
        AssertThat(send.sendto(recv_addr, msg), (int)msg.size());
        
        Assert(recv.recv(buf));
        AssertThat(buf, msg);
        
        Assert(recv.recv(buf));
        AssertThat(buf, msg);
    }
    
    TestCase(udp_nonblocking_select)
    {
        socket send = rpp::make_udp_randomport();
        socket recv = rpp::make_udp_randomport();
        AssertFalse(send.is_blocking()); // should be false by default
        AssertFalse(recv.is_blocking()); // should be false by default
        auto recv_addr = ipaddress(AF_IPv4, "127.0.0.1", recv.port());

        // no data to receive, should return false
        AssertFalse(recv.select(100, socket::SF_Read));
        AssertTrue(recv.good());
        send.sendto(recv_addr, "udp_select");

        // must be ready to receive immediately
        rpp::Timer t1;
        AssertTrue(recv.select(100, socket::SF_Read));
        AssertTrue(recv.good());
        AssertThat(recv.recv_str(), "udp_select"s);
        AssertTrue(recv.good());
        AssertLessOrEqual(t1.elapsed_ms(), 1.0);

        // no data to receive, should return false
        AssertFalse(recv.select(100, socket::SF_Read));

        // now test two consecutive datagrams
        send.sendto(recv_addr, "udp_select1");
        send.sendto(recv_addr, "udp_select2");
        AssertTrue(recv.select(100, rpp::socket::SF_Read));
        AssertTrue(recv.good());
        AssertThat(recv.recv_str(), "udp_select1"s);
        AssertThat(recv.recv_str(), "udp_select2"s);
        AssertTrue(recv.good());

        // now test for timeout
        AssertThat(recv.available(), 0);
        rpp::Timer t2;
        AssertFalse(recv.select(250, socket::SF_Read));
        AssertTrue(recv.good());
        AssertGreaterOrEqual(t2.elapsed_ms(), 249.0);
        AssertThat(recv.available(), 0);
        AssertTrue(recv.good());
    }

    TestCase(udp_flush)
    {
        socket send = rpp::make_udp_randomport();
        socket recv = rpp::make_udp_randomport();
        auto recv_addr = ipaddress(AF_IPv4, "127.0.0.1", recv.port());

        send.sendto(recv_addr, "udp_flush");
        AssertNotEqual(recv.available(), 0);

        recv.flush();
        AssertEqual(recv.available(), 0);

        // send and flush multiple packets
        send.sendto(recv_addr, "udp_flush1xxxxxxxxxx");
        send.sendto(recv_addr, "udp_flush2xxxxxxxxxx");
        send.sendto(recv_addr, "udp_flush3xxxxxxxxxx");
        AssertNotEqual(recv.available(), 0);
        print_info("available after 3x sendto: %d\n", recv.available());
        
        print_info("available before flush: %d\n", recv.available());
        recv.flush();
        AssertEqual(recv.available(), 0);
    }

    TestCase(udp_peek)
    {
        socket send = rpp::make_udp_randomport();
        socket recv = rpp::make_udp_randomport();
        auto recv_addr = ipaddress(AF_IPv4, "127.0.0.1", recv.port());

        send.sendto(recv_addr, "udp_peek1");
        send.sendto(recv_addr, "udp_peek22");

        AssertThat(recv.peek_datagram_size(), (int)"udp_peek1"s.size());
        AssertThat(recv.peek_str(), "udp_peek1"s);
        AssertThat(recv.peek_str(), "udp_peek1"s);
        AssertThat(recv.recv_str(), "udp_peek1"s);

        AssertThat(recv.peek_datagram_size(), (int)"udp_peek22"s.size());
        AssertThat(recv.peek_str(), "udp_peek22"s);
        AssertThat(recv.peek_str(), "udp_peek22"s);
        AssertThat(recv.recv_str(), "udp_peek22"s);

        AssertThat(recv.peek_datagram_size(), 0);
        AssertThat(recv.peek_str(), ""s);
        AssertThat(recv.recv_str(), ""s);
    }

    TestCase(recv_vector_data)
    {
        socket send = rpp::make_udp_randomport();
        socket recv = rpp::make_udp_randomport();
        auto recv_addr = ipaddress(AF_IPv4, "127.0.0.1", recv.port());

        std::vector<uint8_t> v1 {'a','b','c','d'};
        std::vector<uint8_t> v2 {'e','f','g','h'};
        send.sendto(recv_addr, v1);
        send.sendto(recv_addr, v2);
        
        AssertThat(recv.recv_data(), v1);
    }

    //////////////////////////////////////////////////////////////////

    socket create(std::string msg, socket&& s)
    {
        AssertMsg(s.good(), "expected good() for '%s'", msg.c_str());
        AssertMsg(s.connected(), "expected connected() for '%s'", msg.c_str());
        if (!s.good() || !s.connected())
            print_error("%s %s socket error: %s\n", msg.c_str(), s.str().c_str(), s.last_err().c_str());
        else
            print_info("%s %s\n", msg.c_str(), s.str().c_str());
        return std::move(s);
    }
    socket listen(int port)
    {
        return create("server: listening on " + std::to_string(port), socket::listen_to({AF_IPv4, port}));
    }
    socket listen(socket&& s)
    {
        int port = s.port();
        return create("server: listening on " + std::to_string(port), std::move(s));
    }
    socket accept(const socket& server)
    {
        return create("server: accepted client", server.accept(5000/*ms*/));
    }
    socket connect(std::string ip, int port)
    {
        return create("remote: connected to "+ip+":"+std::to_string(port),
                      socket::connect_to({ip, port}, 5000/*ms*/));
    }

    /**
     * This test simulates a very simple client - server setup
     */
    TestCase(nonblocking_sockets)
    {
        socket server = listen(rpp::make_tcp_randomport()); // this is our server
        std::thread remote([this,port=server.port()] { this->nonblocking_remote(port); }); // spawn remote client
        socket client = accept(server);

        // wait 1ms for a client that will never come
        socket failClient = server.accept(1);
        Assert(failClient.bad());

        std::string msg = "Server says: Hello!";
        print_info("server: sending '%s'\n", msg.c_str());
        client.send(msg);
        sleep(100);

        std::string resp = client.recv_str();
        print_info("server: received '%s'\n", resp.c_str());
        AssertNotEqual(resp, "");
        sleep(50);

        print_info("server: closing down\n");
        client.close();
        server.close();
        remote.join(); // wait for remote thread to finish
    }

    void nonblocking_remote(int serverPort) // simulated remote endpoint
    {
        int receivedMessages = 0;
        socket server = connect("127.0.0.1", serverPort);
        while (server.connected())
        {
            std::string resp = server.recv_str();
            if (resp != "")
            {
                print_info("remote: received '%s'\n", resp.c_str());
                Assert(server.send("Client says: Thanks!") > 0);
                ++receivedMessages;
                sleep(10);
            }
            //sleep(0);
        }
        AssertThat(receivedMessages, 1);
        print_info("remote: server disconnected: %s\n", server.last_err().c_str());
        print_info("remote: closing down\n");
    }

    // localhost MTU size is unreliable, 65536 is very common,
    // but it can easily not be the case on some Windows machines
    // Use a relatively small amount which should always go through
    static constexpr int TransmitSize = 32768;

    TestCase(transmit_data)
    {
        print_info("========= TRANSMIT DATA =========\n");

        socket server = listen(rpp::make_tcp_randomport()); // this is our server
        std::thread remote([this,port=server.port()] { this->transmitting_remote(port); });
        socket client = accept(server);
        client.set_rcv_buf_size(TransmitSize);

        for (int i = 0; i < 20; ++i)
        {
            std::string data = client.recv_str();
            if (data != "")
            {
                print_info("server: received %d bytes of data from client ", (int)data.length());
                AssertThat(data.size(), TransmitSize);

                size_t j = 0;
                for (; j < data.length(); ++j)
                {
                    if (data[j] != '$')
                    {
                        print_info("(corrupted at position %d):\n", (int)j);
                        print_info("%.*s\n", 10, &data[j]);
                        print_info("^\n");
                        break;
                    }
                }
                if (j == data.length()) {
                    print_info("(valid)\n");
                }
            }
            sleep(5);
        }

        print_info("server: closing down\n");
        client.close();
        server.close();
        remote.join();
    }

    void transmitting_remote(int serverPort)
    {
        std::vector<char> sendBuffer(TransmitSize, '$');

        socket server = connect("127.0.0.1", serverPort);
        server.set_snd_buf_size(TransmitSize*2);

        while (server.connected())
        {
            int sentBytes = server.send(sendBuffer.data(), (int)sendBuffer.size());
            if (sentBytes > 0)
                print_info("remote: sent %d bytes of data\n", sentBytes);
            else
                print_info("remote: failed to send data: %s\n", socket::last_err().c_str());

            // we need to create a large gap in the data
            sleep(15);
        }
        print_info("remote: server disconnected\n");
        print_info("remote: closing down\n");
    }
};
