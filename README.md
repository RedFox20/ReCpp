# ReCpp
### Reusable Standalone C++ Libraries and Modules

The goal of this project is to provide C++ programmers with a small set of reusable and functional C++ libraries. Each module is stand-alone and does not depend on other modules in this package. Each module is performance oriented and provides the least amount of overhead possible.

Initial target platforms are MSVC++ and perhaps in the future Linux GCC

#### `io/file_io.h` Simple and fast File IO
| Class               | Description                                                 |
| ------------------- | ----------------------------------------------------------  |
| `io::file`        | Core class for reading/writing files                        |
| `io::load_buffer` | Used with io::file::read_all() to read all data from a file |


#### `delegate.h` Function delegates and multicast delegates (events)
| Class               | Description                                                 |
| ------------------- | ----------------------------------------------------------  |
| `delegate<f(a)>`  | Function delegate that can contain static functions, instance member functions, lambdas and functors. |
| `event<void(a)>`  | Multicast delegate object, acts as an optimized container for registering multiple delegates to a single event. |

#### `sockets.h` Refined convenience layer on top of Winsock/UNIX sockets designed for TCP/UDP based packets.
##### Low-Level Sockets API
| Socket Method                     | Basic Description                                                     |
| --------------------------------- | --------------------------------------------------------------------- |
| `Socket_ListenTo(port)`           | Creates a nonblocking listener socket (defaults to TCP)               |
| `Socket_Accept(server)`           | Accepts any pending connections in a nonblocking way                  |
| `Socket_Accept(server,timeout)`   | Waits for any pending connections until the timeout value is reached  |
| `Socket_ConnectTo(host,port,af)`  | Connects to a specified server in a nonblocking way (defaults to TCP) |
| `Socket_ConnectTo(h,p,a,timeout)` | ConnectTo that blocks until timeout is reached                        |
| `Socket_Recv(sock,data,len)`      | A nonblocking receive method                                          |
| `Socket_Send(sock,data,len)`      | A typical send method                                                 |
| `Socket_RecvStr(sock)`            | Receives available bytes as a string                                  |
| `Socket_SendStr(sock,str)`        | Sends a null terminated string to the socket                          |
| `Socket_Available(sock)`          | Returns the number of bytes in recv buffer through ioctl              |
| `Socket_IsConnected(sock)`        | Checks if the socket is still connected and operational               |

##### Example server and client code using the low-level Sockets API
 ```cpp
 void server_example()
 {
 	int server = Socket_ListenTo(1337);       // listen on localhost:1337
 	int client = Socket_Accept(server, 5000); // wait 5s for only 1 client
 	
 	while (Socket_IsConnected(client))
 	{
 		if (Socket_Available(client) > 0)
 			printf("Client says: %s\n", Socket_RecvStr(client).c_str());
 		
 		Socket_SendStr(client, "Hello dear client");
 	}
 }
 void client_example()
 {
 	// connect to localhost:1337 using IPv4 and timeout of 5s
 	int server = Socket_ConnectTo("127.0.0.1", 1337, AF_IPv4, 5000);
 	
 	while (Socket_IsConnected(server))
 	{
 		if (Socket_Available(server))
 			printf("Server says: %s\n", Socket_RecvStr(server).c_str());
 		
 		Socket_SendStr(client, "Gimme auth plz");
 	}
 }
 ```
