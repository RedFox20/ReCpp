/**
 * Android JNI Utils, Copyright (c) 2024 Jorma Rebane
 * Distributed under MIT Software License
 */
#if __ANDROID__
#include "jni_cpp.h"
#include "debugging.h"

namespace rpp { namespace jni {
    //////////////////////////////////////////////////////////////////////////////////////
    static JavaVM* javaVM;
    static jobject androidActivity;

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
        if (mainActivityClass && !androidActivity)
        {
            try
            {
                if (auto mainClass = Class{mainActivityClass})
                {
                    if (auto currentActivity = mainClass.SField("currentActivity", "Landroid/app/Activity;", std::nothrow))
                    {
                        androidActivity = currentActivity.Object().toGlobal();
                    }
                    else if (auto activity = mainClass.SField("activity", "Landroid/app/Activity;", std::nothrow))
                    {
                        androidActivity = activity.Object().toGlobal();
                    }
                    else if (auto activity = mainClass.SMethod("activity", "()Landroid/app/Activity;", std::nothrow))
                    {
                        androidActivity = activity.Object(nullptr).toGlobal();
                    }
                    else
                    {
                        LogError("failed to init main activity");
                    }
                }
            }
            catch (const std::exception& e)
            {
                LogError("failed to init main activity: %s", e.what());
            }
        }
        return androidActivity;
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
        return ToString(getEnv(), s.get());
    }

    JString MakeString(const char* text) noexcept
    {
        return MakeRef(getEnv()->NewStringUTF(text));
    }

    std::string ToString(JNIEnv* env, jstring s) noexcept
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

    JArray MakeArray(const std::vector<const char*>& strings)
    {
        JNIEnv* env = getEnv();
        Class stringClass{"java/lang/String"};
        jobjectArray arr = env->NewObjectArray(strings.size(), stringClass, 0);
        if (!arr) JniThrow("Failed to create java.lang.String[]");
        for (size_t i = 0; i < strings.size(); ++i)
        {
            env->SetObjectArrayElement(arr, i, env->NewStringUTF(strings[i]));
        }
        return JArray{ MakeRef((jarray)arr), JniType::Object };
    }

    //////////////////////////////////////////////////////////////////////////////////////

    Class::Class(const char* className)
    {
        env = getEnv();
        clazz = MakeRef<jclass>(env->FindClass(className));
        clazz.makeGlobal();
        if (!clazz) JniThrow("Class not found: '%s'", className);
        name = className;
    }

    Class::Class(const char* className, std::nothrow_t) noexcept
    {
        env = getEnv();
        clazz = MakeRef<jclass>(env->FindClass(className));
        clazz.makeGlobal();
        if (!clazz) ClearException();
        name = className;
    }

    Method Class::Method(const char* methodName, const char* signature)
    {
        jmethodID method = env->GetMethodID(clazz, methodName, signature);
        if (!method) JniThrow("Method '%s' not found in '%s'", methodName, name);
        return {*this, method, methodName, signature};
    }

    Method Class::SMethod(const char* methodName, const char* signature)
    {
        jmethodID method = env->GetStaticMethodID(clazz, methodName, signature);
        if (!method) JniThrow("Static method '%s' not found in '%s'", methodName, name);
        return {*this, method, methodName, signature};
    }

    Method Class::Method(const char* methodName, const char* signature, std::nothrow_t) noexcept
    {
        jmethodID method = env->GetMethodID(clazz, methodName, signature);
        if (!method) ClearException();
        return {*this, method, methodName, signature};
    }

    Method Class::SMethod(const char* methodName, const char* signature, std::nothrow_t) noexcept
    {
        jmethodID method = env->GetStaticMethodID(clazz, methodName, signature);
        if (!method) ClearException();
        return {*this, method, methodName, signature};
    }

    Field Class::Field(const char* fieldName, const char* type)
    {
        jfieldID field = env->GetFieldID(clazz, fieldName, type);
        if (!field) JniThrow("Field '%s' of type '%s' not found in '%s'", fieldName, type, name);
        return {*this, field, fieldName, type};
    }

    Field Class::SField(const char* fieldName, const char* type)
    {
        jfieldID field = env->GetStaticFieldID(clazz, fieldName, type);
        if (!field) JniThrow("Static Field '%s' of type '%s' not found in '%s'", fieldName, type, name);
        return {*this, field, fieldName, type};
    }

    Field Class::Field(const char* fieldName, const char* type, std::nothrow_t) noexcept
    {
        jfieldID field = env->GetFieldID(clazz, fieldName, type);
        if (!field) ClearException();
        return {*this, field, fieldName, type};
    }
    Field Class::SField(const char* fieldName, const char* type, std::nothrow_t) noexcept
    {
        jfieldID field = env->GetStaticFieldID(clazz, fieldName, type);
        if (!field) ClearException();
        return {*this, field, fieldName, type};
    }

    //////////////////////////////////////////////////////////////////////////////////////


    Method::Method(Class& clazz, jmethodID method, const char* name, const char* signature) noexcept
        : clazz{clazz}, method{method}, name{name}, signature{signature}
    {
    }

    void Method::Void(jobject instance, ...) noexcept
    {
        va_list args;
        va_start(args, instance);
        if (instance) clazz.env->CallVoidMethodV(instance, method, args);
        else clazz.env->CallStaticVoidMethodV(clazz.clazz, method, args);
        va_end(args);
    }

    Ref<jobject> Method::Object(jobject instance, ...) noexcept
    {
        va_list args;
        va_start(args, instance);
        Ref<jobject> obj { instance
                         ? clazz.env->CallObjectMethodV(instance, method, args)
                         : clazz.env->CallStaticObjectMethodV(clazz.clazz, method, args)};
        va_end(args);
        return obj;
    }

    JString Method::String(jobject instance, ...) noexcept
    {
        va_list args;
        va_start(args, instance);
        Ref<jstring> str { instance
                         ? (jstring)clazz.env->CallObjectMethodV(instance, method, args)
                         : (jstring)clazz.env->CallStaticObjectMethodV(clazz.clazz, method, args)};
        va_end(args);
        return JString{ std::move(str) };
    }

    jboolean Method::Boolean(jobject instance, ...) noexcept
    {
        va_list args;
        va_start(args, instance);
        jboolean result = instance
                        ? clazz.env->CallBooleanMethodV(instance, method, args)
                        : clazz.env->CallStaticBooleanMethodV(clazz.clazz, method, args);
        va_end(args);
        return result;
    }

    jbyte Method::Byte(jobject instance, ...) noexcept
    {
        va_list args;
        va_start(args, instance);
        jbyte result = instance
                     ? clazz.env->CallByteMethodV(instance, method, args)
                     : clazz.env->CallStaticByteMethodV(clazz.clazz, method, args);
        va_end(args);
        return result;
    }

    jchar Method::Char(jobject instance, ...) noexcept
    {
        va_list args;
        va_start(args, instance);
        jchar result = instance
                     ? clazz.env->CallCharMethodV(instance, method, args)
                     : clazz.env->CallStaticCharMethodV(clazz.clazz, method, args);
        va_end(args);
        return result;
    }

    jshort Method::Short(jobject instance, ...) noexcept
    {
        va_list args;
        va_start(args, instance);
        jshort result = instance
                      ? clazz.env->CallShortMethodV(instance, method, args)
                      : clazz.env->CallStaticShortMethodV(clazz.clazz, method, args);
        va_end(args);
        return result;
    }

    jint Method::Int(jobject instance, ...) noexcept
    {
        va_list args;
        va_start(args, instance);
        jint result = instance
                      ? clazz.env->CallIntMethodV(instance, method, args)
                      : clazz.env->CallStaticIntMethodV(clazz.clazz, method, args);
        va_end(args);
        return result;
    }

    jlong Method::Long(jobject instance, ...) noexcept
    {
        va_list args;
        va_start(args, instance);
        jlong result = instance
                     ? clazz.env->CallLongMethodV(instance, method, args)
                     : clazz.env->CallStaticLongMethodV(clazz.clazz, method, args);
        va_end(args);
        return result;
    }

    jfloat Method::Float(jobject instance, ...) noexcept
    {
        va_list args;
        va_start(args, instance);
        jfloat result = instance
                      ? clazz.env->CallFloatMethodV(instance, method, args)
                      : clazz.env->CallStaticFloatMethodV(clazz.clazz, method, args);
        va_end(args);
        return result;
    }

    jdouble Method::Double(jobject instance, ...) noexcept
    {
        va_list args;
        va_start(args, instance);
        jdouble result = instance
                       ? clazz.env->CallDoubleMethodV(instance, method, args)
                       : clazz.env->CallStaticDoubleMethodV(clazz.clazz, method, args);
        va_end(args);
        return result;
    }

    JArray Method::Array(JniType type, jobject instance, ...) noexcept
    {
        va_list args;
        va_start(args, instance);
        Ref<jarray> arr { instance
                        ? (jarray)clazz.env->CallObjectMethodV(instance, method, args)
                        : (jarray)clazz.env->CallStaticObjectMethodV(clazz.clazz, method, args)};
        va_end(args);
        return { std::move(arr), type };
    }

    //////////////////////////////////////////////////////////////////////////////////////

    Field::Field(Class& clazz, jfieldID field, const char* name, const char* type) noexcept
        : clazz(clazz), field(field), name(name), type(type)
    {
    }

    Ref<jobject> Field::Object(jobject instance) noexcept
    {
        jobject obj = instance
                    ? clazz.env->GetObjectField(instance, field)
                    : clazz.env->GetStaticObjectField(clazz.clazz, field);
        return Ref<jobject>{obj};
    }

    JString Field::String(jobject instance) noexcept
    {
        jstring str = instance
                    ? (jstring)clazz.env->GetObjectField(instance, field)
                    : (jstring)clazz.env->GetStaticObjectField(clazz.clazz, field);
        return JString{str};
    }

    jboolean Field::Boolean(jobject instance) noexcept
    {
        return instance
            ? clazz.env->GetBooleanField(instance, field)
            : clazz.env->GetStaticBooleanField(clazz.clazz, field);
    }

    jbyte Field::Byte(jobject instance) noexcept
    {
        return instance
            ? clazz.env->GetByteField(instance, field)
            : clazz.env->GetStaticByteField(clazz.clazz, field);
    }

    jchar Field::Char(jobject instance) noexcept
    {
        return instance
            ? clazz.env->GetCharField(instance, field)
            : clazz.env->GetStaticCharField(clazz.clazz, field);
    }

    jshort Field::Short(jobject instance) noexcept
    {
        return instance
            ? clazz.env->GetShortField(instance, field)
            : clazz.env->GetStaticShortField(clazz.clazz, field);
    }

    jint Field::Int(jobject instance) noexcept
    {
        return instance
            ? clazz.env->GetIntField(instance, field)
            : clazz.env->GetStaticIntField(clazz.clazz, field);
    }

    jlong Field::Long(jobject instance) noexcept
    {
        return instance
            ? clazz.env->GetLongField(instance, field)
            : clazz.env->GetStaticLongField(clazz.clazz, field);
    }

    jfloat Field::Float(jobject instance) noexcept
    {
        return instance
            ? clazz.env->GetFloatField(instance, field)
            : clazz.env->GetStaticFloatField(clazz.clazz, field);
    }

    jdouble Field::Double(jobject instance) noexcept
    {
        return instance
            ? clazz.env->GetDoubleField(instance, field)
            : clazz.env->GetStaticDoubleField(clazz.clazz, field);
    }

    //////////////////////////////////////////////////////////////////////////////////////

}} // namespace rpp::jni

#endif // !__ANDROID__
