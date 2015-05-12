#include "tests.h"
#include <sockets.h>
#include <thread>

TestImpl(sockets_test)
{
	Implement(sockets_test)
	{
		nonblocking_sockets();
	}

	void nonblocking_sockets()
	{
		// this is our server
		int server = Socket_ListenTo(1337);
		Assert(server != -1);
		printf("server: listening on sock{%d} @ %d\n", server, 1337);

		std::thread remote(nonblocking_remote);

		// wait 5s for a client
		int client = Socket_Accept(server, 5000);
		printf("server: accepted client sock{%d}\n", client);
		Assert(client != -1);

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
		int server = Socket_ConnectTo("127.0.0.1", 1337, AF_IPv4, 5000);
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
} Impl;
