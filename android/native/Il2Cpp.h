// Port of src/Il2Cpp.h for Android (same API; Str -> Text, which returns UTF-8).
#pragma once
#include "Platform.h"
#include <stdint.h>
#include <stddef.h>

// Minimal IL2CPP object layouts (64-bit)
struct Il2CppObject { void *klass; void *monitor; };
struct Il2CppArray  { Il2CppObject obj; void *bounds; uintptr_t max_length; };  // elements start at +0x20
struct Il2CppString { Il2CppObject obj; int32_t length; uint16_t chars[1]; };
struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };

typedef void Il2CppClass;
typedef void MethodInfo;
typedef void FieldInfo;

// Everything is looked up BY NAME while the game runs (no hard-coded addresses),
// so small game updates usually don't break the tweak.
namespace IL {
    bool Ready();   // IL2CPP is up and Assembly-CSharp is loaded (checks at most once a second)
    void *Api(const char *name);   // any il2cpp_* export (nullptr until libil2cpp.so is loaded)

    Il2CppClass *FindClass(const char *ns, const char *name);
    // First method called `name` with `argc` parameters (also searches parent classes).
    // p0 / p1 optionally pin the parameter types, e.g. "System.Type", "System.Byte[]".
    const MethodInfo *FindMethod(Il2CppClass *k, const char *name, int argc,
                                 const char *p0 = nullptr, const char *p1 = nullptr);
    int FieldOffset(Il2CppClass *k, const char *name);          // -1 if missing
    bool FieldTypeName(Il2CppClass *k, const char *name, char *out, size_t size);   // e.g. "System.Int32"
    FieldInfo *StaticField(Il2CppClass *k, const char *name);   // runs the class constructor first
    void StaticRead(FieldInfo *f, void *out);
    void StaticWrite(FieldInfo *f, void *value);
    const char *ClassName(Il2CppClass *k);

    Il2CppObject *TypeOf(Il2CppClass *k);                        // System.Type object
    Il2CppClass  *ClassOf(void *obj);

    // Calls C# through il2cpp_runtime_invoke, which catches C# exceptions for us.
    Il2CppObject *Invoke(const MethodInfo *m, void *obj, void **args, bool *ok = nullptr);
    bool InvokeBool(const MethodInfo *m, void *obj, void **args, bool fallback);
    int  InvokeInt (const MethodInfo *m, void *obj, void **args, int fallback);
    void *Unbox(Il2CppObject *boxed);

    Il2CppString *NewString(const char *utf8);
    Str           Text(void *il2cppString);   // UTF-8 ("" for null)
    Il2CppArray  *NewArray(Il2CppClass *elementClass, size_t length);
    Il2CppObject *NewObject(Il2CppClass *k);

    // GC handles are POINTER-sized in this game's Unity (6000.x). v1.0.0 kept them in
    // 32 bits, which chopped the address in half and froze the game on the loading screen.
    typedef uintptr_t GCHandle;
    GCHandle Keep(void *obj);        // strong GC handle (object can't be garbage collected)
    void     Release(GCHandle handle);
    void    *Target(GCHandle handle);

    bool Alive(void *unityObject);   // UnityEngine.Object whose native side still exists

    void SetRef(void *obj, int offset, void *value);   // pointer store into a managed object (GC write barrier)
    template <typename T> inline T &At(void *obj, int offset) { return *(T *)((char *)obj + offset); }
    inline void  *Data(Il2CppArray *a) { return (char *)a + sizeof(Il2CppArray); }
    inline size_t Len(Il2CppArray *a) { return a ? (size_t)a->max_length : 0; }
}
