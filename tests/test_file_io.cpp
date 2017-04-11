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
        AssertMsg(refFile.good(), "Test file '%s' is missing", FileName.c_str());
        FileSize = (int)refFile.seekg(0, SEEK_END).tellg();
    }

    TestCleanup(test_file_io)
    {
        if (folder_exists("./test_tmp"))
            Assert(delete_folder("test_tmp", true/*recursive*/));
    }

    TestCase(basic_file)
    {
        file f = { FileName };
        Assert(f.good());
        Assert(!f.bad());
        Assert(f.size() > 0);
        Assert(f.size() == FileSize);
    }

    TestCase(if_initializer)
    {
        if (file f = file(FileName))
        {
            Assert(f.good() && !f.bad());
        }
        else Assert(f.good() && !f.bad());
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
        AssertThat(file_size(FileName),  FileSize);
        AssertThat(file_sizel(FileName), (int64)FileSize);
    }

    TestCase(write_size_sanity)
    {
        Assert(create_folder("./test_tmp"));
        file f = file("./test_tmp/_size_sanity_test.txt", CREATENEW);
        Assert(f.good());
        int expectedSize = 0;
        srand((uint)time(0));
        for (int i = 0; i < 10; ++i)
        {
            int randCount = rand() % 8192;
            auto data = vector<char>(randCount, 'A');
            int written = f.write(data.data(), (int)data.size());
            AssertThat(written, randCount);
            expectedSize += randCount;
        }
        
        int fileSize = f.size();
        AssertThat(fileSize, expectedSize);
        f.close();
    }

    TestCase(create_delete_folder)
    {
        Assert(create_folder("") == false);
        Assert(create_folder("./") == false);

        // these tests are extremely volatile, don't run without a step-in debugger
        //Assert(create_folder("dangerous"));
        //Assert(change_dir("dangerous"));
        //Assert(delete_folder("", true) == false); // can delete system root dir
        //Assert(delete_folder("./", true) == false); // may accidentally delete current folder
        //Assert(change_dir(".."));
        //Assert(delete_folder("dangerous"));

        Assert(create_folder("./test_tmp/folder/path"));
        Assert(folder_exists("./test_tmp/folder/path"));
        Assert(delete_folder("./test_tmp/", true/*recursive*/));

        Assert(create_folder("test_tmp/folder/path"));
        Assert(folder_exists("test_tmp/folder/path"));
        Assert(delete_folder("test_tmp", true/*recursive*/));
    }

    TestCase(path_utils)
    {
        AssertThat(file_name("/root/dir/file.ext"   ), "file");
        AssertThat(file_name("/root/dir/file"       ), "file");
        AssertThat(file_name("/root/dir/"           ), "");
        AssertThat(file_name("file.ext"             ), "file");
        AssertThat(file_name(""                     ), "");

        AssertThat(file_nameext("/root/dir/file.ext"), "file.ext");
        AssertThat(file_nameext("/root/dir/file"    ), "file");
        AssertThat(file_nameext("/root/dir/"        ), "");
        AssertThat(file_nameext("file.ext"          ), "file.ext");
        AssertThat(file_nameext(""                  ), "");

        AssertThat(folder_name("/root/dir/file.ext" ), "dir");
        AssertThat(folder_name("/root/dir/file"     ), "dir");
        AssertThat(folder_name("/root/dir/"         ), "dir");
        AssertThat(folder_name("dir/"               ), "dir");
        AssertThat(folder_name("file.ext"           ), "");
        AssertThat(folder_name(""                   ), "");

        AssertThat(folder_path("/root/dir/file.ext" ), "/root/dir/");
        AssertThat(folder_path("/root/dir/file"     ), "/root/dir/");
        AssertThat(folder_path("/root/dir/"         ), "/root/dir/");
        AssertThat(folder_path("dir/"               ), "dir/");
        AssertThat(folder_path("file.ext"           ), "");
        AssertThat(folder_path(""                   ), "");

        AssertThat(normalized("/root\\dir\\file.ext", '/'), "/root/dir/file.ext");
        AssertThat(normalized("\\root/dir/file.ext",  '/'), "/root/dir/file.ext");

        AssertThat(normalized("/root\\dir\\file.ext", '\\'), "\\root\\dir\\file.ext");
        AssertThat(normalized("\\root/dir/file.ext",  '\\'), "\\root\\dir\\file.ext");
    }

} Impl;
