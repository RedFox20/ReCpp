#include "tests.h"
#include <sockets.h>
#include <thread>

TestImpl(test_sockets)
{
	Implement(test_sockets)
	{
		nonblocking_sockets();
		transmit_data();
	}

	int Server_ListenTo(int port)
	{
		int server = Socket_ListenTo(port);
		Assert(server != -1);
		printf("server: listening on sock{%d} @ %d\n", server, port);
		return server;
	}

	int Server_AcceptClient(int server)
	{
		// wait 5s for a client
		int client = Socket_Accept(server, 5000);
		printf("server: accepted client sock{%d}\n", client);
		Assert(client != -1);
		return client;
	}

	void nonblocking_sockets()
	{
		int server = Server_ListenTo(1337); // this is our server
		std::thread remote(nonblocking_remote); // spawn remote client

		int client = Server_AcceptClient(server);
		// wait 1ms for a client that will never come
		int failClient = Socket_Accept(server, 1);
		Assert(failClient == -1);

		Socket_SendStr(client, "Server says: Hello!");
		sleep(500);

		std::string resp = Socket_RecvStr(client);
		Assert(resp != "") else printf("%s\n", resp.c_str());
		sleep(500);

		printf("server: closing down\n");
		Socket_Close(client);
		Socket_Close(server);

		remote.join(); // wait for remote thread to finish
	}

	// simulated remote endpoint
	static void nonblocking_remote()
	{
		int server = Socket_ConnectTo("127.0.0.1", 1337, 5000, AF_IPv4);
		Assert(server != -1);

		printf("remote: connected to server sock{%d}\n", server);

		while (Socket_IsConnected(server))
		{
			std::string resp = Socket_RecvStr(server);
			if (resp != "")
			{
				printf("%s\n", resp.c_str());
				Assert(Socket_SendStr(server, "Client says: Thanks!") > 0);
			}
			sleep(1);
		}
		printf("remote: server disconnected\n");
		printf("remote: closing down\n");
	}

	void transmit_data()
	{
		printf("========= TRANSMIT DATA =========\n");

		int server = Server_ListenTo(1337);
		std::thread remote(transmitting_remote);
		int client = Server_AcceptClient(server);

		for (int i = 0; i < 10; ++i)
		{
			std::string data = Socket_RecvStr(client);
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
		Socket_Close(client);
		Socket_Close(server);
		remote.join();
	}

	static void transmitting_remote()
	{
		int server = Socket_ConnectTo("127.0.0.1", 1337, 5000, AF_IPv4);
		Assert(server != -1);

		printf("remote: connected to server sock{%d}\n", server);

		char sendBuffer[80000];
		memset(sendBuffer, '$', sizeof sendBuffer);

		while (Socket_IsConnected(server))
		{
			int sentBytes = Socket_Send(server, sendBuffer, sizeof sendBuffer);
			if (sentBytes > 0)
				printf("remote: sent %d bytes of data\n", sentBytes);
			else
				printf("remote: failed to send data: %s\n", Socket_LastErrStr().c_str());

			sleep(1000);
		}
		printf("remote: server disconnected\n");
		printf("remote: closing down\n");
	}

} Impl;
