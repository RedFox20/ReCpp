# ReCpp [![Build Status](https://travis-ci.org/RedFox20/ReCpp.svg?branch=master)](https://travis-ci.org/RedFox20/ReCpp) [![Build status](https://ci.appveyor.com/api/projects/status/atrsrssb19cgwioy?svg=true)](https://ci.appveyor.com/project/RedFox20/recpp) [![CircleCI](https://circleci.com/gh/RedFox20/ReCpp.svg?style=svg)](https://circleci.com/gh/RedFox20/ReCpp)
## Reusable Standalone C++ Libraries and Modules

The goal of this project is to provide C++14 programmers with a small set of reusable and functional C++14 libraries. Each module is stand-alone and does not depend on other modules besides `strview` from this package. Each module is performance oriented and provides the least amount of overhead possible.

Currently supported and tested platforms are VC++2017 on Windows and Clang 3.8 on Ubuntu,iOS 

### [rpp/strview.h](rpp/strview.h) - A lightweight and powerful string view class
This is definitely one of the core classes of ReCpp. It's the only overarching dependency because of its simplicity
and usefulness. Currently only char* strview is supported because all the games and applications where strview has
been used rely on UTF-8.

The string view header also provides basic example parsers such as `line_parser`, `keyval_parser` and `bracket_parser`.
It's extremely easy to create a blazing fast parser with `rpp::strview`

TODO: Add code examples on how to use strview for parsing

### [rpp/file_io.h](rpp/file_io.h) - Simple and fast File IO
This is an extremely useful cross-platform file and filesystem module. It provides all the basic
functionality for most common file operations and path manipulations.

### Example of using file_io for basic file manipulation
```cpp
#include <rpp/file_io.h>
using namespace rpp;

void fileio_read_sample(strview filename = "README.md"_sv)
{
    if (file f = { filename, READONLY })
    {
        // reads all data in the most efficient way
        load_buffer data = f.read_all(); 
        
        // use the data as a binary blob
        for (char& ch : data) { }
    }
}
void fileio_writeall_sample(strview filename = "test.txt"_sv)
{
    string someText = "/**\n * A simple self-expanding buffer\n */\nstruct";
    
    // write a new file with the contents of someText
    file::write_new(filename, someText.data(), someText.size());
    
    // or just write it as a string 
    file::write_new(filename, someText);
}
void fileio_info_sample(strview file = "README.md"_sv)
{
    if (file_exists(file))
    {
        printf(" === %s === \n", file.str);
        printf("  file_size     : %d\n",   file_size(file));
        printf("  file_modified : %llu\n", file_modified(file));
        printf("  full_path     : %s\n",   full_path(file).data());
        printf("  file_name     : %s\n",   file_name(file).data());
        printf("  folder_name   : %s\n",   folder_name(full_path(file)).data());
    }
}
void fileio_path_manipulation(strview file = "/root/../path\\example.txt"_sv)
{
    printf(" === %s === \n", file.str);
    printf("  full_path     : %s\n", full_path(file).data());
    printf("  merge_dirups  : %s\n", merge_dirups(file).data());
    printf("  file_name     : %s\n", file_name(file).str);
    printf("  file_nameext  : %s\n", file_nameext(file).str);
    printf("  file_ext      : %s\n", file_ext(file).str);
    printf("  folder_name   : %s\n", file_ext(file).str);
    printf("  folder_path   : %s\n", file_ext(file).str);
    printf("  normalized    : %s\n", normalized(file).data());
    printf("  path_combine  : %s\n", path_combine(folder_name(file), "another.txt").data());
}
void fileio_listing_dirs(strview path = "../"_sv)
{
    printf(" working dir   : %s\n", working_dir().data());
    printf(" relative path : %s\n", path.str);
    printf(" full path     : %s\n", full_path(path).data());
    
    vector<string> cppFiles = list_files_recursive(path, "cpp");
    for (auto& relativePath : cppFiles)
        printf("  source  : %s\n", relativePath.data());
    
    vector<string> headerFiles = list_files_fullpath("./rpp", "h");
    for (auto& fullPath : headerFiles)
        printf("  header  : %s\n", fullPath.data());
}
```

### [rpp/delegate.h](rpp/delegate.h) - Fast function delegates and multicast delegates (events)
| Class               | Description                                              |
| ------------------- | -------------------------------------------------------- |
| `delegate<f(a)>`  | Function delegate that can contain static functions, instance member functions, lambdas and functors. |
| `event<void(a)>`  | Multicast delegate object, acts as an optimized container for registering multiple delegates to a single event. |
### Examples of using delegates for any convenient case
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

### [rpp/vec.h](rpp/vec.h) Everything you need to write a 3D and 2D game in Modern OpenGL
Contains basic Vector2, Vector3, Vector4 and Matrix4 types with a plethora of extremely useful utility functions.
Main features are convenient operators, SSE2 intrinsics and GLSL style vector swizzling.

TODO: Add code examples on how to use vec for basic vector math


