# ReCpp
## Reusable Standalone C++ Libraries and Modules

The goal of this project is to provide C++14 programmers with a small set of reusable and functional C++14 libraries. Each module is stand-alone and does not depend on other modules besides `strview` from this package. Each module is performance oriented and provides the least amount of overhead possible.

Currently supported and tested platforms are VC++2017 on Windows and Clang 3.8 on Ubuntu,iOS 

### rpp/strview.h Lightweight and powerful string view class with several years of proved production ready capability.
This is definitely one of the core classes of ReCpp. It's the only overarching dependency because of its simplicity
and usefulness. Currently only char* strview is supported because all the games and applications where strview has
been used rely on UTF-8.

The string view header also provides basic example parsers such as `line_parser`, `keyval_parser` and `bracket_parser`.
It's extremely easy to create a blazing fast parser with `rpp::strview`


TODO: Brief overview of rpp::strview methods

```cpp
struct strview
{
    const char* str;
    int len;
}
```

TODO: Add code examples on how to use strview for parsing

### `rpp/file_io.h` Simple and fast File IO
This is an extremely useful cross-platform file and filesystem module. It provides all the basic
functionality for most common file operations and path manipulations.

Brief overview of core types and prototypes
```cpp
    enum IOFlags
    {
        READONLY,           // opens an existing file for reading
        READWRITE,          // opens an existing file for read/write
        CREATENEW,          // creates new file for writing
        APPEND,             // opens an existing file for appending only
    };

    struct load_buffer // Used with io::file::read_all() to get all data from a file
    {
        load_buffer(char* buffer, int size);
        ~load_buffer();
        int   size() const;
        char* data() const;
    };

    class file
    {
        file(const strview& filename, IOFlags mode = READONLY);
        file(const wstring& filename, IOFlags mode = READONLY);
        bool open(const strview filename, IOFlags mode = READONLY);
        void close();
        void flush();
        
        bool good() const;
        bool bad() const;
        int size() const;
        int64 sizel() const;
        int read(void* buffer, int bytesToRead);
        load_buffer read_all();

        static load_buffer read_all(const strview& filename);
        static load_buffer read_all(const wchar_t* filename);

        int write(const void* buffer, int bytesToWrite);
        int write(const strview& str);
        int writef(const char* format, ...); // Writes a formatted string to file
        static int write_new(const strview& filename, const void* buffer, int bytesToWrite);

        int seek(int filepos, int seekmode = SEEK_SET);
        uint64 seekl(int64 filepos, int seekmode = SEEK_SET);
        int tell() const;
        int64 tell64() const;

        bool time_info(time_t* outCreated, time_t* outAccessed, time_t* outModified) const;
        time_t time_created() const;
        time_t time_accessed() const;
        time_t time_modified() const;
        int size_and_time_modified(time_t* outModified) const;
    };


    bool file_exists(const strview filename);
    bool folder_exists(const strview folder);

    int   file_size(const strview filename);
    int64 file_sizel(const strview filename);
    time_t file_created(const strview filename);
    time_t file_accessed(const strview filename);
    time_t file_modified(const strview filename);

    // Deletes a single file, ex: "root/dir/file.ext"
    delete_file(const strview filename);

    // Creates a folder, recursively creating folders that do not exist
    // TRUE if the final folder was actually created (can fail due to access rights)
    bool create_folder(const strview foldername);

    // Deletes a folder, by default only if it's empty.
    bool delete_folder(const strview foldername, bool recursive = false);

    // Resolves a relative path to a full path name using filesystem path resolution
    string full_path(const strview path);

    // merges all ../ of a full path
    string merge_dirups(const strview path);

    // Extract the filename (no extension) from a file path
    //        Ex: /root/dir/file.ext ==> file
    strview file_name(const strview path);

    // Extract the file part (with ext) from a file path
    //        Ex: /root/dir/file.ext ==> file.ext
    strview file_nameext(const strview path);

    // Extract the extension from a file path
    //        Ex: /root/dir/file.ext ==> ext
    strview file_ext(const strview path);

    // Extract the foldername from a path name
    //        Ex: /root/dir/file.ext ==> dir
    strview folder_name(const strview path);

    // Extracts the full folder path from a file path.
    strview folder_path(const strview path);
    wstring folder_path(const wstring& path);

    // Normalizes the path string to use a specific type of slash
    string normalized(const strview path, char sep = '/');
    
    // Efficiently combines two path strings, removing any repeated / or \
    string path_combine(strview path1, strview path2);


    /// Lists all folders inside this directory
    vector<string> list_dirs(strview dir);
    vector<string> list_dirs_fullpath(strview dir);

    // Lists all files inside this directory that have the specified extension (default: all files)
    vector<string> list_files(strview dir, strview ext = {});
    vector<string> list_files_fullpath(strview dir, strview ext = {});
    
    vector<string> list_files_recursive(strview dir, strview ext = {});
    vector<string> list_files_recursive(strview dir, const vector<strview>& exts);

    string working_dir();
    bool change_dir(const strview new_wd);
```

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

### `rpp/delegate.h` Fast function delegates and multicast delegates (events)
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

### `rpp/vec.h` Fast function delegates and multicast delegates (events)

