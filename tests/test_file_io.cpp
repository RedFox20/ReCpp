#include <fstream> // use fstream as a baseline
#include <rpp/file_io.h>
#include <rpp/debugging.h>
#include <rpp/tests.h>
using namespace rpp;

TestImpl(test_file_io)
{
    string TestDir;
    string TestFile;
    int TestSize = 0;

    TestInit(test_file_io)
    {
        TestDir  = path_combine(temp_dir(), "_rpp_test_tmp");
        TestFile = path_combine(temp_dir(), "_rpp_test.txt");
    }
    TestCleanup()
    {
        if (folder_exists(TestDir))
            Assert(delete_folder(TestDir, delete_mode::recursive));
        if (file_exists(TestFile))
            Assert(delete_file(TestFile));
    }

    TestCase(basic_file)
    {
        std::ofstream fstr { TestFile };
        AssertMsg(fstr.good(), "std::ofstream create failed: '%s'", TestFile.c_str());

        string aaaa = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        fstr << aaaa;
        TestSize = (int)fstr.tellp();
        fstr.close();


        file f = { TestFile };
        AssertThat(f.good(), true);
        AssertThat(f.bad(), false);
        AssertThat(f.size() > 0, true);
        AssertThat(f.size(), TestSize);
        AssertThat(f.read_text(), aaaa);
    }

    TestCase(if_initializer)
    {
        if (file f = { TestFile, file::READONLY })
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
        AssertThat(file_size(TestFile), TestSize);
        AssertThat(file_sizel(TestFile), (int64)TestSize);
    }

    TestCase(write_size_sanity)
    {
        Assert(create_folder(TestDir));
        file f = { TestDir + "/_size_sanity_test.txt", file::CREATENEW };
        Assert(f.good());
        int expectedSize = 0;
        srand((uint)time(nullptr));
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
        Assert(create_folder("") == false); // this is most likely a programming error, so give false
        Assert(create_folder("./") == true); // because "./" always exists, it should return true

        // these tests are extremely volatile, don't run without a step-in debugger
        //Assert(create_folder("dangerous"));
        //Assert(change_dir("dangerous"));
        //Assert(delete_folder("", true) == false); // can delete system root dir
        //Assert(delete_folder("./", true) == false); // may accidentally delete current folder
        //Assert(change_dir(".."));
        //Assert(delete_folder("dangerous"));

        Assert(create_folder(TestDir+"/folder/path"));
        Assert(folder_exists(TestDir+"/folder/path"));
        Assert(delete_folder(TestDir+"/", delete_mode::recursive));
        Assert(!folder_exists(TestDir));

        Assert(create_folder(TestDir+"/folder/path"));
        Assert(folder_exists(TestDir+"/folder/path"));
        Assert(delete_folder(TestDir, delete_mode::recursive));
        Assert(!folder_exists(TestDir));

        Assert(create_folder(TestDir+"/folder/path/"));
        Assert(folder_exists(TestDir+"/folder/path/"));
        Assert(delete_folder(TestDir, delete_mode::recursive));
        Assert(!folder_exists(TestDir));
    }

    TestCase(path_utils)
    {
        AssertThat(merge_dirups("../lib/../bin/file.txt"), "../bin/file.txt");

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

        AssertThat(file_ext("/root/dir/file.ext"), "ext");
        AssertThat(file_ext("/root/dir/file"    ), "");
        AssertThat(file_ext("/root/dir/"        ), "");
        AssertThat(file_ext("file.ext"          ), "ext");
        AssertThat(file_ext("/.git/f.reallylong"), "");
        AssertThat(file_ext("/.git/filewnoext"  ), "");
        AssertThat(file_ext(""                  ), "");

        AssertThat(file_replace_ext("/dir/file.old", "new"), "/dir/file.new");
        AssertThat(file_replace_ext("/dir/file",     "new"), "/dir/file.new");
        AssertThat(file_replace_ext("/dir/",         "new"), "/dir/");
        AssertThat(file_replace_ext("file.old",      "new"), "file.new");
        AssertThat(file_replace_ext("",              "new"), "");

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

    TestCase(path_combine2)
    {
        AssertThat(path_combine("tmp",  "file.txt" ), "tmp/file.txt");
        AssertThat(path_combine("tmp/", "file.txt" ), "tmp/file.txt");
        AssertThat(path_combine("tmp/", "/file.txt"), "tmp/file.txt");
        AssertThat(path_combine("tmp/", "/folder//"), "tmp/folder");
        AssertThat(path_combine("tmp/", ""         ), "tmp");
        AssertThat(path_combine("tmp",  ""         ), "tmp");
        AssertThat(path_combine("",     "tmp"      ), "tmp");
        AssertThat(path_combine("",     "/tmp"     ), "tmp");
        AssertThat(path_combine("",     "/tmp/"    ), "tmp");
        AssertThat(path_combine("",     ""         ), "");
    }
    
    TestCase(path_combine3)
    {
        AssertThat(path_combine("tmp",  "path", "file.txt" ), "tmp/path/file.txt");
        AssertThat(path_combine("tmp/", "path", "file.txt" ), "tmp/path/file.txt");
        AssertThat(path_combine("tmp/", "path/", "file.txt" ),"tmp/path/file.txt");
        AssertThat(path_combine("tmp/", "path", "/file.txt"), "tmp/path/file.txt");
        AssertThat(path_combine("tmp/", "path", "/folder//"), "tmp/path/folder");
        AssertThat(path_combine("tmp/", "/path/", "/folder//"), "tmp/path/folder");
        AssertThat(path_combine("tmp/", "path", ""         ), "tmp/path");
        AssertThat(path_combine("tmp/", "path/", ""        ), "tmp/path");
        AssertThat(path_combine("tmp",  "",     ""         ), "tmp");
        AssertThat(path_combine("",     "",     "tmp"      ), "tmp");
        AssertThat(path_combine("",     "",     "/tmp"     ), "tmp");
        AssertThat(path_combine("",     "",     "/tmp/"    ), "tmp");
        AssertThat(path_combine("",     "/",     "tmp"     ), "tmp");
        AssertThat(path_combine("",     "/",     "/tmp"    ), "tmp");
        AssertThat(path_combine("",     "/",     "/tmp/"   ), "tmp");
        AssertThat(path_combine("",     "",     ""         ), "");
    }

    static bool contains(const vector<string>& v, const string& s)
    {
        for (const string& item : v)
            if (item == s) return true;
        return false;
    }

    TestCase(file_and_folder_listing)
    {
        string originalDir = rpp::working_dir();
        Assert(create_folder(TestDir+"/folder/path"));
        Assert(rpp::change_dir(TestDir));
        file::write_new("folder/test1.txt",      "text1");
        file::write_new("folder/path/test2.txt", "text2");
        file::write_new("folder/path/test3.txt", "text3");
        file::write_new("folder/path/dummy.obj", "dummy");

        // TEST: list_files (names only)
        vector<string> relpaths = list_files("folder/path", ".txt");
        AssertThat(relpaths.size(), 2u);
        Assert(contains(relpaths, "test2.txt"));
        Assert(contains(relpaths, "test3.txt"));

        // TEST: list_files_relpath (relative to folder/path)
        vector<string> relpaths_r = list_files_relpath("folder/path", ".txt");
        AssertThat(relpaths_r.size(), 2u);
        Assert(contains(relpaths_r, "folder/path/test2.txt"));
        Assert(contains(relpaths_r, "folder/path/test3.txt"));

        // TEST: list_files_recursive
        vector<string> relpaths2 = list_files_recursive("", ".txt");
        AssertThat(relpaths2.size(), 3u);
        Assert(contains(relpaths2, "folder/test1.txt"));
        Assert(contains(relpaths2, "folder/path/test2.txt"));
        Assert(contains(relpaths2, "folder/path/test3.txt"));

        // TEST: list_files_fullpath
        string fullpath = full_path(TestDir);
        vector<string> fullpaths = list_files_fullpath("folder/path", ".txt");
        AssertThat(fullpaths.size(), 2u);
        Assert(contains(fullpaths, path_combine(fullpath,"folder/path/test2.txt")));
        Assert(contains(fullpaths, path_combine(fullpath,"folder/path/test3.txt")));

        // TEST: list_files_fullpath_recursive
        vector<string> fullpaths2 = list_files_fullpath_recursive("", ".txt");
        AssertThat(fullpaths2.size(), 3u);
        Assert(contains(fullpaths2, path_combine(fullpath,"folder/test1.txt")));
        Assert(contains(fullpaths2, path_combine(fullpath,"folder/path/test2.txt")));
        Assert(contains(fullpaths2, path_combine(fullpath,"folder/path/test3.txt")));

        // TEST: list_dirs_relpath (relative to folder)
        vector<string> dirs_r = list_dirs_relpath_recursive("folder");
        Assert(contains(dirs_r, "folder/path"));

        // TEST: list_alldir
        vector<string> dirs, files;
        list_alldir(dirs, files, "", true);
        Assert(contains(dirs, "folder"));
        Assert(contains(dirs, "folder/path"));
        Assert(contains(files, "folder/test1.txt"));
        Assert(contains(files, "folder/path/test2.txt"));
        Assert(contains(files, "folder/path/test3.txt"));
        Assert(contains(files, "folder/path/dummy.obj"));

        Assert(rpp::change_dir(originalDir));
        Assert(delete_folder(TestDir+"/", delete_mode::recursive));
    }

    TestCase(system_dirs)
    {
        print_info("working_dir: \"%s\"\n", working_dir().c_str());
        print_info("module_dir:  \"%s\"\n", module_dir().c_str());
        print_info("module_path: \"%s\"\n", module_path().c_str());
        print_info("temp_dir:    \"%s\"\n", temp_dir().c_str());
        print_info("home_dir:    \"%s\"\n", home_dir().c_str());

        AssertThat(working_dir().back(), '/');
        AssertThat(module_dir().back(), '/');
        AssertThat(temp_dir().back(), '/');
        AssertThat(home_dir().back(), '/');
    }

};
