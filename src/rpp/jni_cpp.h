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
    template<class JObject> struct Ref
    {
        JObject obj;

        Ref() noexcept : obj{nullptr}
        {
        }
        // must be explicit, because it functions like unique_ptr
        explicit Ref(JObject obj) noexcept : obj{obj}
        {
        }
        ~Ref() noexcept
        {
            if (obj) getEnv()->DeleteLocalRef(obj);
        }
        Ref(Ref&& r) noexcept : obj{r.obj}
        {
            r.obj = nullptr;
        }
        Ref(const Ref& r) noexcept : obj{ getEnv()->NewLocalRef(r.obj) }
        {
        }
        Ref& operator=(Ref&& r) noexcept
        {
            std::swap(obj, r.obj);
            return *this;
        }
        Ref& operator=(const Ref& r) noexcept
        {
            if (this != &r)
            {
                // reconstruct
                auto* env = getEnv();
                if (obj) env->DeleteLocalRef(obj);
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
            if (obj) {
                getEnv()->DeleteLocalRef(obj);
                obj = nullptr;
            }
        }

        /**
         * @brief Converts this Local Ref to a Global Ref and resets this Ref
         * A global ref must be managed manually by the user
         */
        JObject to_global() noexcept
        {
            JObject g = nullptr;
            if (obj) {
                auto* env = getEnv();
                g = env->NewGlobalRef(obj);
                env->DeleteLocalRef(obj);
                obj = nullptr;
            }
            return g;
        }
    };

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
        std::string  StringObj(jobject instance, ...) noexcept;
        jboolean     Boolean(jobject instance, ...) noexcept;
        jbyte        Byte(jobject instance, ...) noexcept;
        jchar        Char(jobject instance, ...) noexcept;
        jshort       Short(jobject instance, ...) noexcept;
        jint         Int(jobject instance, ...) noexcept;
        jlong        Long(jobject instance, ...) noexcept;
        jfloat       Float(jobject instance, ...) noexcept;
        jdouble      Double(jobject instance, ...) noexcept;

        // wrapper helpers for arguments with Ref<> parameters
        template<class...Args>
        void Void(const Ref<jobject>& instance, const Ref<Args>&... args) noexcept
        {
            Void(instance.get(), (Args)args...);
        }
        template<class...Args>
        Ref<jobject> Object(const Ref<jobject>& instance, const Ref<Args>&... args) noexcept
        {
            return Object(instance.get(), (Args)args...);
        }
        template<class...Args>
        std::string String(const Ref<jobject>& instance, const Ref<Args>&... args) noexcept
        {
            return StringObj(instance.get(), (Args)args...);
        }
        template<class...Args>
        jboolean Boolean(const Ref<jobject>& instance, const Ref<Args>&... args) noexcept
        {
            return Boolean(instance.get(), (Args)args...);
        }
        template<class...Args>
        jbyte Byte(const Ref<jobject>& instance, const Ref<Args>&... args) noexcept
        {
            return Byte(instance.get(), (Args)args...);
        }
        template<class...Args>
        jchar Char(const Ref<jobject>& instance, const Ref<Args>&... args) noexcept
        {
            return Char(instance.get(), (Args)args...);
        }
        template<class...Args>
        jshort Short(const Ref<jobject>& instance, const Ref<Args>&... args) noexcept
        {
            return Short(instance.get(), (Args)args...);
        }
        template<class...Args>
        jint Int(const Ref<jobject>& instance, const Ref<Args>&... args) noexcept
        {
            return Int(instance.get(), (Args)args...);
        }
        template<class...Args>
        jlong Long(const Ref<jobject>& instance, const Ref<Args>&... args) noexcept
        {
            return Long(instance.get(), (Args)args...);
        }
        template<class...Args>
        jfloat Float(const Ref<jobject>& instance, const Ref<Args>&... args) noexcept
        {
            return Float(instance.get(), (Args)args...);
        }
        template<class...Args>
        jdouble Double(const Ref<jobject>& instance, const Ref<Args>&... args) noexcept
        {
            return Double(instance.get(), (Args)args...);
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
        std::string  String(jobject instance = nullptr) noexcept;
        jboolean     Boolean(jobject instance = nullptr) noexcept;
        jbyte        Byte(jobject instance = nullptr) noexcept;
        jchar        Char(jobject instance = nullptr) noexcept;
        jshort       Short(jobject instance = nullptr) noexcept;
        jint         Int(jobject instance = nullptr) noexcept;
        jlong        Long(jobject instance = nullptr) noexcept;
        jfloat       Float(jobject instance = nullptr) noexcept;
        jdouble      Double(jobject instance = nullptr) noexcept;
    };

    Ref<jstring>      String(const char* text);
    Ref<jobjectArray> Array(const std::vector<const char*>& strings);

    std::string to_string(JNIEnv* env, jstring s);

    ////////////////////////////////////////////////////////////////////////////////////////////////
}} // namespace rpp::jni
#endif // __ANDROID__
