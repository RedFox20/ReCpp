/**
 * Android JNI Utils, Copyright (c) 2024 Jorma Rebane
 * Distributed under MIT Software License
 */
#pragma once
#if __ANDROID__
#include <jni.h>
#include <string>
#include <vector>

namespace rpp { namespace jni {
    ////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Explicitly initializes JVM and JNI Environment
     */
    jint initVM(JavaVM* vm) noexcept;

    /**
     * @brief if RPP_DEFINE_JNI_ONLOAD is defined, then JNI_OnLoad will be defined
     */
    #if RPP_DEFINE_JNI_ONLOAD
        // inline - to avoid multiple definition errors
        extern "C" inline jint JNI_OnLoad(JavaVM* vm, void* reserved)
        {
            return rpp::jni::initVM(vm);
        }
    #endif // RPP_DEFINE_JNI_ONLOAD

    /**
     * @brief Gets the JNI environment for the current thread
     * @warning You must first call `initVM()` or define RPP_DEFINE_JNI_ONLOAD
     */
    JNIEnv* getEnv() noexcept;

    /**
     * @brief Get the main Android `Activity` object from the MyMainApp.currentActivity field
     * @param mainActivityClass The main activity class name, eg: "com/unity3d/player/UnityPlayer"
     *        if null, then returns the previously configured main activity
     * @warning The main activity can only be configured once per app init
    */
    jobject getActivity(const char* mainActivityClass = nullptr);

    /**
     * @brief Manually sets the main activity object for the app
     * @param globalHandle The global reference to the main activity object
     * @warning The main activity can only be configured once per app init
     */
    void initMainActivity(jobject globalHandle);

    /**
     * @brief Checks for pending JNI exceptions and rethrows them as a C++ exception
     * @param message Exception message to show
     */
    void checkForJNIException(const char* message = nullptr);

    ////////////////////////////////////////////////////////////////////////////////////////////////

    struct Class;
    struct Method;
    struct Field;

    // JNI Local objects need to be deleted to avoid errors,
    // this is a smart pointer to automatically manage those references
    // WARNING: Only global references can be stored across JNI calls
    //          thread_local is mandatory for Global refs
    template<class JObject>
    struct Ref
    {
        JObject obj = nullptr;
        bool isGlobal = false;

        Ref() noexcept = default;
        // must be explicit, because it functions like unique_ptr
        explicit Ref(JObject obj) noexcept : obj{obj} {}
        // initialize from a previousl untracked object
        Ref(JObject obj, bool isGlobal) noexcept : obj{obj} {}
        ~Ref() noexcept { reset(); }

        Ref(Ref&& r) noexcept : obj{r.obj}, isGlobal{r.isGlobal}
        {
            r.obj = nullptr;
            r.isGlobal = false;
        }
        Ref(const Ref& r) noexcept
        {
            copy(r);
        }
        Ref& operator=(Ref&& r) noexcept
        {
            std::swap(obj, r.obj);
            std::swap(isGlobal, r.isGlobal);
            return *this;
        }
        Ref& operator=(const Ref& r) noexcept
        {
            if (this != &r)
            {
                reset();
                copy(r);
            }
            return *this;
        }

        // implicit conversion of Ref<JObject> to JObject
        operator JObject() const noexcept { return obj; }
        JObject get() const noexcept { return obj; }

        /**
         * @brief reinterpret_casts the object to another type
         * @warning This is unsafe and the returned object is not tracked or type-checked!
         */
        template<class T> T as() const noexcept { return reinterpret_cast<T>(obj); }
        explicit operator bool() const noexcept { return obj != nullptr; }

        void reset() noexcept
        {
            if (obj)
            {
                auto* env = getEnv();
                if (isGlobal) env->DeleteGlobalRef(obj);
                else          env->DeleteLocalRef(obj);
                obj = nullptr;
                isGlobal = false;
            }
        }

        /**
         * @brief Converts the Ref<JObject> to a Ref<T> and resets this Ref
         */
        template<class T> Ref<T> releaseAs() noexcept
        {
            Ref<T> ref { reinterpret_cast<T>(obj), isGlobal };
            obj = nullptr;
            isGlobal = false;
            return ref;
        }

        /**
         * @returns Global Ref from this Ref<T>
         */
        Ref<JObject> toGlobal() noexcept
        {
            Ref<JObject> g;
            if (obj)
            {
                // even if it's already global, we need to increase the global refcount
                g.obj = reinterpret_cast<JObject>(getEnv()->NewGlobalRef(obj));
                g.isGlobal = true;
            }
            return g;
        }

        /**
         * @brief Converts this Local Ref to a Global Ref (if needed)
         */
        void makeGlobal() noexcept
        {
            if (obj && !isGlobal)
            {
                auto* env = getEnv();
                JObject g = reinterpret_cast<JObject>(env->NewGlobalRef(obj));
                // decrease the local refcount
                env->DeleteLocalRef(obj);
                obj = g;
                isGlobal = true;
            }
        }

    private:
        void copy(const Ref& r) noexcept
        {
            isGlobal = r.isGlobal;
            if (r.obj)
            {
                auto* env = getEnv();
                if (r.isGlobal) obj = reinterpret_cast<JObject>(env->NewGlobalRef(r.obj));
                else            obj = reinterpret_cast<JObject>(env->NewLocalRef(r.obj));
            }
        }
    };

    /**
     * @brief Makes a new Local Ref of a JNI object
     * @warning DO NOT STORE LocalRef into static/global storage!
     */
    template<class JObject> Ref<JObject> makeRef(JObject obj) noexcept
    {
        return Ref<JObject>{obj};
    }

    /**
     * @brief Makes a new Global Ref of a JNI object
     * Global Refs can be used across JNI calls and are not automaticaly GC-ed
     * @warning Global Refs must be stored in thread_local storage!!
     */
    template<class JObject> Ref<JObject> makeGlobalRef(JObject obj) noexcept
    {
        Ref<JObject> r{obj};
        r.makeGlobal();
        return r;
    }

    /**
     * @brief Describes all JNI types
     */
    enum class JniType
    {
        Object,
        Boolean,
        Byte,
        Char,
        Short,
        Int,
        Long,
        Float,
        Double
    };

    /**
     * JNI string class wrapper
     */
    struct JString
    {
        Ref<jstring> s;

        JString() noexcept = default;
        JString(const Ref<jstring>& s) noexcept : s{s} {}
        JString(Ref<jstring>&& s) noexcept : s{std::move(s)} {}
        explicit JString(jstring s) noexcept : s{s} {}

        operator jstring() const noexcept { return s.get(); }
        jstring get() const noexcept { return s.get(); }
        explicit operator bool() const noexcept { return (bool)s; }

        /** @returns New GlobalRef JString that can be stored into global state */
        JString toGlobal() noexcept { return { s.toGlobal() }; }

        int getLength() const noexcept { return s ? getEnv()->GetStringLength(s) : 0; }

        /** @returns jstring converted to C++ std::string */
        std::string str() const noexcept;

        /**
         * @brief Creates a new local jstring
         */
        static JString from(const char* text) noexcept;

        inline static JString from(const std::string& text) noexcept
        {
            return from(text.c_str());
        }
    };

    // core util: JNI jstring to std::string
    std::string toString(JNIEnv* env, jstring s) noexcept;

    /**
     * Provides access to raw array elements such as jbyte*, jint*
     */
    struct ElementsRef
    {
        const jarray arr;
        const JniType type;
        void* e;

        // acquires ref to jarray elements
        ElementsRef(jarray a, JniType t) noexcept;
        ~ElementsRef() noexcept;

        ElementsRef(const ElementsRef&) = delete;
        ElementsRef(ElementsRef&&) = delete;
        ElementsRef& operator=(const ElementsRef&) = delete;
        ElementsRef& operator=(ElementsRef&&) = delete;

        int getLength() const noexcept
        {
            return arr ? getEnv()->GetArrayLength(arr) : 0;
        }
        jobject objectAt(int i) const noexcept
        {
            return getEnv()->GetObjectArrayElement((jobjectArray)arr, i);
        }
        void setObjectAt(int i, jobject obj) noexcept
        {
            getEnv()->SetObjectArrayElement((jobjectArray)arr, i, obj);
        }

        jboolean& boolAt(int i)  noexcept { return ((jboolean*)e)[i]; }
        jbyte& byteAt(int i)     noexcept { return ((jbyte*)e)[i]; }
        jchar& charAt(int i)     noexcept { return ((jchar*)e)[i]; }
        jshort& shortAt(int i)   noexcept { return ((jshort*)e)[i]; }
        jint& intAt(int i)       noexcept { return ((jint*)e)[i]; }
        jlong& longAt(int i)     noexcept { return ((jlong*)e)[i]; }
        jfloat& floatAt(int i)   noexcept { return ((jfloat*)e)[i]; }
        jdouble& doubleAt(int i) noexcept { return ((jdouble*)e)[i]; }
    };

    /**
     * Provides a wrapper for accessing JNI array elements
     */
    struct JArray
    {
        Ref<jarray> array;
        const JniType type;

        JArray() noexcept : array{}, type{JniType::Object}
        {
        }
        JArray(jarray arr, JniType t) noexcept : array{arr}, type{t}
        {
        }
        JArray(const Ref<jarray>& a, JniType t) noexcept : array{a}, type{t}
        {
        }
        JArray(Ref<jarray>&& a, JniType t) noexcept : array{std::move(a)}, type{t}
        {
        }
        JArray(const JArray& arr) noexcept : array{arr.array}, type{arr.type}
        {
        }
        JArray(JArray&& arr) noexcept : array{std::move(arr.array)}, type{arr.type}
        {
        }
        ~JArray() noexcept = default;

        // implicit conversion to jarray
        operator jarray() const noexcept { return array; }
        jarray get() const noexcept { return array; }
        explicit operator bool() const noexcept { return (bool)array; }

        /** @returns New GlobalRef JArray that can be stored into global state */
        JArray toGlobal() noexcept { return { array.toGlobal(), type }; }

        int getLength() const noexcept
        {
            return array ? getEnv()->GetArrayLength(array) : 0;
        }
        jobject getObjectAt(int index) const noexcept
        {
            return getEnv()->GetObjectArrayElement((jobjectArray)array.get(), index);
        }
        void setObjectAt(int i, jobject obj) noexcept
        {
            getEnv()->SetObjectArrayElement((jobjectArray)array.get(), i, obj);
        }

        JString getStringAt(int index) const noexcept;

        /**
         * @brief Access jbyteArray, jintArray, etc.
         * @warning This can be inefficient for large arrays
         */
        ElementsRef getElements() noexcept { return ElementsRef{array.get(), type}; }

        /**
         * @brief Creates a String array:  java.lang.String[]
         * @throws if creating new array fails (type lookup error)
         */
        static JArray from(const std::vector<const char*>& strings);
    };

    /**
     * @brief JNI Class wrapper for accessing methods and fields
     * @note The clazz is a GlobalRef, so it can be stored in global thread_local storage
     */
    struct Class
    {
        JNIEnv* env;
        Ref<jclass> clazz;
        const char* name;

        Class(const char* className);
        Class(const char* className, std::nothrow_t) noexcept;

        operator jclass() const { return clazz; }
        explicit operator bool() const { return clazz != nullptr; }

        jni::Method method(const char* methodName, const char* signature);
        jni::Method method(const char* methodName, const char* signature, std::nothrow_t) noexcept;

        jni::Method staticMethod(const char* methodName, const char* signature);
        jni::Method staticMethod(const char* methodName, const char* signature, std::nothrow_t) noexcept;

        jni::Field field(const char* fieldName, const char* type);
        jni::Field field(const char* fieldName, const char* type, std::nothrow_t) noexcept;

        jni::Field staticField(const char* fieldName, const char* type);
        jni::Field staticField(const char* fieldName, const char* type, std::nothrow_t) noexcept;
    };

    // unwrap arguments to their native JNI types
    template<class T> inline const T& unwrap(const T& value) noexcept { return value; }
    template<class T> inline T unwrap(const Ref<T>& ref) noexcept { return ref.get(); }
    inline jstring unwrap(const JString& str) noexcept { return str.get(); }
    inline jarray unwrap(const JArray& arr) noexcept { return arr.get(); }

    /**
     * @brief JNI Method wrapper for calling Java methods
     */
    struct Method
    {
        Class& clazz;
        const jmethodID method; // method ID-s don't need to be freed
        const char* name;
        const char* signature;
        const bool isStatic;

        Method(Class& clazz, jmethodID method, const char* name, const char* signature, bool isStatic) noexcept;
        ~Method() noexcept = default;

        explicit operator bool() const noexcept { return method != nullptr; }

        // call this Method with the specified return type;
        // if instance is nullptr, it assumes static method
        Ref<jobject> objectV(jobject instance, ...) noexcept;
        JString      stringV(jobject instance, ...) noexcept;
        JArray       arrayV(JniType type, jobject instance, ...) noexcept;

        void         voidV(jobject instance, ...) noexcept;
        jboolean     booleanV(jobject instance, ...) noexcept;
        jbyte        byteV(jobject instance, ...) noexcept;
        jchar        charV(jobject instance, ...) noexcept;
        jshort       shortV(jobject instance, ...) noexcept;
        jint         intV(jobject instance, ...) noexcept;
        jlong        longV(jobject instance, ...) noexcept;
        jfloat       floatV(jobject instance, ...) noexcept;
        jdouble      doubleV(jobject instance, ...) noexcept;

        /** @brief Invokes method which returns LocalRef Object */
        template<class...Args> Ref<jobject> objectF(jobject instance, const Args&... args) noexcept
        {
            return objectV(instance, unwrap(args)...);
        }
        /** @brief Invokes method which return GlobalRef Object */
        template<class...Args> Ref<jobject> globalObjectF(jobject instance, const Args&... args) noexcept
        {
            return objectV(instance, unwrap(args)...).toGlobal();
        }
        template<class...Args> JString stringF(jobject instance, const Args&... args) noexcept
        {
            return stringV(instance, unwrap(args)...);
        }
        template<class...Args> JArray arrayF(JniType type, jobject instance, const Args&... args) noexcept
        {
            return arrayV(type, instance, unwrap(args)...);
        }

        template<class...Args> void voidF(jobject instance, const Args&... args) noexcept
        {
            voidV(instance, unwrap(args)...);
        }
        template<class...Args> jboolean booleanF(jobject instance, const Args&... args) noexcept
        {
            return booleanV(instance, unwrap(args)...);
        }
        template<class...Args> jbyte byteF(jobject instance, const Args&... args) noexcept
        {
            return byteV(instance, unwrap(args)...);
        }
        template<class...Args> jchar charF(jobject instance, const Args&... args) noexcept
        {
            return charV(instance, unwrap(args)...);
        }
        template<class...Args> jshort shortF(jobject instance, const Args&... args) noexcept
        {
            return shortV(instance, unwrap(args)...);
        }
        template<class...Args> jint intF(jobject instance, const Args&... args) noexcept
        {
            return intV(instance, unwrap(args)...);
        }
        template<class...Args> jlong longF(jobject instance, const Args&... args) noexcept
        {
            return longV(instance, unwrap(args)...);
        }
        template<class...Args> jfloat floatF(jobject instance, const Args&... args) noexcept
        {
            return floatV(instance, unwrap(args)...);
        }
        template<class...Args> jdouble doubleF(jobject instance, const Args&... args) noexcept
        {
            return doubleV(instance, unwrap(args)...);
        }
    };

    /**
     * @brief JNI Field wrapper for accessing Java fields
     */
    struct Field
    {
        Class& clazz;
        const jfieldID field;// field ID-s don't need to be freed
        const char* name;
        const char* type;
        const bool isStatic;

        Field(Class& clazz, jfieldID field, const char* name, const char* type, bool isStatic) noexcept;
        ~Field() noexcept = default;

        explicit operator bool() const noexcept { return field != nullptr; }

        // access this field with the specified type;
        // if instance is nullptr, it assumes static field
        Ref<jobject> getObject(jobject instance = nullptr) noexcept;
        Ref<jobject> getGlobalObject(jobject instance = nullptr) noexcept { return getObject(instance).toGlobal(); }
        JString      getString(jobject instance = nullptr) noexcept;
        JArray       getArray(JniType type, jobject instance = nullptr) noexcept;
        jboolean     getBoolean(jobject instance = nullptr) noexcept;
        jbyte        getByte(jobject instance = nullptr) noexcept;
        jchar        getChar(jobject instance = nullptr) noexcept;
        jshort       getShort(jobject instance = nullptr) noexcept;
        jint         getInt(jobject instance = nullptr) noexcept;
        jlong        getLong(jobject instance = nullptr) noexcept;
        jfloat       getFloat(jobject instance = nullptr) noexcept;
        jdouble      getDouble(jobject instance = nullptr) noexcept;
    };

    ////////////////////////////////////////////////////////////////////////////////////////////////
}} // namespace rpp::jni
#endif // __ANDROID__
