#include "tests.h"
#include <io\file_io.h>


TestImpl(file_test)
{
	Implement(file_test)
	{
		file f("../README.md");
		Assert(f.good());
		Assert(f.size() > 0);
	}
} Impl;