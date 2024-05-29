/**
 * Android JNI Utils, Copyright (c) 2024 Jorma Rebane
 * Distributed under MIT Software License
 */
#if __ANDROID__
#include "jni_cpp.h"
#include <android/log.h>

namespace rpp { namespace jni
{
    //////////////////////////////////////////////////////////////////////////////////////

    #define JniWarn(fmt, ...)  __android_log_print(ANDROID_LOG_WARN, "ReCpp", fmt, ##__VA_ARGS__);
    #define JniError(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, "ReCpp", fmt, ##__VA_ARGS__);

    // uses printf style formatting to build an exception message
    #define ThrowFmt(format, ...) do { \
        char __err_fmt[1024]; \
        snprintf(__err_fmt, sizeof(__err_fmt), format, ##__VA_ARGS__); \
        throw std::runtime_error(__err_fmt); \
    } while(0)

    //////////////////////////////////////////////////////////////////////////////////////

    static JavaVM* javaVM;
    static thread_local JNIEnv* jniEnv; // env ptr is only valid per-thread
    static Ref<jobject> mainActivity; // GlobalRef

    jint initVM(JavaVM* vm, JNIEnv* env) noexcept
    {
        javaVM = vm;
        jniEnv = env ? env : getEnv();
        return JNI_VERSION_1_6;
    }

    static void attachEnv(JNIEnv** pEnv) noexcept
    {
        if (!javaVM)
        {
            JniError("getEnv() used before JNI_OnLoad(). Avoid calling JNI methods in static initializers.");
            return;
        }

        // check if there is an attached Env? If not, try to attach it
        if (javaVM->GetEnv((void**)pEnv, JNI_VERSION_1_6) != JNI_OK)
        {
            jint status = javaVM->AttachCurrentThread(pEnv, nullptr);
            if (status != JNI_OK || *pEnv == nullptr)
            {
                JniError("getEnv() AttachCurrentThread failed: status %d", status);
            }
        }

        if (*pEnv == nullptr)
        {
            JniError("getEnv() failed to get a valid JNIEnv");
        }
    }

    JNIEnv* getEnv() noexcept
    {
        if (!jniEnv) attachEnv(&jniEnv);
        return jniEnv;
    }

    jobject getMainActivity(const char* mainActivityClass)
    {
        if (mainActivity)
            return mainActivity.get();

        try
        {
            std::string activityName = mainActivityClass ? mainActivityClass : "";
            if (activityName.empty())
                ThrowFmt("mainActivityClassName is empty");

            auto mainClass = Class{activityName.c_str()};

            // https://github.com/qt/qtbase/blob/5.15/src/android/jar/src/org/qtproject/qt5/android/QtNative.java#L142
            //    prototype:  public static Activity activity() { .. }
            if (auto activity = mainClass.staticMethod("activity", "()Landroid/app/Activity;"))
            {
                mainActivity = activity.globalObjectF(nullptr);
                if (!mainActivity) ThrowFmt("Class %s STATIC METHOD `activity()` returned null", activityName.c_str());
            }
            else if (auto field = mainClass.staticField("currentActivity", "Landroid/app/Activity;"))
            {
                mainActivity = field.getGlobalObject();
                if (!mainActivity) ThrowFmt("Class %s STATIC FIELD `currentActivity` returned null", activityName.c_str());
            }
            else if (auto field = mainClass.staticField("activity", "Landroid/app/Activity;"))
            {
                mainActivity = field.getGlobalObject();
                if (!mainActivity) ThrowFmt("Class %s STATIC FIELD `activity` returned null", activityName.c_str());
            }
            else
            {
                ThrowFmt("no recognized main activity field or method in %s", activityName.c_str());
            }
            return mainActivity.get();
        }
        catch (const std::exception& e)
        {
            JniError("getMainActivity failed: %s", e.what());
        }
        return nullptr;
    }

    void initMainActivity(jobject mainActivityRef) noexcept
    {
        if (!mainActivity)
        {
            mainActivity = makeGlobalRef(mainActivityRef);
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
            ThrowFmt(format, ##__VA_ARGS__); \
        } while (0)

    //////////////////////////////////////////////////////////////////////////////////////

    JniRef::JniRef(jobject localOrGlobalRef) noexcept
        : JniRef{} // default init
    {
        if (!localOrGlobalRef)
            return;
        jobjectRefType type = getEnv()->GetObjectRefType(localOrGlobalRef);
        if (type == JNILocalRefType) {
            obj = localOrGlobalRef;
        } else if (type == JNIGlobalRefType) {
            obj = localOrGlobalRef;
            isGlobal = true;
        } else {
            JniError("jni::Ref<T>() invalid reference: %p", localOrGlobalRef);
        }
    }

    void JniRef::reset() noexcept
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

    void JniRef::reset(const JniRef& r) noexcept
    {
        if (obj)
        {
            reset();
        }

        if (r.obj)
        {
            auto* env = getEnv();
            if (r.isGlobal) obj = env->NewGlobalRef(r.obj);
            else            obj = env->NewLocalRef(r.obj);
        }
        isGlobal = r.isGlobal;
    }

    void JniRef::makeGlobal() noexcept
    {
        if (obj && !isGlobal)
        {
            auto* env = getEnv();
            jobject globalRef = env->NewGlobalRef(obj);
            // decrease the local refcount because it was !global
            env->DeleteLocalRef(obj);

            obj = globalRef;
            isGlobal = true;
        }
    }

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
        jobjectArray arr = env->NewObjectArray(strings.size(), stringClass.get(), 0);
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
        auto* env = getEnv();
        clazz = makeGlobalRef<jclass>(env->FindClass(className));
        if (!clazz) JniThrow("Class not found: '%s'", className);
        name = className;
    }

    Class::Class(const char* className, std::nothrow_t) noexcept
    {
        auto* env = getEnv();
        clazz = makeGlobalRef<jclass>(env->FindClass(className));
        if (!clazz) ClearException();
        name = className;
    }

    #define SignatureError(fmt, ...) do { \
        JniError(fmt, ##__VA_ARGS__); \
        if (throwOnError) { ThrowFmt(fmt, ##__VA_ARGS__); } \
    } while(0)

    static void checkMethodSignature(Class& c, const char* methodName, const char* signature, bool throwOnError)
    {
        if (!signature || !signature[0] || signature[0] != '(')
            SignatureError("jni::Method %s for Class %s has invalid signature: %s", methodName, c.name, signature);
    }

    static void checkFieldSignature(Class& c, const char* fieldName, const char* signature, bool throwOnError)
    {
        if (!signature || !signature[0] || signature[0] == '(')
            SignatureError("jni::Field %s for Class %s has invalid signature: %s", fieldName, c.name, signature);
    }

    Method Class::method(const char* methodName, const char* signature)
    {
        checkMethodSignature(*this, methodName, signature, /*throwOnError*/true);
        auto* env = getEnv();
        jmethodID method = env->GetMethodID(clazz.get(), methodName, signature);
        if (!method) JniThrow("Method '%s' not found in '%s'", methodName, name);
        return {*this, method, methodName, signature, /*isStatic*/false};
    }
    Method Class::method(const char* methodName, const char* signature, std::nothrow_t) noexcept
    {
        checkMethodSignature(*this, methodName, signature, /*throwOnError*/false);
        auto* env = getEnv();
        jmethodID method = env->GetMethodID(clazz.get(), methodName, signature);
        if (!method) ClearException();
        return {*this, method, methodName, signature, /*isStatic*/false};
    }

    Method Class::staticMethod(const char* methodName, const char* signature)
    {
        checkMethodSignature(*this, methodName, signature, /*throwOnError*/true);
        auto* env = getEnv();
        jmethodID method = env->GetStaticMethodID(clazz.get(), methodName, signature);
        if (!method) JniThrow("Static method '%s' not found in '%s'", methodName, name);
        return {*this, method, methodName, signature, /*isStatic*/true};
    }

    Method Class::staticMethod(const char* methodName, const char* signature, std::nothrow_t) noexcept
    {
        checkMethodSignature(*this, methodName, signature, /*throwOnError*/false);
        auto* env = getEnv();
        jmethodID method = env->GetStaticMethodID(clazz.get(), methodName, signature);
        if (!method) ClearException();
        return {*this, method, methodName, signature, /*isStatic*/true};
    }

    Field Class::field(const char* fieldName, const char* type)
    {
        checkFieldSignature(*this, fieldName, type, /*throwOnError*/true);
        auto* env = getEnv();
        jfieldID field = env->GetFieldID(clazz.get(), fieldName, type);
        if (!field) JniThrow("Field '%s' of type '%s' not found in '%s'", fieldName, type, name);
        return {*this, field, fieldName, type, /*isStatic*/false};
    }
    Field Class::field(const char* fieldName, const char* type, std::nothrow_t) noexcept
    {
        checkFieldSignature(*this, fieldName, type, /*throwOnError*/false);
        auto* env = getEnv();
        jfieldID field = env->GetFieldID(clazz.get(), fieldName, type);
        if (!field) ClearException();
        return {*this, field, fieldName, type, /*isStatic*/false};
    }

    Field Class::staticField(const char* fieldName, const char* type)
    {
        checkFieldSignature(*this, fieldName, type, /*throwOnError*/true);
        auto* env = getEnv();
        jfieldID field = env->GetStaticFieldID(clazz.get(), fieldName, type);
        if (!field) JniThrow("Static Field '%s' of type '%s' not found in '%s'", fieldName, type, name);
        return {*this, field, fieldName, type, /*isStatic*/true};
    }
    Field Class::staticField(const char* fieldName, const char* type, std::nothrow_t) noexcept
    {
        checkFieldSignature(*this, fieldName, type, /*throwOnError*/false);
        auto* env = getEnv();
        jfieldID field = env->GetStaticFieldID(clazz.get(), fieldName, type);
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
            /*return retval;*/ \
        } else if (isStatic && instance) { \
            __android_log_print(ANDROID_LOG_WARN, "ReCpp", "Static jni::Method %s called with instance=%p", name, instance); \
        } \
    } while(0)

    Ref<jobject> Method::objectV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args; va_start(args, instance);
        auto* env = getEnv();
        jobject o = isStatic
                  ? env->CallStaticObjectMethodV(clazz.get(), method, args)
                  : env->CallObjectMethodV(instance, method, args);
        va_end(args);
        return Ref<jobject>{o};
    }

    JString Method::stringV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args; va_start(args, instance);
        auto* env = getEnv();
        jstring s = isStatic
                  ? (jstring)env->CallStaticObjectMethodV(clazz.get(), method, args)
                  : (jstring)env->CallObjectMethodV(instance, method, args);
        va_end(args);
        return JString{ s };
    }

    JArray Method::arrayV(JniType type, jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args; va_start(args, instance);
        auto* env = getEnv();
        jarray a = isStatic
               ? (jarray)env->CallStaticObjectMethodV(clazz.get(), method, args)
               : (jarray)env->CallObjectMethodV(instance, method, args);
        va_end(args);
        return { a, type };
    }

    void Method::voidV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, );
        va_list args; va_start(args, instance);
        auto* env = getEnv();
        if (isStatic) env->CallStaticVoidMethodV(clazz.get(), method, args);
        else          env->CallVoidMethodV(instance, method, args);
        va_end(args);
    }

    jboolean Method::booleanV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args; va_start(args, instance);
        auto* env = getEnv();
        jboolean v = isStatic
                   ? env->CallStaticBooleanMethodV(clazz.get(), method, args)
                   : env->CallBooleanMethodV(instance, method, args);
        va_end(args);
        return v;
    }

    jbyte Method::byteV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args; va_start(args, instance);
        auto* env = getEnv();
        jbyte v = isStatic
                ? env->CallStaticByteMethodV(clazz.get(), method, args)
                : env->CallByteMethodV(instance, method, args);
        va_end(args);
        return v;
    }

    jchar Method::charV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args; va_start(args, instance);
        auto* env = getEnv();
        jchar v = isStatic
                ? env->CallStaticCharMethodV(clazz.get(), method, args)
                : env->CallCharMethodV(instance, method, args);
        va_end(args);
        return v;
    }

    jshort Method::shortV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args; va_start(args, instance);
        auto* env = getEnv();
        jshort v = isStatic
                 ? env->CallStaticShortMethodV(clazz.get(), method, args)
                 : env->CallShortMethodV(instance, method, args);
        va_end(args);
        return v;
    }

    jint Method::intV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args; va_start(args, instance);
        auto* env = getEnv();
        jint v = isStatic
               ? env->CallStaticIntMethodV(clazz.get(), method, args)
               : env->CallIntMethodV(instance, method, args);
        va_end(args);
        return v;
    }

    jlong Method::longV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args; va_start(args, instance);
        auto* env = getEnv();
        jlong v = isStatic
                ? env->CallStaticLongMethodV(clazz.get(), method, args)
                : env->CallLongMethodV(instance, method, args);
        va_end(args);
        return v;
    }

    jfloat Method::floatV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args; va_start(args, instance);
        auto* env = getEnv();
        jfloat v = isStatic
                 ? env->CallStaticFloatMethodV(clazz.get(), method, args)
                 : env->CallFloatMethodV(instance, method, args);
        va_end(args);
        return v;
    }

    jdouble Method::doubleV(jobject instance, ...) noexcept
    {
        CheckMethodInstance(instance, {});
        va_list args; va_start(args, instance);
        auto* env = getEnv();
        jdouble v = isStatic
                  ? env->CallStaticDoubleMethodV(clazz.get(), method, args)
                  : env->CallDoubleMethodV(instance, method, args);
        va_end(args);
        return v;
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
        auto* env = getEnv();
        jobject obj = isStatic
                    ? env->GetStaticObjectField(clazz.get(), field)
                    : env->GetObjectField(instance, field);
        return Ref<jobject>{obj};
    }

    Ref<jobject> Field::getGlobalObject(jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        auto* env = getEnv();
        jobject obj = isStatic
                    ? env->GetStaticObjectField(clazz.get(), field)
                    : env->GetObjectField(instance, field);
        return makeGlobalRef(obj);
    }

    JString Field::getString(jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        auto* env = getEnv();
        jstring str = isStatic
                    ? (jstring)env->GetStaticObjectField(clazz.get(), field)
                    : (jstring)env->GetObjectField(instance, field);
        return JString{str};
    }

    JArray Field::getArray(JniType type, jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        auto* env = getEnv();
        jarray arr = isStatic
                   ? (jarray)env->GetStaticObjectField(clazz.get(), field)
                   : (jarray)env->GetObjectField(instance, field);
        return JArray{arr, type};
    }

    jboolean Field::getBoolean(jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        auto* env = getEnv();
        return isStatic
            ? env->GetStaticBooleanField(clazz.get(), field)
            : env->GetBooleanField(instance, field);
    }

    jbyte Field::getByte(jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        auto* env = getEnv();
        return isStatic
            ? env->GetStaticByteField(clazz.get(), field)
            : env->GetByteField(instance, field);
    }

    jchar Field::getChar(jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        auto* env = getEnv();
        return isStatic
            ? env->GetStaticCharField(clazz.get(), field)
            : env->GetCharField(instance, field);
    }

    jshort Field::getShort(jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        auto* env = getEnv();
        return isStatic
            ? env->GetStaticShortField(clazz.get(), field)
            : env->GetShortField(instance, field);
    }

    jint Field::getInt(jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        auto* env = getEnv();
        return isStatic
            ? env->GetStaticIntField(clazz.get(), field)
            : env->GetIntField(instance, field);
    }

    jlong Field::getLong(jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        auto* env = getEnv();
        return isStatic
            ? env->GetStaticLongField(clazz.get(), field)
            : env->GetLongField(instance, field);
    }

    jfloat Field::getFloat(jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        auto* env = getEnv();
        return isStatic
            ? env->GetStaticFloatField(clazz.get(), field)
            : env->GetFloatField(instance, field);
    }

    jdouble Field::getDouble(jobject instance) noexcept
    {
        CheckFieldInstance(instance, {});
        auto* env = getEnv();
        return isStatic
            ? env->GetStaticDoubleField(clazz.get(), field)
            : env->GetDoubleField(instance, field);
    }

    //////////////////////////////////////////////////////////////////////////////////////

}} // namespace rpp::jni

#endif // !__ANDROID__
