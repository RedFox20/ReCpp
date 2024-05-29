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
     * @param env Optionally provide an already attached JNIEnv
     */
    jint initVM(JavaVM* vm, JNIEnv* env = nullptr) noexcept;

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
     * @brief Gets the JNI environment for the CURRENT THREAD ONLY
     * @warning You must first call `initVM()` or define RPP_DEFINE_JNI_ONLOAD
     *
     * @warning JNIEnv is only valid per-thread, so do not store this globally,
     *          except if using thread_local storage.
     */
    JNIEnv* getEnv() noexcept;

    /**
     * @brief Get the main Android `Activity` object from the MyMainApp.currentActivity field
     * @param mainActivityClass The main activity class name, eg: "com/unity3d/player/UnityPlayer"
     *        if null, then returns the previously configured main activity
     * @warning The main activity can only be configured once per app init
    */
    jobject getMainActivity(const char* mainActivityClass = nullptr);

    /**
     * @brief Manually sets the main activity object for the app.
     *        The ref count is increased and ref is internally converted to a Global Ref,
     *        so the caller of this function just needs to perform usual ref management.
     * @code
     *     jobject activity = myGetJniActivityLocalRef(...);
     *     rpp::jni::initMainActivity(activity);
     *     env->DeleteLocalRef(activity);
     * @endcode
     * @param mainActivityRef Reference to the main activity object
     * @warning The main activity can only be configured once per application
     */
    void initMainActivity(jobject mainActivityRef) noexcept;

    /**
     * @brief Checks for pending JNI exceptions and rethrows them as a C++ exception
     * @param message Exception message to show
     */
    void checkForJNIException(const char* message = nullptr);

    ////////////////////////////////////////////////////////////////////////////////////////////////

    struct Class;
    struct Method;
    struct Field;

    struct JniRef
    {
        jobject obj = nullptr;
        bool isGlobal = false;

        JniRef() = default;
        explicit JniRef(jobject localOrGlobalRef) noexcept;
        ~JniRef() noexcept { reset(); }

        void reset() noexcept;
        void reset(const JniRef& r) noexcept;
        /** @brief Converts this Local Ref to a Global Ref (if needed) */
        void makeGlobal() noexcept;

        JniRef(const JniRef& r) noexcept
        {
            reset(r);
        }
        JniRef& operator=(const JniRef& r) noexcept
        {
            if (this != &r) reset(r);
            return *this;
        }

        JniRef(JniRef&& r) noexcept
        {
            std::swap(obj, r.obj);
            std::swap(isGlobal, r.isGlobal);
        }
        JniRef& operator=(JniRef&& r) noexcept
        {
            std::swap(obj, r.obj);
            std::swap(isGlobal, r.isGlobal);
            return *this;
        }
    };

    /**
     *  JNI LocalRef need to be deleted to avoid running out of allowed references.
     *  This is a smart pointer to automatically manage LocalRef-s and GlobalRef-s.
     *
     *  @warning Only GlobalRef can be stored across JNI calls!
     *  @warning LocalRefs are only valid on a single thread and until code execution
     *           returns to JVM context
     */
    template<class JObject>
    struct Ref : public JniRef
    {
        using JniRef::JniRef;

        /**
         * @brief Initialize from a previous untracked object.
         * It can be a LocalRef, GlobalRef or Invalid ref.
         */
        explicit Ref(JObject localOrGlobalRef) noexcept
            : JniRef{localOrGlobalRef}
        {
        }

        /**
         * @brief Initialize from a previously untracked object.
         * The type is specified by @param global.
         */
        Ref(JObject o, bool global) noexcept
        {
            obj = o;
            isGlobal = global;
        }

        /**
         * @return Gets the raw internal JNI jobject.
         * @warning Implicit operators are forbidden since they can cause ownership bugs.
         */
        JObject get() const noexcept { return static_cast<JObject>(obj); }

        /**
         * @brief static_cast the object to another type
         * @warning This is unsafe and the returned object is not tracked or type-checked!
         */
        template<class T> T as() const noexcept { return static_cast<T>(obj); }
        explicit operator bool() const noexcept { return obj != nullptr; }

        /**
         * @brief Converts the Ref<JObject> to a Ref<T> and resets this Ref
         */
        template<class T> Ref<T> releaseAs() noexcept
        {
            Ref<T> ref { static_cast<T>(obj), isGlobal };
            obj = nullptr;
            isGlobal = false;
            return ref;
        }

        /**
         * @returns A new Global Ref from this Ref<T>
         */
        Ref<JObject> toGlobal() noexcept
        {
            Ref<JObject> g;
            if (obj)
            {
                // even if it's already global, we need to increase the global refcount
                g.obj = static_cast<JObject>(getEnv()->NewGlobalRef(obj));
                g.isGlobal = true;
            }
            return g;
        }
    };

    /**
     * @brief Takes immeidate ownership of a JNI object and makes a new LocalRef of it.
     * LocalRefs are limited within default JVM context and this can help manage them.
     * @warning DO NOT STORE LocalRef into static/global storage!!!
     * @param untrackedLocalOrGlobalRef jobject to turn into a smart LocalRef
     *        - it can be a LocalRef or GlobalRef but is expected to be untracked
     */
    template<class JObject>
    Ref<JObject> makeRef(JObject untrackedLocalOrGlobalRef) noexcept
    {
        return Ref<JObject>{untrackedLocalOrGlobalRef};
    }

    /**
     * @brief Takes immeidate ownership of a JNI object and makes a new GlobalRef of it.
     * Global Refs can be used across JNI calls and are not automaticaly GC-ed
     * @note GlobalRefs can be safely stored in global static state
     * @param untrackedLocalOrGlobalRef jobject to turn into a smart GlobalRef
     *        - it can be a LocalRef or a GlobalRef but is expected to be untracked
     */
    template<class JObject>
    Ref<JObject> makeGlobalRef(JObject untrackedLocalOrGlobalRef) noexcept
    {
        Ref<JObject> r{untrackedLocalOrGlobalRef};
        r.makeGlobal(); // no-op if already global
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

        jstring get() const noexcept { return s.get(); }
        explicit operator bool() const noexcept { return (bool)s; }

        /** @returns New GlobalRef JString that can be stored into global state */
        JString toGlobal() noexcept { return { s.toGlobal() }; }

        int getLength() const noexcept { return s ? getEnv()->GetStringLength(s.get()) : 0; }

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

        jarray get() const noexcept { return array.get(); }
        explicit operator bool() const noexcept { return (bool)array; }

        /** @returns New GlobalRef JArray that can be stored into global state */
        JArray toGlobal() noexcept { return { array.toGlobal(), type }; }

        int getLength() const noexcept
        {
            return array ? getEnv()->GetArrayLength(array.get()) : 0;
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
     * @note The clazz is a GlobalRef, so it can be stored in global storage
     */
    struct Class
    {
        Ref<jclass> clazz; // Always a GlobalRef (required by JNI)
        const char* name;

        Class(const char* className);
        Class(const char* className, std::nothrow_t) noexcept;

        jclass get() const noexcept { return clazz.get(); }
        explicit operator bool() const noexcept { return (bool)clazz; }

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

    // a parameter which accepts jobject or JniRef
    struct JniRefParam
    {
        jobject o;
        JniRefParam(jobject o) noexcept : o{o} {}
        JniRefParam(JniRef&& r) noexcept : o{r.obj} {}
        JniRefParam(const JniRef& r) noexcept : o{r.obj} {}
        JniRefParam(std::nullptr_t) noexcept : o{nullptr} {}
    };

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
        template<class...Args> Ref<jobject> objectF(JniRefParam instance, const Args&... args) noexcept
        {
            return objectV(instance.o, unwrap(args)...);
        }
        /** @brief Invokes method which return GlobalRef Object */
        template<class...Args> Ref<jobject> globalObjectF(JniRefParam instance, const Args&... args) noexcept
        {
            return objectV(instance.o, unwrap(args)...).toGlobal();
        }
        template<class...Args> JString stringF(JniRefParam instance, const Args&... args) noexcept
        {
            return stringV(instance.o, unwrap(args)...);
        }
        template<class...Args> JArray arrayF(JniType type, JniRefParam instance, const Args&... args) noexcept
        {
            return arrayV(type, instance.o, unwrap(args)...);
        }

        template<class...Args> void voidF(JniRefParam instance, const Args&... args) noexcept
        {
            voidV(instance.o, unwrap(args)...);
        }
        template<class...Args> jboolean booleanF(JniRefParam instance, const Args&... args) noexcept
        {
            return booleanV(instance.o, unwrap(args)...);
        }
        template<class...Args> jbyte byteF(JniRefParam instance, const Args&... args) noexcept
        {
            return byteV(instance.o, unwrap(args)...);
        }
        template<class...Args> jchar charF(JniRefParam instance, const Args&... args) noexcept
        {
            return charV(instance.o, unwrap(args)...);
        }
        template<class...Args> jshort shortF(JniRefParam instance, const Args&... args) noexcept
        {
            return shortV(instance.o, unwrap(args)...);
        }
        template<class...Args> jint intF(JniRefParam instance, const Args&... args) noexcept
        {
            return intV(instance.o, unwrap(args)...);
        }
        template<class...Args> jlong longF(JniRefParam instance, const Args&... args) noexcept
        {
            return longV(instance.o, unwrap(args)...);
        }
        template<class...Args> jfloat floatF(JniRefParam instance, const Args&... args) noexcept
        {
            return floatV(instance.o, unwrap(args)...);
        }
        template<class...Args> jdouble doubleF(JniRefParam instance, const Args&... args) noexcept
        {
            return doubleV(instance.o, unwrap(args)...);
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
        Ref<jobject> getGlobalObject(jobject instance = nullptr) noexcept;
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
