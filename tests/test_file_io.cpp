#include "tests.h"
#include <io\file_io.h>


TestImpl(file_test)
{
	Implement(file_test)
	{
		using namespace io;

		const char* filename = "README.md";
		if (!file_exists(filename)) filename = "../README.md";
		printf("working_dir: %s\n", working_dir().c_str());

		file f(filename);
		Assert(f.good());
		Assert(f.size() > 0);
	}
} Impl;
