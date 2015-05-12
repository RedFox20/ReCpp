#pragma once
/**
 * Copyright (c) 2015 ReCpp - Jorma Rebane
 */
#include <string>
#include <vector>

namespace io {

	enum IOFlags {
		READONLY,			// opens an existing file for reading
		READONLY_EXECUTE,	// open an existing file for read & execute
		READWRITE, OPEN_EXISTING = READWRITE,        // opens an existing file for read/write
		READWRITE_CREATE, OPEN_NEW = READWRITE_CREATE, // creates or opens an existing file for read/write
		CREATENEW,			// creates new file for writing
		CREATETEMP,		// creates a temporary file, data is not flushed to disk and File is deleted on close
	};


	#ifndef SEEK_SET
		#define SEEK_SET 0
		#define SEEK_CUR 1
		#define SEEK_END 2
	#endif


	/**
	 * Automatic whole file loading buffer
	 */
	struct load_buffer
	{
		char* Buffer;  // dynamic or internal buffer
		int   Size;    // buffer size in bytes

		inline load_buffer() : Buffer(0), Size(0) {}
		// takes ownership of a malloc-ed pointer and frees it when out of scope
		inline load_buffer(char* buffer, int size) : Buffer(buffer), Size(size) {}
		~load_buffer();

		load_buffer(const load_buffer& rhs) = delete; // NOCOPY
		load_buffer& operator=(const load_buffer& rhs) = delete; // NOCOPY

		load_buffer(load_buffer&& mv);
		load_buffer& operator=(load_buffer&& mv);

		// acquire the data pointer of this load_buffer, making the caller own the buffer
		char* steal_ptr();

		template<class T> inline operator T*() { return (T*)Buffer; }
		inline int size()      const { return Size; }
		inline char* data()    const { return Buffer; }
		inline operator bool() const { return Buffer != nullptr; }

		inline char* begin() const { return Buffer; }
		inline char* end()   const { return Buffer + Size; }
	};





	/** @brief Helper class for supporting non-standard string types
	 *         some of which are possibly not null terminated
	 */
	template<class CHAR> struct _filepath
	{
		static const int COUNT = 260;
		const CHAR str[COUNT];
		_filepath(const CHAR* ptr, int len)
		{
			int n = (len >= COUNT) ? (COUNT - 1) : len;
			memcpy(str, ptr, sizeof(CHAR) * n);
			str[n] = 0;
		}
		template<class STRING> _filepath(const STRING& s)
			: _filepath(s.c_str(), (int)s.length()) {}
		const CHAR* c_str() const { return str; }
	};


	template<class CHAR> using cstring = const std::basic_string<CHAR>&;
	template<class CHAR> using cfpaths = const _filepath<CHAR>&;



	/**
	 * OS Buffered FILE structure for performing random access read/write
	 *
	 *  Example usage:
	 *         file f("test.obj");
	 *         char* buffer = malloc(f.size());
	 *         f.read(buffer, f.size());
	 *
	 * @note No additional buffering is performed, since OS level buffering
	 *       is switched on.
	 *
	 */
	struct file
	{
		void*	Handle;	// File handle
		IOFlags	Mode;	// File openmode READWRITE or READONLY


		/**
		 * @brief Creates a default unopened file object
		 */
		inline file() : Handle(0), Mode(READONLY)
		{
		}

		/**
		 * Opens an existing file for reading with mode = READONLY
		 * Creates a new file for reading/writing with mode = READWRITE
		 * @param filename File name to open or create
		 * @param mode File open mode
		 */
		file(const char* filename, IOFlags mode = READONLY);
		file(const wchar_t* filename, IOFlags mode = READONLY);

		/**
		 * Opens an existing file for reading with mode = READONLY
		 * Creates a new file for reading/writing with mode = READWRITE
		 * @param filename File name to open or create
		 * @param mode File open mode
		 */
		template<class C> file(const cstring<C>& filename, IOFlags mode = READONLY)
			: file(filename.c_str(), mode) {}

		/**
		 * Opens an existing file for reading with mode = READONLY
		 * Creates a new file for reading/writing with mode = READWRITE
		 * @param filename File name to open or create
		 * @param mode File open mode
		 */
		template<class C> file(const cfpaths<C>& filename, IOFlags mode = READONLY)
			: file(filename.c_str(), mode) {}

		file(file&& f);
		~file();

		file& operator=(file&& f);

		file(const file& f) = delete;
		file& operator=(const file& f) = delete;
	public:

		/**
		 * Opens an existing file for reading with mode = READONLY
		 * Creates a new file for reading/writing with mode = READWRITE
		 * @param filename File name to open or create
		 * @param mode File open mode
		 * @return TRUE if file open/create succeeded, FALSE if failed
		 */
		bool open(const char* filename, IOFlags mode = READONLY);
		bool open(const wchar_t* filename, IOFlags mode = READONLY);

		template<class C> bool open(const cstring<C>& filename, IOFlags mode = READONLY)
		{
			return open(filename.c_str(), mode);
		}
		template<class C> bool open(const cfpaths<C>& filename, IOFlags mode = READONLY)
		{
			return open(filename.c_str(), mode);
		}
		void close();

		/**
		 * @return TRUE if file handle is valid (file exists or has been created)
		 */
		bool good() const;
		inline operator bool() const { return good(); }

		/**
		 * @return TRUE if the file handle is INVALID
		 */
		bool bad() const;

		/**
		 * @return Size of the file in bytes from the system
		 */
		int size() const;

		/**
		 * @return Long size of file in bytes from the system
		 */
		unsigned __int64 sizel() const;

		/**
		 * Reads a block of bytes from the file. Standard OS level
		 * IO buffering is performed.
		 *
		 * @param buffer Buffer to read bytes to
		 * @param bytesToRead Number of bytes to read from the file
		 * @return Number of bytes actually read from the file
		 */
		int read(void* buffer, int bytesToRead);

		/**
		 * Reads the entire contents of the file into a load_buffer
		 * unbuffered_file is used internally
		 */
		load_buffer read_all();

		/**
		 * Reads the entire contents of the file into a load_buffer
		 * The file is opened as READONLY, unbuffered_file is used internally
		 */
		static load_buffer read_all(const char* filename);
		static load_buffer read_all(const wchar_t* filename);
		template<class C> static load_buffer read_all(cstring<C> filename)
		{
			return read_all(filename.c_str());
		}
		template<class C> static load_buffer read_all(cfpaths<C> filename)
		{
			return read_all(filename.c_str());
		}

		/**
		 * Writes a block of bytes to the file. Regular Windows IO
		 * buffering is ENABLED for WRITE.
		 *
		 * @param buffer Buffer to write bytes from
		 * @param bytesToWrite Number of bytes to write to the file
		 * @return Number of bytes actually written to the file
		 */
		int write(const void* buffer, int bytesToWrite);

		/**
		 * Creates a new file and fills it with the provided data.
		 * Regular Windows IO buffering is ENABLED for WRITE.
		 *
		 * Openmode is IOFlags::CREATENEW
		 *
		 * @param filename Name of the file to create and write to
		 * @param buffer Buffer to write bytes from
		 * @param bytesToWrite Number of bytes to write to the file
		 * @return Number of bytes actually written to the file
		 */
		static int write_new(const char* filename, const void* buffer, int bytesToWrite);
		static int write_new(const wchar_t* filename, const void* buffer, int bytesToWrite);

		template<class C> static int write_new(cstring<C> filename, const void* buffer, int bytesToWrite)
		{
			return write_new(filename.c_str(), buffer, bytesToWrite);
		}
		template<class C> static int write_new(cfpaths<C> filename, const void* buffer, int bytesToWrite)
		{
			return write_new(filename.c_str(), buffer, bytesToWrite);
		}

		/**
		 * Seeks to the specified position in a file. Seekmode is
		 * determined like in fseek: SEEK_SET, SEEK_CUR and SEEK_END
		 *
		 * @param filepos Position in file where to seek to
		 * @param seekmode Seekmode to use: SEEK_SET, SEEK_CUR or SEEK_END
		 * @return Current position in the file
		 */
		int seek(int filepos, int seekmode = SEEK_SET);

		/**
		 * @return Current position in the file
		 */
		int tell() const;

		/**
		 * @return File creation time
		 */
		unsigned __int64 time_created() const;

		/**
		 * @return File access time - when was this file last accessed?
		 */
		unsigned __int64 time_accessed() const;

		/**
		 * @return File write time - when was this file last modified
		 */
		unsigned __int64 time_modified() const;
	};




	/** @return TRUE if the specified file exists */
	bool file_exists(const char* file);
	bool file_exists(const wchar_t* file);
	template<class C> bool file_exists(cstring<C> file) { return file_exists(file.c_str()); }
	template<class C> bool file_exists(cfpaths<C> file) { return file_exists(file.str); }


	/** @return TRUE if the specified folder exists */
	bool folder_exists(const char* folder);
	bool folder_exists(const wchar_t* folder);
	template<class C> bool folder_exists(cstring<C> folder) { return folder_exists(folder.c_str()); }
	template<class C> bool folder_exists(cfpaths<C> folder) { return folder_exists(folder.str); }


	/** @return Size of the specified file */
	unsigned int file_size(const char* file);
	unsigned int file_size(const wchar_t* file);
	template<class C> unsigned int file_size(cstring<C> file) { return file_size(file.c_str()); }
	template<class C> unsigned int file_size(cfpaths<C> file) { return file_size(file.str); }


	/** @return Long size of the specified file */
	unsigned __int64 file_sizel(const char* file);
	unsigned __int64 file_sizel(const wchar_t* file);
	template<class C> unsigned __int64 file_sizel(cstring<C> file) { return file_sizel(file.c_str()); }
	template<class C> unsigned __int64 file_sizel(cfpaths<C> file) { return file_sizel(file.str); }


	/** @return File's last modification date */
	time_t file_modified(const char* file);
	time_t file_modified(const wchar_t* file);
	template<class C> time_t file_modified(cstring<C> file) { return file_modified(file.c_str()); }
	template<class C> time_t file_modified(cfpaths<C> file) { return file_modified(file.str); }


	/** @brief Recursively creates the specified directory */
	bool create_folder(const char* folder);
	bool create_folder(const wchar_t* folder);
	template<class C> bool create_folder(cstring<C> folder) { return create_folder(folder.c_str()); }
	template<class C> bool create_folder(cfpaths<C> folder) { return create_folder(folder.str); }


	/** @brief Deletes the specified folder with all its contents */
	bool delete_folder(const char* folder);
	bool delete_folder(const wchar_t* folder);
	template<class C> bool delete_folder(cstring<C> folder) { return delete_folder(folder.c_str()); }
	template<class C> bool delete_folder(cfpaths<C> folder) { return delete_folder(folder.str); }



	/**
	* @brief Lists all subdirectories in the specified directory
	* @param out Resulting names of subdirectories such as { "data", "bin", "lib" }
	* @param directory Parent directory to get listing from
	* @param matchPattern Folder wildcard pattern, such as "gloss*" (glossary,glossness) or "gloss?" (glossy,glossa)
	*/
	int list_dirs(std::vector<std::string>& out, const char* directory, const char* matchPattern = "*");
	inline int list_dirs(std::vector<std::string>& out, const std::string& directory, const char* matchPattern = "*")
	{
		return list_dirs(out, directory.c_str(), matchPattern);
	}


	/**
	* @brief Lists all files in the specified directory
	* @param out Resulting names of files such as { "config", "hello.txt" }
	* @param directory Parent directory to get listing from
	* @param matchPattern File wildcard pattern such as "gloss*.txt" or "gloss*.*"
	*/
	int list_files(std::vector<std::string>& out, const char* directory, const char* matchPattern = "*.*");
	inline int list_files(std::vector<std::string>& out, const std::string& directory, const char* matchPattern = "*.*")
	{
		return list_files(out, directory.c_str(), matchPattern);
	}


	/** @return The current working directory of the application */
	std::string working_dir();
	/** @return The current working directory of the application */
	std::wstring working_dirw();


	/** @brief Set the working directory of the application to a new value */
	void working_dir(const char* newDir);
	/** @brief Set the working directory of the application to a new value */
	void working_dir(const wchar_t* newDir);
	template<class C> void working_dir(cstring<C> newDir) { working_dir(newDir.c_str()); }
	template<class C> void working_dir(cfpaths<C> newDir) { working_dir(newDir.str); }


	/** @brief Transform a relative path to a full path name */
	std::string full_path(const char* relPath);
	/** @brief Transform a relative path to a full path name */
	std::wstring full_path(const wchar_t* relPath);
	template<class C> cstring<C> full_path(cstring<C> relPath) { return full_path(relPath.c_str()); }
	template<class C> cstring<C> full_path(cfpaths<C> relPath) { return full_path(relPath.str); }


	/** @brief Extract the filename from a path name */
	std::string file_name(const char* file);
	/** @brief Extract the filename from a path name */
	std::wstring file_name(const wchar_t* file);
	template<class C> cstring<C> file_name(cstring<C> file) { return file_name(file.c_str()); }
	template<class C> cstring<C> file_name(cfpaths<C> file) { return file_name(file.str); }


	/** @brief Extract the foldername from a path name */
	std::string folder_name(const char* path);
	/** @brief Extract the foldername from a path name */
	std::wstring folder_name(const wchar_t* path);
	template<class C> cstring<C> folder_name(cstring<C> path) { return folder_name(path.c_str()); }
	template<class C> cstring<C> folder_name(cfpaths<C> path) { return folder_name(path.str); }





	/**
	 * @brief Dirwatch filtering flags
	 */
	enum dirwatch_flags
	{
		notify_filename_change = 0x001, // a file name has changed
		notify_dirname_change = 0x002, // a directory name has changed
		notify_attrib_change = 0x004, // a file attribute has changed
		notify_filesize_change = 0x008, // a file size has changed
		notify_file_modified = 0x010, // a file was modified
		notify_file_accessed = 0x020, // a file was accessed
		notify_file_created = 0x040, // a file was created
		notify_security_change = 0x100, // a file security-descriptor was changed
	};


	/**
	 * @brief Simple method of checking directory changes at OS level,
	 *        meaning low overhead, high performance
	 * @note  There is no method to monitor which specific files changed
	 */
	struct dirwatch
	{
		void* Handle;

		/**
		 * @brief Creates an uninitialized dirwatch object
		 */
		inline dirwatch() : Handle(0) {}

		/**
		 * @brief Start monitoring the specified directory
		 * @param folder Name of the directory to monitor
		 * @param flags [notify_filename_change] Directory watch flags to combine
		 * @param monitorSubDirs [false] Set this TRUE if you also wish to monitor all subdirectories
		 */
		dirwatch(const char* folder, dirwatch_flags flags = notify_filename_change, bool monitorSubDirs = false);

		/**
		 * @brief Start monitoring the specified directory
		 * @param folder Name of the directory to monitor
		 * @param flags [notify_filename_change] Directory watch flags to combine
		 * @param monitorSubDirs [false] Set this TRUE if you also wish to monitor all subdirectories
		 */
		dirwatch(const std::string& folder, dirwatch_flags flags = notify_filename_change, bool monitorSubDirs = false);

		~dirwatch();

		/**
		 * @brief Closes the dirwatch object
		 */
		void close();

		/**
		 * @brief Start monitoring the specified directory
		 * @param folder Name of the directory to monitor
		 * @param flags [notify_filename_change] Directory watch flags to combine
		 * @param monitorSubDirs [false] Set this TRUE if you also wish to monitor all subdirectories
		 */
		void initialize(const char* folder, dirwatch_flags flags = notify_filename_change, bool monitorSubDirs = false);

		/**
		 * @brief Start monitoring the specified directory
		 * @param folder Name of the directory to monitor
		 * @param flags [notify_filename_change] Directory watch flags to combine
		 * @param monitorSubDirs [false] Set this TRUE if you also wish to monitor all subdirectories
		 */
		void initialize(const std::string& folder, dirwatch_flags flags = notify_filename_change, bool monitorSubDirs = false);

		/**
		 * @brief Waits until directory is modified or until specified timeout
		 * @param timoutMillis [INFINITE -1] Specify timeout for wait operation
		 * @return TRUE if directory was modified, FALSE if timeout
		 */
		bool wait(int timeoutMillis = -1) const;

		/**
		 * @brief Checks if the directory was modified recently
		 * @return TRUE if directory was modified, FALSE if no change happened
		 */
		bool changed() const;
	};


}