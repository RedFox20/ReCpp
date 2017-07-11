#include <rpp/tests.h>
#include <rpp/allocators.h>
using namespace rpp;

struct vec2
{
	float x;
	float y;
};
struct person
{
	string name;
	string surname;
};

TestImpl(test_allocators)
{
	TestInit(test_allocators)
	{
	}

	TestCase(fixsize_pool)
	{
		//specific_allocator<fixsize_pool, vec2> a1;
		//size_allocator<fixsize_pool, sizeof(vec2)> a2;

		//vec2* v1 = new (a1) vec2{ 0.5f, 0.5f };
		//vec2* v2 = new (a2) vec2{ 0.5f, 0.5f };

		//specific_allocator<fixsize_pool, person> a3;
		//size_allocator<fixsize_pool, sizeof(person)> a4;

		//person* p3 = new (a3) person{ "hiyah", "there" };
		//person* p4 = new (a4) person{ "hello", "world" };
		//p3->~person();
		//p4->~person();
	}

} Impl;