#include "file_io.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h> // stat,fstat
#if _WIN32
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
	#include <direct.h>   // mkdir
	#include <io.h>
#else
	#include <unistd.h>
	#include <cstring>
#endif


namespace rpp /* ReCpp */
{
	load_buffer::~load_buffer()
	{
		if (Buffer) free(Buffer); // MEM_RELEASE
	}
	load_buffer::load_buffer(load_buffer&& mv) : Buffer(mv.Buffer), Size(mv.Size)
	{
		mv.Buffer = 0;
		mv.Size = 0;
	}
	load_buffer& load_buffer::operator=(load_buffer&& mv)
	{
		if (this != &mv)
		{
			char* b = Buffer;
			int siz = Size;
			Buffer = mv.Buffer;
			Size = mv.Size;
			mv.Buffer = b;
			mv.Size = siz;
		}
		return *this;
	}
	// acquire the data pointer of this load_buffer, making the caller own the buffer
	char* load_buffer::steal_ptr()
	{
		char* ptr = Buffer;
		Buffer = 0;
		Size = 0;
		return ptr;
	}






	static void* OpenFile(const char* filename, IOFlags mode)
	{
		const char* modeStr;
		switch (mode)
		{
		default: /* default mode */
		case READONLY:  modeStr = "rb";  break; // open existing for read-binary
		case READWRITE: modeStr = "wbx"; break; // open existing for r/w-binary
		case CREATENEW: modeStr = "wb";  break;  // create new file for write-binary
		}
		return fopen(filename, modeStr);
	}


	file::file(const char* filename, IOFlags mode)
		: Handle(OpenFile(filename, mode)), Mode(mode)
	{
	}
	file::file(const std::string& filename, IOFlags mode)
		: Handle(OpenFile(filename.c_str(), mode)), Mode(mode)
	{
	}
	file::file(file&& f)
		: Handle(f.Handle), Mode(f.Mode)
	{
		f.Handle = 0;
	}
	file::~file()
	{
		if (Handle)
			fclose((FILE*)Handle);
	}
	file& file::operator=(file&& f)
	{
		if (Handle)
			fclose((FILE*)Handle);
		Handle = f.Handle;
		Mode = f.Mode;
		f.Handle = 0;
		return *this;
	}
	bool file::open(const char* filename, IOFlags mode)
	{
		if (Handle)
			fclose((FILE*)Handle);
		Mode = mode;
		return (Handle = OpenFile(filename, mode)) != NULL;
	}
	void file::close()
	{
		if (Handle)
		{
			fclose((FILE*)Handle);
			Handle = NULL;
		}
	}
	bool file::good() const
	{
		return Handle != NULL;
	}
	bool file::bad() const
	{
		return Handle == NULL;
	}
	int file::size() const
	{
		if (!Handle) 
			return 0;

		struct stat s;
		if (fstat(fileno((FILE*)Handle), &s))
		{
			fprintf(stderr, "fstat error: [%s]\n", strerror(errno));
			return 0ull;
		}
		return (int)s.st_size;
	}
	int file::read(void* buffer, int bytesToRead)
	{
		return (int)fread(buffer, bytesToRead, 1, (FILE*)Handle) * bytesToRead;
	}
	load_buffer file::read_all()
	{
		int fileSize = size();
		if (!fileSize)
			return load_buffer(0, 0);

		char* buffer = (char*)malloc(fileSize);
		int bytesRead = read(buffer, fileSize);
		return load_buffer(buffer, bytesRead);
	}
	load_buffer file::read_all(const char* filename)
	{
		// make sure file f; remains until end of scope, or read_all will bug out
		file f(filename, READONLY);
		return f.read_all();
	}
	int file::write(const void* buffer, int bytesToWrite)
	{
		return (int)fwrite(buffer, bytesToWrite, 1, (FILE*)Handle) * bytesToWrite;
	}
	int file::write_new(const char* filename, const void* buffer, int bytesToWrite)
	{
		return file(filename, IOFlags::CREATENEW).write(buffer, bytesToWrite);
	}
	int file::seek(int filepos, int seekmode)
	{
		fseek((FILE*)Handle, filepos, seekmode);
		return ftell((FILE*)Handle);
	}
	int file::tell() const
	{
		return ftell((FILE*)Handle);
	}
	time_t file::time_created() const
	{
		struct stat s;
		if (fstat(fileno((FILE*)Handle), &s))
		{
			fprintf(stderr, "fstat error: [%s]\n", strerror(errno));
			return 0ull;
		}
		return s.st_ctime;
	}
	time_t file::time_accessed() const
	{
		struct stat s;
		if (fstat(fileno((FILE*)Handle), &s))
		{
			fprintf(stderr, "fstat error: [%s]\n", strerror(errno));
			return 0ull;
		}
		return s.st_atime;
	}
	time_t file::time_modified() const
	{
		struct stat s;
		if (fstat(fileno((FILE*)Handle), &s))
		{
			fprintf(stderr, "fstat error: [%s]\n", strerror(errno));
			return 0ull;
		}
		return s.st_mtime;
	}




	#define PATH_LEN 512
	inline const char* temp_path(const char* str, int length)
	{
		static char buffer[PATH_LEN];
		if (length < PATH_LEN)
			memcpy(buffer, str, length);
		buffer[length] = 0;
		return buffer;
	}


	bool file_exists(const char* filename)
	{
		struct stat s;
		if (stat(filename, &s))
			return false;
		return (s.st_mode & S_IFDIR) == 0;
	}
	bool file_exists(const std::string& filename)
	{
		return file_exists(filename.c_str());
	}
	bool file_exists(const char* filename, int length)
	{
		return file_exists(temp_path(filename, length));
	}

	bool folder_exists(const char* folder)
	{
		struct stat s;
		if (stat(folder, &s))
			return false;
		return (s.st_mode & S_IFDIR) != 0;
	}
	bool folder_exists(const std::string& folder)
	{
		return folder_exists(folder.c_str());
	}
	bool folder_exists(const char* filename, int length)
	{
		return folder_exists(temp_path(filename, length));
	}

	unsigned int file_size(const char* filename)
	{
		struct stat s;
		if (stat(filename, &s))
			return 0;
		return (unsigned int)s.st_size;
	}
	unsigned int file_size(const std::string& filename)
	{
		return file_size(filename.c_str());
	}
	unsigned int file_size(const char* filename, int length)
	{
		return file_size(temp_path(filename, length));
	}

	time_t file_modified(const char* filename)
	{
		return file(filename).time_modified();
	}
	time_t file_modified(const std::string& filename)
	{
		return file(filename).time_modified();
	}
	time_t file_modified(const char* filename, int length)
	{
		return file(temp_path(filename, length)).time_modified();
	}

	bool create_folder(const char* filename)
	{
	#ifdef _WIN32
		return mkdir(filename) == 0;
	#else
		return mkdir(filename, 0700) == 0;
	#endif
	}
	bool create_folder(const std::string& filename)
	{
		return create_folder(filename.c_str());
	}
	bool create_folder(const char* filename, int length)
	{
		return create_folder(temp_path(filename, length));
	}

	bool delete_folder(const char* filename)
	{
		return rmdir(filename) == 0;
	}
	bool delete_folder(const std::string& file)
	{
		return delete_folder(file.c_str());
	}
	bool delete_folder(const char* filename, int length)
	{
		return delete_folder(temp_path(filename, length));
	}


#if _WIN32
	int path::list_dirs(std::vector<std::string>& out, const char* directory, const char* matchPattern)
	{
		out.clear();
		char findPattern[PATH_LEN];
		sprintf(findPattern, "%s\\%s", directory, matchPattern);
		WIN32_FIND_DATAA ffd;
		HANDLE hFind = FindFirstFileA(findPattern, &ffd);
		if (hFind == INVALID_HANDLE_VALUE)
			return 0;
		do
		{
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (strcmp(ffd.cFileName, ".") != 0 && strcmp(ffd.cFileName, "..") != 0)
					out.emplace_back(ffd.cFileName);
			}
		} while (FindNextFileA(hFind, &ffd));
		FindClose(hFind);
		return (int)out.size();
	}
	int path::list_files(std::vector<std::string>& out, const char* directory, const char* matchPattern)
	{
		out.clear();
		char findPattern[PATH_LEN];
		sprintf(findPattern, "%s\\%s", directory, matchPattern);
		WIN32_FIND_DATAA ffd;
		HANDLE hFind = FindFirstFileA(findPattern, &ffd);
		if (hFind == INVALID_HANDLE_VALUE)
			return 0;
		do
		{
			if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				out.emplace_back(ffd.cFileName);
		} while (FindNextFileA(hFind, &ffd));
		FindClose(hFind);
		return (int)out.size();
	}
#else
	int path::list_dirs(std::vector<std::string>& out, const char* directory, const char* matchPattern)
	{
		out.clear();
		return (int)out.size();
	}
	int path::list_files(std::vector<std::string>& out, const char* directory, const char* matchPattern)
	{
		out.clear();
		return (int)out.size();
	}
#endif


	std::string path::working_dir()
	{
		char path[PATH_LEN];
		return std::string(getcwd(path, sizeof path) ? path : "");
	}

	void path::set_working_dir(const std::string& new_wd)
	{
		chdir(new_wd.c_str());
	}


	static int getfullpath(const char* filename, char* buffer)
	{
	#if _WIN32
		if (int len = GetFullPathNameA(filename, PATH_LEN, buffer, NULL))
			return len;
	#else
		if (char* result = realpath(filename, buffer))
			return strlen(result);
	#endif
		buffer[0] = '\0';
		return 0;
	}


	std::string path::fullpath(const std::string& relativePath)
	{
		return fullpath(relativePath.c_str());
	}

	std::string path::fullpath(const char* relativePath)
	{
		char path[PATH_LEN];
		return std::string(path, getfullpath(relativePath, path));
	}


	static const char* scanback(const char* start, const char* end)
	{
		if (end < start) // if end is invalid, default to start
			return start;
		for (; start < end; --end)
			if (*end == '/' || *end == '\\')
				return ++end; // return part after encounter
		return end;
	}
	static int getfilepart(const char* filename, const char** filepart)
	{
		int len = (int)strlen(filename);
		const char* ptr = scanback(filename, filename + len - 1);
		int filepartlen = int(filename + len - ptr);
		*filepart = ptr;
		return filepartlen;
	}


	std::string path::filename(const std::string& someFilePath)
	{
		return filename(someFilePath.c_str());
	}
	std::string path::filename(const char* someFilePath)
	{
		const char* filepart;
		int len = getfilepart(someFilePath, &filepart);
		return std::string(filepart, len);
	}


	std::string path::filename_namepart(const std::string& someFilePath)
	{
		return filename_namepart(someFilePath.c_str());
	}
	std::string path::filename_namepart(const char * someFilePath)
	{
		const char* filepart;
		int len = getfilepart(someFilePath, &filepart);
		const char* ptr = filepart + len - 1;
		for (; filepart < ptr; --ptr) // scan back till first .
			if (*ptr == '.')
				break;
		if (ptr == filepart) // no extension
			ptr = filepart + len;   // use original end
		return std::string(filepart, ptr);
	}


	std::string path::foldername(const std::string& someFolderPath)
	{
		return foldername(someFolderPath.c_str());
	}
	std::string path::foldername(const char* someFolderPath)
	{
		int len = (int)strlen(someFolderPath);
		const char* ptr = scanback(someFolderPath, someFolderPath + len - 1);
		if (someFolderPath < ptr)
			--ptr;
		return std::string(someFolderPath, ptr);
	}


}