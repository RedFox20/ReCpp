#include "tests.h"
#include <rpp/sockets.h>
#include <thread>
#ifdef __MINGW32__ // std::thread support for MinGW...
	#include "mingw.thread.h"
#endif

using namespace rpp;

using Socket = rpp::socket;

TestImpl(test_cppsockets)
{
	Implement(test_cppsockets)
	{
		nonblocking_sockets();
		transmit_data();
	}

	static Socket create(const char* msg, Socket&& s)
	{
		Assert(s.good() && s.connected());
		printf("%s %s\n", msg, s.name().c_str());
		return std::move(s);
	}
	static Socket listen(int port)
	{
		return create("server: listening on", Socket::listen_to(port));
	}
	static Socket accept(const Socket& server)
	{
		return create("server: accepted client", server.accept(5000/*ms*/));
	}
	static Socket connect(const char* ip, int port)
	{
		return create("remote: connected to", Socket::connect_to(ip, port, 5000/*ms*/, AF_IPv4));
	}

	/**
	 * This test simulates a very simple client - server setup
	 */
	void nonblocking_sockets()
	{
		Socket server = listen(1337); // this is our server
		thread remote(nonblocking_remote); // spawn remote client
		Socket client = accept(server);

		// wait 1ms for a client that will never come
		Socket failClient = server.accept(1);
		Assert(failClient.bad());

		client.send("Server says: Hello!");
		sleep(500);

		string resp = client.recv_str();
		Assert(resp != "") else printf("%s\n", resp.c_str());
		sleep(500);

		printf("server: closing down\n");
		client.close();
		server.close();
		remote.join(); // wait for remote thread to finish
	}
	static void nonblocking_remote() // simulated remote endpoint
	{
		Socket server = connect("127.0.0.1", 1337);
		while (server.connected())
		{
			string resp = server.recv_str();
			if (resp != "")
			{
				printf("%s\n", resp.c_str());
				Assert(server.send("Client says: Thanks!") > 0);
			}
			sleep(1);
		}
		printf("remote: server disconnected\n");
		printf("remote: closing down\n");
	}

	void transmit_data()
	{
		printf("========= TRANSMIT DATA =========\n");

		Socket server = listen(1337);
		thread remote(transmitting_remote);
		Socket client = accept(server);

		for (int i = 0; i < 10; ++i)
		{
			string data = client.recv_str();
			if (data != "")
			{
				printf("server: received %d bytes of data from client ", data.length());

				size_t j = 0;
				for (; j < data.length(); ++j)
				{
					if (data[j] != '$')
					{
						printf("(corrupted at position %d):\n", j);
						printf("%.*s\n", 10, &data[j]);
						printf("^\n");
						break;
					}
				}
				if (j == data.length())
					printf("(valid)\n");
			}
			sleep(500);
		}

		printf("server: closing down\n");
		client.close();
		server.close();
		remote.join();
	}

	static void transmitting_remote()
	{
		char sendBuffer[80000];
		memset(sendBuffer, '$', sizeof sendBuffer);

		Socket server = connect("127.0.0.1", 1337);
		while (server.connected())
		{
			int sentBytes = server.send(sendBuffer, sizeof sendBuffer);
			if (sentBytes > 0)
				printf("remote: sent %d bytes of data\n", sentBytes);
			else
				printf("remote: failed to send data: %s\n", Socket::last_err().c_str());
			sleep(1000);
		}
		printf("remote: server disconnected\n");
		printf("remote: closing down\n");
	}

} Impl;
