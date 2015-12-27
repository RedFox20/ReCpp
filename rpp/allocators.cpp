#include "allocators.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
	static HANDLE AHeap; // separate heap for allocators only
	static inline void* system_alloc(size_t size) noexcept
	{
		if (!AHeap) AHeap = HeapCreate(0, 0, 0);
		return HeapAlloc(AHeap, 0, size);
	}
	static inline void system_free(void* ptr) noexcept
	{
		HeapFree(AHeap, 0, ptr);
	}
#else
	static void* system_alloc(size_t size) noexcept
	{
		return malloc(size);
	}
	static void system_free(void* ptr) noexcept
	{
		free(ptr);
	}
#endif



fixsize_pool* fixsize_pool::create(int sizeOf, int count) noexcept
{
	int poolSize = sizeof(fixsize_pool) + sizeOf * count;

	fixsize_pool* pool = (fixsize_pool*)system_alloc(poolSize);
	pool->sizeOf = sizeOf;
	pool->next   = (char*)&pool[1];
	pool->end    = (char*)pool + poolSize;
	return pool;
}

void fixsize_pool::destroy(fixsize_pool* pool) noexcept
{
	system_free(pool);
}

void* fixsize_pool::alloc() noexcept
{
	if (next >= end)
		return nullptr;
	void* ptr = next;
	next += sizeOf;
	return ptr;
}
