# ReCpp
### Reusable Standalone C++ Libraries and Modules

The goal of this project is to provide C++ programmers with a small set of reusable and functional C++ libraries. Each module is stand-alone and does not depend on other modules in this package. Each module is performance oriented and provides the least amount of overhead possible.

Initial target platforms are MSVC++ and perhaps in the future Linux GCC

#### `io/file_io.h` Simple and fast File IO
| Class                | Description                                                 |
| -------------------- | ----------------------------------------------------------  |
| `io::file`           | Core class for reading/writing files                        |
| `io::load_buffer`    | Used with io::file::read_all() to read all data from a file |
| `io::dirwatch`       | Helper class for monitoring directory changes at os level   |

| IO Method                | Description                                         |
| ------------------------ | --------------------------------------------------- |
| `file_exists(path)`      | Checks if a file exists with the specified path     |
| `folder_exists(path)`    | Checks if a folder exists with the specified path   |
| `file_size(path)`        | Gets the size of a file with the specified path     |
| `file_sizel(path)`       | Gets the int64 size of a file with the specified path |
| `file_modified(path)`    | Gets the file modification time                     |
| `delete_folder(path)`    | Deletes the specified folder with all its contents  |
| `list_dirs(v,dir,ptrn)`  | Lists all subdirectories at the specified path      |
| `list_files(v,dir,ptrn)` | Lists all files at the specified path               |
| `working_dir()`          | (char*)  Gets the working directory of the process   |
| `working_dirw()`         | (wchar*) Gets the working directory of the process   |
| `working_dir(newdir)`    | (char*)  Sets the working directory of the process   |
| `working_dirw(newdir)`   | (wchar*) Sets the working directory of the process   |
| `full_path(relpath)`     | Gets the fullpath of a relative path                 |
| `file_name(filepath)`    | Gets the filename part of a filepath                 |
| `folder_name(path)`      | Gets the foldername part of a path                   |

##### Example of using file_io for basic file manipulation
```cpp
using namespace io;

void fileio_read_sample(const char* filename = "./README.md")
{
	if (file f = file(filename, READONLY))
	{
		// reads all data in the most efficient way
		load_buffer data = f.read_all(); 
		
		// use the data as a binary blob
		for (char& ch : data) { }
	}
}
void fileio_writeall_sample(const char* file = "./test.txt")
{
	std::string someText = "/**\n * A simple self-expanding buffer\n */\nstruct";
	
	// write a new file with the contents of someText
	file::write_new(file, someText.data(), someText.size());
}
void fileio_methods_sample(const char* file = "./README.md")
{
	if (file_exists(file))
	{
		printf(" === %s === \n", file);
		printf("  file_size     : %d\n",   file_size(file));
		printf("  file_modified : %llu\n", file_modified(file));
		printf("  full_path     : %s\n",   full_path(file).c_str());
		printf("  file_name     : %s\n",   file_name(file).c_str());
		printf("  folder_name   : %s\n",   folder_name(full_path(file)).c_str());
	}
}
```

#### `delegate.h` Function delegates and multicast delegates (events)
| Class               | Description                                              |
| ------------------- | -------------------------------------------------------- |
| `delegate<f(a)>`  | Function delegate that can contain static functions, instance member functions, lambdas and functors. |
| `event<void(a)>`  | Multicast delegate object, acts as an optimized container for registering multiple delegates to a single event. |
##### Examples of using delegates for any convenient case
```cpp
void delegate_samples()
{
	// create a new function delegate - in this case using a lambda
	delegate<void(int)> fn = [](int a) { 
		printf("lambda %d!\n", a); 
	};
	
	// invoke the delegate like any other function
	fn(42);
}
void event_sample()
{
	// create an event container
	event<void(int,int)> onMouseMove;

	// add a single notification target (more can be added if needed)
	onMouseMove += [](int x, int y) { 
		printf("mx %d, my %d\n", x, y); 
	};
	
	// call the event and multicast to all registered callbacks
	onMouseMove(22, 34);
}
```

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
 		
 		Socket_SendStr(server, "Gimme auth plz");
 	}
 }
 ```
