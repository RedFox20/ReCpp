#pragma once
#include <cassert>
#include <cstdlib>

	//// @note Some functions get inlined too aggressively, leading to some serious code bloat
	////       Need to hint the compiler to take it easy ^_^'
	#ifndef NOINLINE
		#ifdef _MSC_VER
			#define NOINLINE __declspec(noinline)
		#else
			#define NOINLINE __attribute__((noinline))
		#endif
	#endif

	//// @note Some strong hints that some functions are merely wrappers, so should be forced inline
	#ifndef FINLINE
		#ifdef _MSC_VER
			#define FINLINE __forceinline
		#else
			#define FINLINE  __attribute__((always_inline))
		#endif
	#endif

/**
 * @brief Provides a fixed size pool that provides no 
 *        facility to free bytes besides destroying the whole pool
 */
struct fixsize_pool
{
	int sizeOf; // size of a single element
	char* next;
	char* end;

	/**
	 * @return A new pool that can serve up to count objects.
	 */
	static fixsize_pool* create(int sizeOf, int count = 1024) noexcept;
	/**
	 * @brief Destroys this pool using unsynced free
	 */
	static void destroy(fixsize_pool* pool) noexcept;
	/**
	 * @return A buffer of [sizeOf] bytes or NULL if pool full
	 */
	void* alloc() noexcept;
};

template<class PoolType, class ObjType> struct specific_allocator
{
	PoolType* pool;
	FINLINE specific_allocator(int maxCount = 1024) noexcept
		: pool(PoolType::create(sizeof(ObjType), maxCount)) {}
	FINLINE ~specific_allocator() noexcept {
		PoolType::destroy(pool);
	}
};

template<class PoolType, int SIZE> struct size_allocator
{
	PoolType* pool;
	FINLINE size_allocator(int maxCount = 1024) noexcept
		: pool(PoolType::create(SIZE, maxCount)) {}
	FINLINE ~size_allocator() noexcept {
		PoolType::destroy(pool);
	}
};

template<class T, class U>
inline void* operator new(size_t size, specific_allocator<T,U>& a) noexcept {
	return a.pool->alloc();
}
template<class T, size_t SIZE>
inline void* operator new(size_t size, size_allocator<T, SIZE>& a) noexcept {
	assert(size == SIZE);
	return a.pool->alloc();
}

template<class T, class U>
inline void operator delete(void* ptr, specific_allocator<T, U>& a) noexcept
{
}

template<class T, size_t S>
inline void operator delete(void* ptr, size_allocator<T,S>& a) noexcept
{
}
