/**
 * Android JNI Utils, Copyright (c) 2024 Jorma Rebane
 * Distributed under MIT Software License
 */
#if __ANDROID__
#include "jni_cpp.h"
#include "debugging.h"
#include <android/log.h>

namespace rpp { namespace jni {
    //////////////////////////////////////////////////////////////////////////////////////
    static JavaVM* javaVM;
    static thread_local jobject androidActivity; // needs to be thread_local
    static std::string mainActivityClassName;

    jint initVM(JavaVM* vm) noexcept
    {
        javaVM = vm;
        getEnv();
        return JNI_VERSION_1_6;
    }

    JNIEnv* getEnv() noexcept
    {
        static thread_local JNIEnv* env = [] // init once for each thread
        {
            Assert(javaVM, "getEnv() used before JNI_OnLoad(). Avoid calling JNI methods in static initializers.");
            JNIEnv* e = nullptr;
            if (javaVM->GetEnv((void**)&e, JNI_VERSION_1_6) != JNI_OK)
                javaVM->AttachCurrentThread(&e, nullptr);
            Assert(e, "javaVM->AttachCurrentThread() returned null JNIEnv*");
            return e;
        }();
        return env;
    }

    jobject getActivity(const char* mainActivityClass)
    {
        if (androidActivity)
            return androidActivity;

        // if we enter from another thread, attempt to reuse the previous initialize activity name
        const char* activityName = mainActivityClass ? mainActivityClass : mainActivityClassName.c_str();
        if (!activityName || !*activityName)
        {
            LogError("failed to init main activity: mainActivityClassName is empty");
            return nullptr;
        }

        if (auto mainClass = Class{activityName, std::nothrow})
        {
            if (auto currentActivity = mainClass.staticField("currentActivity", "Landroid/app/Activity;", std::nothrow))
            {
                androidActivity = currentActivity.getGlobalObject();
            }
            else if (auto activity = mainClass.staticField("activity", "Landroid/app/Activity;", std::nothrow))
            {
                androidActivity = activity.getGlobalObject();
            }
            else if (auto activity = mainClass.staticField("activity", "()Landroid/app/Activity;", std::nothrow))
            {
                androidActivity = activity.getGlobalObject();
            }
            else
            {
                LogError("failed to detect main activity field or method in %s", activityName);
                return nullptr;
            }

            // if init succeeded, save the activity name for future use
            if (androidActivity)
            {
                mainActivityClassName = activityName;
                return androidActivity;
            }
            LogError("main activity field or method in %s returned null", activityName);
            return nullptr;
        }
        LogError("failed to find main activity class: %s", activityName);
        return nullptr;
    }

    void initMainActivity(jobject globalHandle)
    {
        if (!androidActivity)
        {
            androidActivity = globalHandle;
        }
    }


    void checkForJNIException(const char* message)
    {
        auto* env = getEnv();
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
            throw std::runtime_error{message ? message : "JNI Call Failed"};
        }
    }

    void ClearException() noexcept
    {
        auto* env = getEnv();
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }

    #define JniThrow(format, ...) do {   \
            if (env->ExceptionCheck()) {     \
                env->ExceptionDescribe();    \
                env->ExceptionClear();       \
            }                                \
            ThrowErr(format, ##__VA_ARGS__); \
        } while (0)

    //////////////////////////////////////////////////////////////////////////////////////

    std::string JString::str() const noexcept
    {
        return toString(getEnv(), s.get());
    }

    JString JString::from(const char* text) noexcept
    {
        return makeRef(getEnv()->NewStringUTF(text));
    }

    std::string toString(JNIEnv* env, jstring s) noexcept
    {
        if (!s) return {};
        jsize len = env->GetStringUTFLength(s);
        std::string result((size_t)len, ' ');
        env->GetStringUTFRegion(s, 0, len, (char*) result.data());
        return result;
    }

    //////////////////////////////////////////////////////////////////////////////////////

    ElementsRef::ElementsRef(jarray a, JniType t) noexcept
        : arr{a}, type{t}
    {
        auto* env = getEnv();
        switch (t) {
            case JniType::Object:  e = nullptr; break;
            case JniType::Boolean: e = env->GetBooleanArrayElements((jbooleanArray)a, nullptr); break;
            case JniType::Byte:    e = env->GetByteArrayElements((jbyteArray)a, nullptr); break;
            case JniType::Char:    e = env->GetCharArrayElements((jcharArray)a, nullptr); break;
            case JniType::Short:   e = env->GetShortArrayElements((jshortArray)a, nullptr); break;
            case JniType::Int:     e = env->GetIntArrayElements((jintArray)a, nullptr); break;
            case JniType::Long:    e = env->GetLongArrayElements((jlongArray)a, nullptr); break;
            case JniType::Float:   e = env->GetFloatArrayElements((jfloatArray)a, nullptr); break;
            case JniType::Double:  e = env->GetDoubleArrayElements((jdoubleArray)a, nullptr); break;
        }
    }

    ElementsRef::~ElementsRef() noexcept
    {
        if (!e)
            return;
        auto* env = getEnv();
        switch (type) {
            case JniType::Object: break;
            case JniType::Boolean: env->ReleaseBooleanArrayElements((jbooleanArray)arr, (jboolean*)e, 0); break;
            case JniType::Byte:    env->ReleaseByteArrayElements((jbyteArray)arr, (jbyte*)e, 0); break;
            case JniType::Char:    env->ReleaseCharArrayElements((jcharArray)arr, (jchar*)e, 0); break;
            case JniType::Short:   env->ReleaseShortArrayElements((jshortArray)arr, (jshort*)e, 0); break;
            case JniType::Int:     env->ReleaseIntArrayElements((jintArray)arr, (jint*)e, 0); break;
            case JniType::Long:    env->ReleaseLongArrayElements((jlongArray)arr, (jlong*)e, 0); break;
            case JniType::Float:   env->ReleaseFloatArrayElements((jfloatArray)arr, (jfloat*)e, 0); break;
            case JniType::Double:  env->ReleaseDoubleArrayElements((jdoubleArray)arr, (jdouble*)e, 0); break;
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////

    JString JArray::getStringAt(int index) const noexcept
    {
        return JString{ (jstring)getObjectAt(index) };
    }

    JArray JArray::from(const std::vector<const char*>& strings)
    {
        JNIEnv* env = getEnv();
        Class stringClass{"java/lang/String"};
        jobjectArray arr = env->NewObjectArray(strings.size(), stringClass, 0);
        if (!arr) JniThrow("Failed to create java.lang.String[]");
        for (size_t i = 0; i < strings.size(); ++i)
        {
            env->SetObjectArrayElement(arr, i, env->NewStringUTF(strings[i]));
        }
        return JArray{ makeRef((jarray)arr), JniType::Object };
    }

    //////////////////////////////////////////////////////////////////////////////////////

    Class::Class(const char* className)
    {
        env = getEnv();
        clazz = makeGlobalRef<jclass>(env->FindClass(className));
        if (!clazz) JniThrow("Class not found: '%s'", className);
        name = className;
    }

    Class::Class(const char* className, std::nothrow_t) noexcept
    {
        env = getEnv();
        clazz = makeGlobalRef<jclass>(env->FindClass(className));
        if (!clazz) ClearException();
        name = className;
    }

    Method Class::method(const char* methodName, const char* signature)
    {
        jmethodID method = env->GetMethodID(clazz, methodName, signature);
        if (!method) JniThrow("Method '%s' not found in '%s'", methodName, name);
        return {*this, method, methodName, signature, /*isStatic*/false};
    }
    Method Class::method(const char* methodName, const char* signature, std::nothrow_t) noexcept
    {
        jmethodID method = env->GetMethodID(clazz, methodName, signature);
        if (!method) ClearException();
        return {*this, method, methodName, signature, /*isStatic*/false};
    }

    Method Class::staticMethod(const char* methodName, const char* signature)
    {
        jmethodID method = env->GetStaticMethodID(clazz, methodName, signature);
        if (!method) JniThrow("Static method '%s' not found in '%s'", methodName, name);
        return {*this, method, methodName, signature, /*isStatic*/true};
    }

    Method Class::staticMethod(const char* methodName, const char* signature, std::nothrow_t) noexcept
    {
        jmethodID method = env->GetStaticMethodID(clazz, methodName, signature);
        if (!method) ClearException();
        return {*this, method, methodName, signature, /*isStatic*/true};
    }

    Field Class::field(const char* fieldName, const char* type)
    {
        jfieldID field = env->GetFieldID(clazz, fieldName, type);
        if (!field) JniThrow("Field '%s' of type '%s' not found in '%s'", fieldName, type, name);
        return {*this, field, fieldName, type, /*isStatic*/false};
    }
    Field Class::field(const char* fieldName, const char* type, std::nothrow_t) noexcept
    {
        jfieldID field = env->GetFieldID(clazz, fieldName, type);
        if (!field) ClearException();
        return {*this, field, fieldName, type, /*isStatic*/false};
    }

    Field Class::staticField(const char* fieldName, const char* type)
    {
        jfieldID field = env->GetStaticFieldID(clazz, fieldName, type);
        if (!field) JniThrow("Static Field '%s' of type '%s' not found in '%s'", fieldName, type, name);
        return {*this, field, fieldName, type, /*isStatic*/true};
    }
    Field Class::staticField(const char* fieldName, const char* type, std::nothrow_t) noexcept
    {
        jfieldID field = env->GetStaticFieldID(clazz, fieldName, type);
        if (!field) ClearException();
        return {*this, field, fieldName, type, /*isStatic*/true};
    }

    //////////////////////////////////////////////////////////////////////////////////////

    Method::Method(Class& clazz, jmethodID method, const char* name, const char* signature, bool isStatic) noexcept
        : clazz{clazz}, method{method}, name{name}, signature{signature}, isStatic{isStatic}
    {
    }

    #define CheckMethodInstance(instance, retval) do { \
        if (!isStatic && !instance) { \
            __android_log_print(ANDROID_LOG_ERROR, "ReCpp", "NonStatic jni::Method %s called with null instance", name); \
            return retval; \
        } else if (isStatic && instance) { \
            __android_log_print(ANDROID_LOG_WARN, "ReCpp", "Static jni::Method %s called with instance=%p", name, instance); \
        } \
    } while(0)

    Ref<jobject> Method::objectV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args;
        va_start(args, instance);
        Ref<jobject> obj { instance
                         ? clazz.env->CallObjectMethodV(instance, method, args)
                         : clazz.env->CallStaticObjectMethodV(clazz.clazz, method, args)};
        va_end(args);
        return obj;
    }

    JString Method::stringV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args;
        va_start(args, instance);
        Ref<jstring> str { instance
                         ? (jstring)clazz.env->CallObjectMethodV(instance, method, args)
                         : (jstring)clazz.env->CallStaticObjectMethodV(clazz.clazz, method, args)};
        va_end(args);
        return JString{ std::move(str) };
    }

    JArray Method::arrayV(JniType type, jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args;
        va_start(args, instance);
        Ref<jarray> arr { instance
                            ? (jarray)clazz.env->CallObjectMethodV(instance, method, args)
                            : (jarray)clazz.env->CallStaticObjectMethodV(clazz.clazz, method, args)};
        va_end(args);
        return { std::move(arr), type };
    }

    void Method::voidV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, );
        va_list args;
        va_start(args, instance);
        if (instance) clazz.env->CallVoidMethodV(instance, method, args);
        else clazz.env->CallStaticVoidMethodV(clazz.clazz, method, args);
        va_end(args);
    }

    jboolean Method::booleanV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args;
        va_start(args, instance);
        jboolean result = instance
                        ? clazz.env->CallBooleanMethodV(instance, method, args)
                        : clazz.env->CallStaticBooleanMethodV(clazz.clazz, method, args);
        va_end(args);
        return result;
    }

    jbyte Method::byteV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args;
        va_start(args, instance);
        jbyte result = instance
                     ? clazz.env->CallByteMethodV(instance, method, args)
                     : clazz.env->CallStaticByteMethodV(clazz.clazz, method, args);
        va_end(args);
        return result;
    }

    jchar Method::charV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args;
        va_start(args, instance);
        jchar result = instance
                     ? clazz.env->CallCharMethodV(instance, method, args)
                     : clazz.env->CallStaticCharMethodV(clazz.clazz, method, args);
        va_end(args);
        return result;
    }

    jshort Method::shortV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args;
        va_start(args, instance);
        jshort result = instance
                      ? clazz.env->CallShortMethodV(instance, method, args)
                      : clazz.env->CallStaticShortMethodV(clazz.clazz, method, args);
        va_end(args);
        return result;
    }

    jint Method::intV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args;
        va_start(args, instance);
        jint result = instance
                      ? clazz.env->CallIntMethodV(instance, method, args)
                      : clazz.env->CallStaticIntMethodV(clazz.clazz, method, args);
        va_end(args);
        return result;
    }

    jlong Method::longV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args;
        va_start(args, instance);
        jlong result = instance
                     ? clazz.env->CallLongMethodV(instance, method, args)
                     : clazz.env->CallStaticLongMethodV(clazz.clazz, method, args);
        va_end(args);
        return result;
    }

    jfloat Method::floatV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args;
        va_start(args, instance);
        jfloat result = instance
                      ? clazz.env->CallFloatMethodV(instance, method, args)
                      : clazz.env->CallStaticFloatMethodV(clazz.clazz, method, args);
        va_end(args);
        return result;
    }

    jdouble Method::doubleV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args;
        va_start(args, instance);
        jdouble result = instance
                       ? clazz.env->CallDoubleMethodV(instance, method, args)
                       : clazz.env->CallStaticDoubleMethodV(clazz.clazz, method, args);
        va_end(args);
        return result;
    }

    //////////////////////////////////////////////////////////////////////////////////////

    Field::Field(Class& clazz, jfieldID field, const char* name, const char* type, bool isStatic) noexcept
        : clazz{clazz}, field{field}, name{name}, type{type}, isStatic{isStatic}
    {
    }

    #define CheckFieldInstance(instance, retval) do { \
        if (!isStatic && !instance) { \
            __android_log_print(ANDROID_LOG_ERROR, "ReCpp", "NonStatic jni::Field %s called with null instance", name); \
            return retval; \
        } else if (isStatic && instance) { \
            __android_log_print(ANDROID_LOG_WARN, "ReCpp", "Static jni::Field %s called with instance=%p", name, instance); \
        } \
    } while(0)

    Ref<jobject> Field::getObject(jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        jobject obj = isStatic
                    ? clazz.env->GetStaticObjectField(clazz.clazz, field)
                    : clazz.env->GetObjectField(instance, field);
        return Ref<jobject>{obj};
    }

    JString Field::getString(jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        jstring str = isStatic
                    ? (jstring)clazz.env->GetStaticObjectField(clazz.clazz, field)
                    : (jstring)clazz.env->GetObjectField(instance, field);
        return JString{str};
    }

    JArray Field::getArray(JniType type, jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        jarray arr = isStatic
                   ? (jarray)clazz.env->GetStaticObjectField(clazz.clazz, field)
                   : (jarray)clazz.env->GetObjectField(instance, field);
        return JArray{arr, type};
    }

    jboolean Field::getBoolean(jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        return isStatic
            ? clazz.env->GetStaticBooleanField(clazz.clazz, field)
            : clazz.env->GetBooleanField(instance, field);
    }

    jbyte Field::getByte(jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        return isStatic
            ? clazz.env->GetStaticByteField(clazz.clazz, field)
            : clazz.env->GetByteField(instance, field);
    }

    jchar Field::getChar(jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        return isStatic
            ? clazz.env->GetStaticCharField(clazz.clazz, field)
            : clazz.env->GetCharField(instance, field);
    }

    jshort Field::getShort(jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        return isStatic
            ? clazz.env->GetStaticShortField(clazz.clazz, field)
            : clazz.env->GetShortField(instance, field);
    }

    jint Field::getInt(jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        return isStatic
            ? clazz.env->GetStaticIntField(clazz.clazz, field)
            : clazz.env->GetIntField(instance, field);
    }

    jlong Field::getLong(jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        return isStatic
            ? clazz.env->GetStaticLongField(clazz.clazz, field)
            : clazz.env->GetLongField(instance, field);
    }

    jfloat Field::getFloat(jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        return isStatic
            ?  clazz.env->GetStaticFloatField(clazz.clazz, field)
            : clazz.env->GetFloatField(instance, field);
    }

    jdouble Field::getDouble(jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        return isStatic
            ? clazz.env->GetStaticDoubleField(clazz.clazz, field)
            : clazz.env->GetDoubleField(instance, field);
    }

    //////////////////////////////////////////////////////////////////////////////////////

}} // namespace rpp::jni

#endif // !__ANDROID__
