#pragma once
#include <stdint.h>
#include <stdarg.h>
typedef uint8_t jboolean; typedef int8_t jbyte; typedef uint16_t jchar; typedef int16_t jshort; typedef int32_t jint; typedef int64_t jlong; typedef float jfloat; typedef double jdouble; typedef jint jsize;
class _jobject {}; class _jclass : public _jobject {}; class _jstring : public _jobject {}; class _jarray : public _jobject {}; class _jbyteArray : public _jarray {}; class _jthrowable : public _jobject {};
typedef _jobject* jobject; typedef _jclass* jclass; typedef _jstring* jstring; typedef _jarray* jarray; typedef _jbyteArray* jbyteArray; typedef _jthrowable* jthrowable;
struct _jmethodID; typedef _jmethodID* jmethodID; struct _jfieldID; typedef _jfieldID* jfieldID;
typedef struct { const char* name; const char* signature; void* fnPtr; } JNINativeMethod;
#define JNI_OK 0
#define JNI_ERR (-1)
#define JNI_FALSE 0
#define JNI_TRUE 1
#define JNI_ABORT 2
#define JNI_COMMIT 1
#define JNI_VERSION_1_6 0x00010006
#define JNIEXPORT __attribute__((visibility("default")))
#define JNICALL
struct _JNIEnv {
  jclass FindClass(const char*);
  jboolean ExceptionCheck(); void ExceptionClear(); void ExceptionDescribe(); jthrowable ExceptionOccurred();
  void DeleteLocalRef(jobject); jobject NewGlobalRef(jobject); void DeleteGlobalRef(jobject);
  jmethodID GetStaticMethodID(jclass, const char*, const char*); jmethodID GetMethodID(jclass, const char*, const char*);
  void CallStaticVoidMethod(jclass, jmethodID, ...); jobject CallStaticObjectMethod(jclass, jmethodID, ...);
  jboolean CallStaticBooleanMethod(jclass, jmethodID, ...); jint CallStaticIntMethod(jclass, jmethodID, ...);
  void CallVoidMethod(jobject, jmethodID, ...); jobject CallObjectMethod(jobject, jmethodID, ...);
  jobject NewObject(jclass, jmethodID, ...);
  jstring NewStringUTF(const char*); const char* GetStringUTFChars(jstring, jboolean*); void ReleaseStringUTFChars(jstring, const char*); jsize GetStringUTFLength(jstring);
  jsize GetArrayLength(jarray); jbyteArray NewByteArray(jsize);
  void SetByteArrayRegion(jbyteArray, jsize, jsize, const jbyte*); void GetByteArrayRegion(jbyteArray, jsize, jsize, jbyte*);
  jbyte* GetByteArrayElements(jbyteArray, jboolean*); void ReleaseByteArrayElements(jbyteArray, jbyte*, jint);
  jint RegisterNatives(jclass, const JNINativeMethod*, jint);
};
typedef _JNIEnv JNIEnv;
struct JavaVMAttachArgs { jint version; const char* name; jobject group; };
struct _JavaVM { jint GetEnv(void**, jint); jint AttachCurrentThread(JNIEnv**, void*); jint DetachCurrentThread(); };
typedef _JavaVM JavaVM;
