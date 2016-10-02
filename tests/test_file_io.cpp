#include "tests.h"
#include <file_io.h>
#include <fstream> // 

using namespace rpp;

TestImpl(test_file_io)
{
    string FileName = "README.md"s;
    int    FileSize = 0;

    TestInit(test_file_io)
    {
        printf("working_dir: %s\n", working_dir().c_str());

        int mode = ios::in | ios::binary;
        ifstream refFile { FileName, mode };
        if (refFile.bad()) {
            FileName = "../" + FileName;
            refFile.open(FileName.c_str(), mode);
        }
        Assert(refFile.good());
        FileSize = (int)refFile.seekg(0, SEEK_END).tellg();
    }

    TestCase(basic_file)
    {
        file f(FileName);
        Assert(f.good());
        Assert(f.size() > 0);
        Assert(f.size() == FileSize);
    }

    TestCase(exists)
    {
        Assert(file_exists(__FILE__));
        Assert(!file_exists("/complete/rubbish/path.txt"));

        string dir = working_dir();
        Assert(folder_exists(dir));
        Assert(folder_exists(dir + "/"));
        Assert(!folder_exists("/complete/rubbish/path"));
    }

    TestCase(size)
    {
        Assert(file_size(FileName) == FileSize);
        Assert(file_size(FileName) == (int)file_sizel(FileName));
    }

    TestCase(create_delete_folder)
    {
        Assert(create_folder("./test/folder/path"));
        Assert(folder_exists("./test/folder/path"));
        Assert(delete_folder("./test/folder/path", true/*recursive*/));
    }

    TestCase(path_utils)
    {
        Assert(file_name("/root/dir/file.ext") == "file");
        Assert(file_name("/root/dir/file")     == "file");
        Assert(file_name("/root/dir/")         == "");
        Assert(file_name("file.ext")           == "file");
        Assert(file_name("")                   == "");

        Assert(file_nameext("/root/dir/file.ext") == "file.ext");
        Assert(file_nameext("/root/dir/file")     == "file");
        Assert(file_nameext("/root/dir/")         == "");
        Assert(file_nameext("file.ext")           == "file.ext");
        Assert(file_nameext("")                   == "");

        Assert(folder_name("/root/dir/file.ext") == "dir");
        Assert(folder_name("/root/dir/file")     == "dir");
        Assert(folder_name("/root/dir/")         == "dir");
        Assert(folder_name("dir/")               == "dir");
        Assert(folder_name("file.ext")           == "");
        Assert(folder_name("")                   == "");

        Assert(folder_path("/root/dir/file.ext") == "/root/dir/");
        Assert(folder_path("/root/dir/file")     == "/root/dir/");
        Assert(folder_path("/root/dir/")         == "/root/dir/");
        Assert(folder_path("dir/")               == "dir/");
        Assert(folder_path("file.ext")           == "");
        Assert(folder_path("")                   == "");

        Assert(normalized("/root\\dir\\file.ext", '/') == "/root/dir/file.ext");
        Assert(normalized("\\root/dir/file.ext",  '/') == "/root/dir/file.ext");

        Assert(normalized("/root\\dir\\file.ext", '\\') == "\\root\\dir\\file.ext");
        Assert(normalized("\\root/dir/file.ext",  '\\') == "\\root\\dir\\file.ext");
    }


} Impl;
