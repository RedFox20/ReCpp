#pragma once
/**
* Copyright (c) 2015 - Jorma Rebane
* Usage license: MIT
*
* Optimized delegate and event class for VC++ 2013/2015/...
*
*
* Usage examples:
*
*  -) Declaring and resetting the delegate
*     delegate<void(int)> fn;
*     fn.reset();               // clear delegate (uninitialize)
*     if (fn) fn(42);           // call if initialized
*
*  -) Regular function
*     delegate<void(int)> fn = &func;
*     fn(42);
*
*  -) Member function
*     delegate<void(int)> fn(myClass, &MyClass::func);  // construct
*     fn.reset(myClass, &MyClass::func);                // or reset
*     fn(42);
*
*  -) Lambdas:
*     delegate<void(int)> fn = [](int a) { cout << a << endl; };
*     fn(42);
*
*  -) Functors:
*     delegate<bool(int,int)> comparer = std::less<int>();
*     bool result = comparer(37, 42);
*
*  -) Events:
*
*     event<void(int,int)> on_mouse_move;
*     on_mouse_move += &scene_mousemove;   // register events
*     on_mouse_move += &gui_mouse_handler; 
*     on_mouse_move(deltaX, deltaY);       // invoke multicast delegate (event)
*     ...
*     on_mouse_move -= &scene_mousemove;   // unregister existing event
*     on_mouse_move.clear();               // unregister all
*/
#include <cstdlib> // malloc/free for event<()>
#include <cstring> // memmove for event<()>


/**
* @brief Function delegate to encapsulate global functions,
*        instance member functions, lambdas and functors
* @note Lambda capture has optimizations for small captures,
*       but big lambda captures allocate a dynamic copy
*
* @note All delegate calls result in 2 virtual calls:
*       callable_proxy(...) => myfuncptr(...)
*       Which is the same as fastest possible delegates and
*       inlining results are rather good thanks to this.
*
* @example
*
*        delegate<int(int,int)> callback = &my_func;
*        int result = callback(10, 20);
*
*        delegate<void(float)> on_move(MyActor, &Actor::update);
*        on_move(DeltaTime);
*/
template<class Func> struct delegate;
template<class Ret, class... Args> struct delegate<Ret(Args...)>
{
	/// for regular and member function calls
	typedef Ret ret_type;
	typedef Ret(*func_type)(Args...);
	#ifdef _MSC_VER // VC++
		typedef Ret(__thiscall *memb_type)(void*, Args...);
		struct dummy {};
		typedef Ret(dummy::*dummy_type)(Args...);
	#elif defined(__GNUG__) // G++
		typedef Ret(__thiscall *memb_type)(void*, Args...);
		struct dummy {};
		typedef Ret(dummy::*dummy_type)(void*, Args...);
	#endif


	/// for functors/lambdas and to ensure delegate copy works
	struct callable
	{
		virtual ~callable() {}
		virtual void copy(void* dst) const = 0;
		virtual Ret operator()(Args&&... args) = 0;
		virtual bool equals(const callable* c) const { return true; }
	};
	struct function : public callable
	{
		func_type ptr;
		function() {}
		function(func_type f) : ptr(f) {}
		virtual void copy(void* dst) const override
		{
			new (dst) function(ptr);
		}
		virtual Ret operator()(Args&&... args) override
		{
			return (Ret)ptr((Args&&)args...);
		}
		virtual bool equals(const callable* c) const override
		{
			return ptr == ((function*)c)->ptr;
		}
	};
	struct member : public callable
	{
		union {
			memb_type ptr;
			dummy_type dptr; // cast helper
		};
		void* obj;
		member() {}
		member(void* o, memb_type f) : ptr(f), obj(o) {}
		member(void* o, dummy_type f) : dptr(f), obj(o) {}
		virtual void copy(void* dst) const override
		{
			new (dst) member(obj, ptr);
		}
		virtual Ret operator()(Args&&... args) override
		{
			return (Ret)ptr(obj, (Args&&)args...);
		}
		virtual bool equals(const callable* c) const override
		{
			return ptr == ((member*)c)->ptr && obj == ((member*)c)->obj;
		}
	};
	template<class FUNC> struct functor : public callable
	{
		/// some very basic small Functor optimization applied here:
		/// @note The data / ftor fields are never compared for functor
		union {
			void* data[3];
			FUNC* ftor;
		};
		static const int SZ = 3 * sizeof(void*);
		functor() {}
		functor(FUNC&& fn)
		{
			if (sizeof(FUNC) <= SZ) new (data) FUNC((FUNC&&)fn);
			else                    ftor = new FUNC((FUNC&&)fn);
		}
		functor(const FUNC& fn)
		{
			if (sizeof(FUNC) <= SZ) new (data) FUNC(fn);
			else                    ftor = new FUNC(fn);
		}
		virtual ~functor() override
		{
			if (sizeof(FUNC) <= SZ) ((FUNC*)data)->~FUNC();
			else                    delete ftor;
		}
		virtual void copy(void* dst) const override
		{
			new (dst) functor(*ftor);
		}
		virtual Ret operator()(Args&&... args) override
		{
			FUNC& fn = (sizeof(FUNC) <= SZ) ? *(FUNC*)data : *ftor;
			return (Ret)fn((Args&&)args...);
		}
	};


	// hacked polymorphic (callable) container
	void* data[4];


	/** @brief Default constructor */
	delegate()
	{
		data[0] = nullptr; // only init what we need to
	}
	/** @brief Regular function constructor */
	delegate(func_type func)
	{
		new (data) function(func); // placement new into data buffer
	}
	/** @brief Handles functor copy cleanup */
	~delegate()
	{
		if (*data) ((callable*)data)->~callable();
	}



	/** @brief Creates a copy of the delegate */
	delegate(const delegate& d)
	{
		((callable*)d.data)->copy(data);
	}
	/** @brief Assigns a copy of the delegate */
	delegate& operator=(const delegate& d)
	{
		if (this != &d)
		{
			~delegate();
			((callable*)d.data)->copy(data);
		}
		return *this;
	}
	/** @brief Forward reference initialization */
	delegate(delegate&& d) // move
	{
		data[0] = d.data[0];
		data[1] = d.data[1];
		data[2] = d.data[2];
		data[3] = d.data[3];
		d.data[0] = nullptr;
	}
	/** @brief Forward reference assignment (swap) */
	delegate& operator=(delegate&& d)
	{
		if (this != &d)
		{
			void* d0 = data[0], *d1 = data[1], *d2 = data[2], *d3 = data[3];
			data[0] = d.data[0];
			data[1] = d.data[1];
			data[2] = d.data[2];
			data[3] = d.data[3];
			d.data[0] = d0, d.data[1] = d1, d.data[2] = d2, d.data[3] = d3;
		}
		return *this;
	}



	/**
	* @brief Object member function constructor (from pointer)
	* @example delegate<void(int)> d(&myClass, &MyClass::func);
	*/
	template<class IClass, class FClass> delegate(IClass* obj,
		Ret(FClass::*membfunc)(Args...))
	{
		#ifdef _MSC_BUILD // VC++
			new (data) member(obj, (dummy_type)membfunc);
		#elif defined(__GNUG__) // G++
			new (data) member(obj, (memb_type)(obj->*membfunc));
		#endif
	}
	/**
	* @brief Object member function constructor (from reference)
	* @example delegate<void(int)> d(myClass, &MyClass::func);
	*/
	template<class IClass, class FClass> delegate(IClass& obj,
		Ret(FClass::*membfunc)(Args...))
	{
		#ifdef _MSC_BUILD // VC++
			new (data) member(&obj, (dummy_type)membfunc);
		#elif defined(__GNUG__) // G++
			new (data) member(&obj, (memb_type)(&obj->*membfunc));
		#endif
	}
	/**
	* @brief Functor type constructor for lambdas and old-style functors
	* @example delegate<void(int)> d = [this](int a) { ... };
	* @example delegate<bool(int,int)> compare = std::less<int>();
	*/
	template<class Functor> delegate(Functor&& ftor)
	{
		new (data) delegate::functor<Functor>(ftor);
	}



	/**
	* @brief Invoke delegate with specified args list
	*/
	Ret operator()(Args&&... args)
	{
		return (Ret)(*(callable*)data)((Args&&)args...);
	}



	/** @return true if this delegate is initialize an can be called */
	inline operator bool() const
	{
		return data[0] != nullptr;
	}



	/** @brief Resets the delegate to its default uninitialized state */
	void reset()
	{
		~delegate();
		data[0] = nullptr; // only set what we need to
	}
	/** @brief Resets the delegate to point to a function */
	void reset(func_type func)
	{
		~delegate();
		new (data) function(func);
	}
	/** @brief Resets the delegate to point to a member function */
	template<class IClass, class FClass>
	void reset(IClass* obj, Ret(FClass::*membfunc)(Args...))
	{
		~delegate();
		#ifdef _MSC_BUILD // VC++
			new (data) member(obj, (dummy_type)membfunc);
		#elif defined(__GNUG__) // G++
			new (data) member(obj, (memb_type)(obj->*membfunc));
		#endif
	}
	/** @brief Resets the delegate to point to a member function */
	template<class IClass, class FClass>
	void reset(IClass& obj, Ret(FClass::*membfunc)(Args...))
	{
		~delegate();
		#ifdef _MSC_BUILD // VC++
			new (data) member(&obj, (dummy_type)membfunc);
		#elif defined(__GNUG__) // G++
			new (data) member(&obj, (memb_type)(&obj->*membfunc));
		#endif
	}
	/** @brief Resets the delegate to point to a functor or lambda */
	template<class Functor> void reset(Functor&& ftor)
	{
		~delegate();
		new (data) delegate::functor<Functor>(ftor);
	}



	/** @brief Basic operator= shortcuts for reset() */
	inline delegate& operator=(func_type func)
	{
		reset(func);
		return *this;
	}
	template<class Functor> inline delegate& operator=(Functor&& functor)
	{
		reset((Functor&&)functor);
		return *this;
	}



	/** @brief Basic comparison of delegates */
	bool operator==(const delegate& d) const
	{
		return data[0] == d.data[0] && // ensure vtables match
			((callable*)data)->equals((callable*)d.data);
	}
	bool operator!=(const delegate& d) const
	{
		return data[0] != d.data[0] || // ensure vtables dont match
			!((callable*)data)->equals((callable*)d.data);
	}
	bool operator==(func_type func) const
	{
		function vt; // vtable reference object
		return data[0] == *(void**)&vt && data[1] == func;
	}
	bool operator!=(func_type func) const
	{
		function vt; // vtable reference object
		return data[0] != *(void**)&vt || data[1] != func;
	}
	template<class Functor> bool operator==(const Functor& fn) const
	{
		//delegate::functor<Functor> vt; // vtable reference object
		return data[0] == *(void**)&fn; // comparing vtables should be enough
	}
	template<class Functor> bool operator!=(const Functor& fn) const
	{
		//delegate::functor<Functor> vt; // vtable reference object
		return data[0] != *(void**)&fn; // comparing vtables should be enough
	}



	/** @brief More complex comparison of delegates */
	bool equals(const delegate& d) const
	{
		return *this == d;
	}
	/** @brief Compares if this delegate is the specified global func */
	bool equals(func_type func) const
	{
		function vt; // vtable reference object
		return data[0] == *(void**)&vt && data[1] == func;
	}
	/** @brief Class member function comparison is instance sensitive */
	template<class IClass, class FClass>
	bool equals(IClass* obj, Ret(FClass::*membfunc)(Args...)) const
	{
		member vt; // vtable reference object
		return data[0] == *(void**)&vt &&
			data[1] == (void*)membfunc && data[2] == obj;
	}
	/** @brief Class member function comparison is instance sensitive */
	template<class IClass, class FClass>
	bool equals(IClass& obj, Ret(FClass::*membfunc)(Args...)) const
	{
		member vt; // vtable reference object
		return data[0] == *(void**)&vt &&
			data[1] == (void*)membfunc && data[2] == (void*)&obj;
	}
	/** @brief Comparing Functors by type. Sadly lambdas can't really be
	*         compared in a sensical fashion
	*/
	template<class Functor> bool equals() const
	{
		delegate::functor<Functor> vt; // vtable reference object
		return data[0] == *(void**)&vt; // comparing vtables should be enough
	}

};






/**
* @brief Event class - a delegate container object
* @note Event class is optimized to have minimal overhead if no events are registered
*       First registration optimized to reserve only 1 event delegate
*       Subsequential growth is amortized
*
* @example
*       event<void(int mx, int my)> evt_mouse_move;
*
*       evt_mouse_move += &mouse_move;
*       evt_mouse_move += &gui_mouse_handler;
*       ...
*       evt_mouse_move -= &mouse_move;
*       evt.clear();
*
*/
template<class Func> struct event;
template<class Ret, class... Args> struct event<Ret(Args...)>
{
	typedef delegate<Ret(Args...)> T; // delegate type


	/// dynamic data container, actual size is sizeof(container) + sizeof(T)*(capacity-1)
	struct container
	{
		int size;
		int capacity;
		T data[1];
	};
	container* ptr; // dynamic delegate array container


	/** @brief Creates an uninitialized event multicast delegate */
	event() : ptr(nullptr)
	{
	}
	~event()
	{
		if (ptr)
		{
			int size = ptr->size;
			auto data = ptr->data;
			for (int i = 0; i < size; ++i)
				data[i].~T();
			free(ptr);
		}
	}
	/** @brief Destructs the event container and frees all used memory */
	void clear()
	{
		~event();
		ptr = nullptr;
	}

	/** @return TRUE if there are callable delegates */
	inline operator bool() const { return ptr && ptr->size; }
	/** @return Number of currently registered event delegates */
	inline int size() const { return ptr ? ptr->size : 0; }


	/** @brief Registers a new delegate to receive notifications */
	void add(T&& d)
	{
		if (!ptr)
		{
			ptr = (container*)malloc(sizeof(container));
			ptr->size = 0;
			ptr->capacity = 1;
		}
		else if (ptr->size == ptr->capacity)
		{
			ptr->capacity += 3;
			if (int rem = ptr->capacity % 4)
				ptr->capacity += 4 - rem;
			ptr = (container*)realloc(ptr,
				sizeof(container) + sizeof(T) * (ptr->capacity - 1));
		}
		new (&ptr->data[ptr->size++]) T((T&&)d);
	}
	/** 
	 * @brief Unregisters the first matching delegate from this event
	 * @note Removing lambdas and functors is somewhat inefficient due to functor copying
	 */
	void remove(T&& d)
	{
		int size = ptr->size;
		auto data = ptr->data;
		for (int i = 0; i < size; ++i)
		{
			if (data[i] == d)
			{
				memmove(&data[i], &data[i + 1],
					sizeof(T)*(size - i - 1)); // unshift
				--ptr->size;
				return;
			}
		}
	}
	template<class Class>
	inline void add(void* obj, Ret(Class::*membfunc)(Args...))
	{
		add(T(obj, membfunc));
	}
	template<class IClass, class FClass>
	inline void add(IClass& obj, Ret(FClass::*membfunc)(Args...))
	{
		add(T(obj, membfunc));
	}
	template<class Class>
	inline void remove(void* obj, Ret(Class::*membfunc)(Args...))
	{
		remove(T(obj, membfunc));
	}
	template<class IClass, class FClass>
	inline void remove(IClass& obj, Ret(FClass::*membfunc)(Args...))
	{
		remove(T(obj, membfunc));
	}
	inline event& operator+=(T&& d)
	{
		add((T&&)d);
		return *this;
	}
	inline event& operator-=(T&& d)
	{
		remove((T&&)d);
		return *this;
	}


	/**
	* @brief Invoke all event delegates.
	* @note Regardless of the specified return type, multicast delegate (event)
	*       cannot return any meaningful values. For your own specific behavior
	*       extend this class and implement a suitable version of operator()
	*/
	void operator()(Args&&... args) const
	{
		int count = ptr->size;
		auto data = ptr->data;
		for (int i = 0; i < count; ++i)
		{
			data[i]((Args&&)args...);
			//(Ret)(*(T::callable*)data[i].data)(std::forward<Args>(args)...);
		}
	}
};