#include "tests.h"
#include <file_io.h>

using namespace rpp;

TestImpl(file_test)
{
	Implement(file_test)
	{
		const char* filename = "README.md";
		if (!file_exists(filename)) filename = "../README.md";
		printf("working_dir: %s\n", path::working_dir().c_str());

		file f(filename);
		Assert(f.good());
		Assert(f.size() > 0);
	}
} Impl;
