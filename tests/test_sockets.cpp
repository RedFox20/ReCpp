#include <rpp/sockets.h>
#include <rpp/timer.h>
#include <rpp/tests.h>
#include <thread>

using namespace rpp;
using Socket = rpp::socket;

TestImpl(test_sockets)
{
    //////////////////////////////////////////////////////////////////

    TestInit(test_sockets) {}

    TestCase(udp_socket_options)
    {
        Socket sock = rpp::make_udp_randomport();
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
        Socket sock = rpp::make_tcp_randomport();
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
        Socket send = rpp::make_udp_randomport();
        Socket recv = rpp::make_udp_randomport();
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
        Socket send = rpp::make_udp_randomport();
        Socket recv = rpp::make_udp_randomport();
        AssertFalse(send.is_blocking()); // should be false by default
        AssertFalse(recv.is_blocking()); // should be false by default
        auto recv_addr = ipaddress(AF_IPv4, "127.0.0.1", recv.port());

        // no data to receive, should return false
        AssertFalse(recv.select(100, rpp::socket::SF_Read));
        AssertTrue(recv.good());
        send.sendto(recv_addr, "udp_select");

        // must be ready to receive immediately
        rpp::Timer t1;
        AssertTrue(recv.select(100, rpp::socket::SF_Read));
        AssertTrue(recv.good());
        AssertThat(recv.recv_str(), "udp_select"s);
        AssertTrue(recv.good());
        AssertLessOrEqual(t1.elapsed_ms(), 1.0);

        // no data to receive, should return false
        AssertFalse(recv.select(100, rpp::socket::SF_Read));

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
        AssertFalse(recv.select(250, rpp::socket::SF_Read));
        AssertTrue(recv.good());
        AssertGreaterOrEqual(t2.elapsed_ms(), 249.0);
        AssertThat(recv.available(), 0);
        AssertTrue(recv.good());
    }

    TestCase(udp_flush)
    {
        Socket send = rpp::make_udp_randomport();
        Socket recv = rpp::make_udp_randomport();
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
        Socket send = rpp::make_udp_randomport();
        Socket recv = rpp::make_udp_randomport();
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
        Socket send = rpp::make_udp_randomport();
        Socket recv = rpp::make_udp_randomport();
        auto recv_addr = ipaddress(AF_IPv4, "127.0.0.1", recv.port());

        std::vector<uint8_t> v1 {'a','b','c','d'};
        std::vector<uint8_t> v2 {'e','f','g','h'};
        send.sendto(recv_addr, v1);
        send.sendto(recv_addr, v2);
        
        AssertThat(recv.recv_data(), v1);
    }

    //////////////////////////////////////////////////////////////////

    Socket create(std::string msg, Socket&& s)
    {
        AssertMsg(s.good(), "expected good() for '%s'", msg.c_str());
        AssertMsg(s.connected(), "expected connected() for '%s'", msg.c_str());
        if (!s.good() || !s.connected())
            print_error("%s %s socket error: %s\n", msg.c_str(), s.name().c_str(), s.last_err().c_str());
        else
            print_info("%s %s\n", msg.c_str(), s.name().c_str());
        return std::move(s);
    }
    Socket listen(int port)
    {
        return create("server: listening on " + std::to_string(port), Socket::listen_to(port));
    }
    Socket accept(const Socket& server)
    {
        return create("server: accepted client", server.accept(5000/*ms*/));
    }
    Socket connect(std::string ip, int port)
    {
        return create("remote: connected to "+ip+":"+std::to_string(port),
                      Socket::connect_to(ip.c_str(), port, 5000/*ms*/, AF_IPv4));
    }

    /**
     * This test simulates a very simple client - server setup
     */
    TestCase(nonblocking_sockets)
    {
        Socket server = listen(13337); // this is our server
        std::thread remote([=] { nonblocking_remote(13337); }); // spawn remote client
        Socket client = accept(server);

        // wait 1ms for a client that will never come
        Socket failClient = server.accept(1);
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
        Socket server = connect("127.0.0.1", serverPort);
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

    // 65536 is the typical localhost MTU for datagrams
    static constexpr int TransmitSize = 65536;

    TestCase(transmit_data)
    {
        print_info("========= TRANSMIT DATA =========\n");

        Socket server = listen(14447);
        std::thread remote([=] { this->transmitting_remote(14447); });
        Socket client = accept(server);
        client.set_rcv_buf_size(TransmitSize*2);

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

        Socket server = connect("127.0.0.1", serverPort);
        server.set_snd_buf_size(TransmitSize*2);

        while (server.connected())
        {
            int sentBytes = server.send(sendBuffer.data(), (int)sendBuffer.size());
            if (sentBytes > 0)
                print_info("remote: sent %d bytes of data\n", sentBytes);
            else
                print_info("remote: failed to send data: %s\n", Socket::last_err().c_str());

            // we need to create a large gap in the data
            sleep(15);
        }
        print_info("remote: server disconnected\n");
        print_info("remote: closing down\n");
    }
};
