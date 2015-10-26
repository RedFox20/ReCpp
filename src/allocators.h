#pragma once


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
	inline specific_allocator(int maxCount = 1024)
		: pool(PoolType::create(sizeof(ObjType), maxCount)) {}
	inline ~specific_allocator() {
		PoolType::destroy(pool);
	}
};

template<class PoolType, size_t SIZE> struct size_allocator
{
	PoolType* pool;
	inline size_allocator(int maxCount = 1024)
		: pool(PoolType::create(SIZE, maxCount)) {}
	inline ~size_allocator() {
		PoolType::destroy(pool);
	}
};

template<class T, class U>
inline void* operator new(size_t size, specific_allocator<T,U>& a) noexcept {
	return a.pool->alloc();
}
template<class T, size_t S>
inline void* operator new(size_t size, size_allocator<T, S>& a) noexcept {
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
