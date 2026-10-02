#import "Il2Cpp.h"
#import "BFShared.h"
#include <dlfcn.h>
#include <string.h>
#include <string>
#include <unordered_map>

typedef void Il2CppDomain;
typedef void Il2CppAssembly;
typedef void Il2CppImage;
typedef void Il2CppType;
typedef void Il2CppException;

#define IL2CPP_API(ret, name, args) static ret (*name##_) args = nullptr;
IL2CPP_API(const Il2CppImage *, il2cpp_get_corlib, (void))
IL2CPP_API(Il2CppDomain *, il2cpp_domain_get, (void))
IL2CPP_API(const Il2CppAssembly **, il2cpp_domain_get_assemblies, (const Il2CppDomain *, size_t *))
IL2CPP_API(const Il2CppImage *, il2cpp_assembly_get_image, (const Il2CppAssembly *))
IL2CPP_API(const char *, il2cpp_image_get_name, (const Il2CppImage *))
IL2CPP_API(Il2CppClass *, il2cpp_class_from_name, (const Il2CppImage *, const char *, const char *))
IL2CPP_API(const MethodInfo *, il2cpp_class_get_methods, (Il2CppClass *, void **))
IL2CPP_API(const char *, il2cpp_method_get_name, (const MethodInfo *))
IL2CPP_API(uint32_t, il2cpp_method_get_param_count, (const MethodInfo *))
IL2CPP_API(const Il2CppType *, il2cpp_method_get_param, (const MethodInfo *, uint32_t))
IL2CPP_API(char *, il2cpp_type_get_name, (const Il2CppType *))
IL2CPP_API(void, il2cpp_free, (void *))
IL2CPP_API(Il2CppClass *, il2cpp_class_get_parent, (Il2CppClass *))
IL2CPP_API(FieldInfo *, il2cpp_class_get_field_from_name, (Il2CppClass *, const char *))
IL2CPP_API(size_t, il2cpp_field_get_offset, (FieldInfo *))
IL2CPP_API(void, il2cpp_field_static_get_value, (FieldInfo *, void *))
IL2CPP_API(void, il2cpp_field_static_set_value, (FieldInfo *, void *))
IL2CPP_API(const char *, il2cpp_class_get_name, (Il2CppClass *))
IL2CPP_API(void, il2cpp_runtime_class_init, (Il2CppClass *))
IL2CPP_API(const Il2CppType *, il2cpp_class_get_type, (Il2CppClass *))
IL2CPP_API(Il2CppObject *, il2cpp_type_get_object, (const Il2CppType *))
IL2CPP_API(Il2CppClass *, il2cpp_object_get_class, (Il2CppObject *))
IL2CPP_API(Il2CppObject *, il2cpp_runtime_invoke, (const MethodInfo *, void *, void **, Il2CppException **))
IL2CPP_API(void *, il2cpp_object_unbox, (Il2CppObject *))
IL2CPP_API(Il2CppString *, il2cpp_string_new, (const char *))
IL2CPP_API(Il2CppArray *, il2cpp_array_new, (Il2CppClass *, uintptr_t))
IL2CPP_API(Il2CppObject *, il2cpp_object_new, (const Il2CppClass *))
IL2CPP_API(uintptr_t, il2cpp_gchandle_new, (Il2CppObject *, bool))
IL2CPP_API(void, il2cpp_gchandle_free, (uintptr_t))
IL2CPP_API(Il2CppObject *, il2cpp_gchandle_get_target, (uintptr_t))

static bool sApiOK = false, sReady = false;
static CFAbsoluteTime sLastTry = 0;
static const Il2CppImage *sImages[256];
static int sImageCount = 0;

#define LOAD(name) name##_ = (decltype(name##_))dlsym(RTLD_DEFAULT, #name)

static bool LoadApi() {
    LOAD(il2cpp_get_corlib); LOAD(il2cpp_domain_get); LOAD(il2cpp_domain_get_assemblies);
    LOAD(il2cpp_assembly_get_image); LOAD(il2cpp_image_get_name); LOAD(il2cpp_class_from_name);
    LOAD(il2cpp_class_get_methods); LOAD(il2cpp_method_get_name); LOAD(il2cpp_method_get_param_count);
    LOAD(il2cpp_method_get_param); LOAD(il2cpp_type_get_name); LOAD(il2cpp_free);
    LOAD(il2cpp_class_get_parent); LOAD(il2cpp_class_get_field_from_name); LOAD(il2cpp_field_get_offset);
    LOAD(il2cpp_field_static_get_value); LOAD(il2cpp_field_static_set_value); LOAD(il2cpp_class_get_name); LOAD(il2cpp_runtime_class_init); LOAD(il2cpp_class_get_type);
    LOAD(il2cpp_type_get_object); LOAD(il2cpp_object_get_class); LOAD(il2cpp_runtime_invoke);
    LOAD(il2cpp_object_unbox); LOAD(il2cpp_string_new); LOAD(il2cpp_array_new); LOAD(il2cpp_object_new);
    LOAD(il2cpp_gchandle_new); LOAD(il2cpp_gchandle_free); LOAD(il2cpp_gchandle_get_target);
    return il2cpp_get_corlib_ && il2cpp_domain_get_ && il2cpp_domain_get_assemblies_ &&
           il2cpp_assembly_get_image_ && il2cpp_image_get_name_ && il2cpp_class_from_name_ &&
           il2cpp_class_get_methods_ && il2cpp_method_get_name_ && il2cpp_method_get_param_count_ &&
           il2cpp_class_get_field_from_name_ && il2cpp_field_get_offset_ && il2cpp_field_static_get_value_ &&
           il2cpp_class_get_type_ && il2cpp_type_get_object_ && il2cpp_object_get_class_ &&
           il2cpp_runtime_invoke_ && il2cpp_object_unbox_ && il2cpp_string_new_ && il2cpp_array_new_ &&
           il2cpp_gchandle_new_ && il2cpp_gchandle_free_ && il2cpp_gchandle_get_target_;
}

bool IL::Ready() {
    if (sReady) return true;
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    if (now - sLastTry < 1.0) return false;
    sLastTry = now;
    if (!sApiOK) {   // UnityFramework is loaded a moment after app launch
        sApiOK = LoadApi();
        if (!sApiOK) return false;
    }
    if (!il2cpp_get_corlib_()) return false;   // runtime not initialised yet
    Il2CppDomain *domain = il2cpp_domain_get_();
    if (!domain) return false;
    size_t n = 0;
    const Il2CppAssembly **list = il2cpp_domain_get_assemblies_(domain, &n);
    if (!list || !n) return false;
    bool haveGame = false;
    sImageCount = 0;
    for (size_t i = 0; i < n && sImageCount < 256; i++) {
        const Il2CppImage *img = il2cpp_assembly_get_image_(list[i]);
        if (!img) continue;
        sImages[sImageCount++] = img;
        const char *nm = il2cpp_image_get_name_(img);
        if (nm && strncmp(nm, "Assembly-CSharp", 15) == 0 && !strstr(nm, "firstpass")) haveGame = true;
    }
    if (!haveGame) return false;
    sReady = true;
    BFLog(@"IL2CPP ready (%d assemblies)", sImageCount);
    return true;
}

Il2CppClass *IL::FindClass(const char *ns, const char *name) {
    static std::unordered_map<std::string, Il2CppClass *> cache;
    std::string key = std::string(ns) + "|" + name;
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    Il2CppClass *k = nullptr;
    for (int i = 0; i < sImageCount && !k; i++) k = il2cpp_class_from_name_(sImages[i], ns, name);
    if (k) cache[key] = k;
    else BFLog(@"class not found: %s.%s", ns, name);
    return k;
}

static bool ParamIs(const MethodInfo *m, uint32_t idx, const char *want) {
    if (!want) return true;
    if (!il2cpp_method_get_param_ || !il2cpp_type_get_name_) return true;   // can't check, accept
    char *tn = il2cpp_type_get_name_(il2cpp_method_get_param_(m, idx));
    bool same = tn && strcmp(tn, want) == 0;
    if (tn && il2cpp_free_) il2cpp_free_(tn);
    return same;
}

const MethodInfo *IL::FindMethod(Il2CppClass *k, const char *name, int argc, const char *p0, const char *p1) {
    for (Il2CppClass *c = k; c; c = il2cpp_class_get_parent_ ? il2cpp_class_get_parent_(c) : nullptr) {
        void *iter = nullptr;
        const MethodInfo *m;
        while ((m = il2cpp_class_get_methods_(c, &iter))) {
            const char *mn = il2cpp_method_get_name_(m);
            if (!mn || strcmp(mn, name) != 0) continue;
            int pc = (int)il2cpp_method_get_param_count_(m);
            if (argc >= 0 && pc != argc) continue;
            if (pc > 0 && !ParamIs(m, 0, p0)) continue;
            if (pc > 1 && !ParamIs(m, 1, p1)) continue;
            return m;
        }
        if (!il2cpp_class_get_parent_) break;
    }
    return nullptr;
}

int IL::FieldOffset(Il2CppClass *k, const char *name) {
    for (Il2CppClass *c = k; c; c = il2cpp_class_get_parent_ ? il2cpp_class_get_parent_(c) : nullptr) {
        FieldInfo *f = il2cpp_class_get_field_from_name_(c, name);
        if (f) return (int)il2cpp_field_get_offset_(f);
        if (!il2cpp_class_get_parent_) break;
    }
    return -1;
}

FieldInfo *IL::StaticField(Il2CppClass *k, const char *name) {
    if (!k) return nullptr;
    if (il2cpp_runtime_class_init_) il2cpp_runtime_class_init_(k);
    return il2cpp_class_get_field_from_name_(k, name);
}

void IL::StaticRead(FieldInfo *f, void *out) {
    if (f) il2cpp_field_static_get_value_(f, out);
}

const char *IL::ClassName(Il2CppClass *k) {
    const char *n = (k && il2cpp_class_get_name_) ? il2cpp_class_get_name_(k) : nullptr;
    return n ? n : "?";
}

void IL::StaticWrite(FieldInfo *f, void *value) {   // value types only (no GC write barrier needed)
    if (f && il2cpp_field_static_set_value_) il2cpp_field_static_set_value_(f, value);
}

Il2CppObject *IL::TypeOf(Il2CppClass *k) {
    return k ? il2cpp_type_get_object_(il2cpp_class_get_type_(k)) : nullptr;
}

Il2CppClass *IL::ClassOf(void *obj) {
    return obj ? il2cpp_object_get_class_((Il2CppObject *)obj) : nullptr;
}

Il2CppObject *IL::Invoke(const MethodInfo *m, void *obj, void **args, bool *ok) {
    if (ok) *ok = false;
    if (!m || !il2cpp_runtime_invoke_) return nullptr;
    Il2CppException *exc = nullptr;
    Il2CppObject *ret = nullptr;
    try {
        ret = il2cpp_runtime_invoke_(m, obj, args, &exc);
    } catch (...) {
        BFLog(@"native exception while calling %s", il2cpp_method_get_name_(m));
        return nullptr;
    }
    if (exc) {
        static int logged = 0;
        if (logged++ < 40) BFLog(@"C# exception inside %s (ignored)", il2cpp_method_get_name_(m));
        return nullptr;
    }
    if (ok) *ok = true;
    return ret;
}

bool IL::InvokeBool(const MethodInfo *m, void *obj, void **args, bool fallback) {
    bool ok = false;
    Il2CppObject *r = Invoke(m, obj, args, &ok);
    return (ok && r) ? *(bool *)il2cpp_object_unbox_(r) : fallback;
}

int IL::InvokeInt(const MethodInfo *m, void *obj, void **args, int fallback) {
    bool ok = false;
    Il2CppObject *r = Invoke(m, obj, args, &ok);
    return (ok && r) ? *(int32_t *)il2cpp_object_unbox_(r) : fallback;
}

void *IL::Unbox(Il2CppObject *boxed) { return boxed ? il2cpp_object_unbox_(boxed) : nullptr; }
Il2CppString *IL::NewString(const char *utf8) { return il2cpp_string_new_(utf8 ? utf8 : ""); }

NSString *IL::Str(void *s) {
    if (!s) return nil;
    Il2CppString *str = (Il2CppString *)s;
    if (str->length <= 0) return @"";
    if (str->length > 4096) return nil;
    return [NSString stringWithCharacters:(const unichar *)str->chars length:(NSUInteger)str->length];
}

Il2CppArray *IL::NewArray(Il2CppClass *elementClass, size_t length) {
    return elementClass ? il2cpp_array_new_(elementClass, (uintptr_t)length) : nullptr;
}

Il2CppObject *IL::NewObject(Il2CppClass *k) {
    return (k && il2cpp_object_new_) ? il2cpp_object_new_(k) : nullptr;
}

IL::GCHandle IL::Keep(void *obj) { return obj ? il2cpp_gchandle_new_((Il2CppObject *)obj, false) : 0; }
void IL::Release(GCHandle h) { if (h) il2cpp_gchandle_free_(h); }
void *IL::Target(GCHandle h) { return h ? il2cpp_gchandle_get_target_(h) : nullptr; }

bool IL::Alive(void *o) {
    // UnityEngine.Object.m_CachedPtr (offset 0x10) is cleared when the object is destroyed
    return o && *(void **)((char *)o + 0x10) != nullptr;
}
