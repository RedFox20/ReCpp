# ReCpp
### Reusable Standalone C++ Libraries and Modules

The goal of this project is to provide C++ programmers with a small set of reusable and functional C++ libraries. Each module is stand-alone and does not depend on other modules in this package. Each module is performance oriented and provides the least amount of overhead possible.

Initial target platforms are MSVC++ and perhaps in the future Linux GCC

### 1. io/file_io.h - Simple and fast File IO
##### io::file - Core class for reading/writing files
##### io::load_buffer - Used with io::file::read_all() to easily read all data from the file


### 2. delegate.h - Function delegates and multicast delegates (events)
##### delegate<f(a)> - Function delegate that can contain static functions, instance member functions, lambdas and functors.
##### event<void(a)> - Multicast delegate object, acts as an optimized container for registering multiple delegates to a single event.