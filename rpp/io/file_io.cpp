/**
 * Copyright (c) 2015 ReCpp - Jorma Rebane
 */
#include "file_io.h"
#include <stdlib.h>
#include <stdio.h>

#if _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#endif


namespace io {


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




	static void* OpenFile(const char* file, int desiredAccess, int sharingMode,
		int creationDisposition, int openFlags)
	{
		SECURITY_ATTRIBUTES secu = { sizeof(secu), NULL, TRUE };
		void* handle = CreateFileA(file, desiredAccess, sharingMode, &secu, creationDisposition, openFlags, 0);
		return handle != INVALID_HANDLE_VALUE ? handle : 0;
	}
	static void* OpenFile(const wchar_t* file, int desiredAccess, int sharingMode,
		int creationDisposition, int openFlags)
	{
		SECURITY_ATTRIBUTES secu = { sizeof(secu), NULL, TRUE };
		void* handle = CreateFileW(file, desiredAccess, sharingMode, &secu, creationDisposition, openFlags, 0);
		return handle != INVALID_HANDLE_VALUE ? handle : 0;
	}
	template<class CHAR> static void* OpenFile(const CHAR* filename, io_flags mode)
	{
		int desiredAccess;
		int sharingMode;			// FILE_SHARE_READ, FILE_SHARE_WRITE
		int creationDisposition;	// OPEN_EXISTING, OPEN_ALWAYS, CREATE_ALWAYS
		int openFlags;
		switch (mode)
		{
		default:
		case READONLY:
			desiredAccess = FILE_GENERIC_READ;
			sharingMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
			creationDisposition = OPEN_EXISTING;
			openFlags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN;
			break;
		case READONLY_EXECUTE:
			desiredAccess = FILE_GENERIC_READ | FILE_GENERIC_EXECUTE;
			sharingMode = FILE_SHARE_READ | FILE_SHARE_WRITE;
			creationDisposition = OPEN_EXISTING;
			openFlags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN;
			break;
		case READWRITE:
			desiredAccess = FILE_GENERIC_READ | FILE_GENERIC_WRITE;
			sharingMode = FILE_SHARE_READ;
			creationDisposition = OPEN_EXISTING; // if not exists, fail
			openFlags = FILE_ATTRIBUTE_NORMAL;
			break;
		case READWRITE_CREATE:
			desiredAccess = FILE_GENERIC_READ | FILE_GENERIC_WRITE;
			sharingMode = FILE_SHARE_READ;
			creationDisposition = OPEN_ALWAYS; // if exists, success, else create file
			openFlags = FILE_ATTRIBUTE_NORMAL;
			break;
		case CREATENEW:
			desiredAccess = FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE;
			sharingMode = FILE_SHARE_READ;
			creationDisposition = CREATE_ALWAYS;
			openFlags = FILE_ATTRIBUTE_NORMAL;
			break;
		case CREATETEMP:
			desiredAccess = FILE_GENERIC_READ | FILE_GENERIC_WRITE | DELETE;
			sharingMode = FILE_SHARE_DELETE | FILE_SHARE_READ;
			creationDisposition = CREATE_ALWAYS;
			openFlags = FILE_FLAG_DELETE_ON_CLOSE | FILE_ATTRIBUTE_TEMPORARY;
			break;
		}
		return OpenFile(filename, desiredAccess, sharingMode,
			creationDisposition, openFlags);
	}


	//#define PATH_LEN 512
	//template<class CHAR> static const CHAR* temp_path(const CHAR* str, int length)
	//{
	//	static CHAR buffer[PATH_LEN];
	//	if (length < PATH_LEN)
	//	{
	//		memcpy(buffer, str, length * sizeof(CHAR));
	//	}
	//	// else: @note OS Paths are limited to 260 chars unless \\?\ is prepended to a WSTR
	//	buffer[length] = 0;
	//	return buffer;
	//}



	file::file(const char* filename, io_flags mode)
		: Handle(OpenFile(filename, mode)), Mode(mode)
	{
	}
	file::file(const wchar_t* filename, io_flags mode)
		: Handle(OpenFile(filename, mode)), Mode(mode)
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
			CloseHandle(Handle);
	}
	file& file::operator=(file&& f)
	{
		if (Handle)
			CloseHandle(Handle);
		Handle = f.Handle;
		Mode = f.Mode;
		f.Handle = 0;
		return *this;
	}
	bool file::open(const char* filename, io_flags mode)
	{
		if (Handle)
			CloseHandle(Handle);
		Mode = mode;
		return (Handle = OpenFile(filename, mode)) != NULL;
	}
	bool file::open(const wchar_t* filename, io_flags mode)
	{
		if (Handle)
			CloseHandle(Handle);
		Mode = mode;
		return (Handle = OpenFile(filename, mode)) != NULL;
	}
	void file::close()
	{
		if (Handle)
		{
			CloseHandle(Handle);
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
		return GetFileSize(Handle, NULL);
	}
	unsigned __int64 file::sizel() const
	{
		LARGE_INTEGER fsize = { 0 };
		GetFileSizeEx(Handle, &fsize);
		return fsize.QuadPart;
	}
	int file::read(void* buffer, int bytesToRead)
	{
		DWORD bytesRead;
		ReadFile(Handle, buffer, bytesToRead, &bytesRead, NULL);
		return (int)bytesRead;
	}

	load_buffer file::read_all()
	{
		int fileSize = GetFileSize(Handle, NULL);
		if (!fileSize)
			return load_buffer(0, 0);

		char* buffer = (char*)malloc(fileSize);
		DWORD bytesRead;
		ReadFile(Handle, buffer, fileSize, &bytesRead, NULL);
		return load_buffer(buffer, bytesRead);
	}
	load_buffer file::read_all(const char* filename)
	{
		return file(filename, READONLY).read_all();
	}
	load_buffer file::read_all(const wchar_t* filename)
	{
		return file(filename, READONLY).read_all();
	}
	int file::write(const void* buffer, int bytesToWrite)
	{
		DWORD bytesWritten;
		WriteFile(Handle, buffer, bytesToWrite, &bytesWritten, NULL);
		return (int)bytesWritten;
	}
	int file::write_new(const char* filename, const void* buffer, int bytesToWrite)
	{
		return file(filename, io_flags::CREATENEW).write(buffer, bytesToWrite);
	}
	int file::write_new(const wchar_t* filename, const void* buffer, int bytesToWrite)
	{
		return file(filename, io_flags::CREATENEW).write(buffer, bytesToWrite);
	}
	int file::seek(int filepos, int seekmode)
	{
		return SetFilePointer(Handle, filepos, NULL, seekmode);
	}
	int file::tell() const
	{
		return SetFilePointer(Handle, 0, NULL, FILE_CURRENT);
	}
	unsigned __int64 file::time_created() const
	{
		FILETIME ft = { 0 };
		GetFileTime(Handle, &ft, 0, 0);
		return *(unsigned __int64*)&ft;
	}
	unsigned __int64 file::time_accessed() const
	{
		FILETIME ft = { 0 };
		GetFileTime(Handle, 0, &ft, 0);
		return *(unsigned __int64*)&ft;
	}
	unsigned __int64 file::time_modified() const
	{
		FILETIME ft = { 0 };
		GetFileTime(Handle, 0, 0, &ft);
		return *(unsigned __int64*)&ft;
	}





	bool file_exists(const char* filename)
	{
		int attr = GetFileAttributesA(filename);
		return attr != -1 && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
	}
	bool file_exists(const wchar_t* filename)
	{
		int attr = GetFileAttributesW(filename);
		return attr != -1 && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
	}

	bool folder_exists(const char* folder)
	{
		int attr = GetFileAttributesA(folder);
		return attr != -1 && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}
	bool folder_exists(const wchar_t* folder)
	{
		int attr = GetFileAttributesW(folder);
		return attr != -1 && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}

	unsigned int file_size(const char* filename)
	{
		WIN32_FILE_ATTRIBUTE_DATA fad;
		if (!GetFileAttributesExA(filename, GetFileExInfoStandard, &fad)) return -1;
		return fad.nFileSizeLow;
	}
	unsigned int file_size(const wchar_t* filename)
	{
		WIN32_FILE_ATTRIBUTE_DATA fad;
		if (!GetFileAttributesExW(filename, GetFileExInfoStandard, &fad)) return -1;
		return fad.nFileSizeLow;
	}

	unsigned __int64 file_sizel(const char* filename)
	{
		WIN32_FILE_ATTRIBUTE_DATA fad;
		if (!GetFileAttributesExA(filename, GetFileExInfoStandard, &fad)) return -1;
		LARGE_INTEGER sz = { fad.nFileSizeLow, (LONG)fad.nFileSizeHigh };
		return sz.QuadPart;
	}
	unsigned __int64 file_sizel(const wchar_t* filename)
	{
		WIN32_FILE_ATTRIBUTE_DATA fad;
		if (!GetFileAttributesExW(filename, GetFileExInfoStandard, &fad)) return -1;
		LARGE_INTEGER sz = { fad.nFileSizeLow, (LONG)fad.nFileSizeHigh };
		return sz.QuadPart;
	}

	time_t file_modified(const char* filename)
	{
		return file(filename).time_modified();
	}
	time_t file_modified(const wchar_t* filename)
	{
		return file(filename).time_modified();
	}

	bool create_folder(const char* filename)
	{
		return !!CreateDirectoryA(filename, NULL);
	}
	bool create_folder(const wchar_t* filename)
	{
		return !!CreateDirectoryW(filename, NULL);
	}

	bool delete_folder(const char* foldername)
	{
		SHFILEOPSTRUCTA file_op = {
			NULL, FO_DELETE, foldername, "",
			FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT,
			false, 0, ""
		};
		return SHFileOperationA(&file_op) == 0;
	}
	bool delete_folder(const wchar_t* foldername)
	{
		SHFILEOPSTRUCTW file_op = {
			NULL, FO_DELETE, foldername, L"",
			FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT,
			false, 0, L""
		};
		return SHFileOperationW(&file_op) == 0;
	}



	int list_dirs(std::vector<std::string>& out, const char* directory, const char* matchPattern)
	{
		out.clear();

		char findPattern[MAX_PATH * 2];
		sprintf(findPattern, "%s\\%s", directory, matchPattern);

		WIN32_FIND_DATAA ffd;
		HANDLE hFind = FindFirstFileA(findPattern, &ffd);
		if (hFind == INVALID_HANDLE_VALUE)
			return 0;

		do
		{
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				// @note rely on intrinsics to optimize these strcmp's
				if (strcmp(ffd.cFileName, ".") != 0 && strcmp(ffd.cFileName, "..") != 0)
					out.emplace_back(ffd.cFileName);
			}
		} while (FindNextFileA(hFind, &ffd));

		FindClose(hFind);
		return (int)out.size();
	}


	int list_files(std::vector<std::string>& out, const char* directory, const char* matchPattern)
	{
		out.clear();

		char findPattern[MAX_PATH * 2];
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


	std::string working_dir()
	{
		char path[512];
		return std::string(path, GetCurrentDirectoryA(sizeof path, path));
	}
	std::wstring working_dirw()
	{
		wchar_t path[512];
		return std::wstring(path, GetCurrentDirectoryW(_countof(path), path));
	}

	void working_dir(const char* newWorkingDir)
	{
		SetCurrentDirectoryA(newWorkingDir);
	}
	void working_dir(const wchar_t* newWorkingDir)
	{
		SetCurrentDirectoryW(newWorkingDir);
	}

	std::string full_path(const char* relativePath)
	{
		char path[512];
		return std::string(path, GetFullPathNameA(relativePath, sizeof path, path, NULL));
	}
	std::wstring full_path(const wchar_t* relativePath)
	{
		wchar_t path[512];
		return std::wstring(path, GetFullPathNameW(relativePath, _countof(path), path, NULL));
	}

	std::string file_name(const char* someFilePath)
	{
		char path[512];
		char* file;
		int len = GetFullPathNameA(someFilePath, sizeof path, path, &file);
		return std::string(file, (path + len) - file);
	}
	std::wstring file_name(const wchar_t* someFilePath)
	{
		wchar_t path[512];
		wchar_t* file;
		int len = GetFullPathNameW(someFilePath, _countof(path), path, &file);
		return std::wstring(file, (path + len) - file);
	}

	std::string folder_name(const char* someFolderPath)
	{
		char path[512];
		char* file;
		GetFullPathNameA(someFolderPath, sizeof path, path, &file);
		return std::string(path, file - path);
	}
	std::wstring folder_name(const wchar_t* someFolderPath)
	{
		wchar_t path[512];
		wchar_t* file;
		GetFullPathNameW(someFolderPath, _countof(path), path, &file);
		return std::wstring(path, file - path);
	}







	dirwatch::dirwatch(const char* folder, dirwatch_flags flags, bool monitorSubDirs) : Handle(0)
	{
		initialize(folder, flags, monitorSubDirs);
	}

	dirwatch::dirwatch(const std::string& folder, dirwatch_flags flags, bool monitorSubDirs) : Handle(0)
	{
		initialize(folder, flags, monitorSubDirs);
	}

	dirwatch::~dirwatch()
	{
		close();
	}

	void dirwatch::close()
	{
		if (Handle)
		{
			FindCloseChangeNotification(Handle);
			Handle = NULL;
		}
	}

	void dirwatch::initialize(const char* folder, dirwatch_flags flags, bool monitorSubDirs)
	{
		if (Handle) FindCloseChangeNotification(Handle);
		Handle = FindFirstChangeNotificationA(folder, monitorSubDirs, flags);
		if (Handle == INVALID_HANDLE_VALUE) Handle = NULL;
	}

	void dirwatch::initialize(const std::string& folder, dirwatch_flags flags, bool monitorSubDirs)
	{
		if (Handle) FindCloseChangeNotification(Handle);
		Handle = FindFirstChangeNotificationA(folder.c_str(), monitorSubDirs, flags);
		if (Handle == INVALID_HANDLE_VALUE) Handle = NULL;
	}

	bool dirwatch::wait(int timeoutMillis) const
	{
		DWORD result = WaitForSingleObject(Handle, timeoutMillis);
		FindNextChangeNotification(Handle);
		return result == WAIT_OBJECT_0;
	}

	bool dirwatch::changed() const
	{
		DWORD result = WaitForSingleObject(Handle, 0);
		FindNextChangeNotification(Handle);
		return result == WAIT_OBJECT_0;
	}



}