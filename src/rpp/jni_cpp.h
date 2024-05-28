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
    template<class JObject>
    struct Ref
    {
        JObject obj;
        bool isGlobal = false;

        Ref() noexcept : obj{nullptr}
        {
        }
        // must be explicit, because it functions like unique_ptr
        explicit Ref(JObject obj) noexcept : obj{obj}
        {
        }
        ~Ref() noexcept
        {
            if (obj)
            {
                if (isGlobal)
                    getEnv()->DeleteGlobalRef(obj);
                else
                    getEnv()->DeleteLocalRef(obj);
            }
        }
        Ref(Ref&& r) noexcept : obj{r.obj}, isGlobal{r.isGlobal}
        {
            r.obj = nullptr;
            r.isGlobal = false;
        }
        // local only ref copy
        Ref(const Ref& r) noexcept : obj{getEnv()->NewLocalRef(r.obj)}, isGlobal{false}
        {
        }
        Ref& operator=(Ref&& r) noexcept
        {
            std::swap(obj, r.obj);
            std::swap(isGlobal, r.isGlobal);
            return *this;
        }
        // local only ref copy
        Ref& operator=(const Ref& r) noexcept
        {
            if (this != &r)
            {
                // reconstruct
                auto* env = getEnv();
                if (obj) isGlobal ? env->DeleteGlobalRef(obj) : env->DeleteLocalRef(obj);
                isGlobal = false;
                obj = env->NewLocalRef(r.obj);
            }
            return *this;
        }

        // implicit conversion of Ref<JObject> to JObject
        operator JObject() const noexcept { return obj; }
        JObject get() const noexcept { return obj; }

        template<class T> T as() const noexcept { return (T)obj; }
        explicit operator bool() const noexcept { return obj != nullptr; }

        void reset() noexcept
        {
            if (obj)
            {
                if (isGlobal)
                    getEnv()->DeleteGlobalRef(obj);
                else
                    getEnv()->DeleteLocalRef(obj);
                obj = nullptr;
                isGlobal = false;
            }
        }

        // transforms into another Type
        template<class T> Ref<T> releaseAs() noexcept
        {
            Ref<T> ref;
            ref.obj = (T)obj;
            ref.isGlobal = isGlobal;
            obj = nullptr;
            isGlobal = false;
            return ref;
        }

        /**
         * @brief Converts this Local Ref to a Global Ref and resets this Ref
         * A global ref must be managed manually by the user
         */
        JObject to_global() noexcept
        {
            if (!isGlobal) {
                JObject g = nullptr;
                if (obj) {
                    auto* env = getEnv();
                    g = (JObject) env->NewGlobalRef(obj);
                    env->DeleteLocalRef(obj);
                    obj = nullptr;
                }
                return g;
            } else {
                return obj;
            }
        }

        void make_global() noexcept
        {
            if (!isGlobal && obj) {
                auto* env = getEnv();
                JObject g = (JObject) env->NewGlobalRef(obj);
                env->DeleteLocalRef(obj);
                obj = g;
                isGlobal = true;
            }
        }
    };

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
        JString(Ref<jstring> s) noexcept : s{std::move(s)}
        {
        }
        JString(jstring s) noexcept : s{Ref<jstring>{s}}
        {
        }

        operator jstring() const noexcept { return s.get(); }
        jstring get() const noexcept { return s.get(); }
        explicit operator bool() const noexcept { return (bool)s; }

        int getLength() const noexcept { return s ? getEnv()->GetStringLength(s) : 0; }

        /** @returns jstring converted to C++ std::string */
        std::string str() const noexcept;
    };

    /**
     * @brief Converts C-string into a jstring (ref counted)
     */
    JString MakeString(const char* text) noexcept;
    inline JString MakeString(const std::string& text) noexcept
    {
        return MakeString(text.c_str());
    }

    // core util: JNI jstring to std::string
    std::string ToString(JNIEnv* env, jstring s) noexcept;

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

        JArray(Ref<jarray> a, JniType t) noexcept : array{std::move(a)}, type{t}
        {
        }

        // implicit conversion to jarray
        operator jarray() const noexcept { return array; }
        jarray get() const noexcept { return array; }
        explicit operator bool() const noexcept { return (bool)array; }

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
    };

    /**
     * @brief Creates a String array:  java.lang.String[]
     * @throws if creating new array fails (type lookup error)
     */
    JArray MakeArray(const std::vector<const char*>& strings);

    /**
     * @brief Makes a new smart ref of a JNI object
     */
    template<class JObject> Ref<JObject> MakeRef(JObject obj) noexcept
    {
        return Ref<JObject>{obj};
    }

    struct Class
    {
        JNIEnv* env;
        Ref<jclass> clazz;
        const char* name;

        Class(const char* className);
        Class(const char* className, std::nothrow_t) noexcept;

        operator jclass() const { return clazz; }
        explicit operator bool() const { return clazz != nullptr; }

        jni::Method Method(const char* methodName, const char* signature);
        jni::Method SMethod(const char* methodName, const char* signature);
        jni::Method Method(const char* methodName, const char* signature, std::nothrow_t) noexcept;
        jni::Method SMethod(const char* methodName, const char* signature, std::nothrow_t) noexcept;
        jni::Field Field(const char* fieldName, const char* type);
        jni::Field SField(const char* fieldName, const char* type);
        jni::Field Field(const char* fieldName, const char* type, std::nothrow_t) noexcept;
        jni::Field SField(const char* fieldName, const char* type, std::nothrow_t) noexcept;
    };

    // unwrap arguments to their native JNI types
    template<class T> inline const T& unwrap(const T& value) noexcept { return value; }
    template<class T> inline T* unwrap(const Ref<T>& ref) noexcept { return ref.get(); }
    inline jstring unwrap(const JString& str) noexcept { return str.get(); }
    inline jarray unwrap(const JArray& arr) noexcept { return arr.get(); }

    struct Method
    {
        Class& clazz;
        jmethodID method; // method ID-s don't need to be freed
        const char* name;
        const char* signature;

        Method(Class& clazz, jmethodID method, const char* name, const char* signature) noexcept;
        ~Method() noexcept = default;

        explicit operator bool() const noexcept { return method != nullptr; }

        // call this Method with the specified return type;
        // if instance is nullptr, it assumes static method
        void         Void(jobject instance, ...) noexcept;
        Ref<jobject> Object(jobject instance, ...) noexcept;
        JString      String(jobject instance, ...) noexcept;
        jboolean     Boolean(jobject instance, ...) noexcept;
        jbyte        Byte(jobject instance, ...) noexcept;
        jchar        Char(jobject instance, ...) noexcept;
        jshort       Short(jobject instance, ...) noexcept;
        jint         Int(jobject instance, ...) noexcept;
        jlong        Long(jobject instance, ...) noexcept;
        jfloat       Float(jobject instance, ...) noexcept;
        jdouble      Double(jobject instance, ...) noexcept;
        JArray       Array(JniType type, jobject instance, ...) noexcept;

        // wrapper helpers for arguments with Ref<> parameters
        template<class...Args>
        void Void(const Ref<jobject>& instance, const Ref<Args>&... args) noexcept
        {
            Void(instance.get(), unwrap(args)...);
        }
        template<class...Args>
        Ref<jobject> Object(const Ref<jobject>& instance, const Args&... args) noexcept
        {
            return Object(instance.get(), unwrap(args)...);
        }
        template<class...Args>
        JString String(const Ref<jobject>& instance, const Args&... args) noexcept
        {
            return String(instance.get(), unwrap(args)...);
        }
        template<class...Args>
        jboolean Boolean(const Ref<jobject>& instance, const Args&... args) noexcept
        {
            return Boolean(instance.get(), unwrap(args)...);
        }
        template<class...Args>
        jbyte Byte(const Ref<jobject>& instance, const Args&... args) noexcept
        {
            return Byte(instance.get(), unwrap(args)...);
        }
        template<class...Args>
        jchar Char(const Ref<jobject>& instance, const Args&... args) noexcept
        {
            return Char(instance.get(), unwrap(args)...);
        }
        template<class...Args>
        jshort Short(const Ref<jobject>& instance, const Args&... args) noexcept
        {
            return Short(instance.get(), unwrap(args)...);
        }
        template<class...Args>
        jint Int(const Ref<jobject>& instance, const Args&... args) noexcept
        {
            return Int(instance.get(), unwrap(args)...);
        }
        template<class...Args>
        jlong Long(const Ref<jobject>& instance, const Args&... args) noexcept
        {
            return Long(instance.get(), unwrap(args)...);
        }
        template<class...Args>
        jfloat Float(const Ref<jobject>& instance, const Args&... args) noexcept
        {
            return Float(instance.get(), unwrap(args)...);
        }
        template<class...Args>
        jdouble Double(const Ref<jobject>& instance, const Args&... args) noexcept
        {
            return Double(instance.get(), unwrap(args)...);
        }
        template<class...Args>
        JArray Array(JniType type, const Ref<jobject>& instance, const Args&... args) noexcept
        {
            return Array(type, instance.get(), unwrap(args)...);
        }
    };

    struct Field
    {
        Class& clazz;
        jfieldID field;// field ID-s don't need to be freed
        const char* name;
        const char* type;

        Field(Class& clazz, jfieldID field, const char* name, const char* type) noexcept;
        ~Field() noexcept = default;

        explicit operator bool() const noexcept { return field != nullptr; }

        // access this field with the specified type;
        // if instance is nullptr, it assumes static field
        Ref<jobject> Object(jobject instance = nullptr) noexcept;
        JString      String(jobject instance = nullptr) noexcept;
        jboolean     Boolean(jobject instance = nullptr) noexcept;
        jbyte        Byte(jobject instance = nullptr) noexcept;
        jchar        Char(jobject instance = nullptr) noexcept;
        jshort       Short(jobject instance = nullptr) noexcept;
        jint         Int(jobject instance = nullptr) noexcept;
        jlong        Long(jobject instance = nullptr) noexcept;
        jfloat       Float(jobject instance = nullptr) noexcept;
        jdouble      Double(jobject instance = nullptr) noexcept;
    };

////////////////////////////////////////////////////////////////////////////////////////////////
}} // namespace rpp::jni
#endif // __ANDROID__
