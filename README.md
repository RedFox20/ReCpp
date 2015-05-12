# ReCpp
### Reusable Standalone C++ Libraries and Modules

The goal of this project is to provide C++ programmers with a small set of reusable and functional C++ libraries. Each module is stand-alone and does not depend on other modules in this package. Each module is performance oriented and provides the least amount of overhead possible.

Initial target platforms are MSVC++ and perhaps in the future Linux GCC

### 1. io/file_io.h - Simple and fast File IO
##### io::file        - Core class for reading/writing files
##### io::load_buffer - Used with io::file::read_all() to easily read all data from the file


### 2. delegate.h - Function delegates and multicast delegates (events)
##### delegate<f(a)> - Function delegate that can contain static functions, instance member functions, lambdas and functors.
##### event<void(a)> - Multicast delegate object, acts as an optimized container for registering multiple delegates to a single event.

### 3. sockets.h - Refined convenience layer on top of Winsock/UNIX sockets designed for TCP/UDP based packets.
##### Socket_ListenTo(port)             - Creates a nonblocking listener socket (defaults to TCP)
##### Socket_Accept(server)             - Accepts any pending connections in a nonblocking way
##### Socket_Accept(server,timeout)     - Waits for any pending connections until the timeout value is reached
##### Socket_ConnectTo(host, port, af)  - Connects to a specified server in a nonblocking way (defaults to TCP)
##### Socket_ConnectTo(host,port,af,timeout) - ConnectTo with the possibility of waiting if the server takes too long to respond
##### Socket_Recv(sock,data,len)        - A nonblocking receive method
##### Socket_Send(sock,data,len)        - A typical send method
##### Socket_RecvStr(sock)              - Receives available bytes as an std::string, if there is no data "" is returned
##### Socket_SendStr(sock,str)          - Sends a null terminated string to the socket
##### Socket_Available(sock)            - Returns the number of bytes in recv buffer through ioctl
##### Socket_IsConnected(sock)          - Checks if the socket is still connected and no critical errors occurred