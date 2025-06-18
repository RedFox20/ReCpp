#if _MSC_VER
#pragma execution_character_set("utf-8")
#endif
#include <rpp/file_io.h>
#include <rpp/debugging.h>
#include <rpp/tests.h>
#include <fstream> // use fstream as a baseline file utility to test against
using namespace rpp;

TestImpl(test_file_io)
{
    std::string TestDir;
    std::string TestFile;
    rpp::ustring TestUnicodeFile;
    rpp::ustring TestUnicodeDir;

    int TestFileSize = 0;
    std::string TestFileContents;

    TestInit(test_file_io)
    {
        TestDir  = path_combine(temp_dir(), "_rpp_test_tmp");
        TestFile = path_combine(temp_dir(), "_rpp_test.txt");
    }
    TestCaseCleanup() // cleanup after every test
    {
        if (folder_exists(TestDir))
            Assert(delete_folder(TestDir, delete_mode::recursive));
        if (file_exists(TestFile))
            Assert(delete_file(TestFile));

        // only for wstring tests
        if (!TestUnicodeDir.empty() && folder_exists(TestUnicodeDir))
            Assert(delete_folder(TestUnicodeDir, delete_mode::recursive));
        if (!TestUnicodeFile.empty() && file_exists(TestUnicodeFile))
            Assert(delete_file(TestUnicodeFile));
    }

    void prepare_unicode_file_paths()
    {
        TestDir  = path_combine(temp_dir(), "_rpp_test_tmp_unicode_😀𝄞ℵ€");
        TestFile = path_combine(temp_dir(), "_rpp_test_unicode_😀𝄞ℵ€.txt");
        TestUnicodeDir  = path_combine(temp_diru(), u"_rpp_test_tmp_unicode_😀𝄞ℵ€");
        TestUnicodeFile = path_combine(temp_diru(), u"_rpp_test_unicode_😀𝄞ℵ€.txt");
    }

    void create_test_file(rpp::strview filename)
    {
        std::string filePath = filename.to_string();
        std::ofstream fstr { filePath };
        AssertMsg(fstr.good(), "std::ofstream create failed: '%s'", filePath.c_str());

        TestFileContents = "abc1abc2abc3abc4abc5abc6abc7abc8abc9abc10"
                           "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
                           "0123456789!@#$%^&*()_+-=[]{};':\",.<>/?`~";
        fstr << TestFileContents;
        TestFileSize = (int)fstr.tellp();
        fstr.close();
    }
    void create_test_file(ustrview filename)
    {
        ustring filePath = filename.to_string();
        rpp::file out { filePath, rpp::file::CREATENEW };
        AssertMsg(out.good(), "rpp::file unicode create failed: '%s'", filePath.c_str());

        TestFileContents = "abc1abc2abc3abc4abc5abc6abc7abc8abc9abc10"
                           "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
                           "0123456789!@#$%^&*()_+-=[]{};':\",.<>/?`~";
        out.write(TestFileContents);
        TestFileSize = (int)out.size();
        out.close();
    }

    TestCase(basic_file)
    {
        create_test_file(TestFile);
        file f = { TestFile };
        AssertThat(f.good(), true);
        AssertThat(f.bad(), false);
        AssertThat(f.size() > 0, true);
        AssertThat(f.size(), TestFileSize);
        AssertThat(f.read_text(), TestFileContents);
    }

    TestCase(basic_file_utf16)
    {
        prepare_unicode_file_paths();
        create_test_file(TestUnicodeFile);

        file f = { TestUnicodeFile };
        AssertThat(f.good(), true);
        AssertThat(f.bad(), false);
        AssertThat(f.size() > 0, true);
        AssertThat(f.size(), TestFileSize);
        AssertThat(f.read_text(), TestFileContents);
    }

    TestCase(if_initializer)
    {
        create_test_file(TestFile);
        if (file f = { TestFile, file::READONLY })
        {
            Assert(f.good() && !f.bad());
        }
        else Assert(f.good() && !f.bad());
    }

    TestCase(current_source_file_and_folder_exists)
    {
        // can only run this test on host machines
    #if _MSC_VER || (__linux__ && !__ANDROID__ && !OCLEA && !__EMSCRIPTEN__)
        Assert(file_exists(__FILE__));
    #endif
        Assert(!file_exists("/complete/rubbish/path.txt"));

        std::string dir = working_dir();
        Assert(folder_exists(dir));
        Assert(folder_exists(dir + "/"));
        Assert(!folder_exists("/complete/rubbish/path"));
    }

    TestCase(current_source_file_and_folder_exists_utf16)
    {
        // can only run this test on host machines
    #if _MSC_VER || (__linux__ && !__ANDROID__ && !OCLEA && !__EMSCRIPTEN__)
        Assert(file_exists(rpp::to_ustring(__FILE__)));
    #endif
        Assert(!file_exists(u"/complete/rubbish/path.txt"_sv));

        ustring dir = rpp::working_diru();
        Assert(folder_exists(dir));
        Assert(folder_exists(dir + u"/"));
        Assert(!folder_exists(u"/complete/rubbish/path"_sv));
    }

    TestCase(size)
    {
        create_test_file(TestFile);
        AssertThat(file_size(TestFile), TestFileSize);
        AssertThat(file_sizel(TestFile), (int64)TestFileSize);
    }

    TestCase(size_utf16)
    {
        prepare_unicode_file_paths();
        create_test_file(TestFile);
        create_test_file(TestUnicodeFile);
        AssertThat(file_size(TestFile), TestFileSize);
        AssertThat(file_sizel(TestFile), (int64)TestFileSize);
        AssertThat(file_size(TestUnicodeFile), TestFileSize);
        AssertThat(file_sizel(TestUnicodeFile), (int64)TestFileSize);
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
            auto data = std::vector<char>(randCount, 'A');
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
        AssertFalse(create_folder("")); // this is most likely a programming error, so give false
        AssertTrue(create_folder("./")); // because "./" always exists, it should return true

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

    TestCase(create_delete_folder_utf16)
    {
        prepare_unicode_file_paths();
    
        AssertFalse(create_folder(u""_sv)); // this is most likely a programming error, so give false
        AssertTrue(create_folder(u"./"_sv)); // because "./" always exists, it should return true

        Assert(create_folder(TestUnicodeDir + u"/folder/path"_sv));
        Assert(folder_exists(TestUnicodeDir + u"/folder/path"_sv));
        Assert(delete_folder(TestUnicodeDir + u"/", delete_mode::recursive));
        Assert(!folder_exists(TestUnicodeDir));

        Assert(create_folder(TestUnicodeDir + u"/folder/path"_sv));
        Assert(folder_exists(TestUnicodeDir + u"/folder/path"_sv));
        Assert(delete_folder(TestUnicodeDir, delete_mode::recursive));
        Assert(!folder_exists(TestUnicodeDir));

        Assert(create_folder(TestUnicodeDir + u"/folder/path/"_sv));
        Assert(folder_exists(TestUnicodeDir + u"/folder/path/"_sv));
        Assert(delete_folder(TestUnicodeDir, delete_mode::recursive));
        Assert(!folder_exists(TestUnicodeDir));
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

    TestCase(path_utils_utf16)
    {
        AssertThat(merge_dirups(u"../lib/../bin/file.txt"_sv), u"../bin/file.txt"_sv);

        AssertThat(file_name(u"/root/dir/file.ext"_sv   ), u"file"_sv);
        AssertThat(file_name(u"/root/dir/file"_sv       ), u"file"_sv);
        AssertThat(file_name(u"/root/dir/"_sv           ), u""_sv);
        AssertThat(file_name(u"file.ext"_sv             ), u"file"_sv);
        AssertThat(file_name(u""_sv                     ), u""_sv);

        AssertThat(file_nameext(u"/root/dir/file.ext"_sv), u"file.ext"_sv);
        AssertThat(file_nameext(u"/root/dir/file"_sv    ), u"file"_sv);
        AssertThat(file_nameext(u"/root/dir/"_sv        ), u""_sv);
        AssertThat(file_nameext(u"file.ext"_sv          ), u"file.ext"_sv);
        AssertThat(file_nameext(u""_sv                  ), u""_sv);

        AssertThat(file_ext(u"/root/dir/file.ext"_sv), u"ext"_sv);
        AssertThat(file_ext(u"/root/dir/file"_sv    ), u""_sv);
        AssertThat(file_ext(u"/root/dir/"_sv        ), u""_sv);
        AssertThat(file_ext(u"file.ext"_sv          ), u"ext"_sv);
        AssertThat(file_ext(u"/.git/f.reallylong"_sv), u""_sv);
        AssertThat(file_ext(u"/.git/filewnoext"_sv  ), u""_sv);
        AssertThat(file_ext(u""_sv                  ), u""_sv);

        AssertThat(file_replace_ext(u"/dir/file.old"_sv, u"new"_sv), u"/dir/file.new"_sv);
        AssertThat(file_replace_ext(u"/dir/file"_sv,     u"new"_sv), u"/dir/file.new"_sv);
        AssertThat(file_replace_ext(u"/dir/"_sv,         u"new"_sv), u"/dir/"_sv);
        AssertThat(file_replace_ext(u"file.old"_sv,      u"new"_sv), u"file.new"_sv);
        AssertThat(file_replace_ext(u""_sv,              u"new"_sv), u""_sv);

        AssertThat(folder_name(u"/root/dir/file.ext"_sv ), u"dir"_sv);
        AssertThat(folder_name(u"/root/dir/file"_sv     ), u"dir"_sv);
        AssertThat(folder_name(u"/root/dir/"_sv         ), u"dir"_sv);
        AssertThat(folder_name(u"dir/"_sv               ), u"dir"_sv);
        AssertThat(folder_name(u"file.ext"_sv           ), u""_sv);
        AssertThat(folder_name(u""_sv                   ), u""_sv);

        AssertThat(folder_path(u"/root/dir/file.ext"_sv ), u"/root/dir/"_sv);
        AssertThat(folder_path(u"/root/dir/file"_sv     ), u"/root/dir/"_sv);
        AssertThat(folder_path(u"/root/dir/"_sv         ), u"/root/dir/"_sv);
        AssertThat(folder_path(u"dir/"_sv               ), u"dir/"_sv);
        AssertThat(folder_path(u"file.ext"_sv           ), u""_sv);
        AssertThat(folder_path(u""_sv                   ), u""_sv);

        AssertThat(normalized(u"/root\\dir\\file.ext"_sv, '/'), u"/root/dir/file.ext"_sv);
        AssertThat(normalized(u"\\root/dir/file.ext"_sv,  '/'), u"/root/dir/file.ext"_sv);
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

    TestCase(path_combine2_utf16)
    {
        AssertThat(path_combine(u"tmp"_sv,  u"file.txt"_sv ), u"tmp/file.txt");
        AssertThat(path_combine(u"tmp/"_sv, u"file.txt"_sv ), u"tmp/file.txt");
        AssertThat(path_combine(u"tmp/"_sv, u"/file.txt"_sv), u"tmp/file.txt");
        AssertThat(path_combine(u"tmp/"_sv, u"/folder//"_sv), u"tmp/folder");
        AssertThat(path_combine(u"tmp/"_sv, u""_sv         ), u"tmp");
        AssertThat(path_combine(u"tmp"_sv,  u""_sv         ), u"tmp");
        AssertThat(path_combine(u""_sv,     u"tmp"_sv      ), u"tmp");
        AssertThat(path_combine(u""_sv,     u"/tmp"_sv     ), u"tmp");
        AssertThat(path_combine(u""_sv,     u"/tmp/"_sv    ), u"tmp");
        AssertThat(path_combine(u""_sv,     u""_sv         ), u"");
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
    
    TestCase(path_combine3_utf16)
    {
        AssertThat(path_combine(u"tmp"_sv,  u"path"_sv, u"file.txt"_sv ), u"tmp/path/file.txt");
        AssertThat(path_combine(u"tmp/"_sv, u"path"_sv, u"file.txt"_sv ), u"tmp/path/file.txt");
        AssertThat(path_combine(u"tmp/"_sv, u"path/"_sv, u"file.txt"_sv ),u"tmp/path/file.txt");
        AssertThat(path_combine(u"tmp/"_sv, u"path"_sv, u"/file.txt"_sv), u"tmp/path/file.txt");
        AssertThat(path_combine(u"tmp/"_sv, u"path"_sv, u"/folder//"_sv), u"tmp/path/folder");
        AssertThat(path_combine(u"tmp/"_sv, u"/path/"_sv, u"/folder//"_sv), u"tmp/path/folder");
        AssertThat(path_combine(u"tmp/"_sv, u"path"_sv, u""_sv         ), u"tmp/path");
        AssertThat(path_combine(u"tmp/"_sv, u"path/"_sv, u""_sv        ), u"tmp/path");
        AssertThat(path_combine(u"tmp"_sv,  u""_sv     , u""_sv         ), u"tmp");
        AssertThat(path_combine(u""_sv,     u""_sv     , u"tmp"_sv      ), u"tmp");
        AssertThat(path_combine(u""_sv,     u""_sv     , u"/tmp"_sv     ), u"tmp");
        AssertThat(path_combine(u""_sv,     u""_sv     , u"/tmp/"_sv    ), u"tmp");
        AssertThat(path_combine(u""_sv,     u"/"_sv    , u"tmp"_sv      ), u"tmp");
        AssertThat(path_combine(u""_sv,     u"/"_sv    , u"/tmp"_sv     ), u"tmp");
        AssertThat(path_combine(u""_sv,     u"/"_sv    , u"/tmp/"_sv    ), u"tmp");
        AssertThat(path_combine(u""_sv,     u""_sv     , u""_sv         ), u"");
    }

    template<typename StringT, typename String2T>
    static bool contains(const std::vector<StringT>& v, const String2T& s)
    {
        for (const StringT& item : v)
            if (item == s) return true;
        return false;
    }

    void print_paths(const char* what, const std::vector<string>& paths)
    {
        for (size_t i = 0; i < paths.size(); ++i) {
            print_info("%s[%zu] = '%s'\n", what, i, paths[i].c_str());
        }
    }

    void print_paths(const char* what, const std::vector<ustring>& paths)
    {
        for (size_t i = 0; i < paths.size(); ++i) {
            print_info("%s[%zu] = '%s'\n", what, i, rpp::to_string(paths[i]).c_str());
        }
    }

    TestCase(file_and_folder_listing)
    {
        std::string originalDir = rpp::working_dir();
        Assert(create_folder(TestDir+"/folder/path"));
        Assert(rpp::change_dir(TestDir));
        file::write_new("folder/test1.txt",      "text1");
        file::write_new("folder/path/test2.txt", "text2");
        file::write_new("folder/path/test3.txt", "text3");
        file::write_new("folder/path/dummy.obj", "dummy");

        // TEST: list_files (names only)
        std::vector<std::string> relpaths = list_files("folder/path", ".txt");
        print_paths("relpaths", relpaths);
        AssertThat(relpaths.size(), 2u);
        Assert(contains(relpaths, "test2.txt"));
        Assert(contains(relpaths, "test3.txt"));

        // TEST: list_files dir_relpath_combine (relative to folder/path)
        std::vector<std::string> relpaths_r = list_files("folder/path", ".txt", rpp::dir_relpath_combine);
        print_paths("relpaths_r", relpaths_r);
        AssertThat(relpaths_r.size(), 2u);
        Assert(contains(relpaths_r, "folder/path/test2.txt"));
        Assert(contains(relpaths_r, "folder/path/test3.txt"));

        // TEST: list_files dir_recursive
        std::vector<std::string> relpaths2 = list_files("", ".txt", rpp::dir_recursive);
        print_paths("relpaths2", relpaths2);
        AssertThat(relpaths2.size(), 3u);
        Assert(contains(relpaths2, "folder/test1.txt"));
        Assert(contains(relpaths2, "folder/path/test2.txt"));
        Assert(contains(relpaths2, "folder/path/test3.txt"));

        // TEST: list_files dir_fullpath
        std::string fullpath = full_path(TestDir);
        std::vector<std::string> fullpaths = list_files("folder/path", ".txt", rpp::dir_fullpath);
        print_paths("fullpaths", fullpaths);
        AssertThat(fullpaths.size(), 2u);
        Assert(contains(fullpaths, path_combine(fullpath,"folder/path/test2.txt")));
        Assert(contains(fullpaths, path_combine(fullpath,"folder/path/test3.txt")));

        std::vector<std::string> fullpaths2 = list_files("folder", ".txt", rpp::dir_fullpath);
        print_paths("fullpaths2", fullpaths2);
        AssertThat(fullpaths2.size(), 1u);
        Assert(contains(fullpaths2, path_combine(fullpath,"folder/test1.txt")));

        // TEST: list_files dir_fullpath_recursive
        std::vector<std::string> fullpaths3 = list_files("", ".txt", rpp::dir_fullpath_recursive);
        print_paths("fullpaths3", fullpaths3);
        AssertThat(fullpaths3.size(), 3u);
        Assert(contains(fullpaths3, path_combine(fullpath,"folder/test1.txt")));
        Assert(contains(fullpaths3, path_combine(fullpath,"folder/path/test2.txt")));
        Assert(contains(fullpaths3, path_combine(fullpath,"folder/path/test3.txt")));

        // TEST: list_dirs_relpath (relative to folder)
        std::vector<std::string> dirs_r = list_dirs("folder", rpp::dir_relpath_combine_recursive);
        print_paths("dirs_r", dirs_r);
        Assert(contains(dirs_r, "folder/path"));

        // TEST: list_alldir dir_recursive
        std::vector<std::string> dirs, files;
        list_alldir(dirs, files, "", rpp::dir_recursive);
        print_paths("dirs", dirs);
        print_paths("files", files);
        Assert(contains(dirs, "folder"));
        Assert(contains(dirs, "folder/path"));
        Assert(contains(files, "folder/test1.txt"));
        Assert(contains(files, "folder/path/test2.txt"));
        Assert(contains(files, "folder/path/test3.txt"));
        Assert(contains(files, "folder/path/dummy.obj"));

        Assert(rpp::change_dir(originalDir));
        Assert(delete_folder(TestDir+"/", delete_mode::recursive));
    }

    TestCase(file_and_folder_listing_utf16)
    {
        prepare_unicode_file_paths();
        ustring originalDir = rpp::working_diru();
        AssertTrue(create_folder(TestUnicodeDir+u"/folder/path"_sv));
        AssertTrue(rpp::change_dir(TestUnicodeDir));
        file::write_new(u"folder/test1.txt"_sv,      "text1");
        file::write_new(u"folder/path/test2.txt"_sv, "text2");
        file::write_new(u"folder/path/test3.txt"_sv, "text3");
        file::write_new(u"folder/path/dummy.obj"_sv, "dummy");

        // TEST: list_files (names only)
        std::vector<ustring> relpaths = list_files(u"folder/path"_sv, u".txt"_sv);
        print_paths("relpaths", relpaths);
        AssertThat(relpaths.size(), 2u);
        Assert(contains(relpaths, u"test2.txt"_sv));
        Assert(contains(relpaths, u"test3.txt"_sv));

        // TEST: list_files dir_relpath_combine (relative to folder/path)
        std::vector<ustring> relpaths_r = list_files(u"folder/path"_sv, u".txt"_sv, rpp::dir_relpath_combine);
        print_paths("relpaths_r", relpaths_r);
        AssertThat(relpaths_r.size(), 2u);
        Assert(contains(relpaths_r, u"folder/path/test2.txt"_sv));
        Assert(contains(relpaths_r, u"folder/path/test3.txt"_sv));

        // TEST: list_files dir_recursive
        std::vector<ustring> relpaths2 = list_files(u""_sv, u".txt"_sv, rpp::dir_recursive);
        print_paths("relpaths2", relpaths2);
        AssertThat(relpaths2.size(), 3u);
        Assert(contains(relpaths2, u"folder/test1.txt"_sv));
        Assert(contains(relpaths2, u"folder/path/test2.txt"_sv));
        Assert(contains(relpaths2, u"folder/path/test3.txt"_sv));

        // TEST: list_files dir_fullpath
        ustring fullpath = full_path(TestUnicodeDir);
        std::vector<ustring> fullpaths = list_files(u"folder/path"_sv, u".txt"_sv, rpp::dir_fullpath);
        print_paths("fullpaths", fullpaths);
        AssertThat(fullpaths.size(), 2u);
        Assert(contains(fullpaths, path_combine(fullpath, u"folder/path/test2.txt"_sv)));
        Assert(contains(fullpaths, path_combine(fullpath, u"folder/path/test3.txt"_sv)));

        std::vector<ustring> fullpaths2 = list_files(u"folder"_sv, u".txt"_sv, rpp::dir_fullpath);
        print_paths("fullpaths2", fullpaths2);
        AssertThat(fullpaths2.size(), 1u);
        Assert(contains(fullpaths2, path_combine(fullpath, u"folder/test1.txt"_sv)));

        // TEST: list_files dir_fullpath_recursive
        std::vector<ustring> fullpaths3 = list_files(u""_sv, u".txt", rpp::dir_fullpath_recursive);
        print_paths("fullpaths3", fullpaths3);
        AssertThat(fullpaths3.size(), 3u);
        Assert(contains(fullpaths3, path_combine(fullpath, u"folder/test1.txt")));
        Assert(contains(fullpaths3, path_combine(fullpath, u"folder/path/test2.txt")));
        Assert(contains(fullpaths3, path_combine(fullpath, u"folder/path/test3.txt")));

        // TEST: list_dirs_relpath (relative to folder)
        std::vector<ustring> dirs_r = list_dirs(u"folder"_sv, rpp::dir_relpath_combine_recursive);
        print_paths("dirs_r", dirs_r);
        Assert(contains(dirs_r, u"folder/path"));

        // TEST: list_alldir dir_recursive
        std::vector<ustring> dirs, files;
        list_alldir(dirs, files, u""_sv, rpp::dir_recursive);
        print_paths("dirs", dirs);
        print_paths("files", files);
        Assert(contains(dirs, u"folder"));
        Assert(contains(dirs, u"folder/path"));
        Assert(contains(files, u"folder/test1.txt"));
        Assert(contains(files, u"folder/path/test2.txt"));
        Assert(contains(files, u"folder/path/test3.txt"));
        Assert(contains(files, u"folder/path/dummy.obj"));

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

        const auto last = [](const std::string& s) -> char { return s.back(); };
        AssertThat(last(working_dir()), '/');
        AssertThat(last(module_dir()), '/');
        AssertThat(last(temp_dir()), '/');
        AssertThat(last(home_dir()), '/');
    }

    // validate that UTF16 file paths work correctly
    TestCase(can_handle_utf8_file_paths)
    {
        prepare_unicode_file_paths();

        AssertTrue(file::write_new(TestFile, "abcdefgh"));
        AssertTrue(file_exists(TestFile));
        AssertThat(file::read_all_text(TestFile), "abcdefgh");
        AssertTrue(delete_file(TestFile));

        AssertTrue(create_folder(TestDir));
        AssertTrue(folder_exists(TestDir));
        AssertTrue(delete_folder(TestDir, delete_mode::recursive));
    }

    TestCase(can_handle_utf16_file_paths)
    {
        prepare_unicode_file_paths();

        AssertTrue(file::write_new(TestUnicodeFile, "abcdefgh"));
        AssertTrue(file_exists(TestUnicodeFile));
        AssertThat(file::read_all_text(TestUnicodeFile), "abcdefgh");
        AssertTrue(delete_file(TestUnicodeFile));

        // test that we can create a directory with unicode characters
        AssertTrue(create_folder(TestUnicodeDir));
        AssertTrue(folder_exists(TestUnicodeDir));
        AssertTrue(delete_folder(TestUnicodeDir, delete_mode::recursive));
    }

};
