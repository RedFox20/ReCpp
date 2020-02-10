#include <rpp/sockets.h>
#include <rpp/debugging.h>
#include <rpp/tests.h>
#include <thread>

using std::thread;
using namespace rpp;
using Socket = rpp::socket;

TestImpl(test_sockets)
{
    TestInit(test_sockets)
    {
    }

    Socket create(const char* msg, Socket&& s)
    {
        AssertMsg(s.good(), "expected good() for '%s'", msg);
        AssertMsg(s.connected(), "expected connected() for '%s'", msg);
        printf("%s %s\n", msg, s.name().c_str());
        return std::move(s);
    }
    Socket listen(int port)
    {
        return create("server: listening on", Socket::listen_to(port));
    }
    Socket accept(const Socket& server)
    {
        return create("server: accepted client", server.accept(5000/*ms*/));
    }
    Socket connect(const char* ip, int port)
    {
        return create("remote: connected to", Socket::connect_to(ip, port, 5000/*ms*/, AF_IPv4));
    }

    /**
     * This test simulates a very simple client - server setup
     */
    TestCase(nonblocking_sockets)
    {
        Socket server = listen(1337); // this is our server
        thread remote([=] { nonblocking_remote(); }); // spawn remote client
        Socket client = accept(server);

        // wait 1ms for a client that will never come
        Socket failClient = server.accept(1);
        Assert(failClient.bad());

        std::string msg = "Server says: Hello!";
        printf("server: sending '%s'\n", msg.c_str());
        client.send(msg);
        sleep(100);

        string resp = client.recv_str();
        if (!Assert(resp != ""))
            printf("%s\n", resp.c_str());
        sleep(50);

        printf("server: closing down\n");
        client.close();
        server.close();
        remote.join(); // wait for remote thread to finish
    }
    void nonblocking_remote() // simulated remote endpoint
    {
        int receivedMessages = 0;
        Socket server = connect("127.0.0.1", 1337);
        while (server.connected())
        {
            string resp = server.recv_str();
            if (resp != "")
            {
                printf("remote: received '%s'\n", resp.c_str());
                Assert(server.send("Client says: Thanks!") > 0);
                ++receivedMessages;
            }
            //sleep(0);
        }
        AssertThat(receivedMessages, 1);
        printf("remote: server disconnected: %s\n", server.last_err().c_str());
        printf("remote: closing down\n");
    }

    static constexpr int TransmitSize = 80000;
    TestCase(transmit_data)
    {
        printf("========= TRANSMIT DATA =========\n");

        Socket server = listen(1337);
        thread remote([=] { this->transmitting_remote(); });
        Socket client = accept(server);
        client.set_rcv_buf_size(TransmitSize*2);

        for (int i = 0; i < 20; ++i)
        {
            string data = client.recv_str();
            if (data != "")
            {
                printf("server: received %d bytes of data from client ", (int)data.length());
                AssertThat(data.size(), TransmitSize);

                size_t j = 0;
                for (; j < data.length(); ++j)
                {
                    if (data[j] != '$')
                    {
                        printf("(corrupted at position %d):\n", (int)j);
                        printf("%.*s\n", 10, &data[j]);
                        printf("^\n");
                        break;
                    }
                }
                if (j == data.length()) {
                    printf("(valid)\n");
                }
            }
            sleep(5);
        }

        printf("server: closing down\n");
        client.close();
        server.close();
        remote.join();
    }

    void transmitting_remote()
    {
        char sendBuffer[TransmitSize];
        memset(sendBuffer, '$', sizeof sendBuffer);

        Socket server = connect("127.0.0.1", 1337);
        server.set_snd_buf_size(TransmitSize*2);

        while (server.connected())
        {
            int sentBytes = server.send(sendBuffer, sizeof sendBuffer);
            if (sentBytes > 0)
                printf("remote: sent %d bytes of data\n", sentBytes);
            else
                printf("remote: failed to send data: %s\n", Socket::last_err().c_str());

            // we need to create a large gap in the data
            sleep(15);
        }
        printf("remote: server disconnected\n");
        printf("remote: closing down\n");
    }

    TestCase(socket_udp_send_receive)
    {
        vector<uint8_t> msg(4000, 'x');
        Socket sender   = rpp::make_udp_randomport();
        Socket receiver = rpp::make_udp_randomport();

        auto send_to = ipaddress(AF_IPv4, "127.0.0.1", receiver.port());
        AssertThat(sender.sendto(send_to, msg), msg.size());

        vector<uint8_t> buf;
        Assert(receiver.recv(buf));

        AssertThat(sender.sendto(send_to, msg), msg.size());
        AssertThat(sender.sendto(send_to, msg), msg.size());
        
        Assert(receiver.recv(buf));
        AssertThat(buf, msg);
        
        Assert(receiver.recv(buf));
        AssertThat(buf, msg);
    }

    TestCase(udp_socket_options)
    {
        Socket sock = rpp::make_udp_randomport();
        AssertFalse(sock.is_blocking()); // should be false by default

        Assert(sock.set_blocking(true));
        Assert(sock.is_blocking());

        LogInfo("default SO_RCVBUF: %zu", sock.get_rcv_buf_size());
        LogInfo("default SO_SNDBUF: %zu", sock.get_snd_buf_size());

        // NOTE: if there is a mismatch here, then some unix-like kernel didn't double the buffer
        //       which is expected behaviour on non-windows platforms
        Assert(sock.set_snd_buf_size(16384));
        AssertThat(sock.get_snd_buf_size(), 16384);

        Assert(sock.set_rcv_buf_size(32768));
        AssertThat(sock.get_rcv_buf_size(), 32768);
    }
};
