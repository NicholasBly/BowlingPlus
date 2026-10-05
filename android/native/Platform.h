// Android stand-ins for the bits of Foundation the iOS code uses: strings (std::string), printf-style
// formatting that accepts std::string for "%s", a monotonic clock, and a small JSON value for the oil
// patterns that NSArray / NSDictionary carried on iOS.
#pragma once
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

typedef std::string Str;

// ---- printf with std::string arguments ----
namespace bfdetail {
template <typename T> inline T Arg(T v) { return v; }
inline const char *Arg(const std::string &s) { return s.c_str(); }
inline const char *Arg(std::string &s) { return s.c_str(); }
Str VFmt(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
}
template <typename... A> inline Str Fmt(const char *fmt, const A &...a) { return bfdetail::VFmt(fmt, bfdetail::Arg(a)...); }
inline Str Fmt(const char *fmt) { return Str(fmt ? fmt : ""); }

// ---- time ----
inline double BFNow() {   // seconds, monotonic (CFAbsoluteTimeGetCurrent() on iOS)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// ---- string helpers (NSString methods the iOS code used) ----
bool StrHas(const Str &s, const Str &sub);
bool StrHasPrefix(const Str &s, const Str &p);
bool StrHasSuffix(const Str &s, const Str &p);
Str StrLower(const Str &s);
Str StrTrim(const Str &s, const char *chars = " \t\r\n");
std::vector<Str> StrSplit(const Str &s, const Str &sep);
std::vector<Str> StrSplitWS(const Str &s);                       // on any whitespace, empty parts dropped
Str StrJoin(const std::vector<Str> &parts, const Str &sep);
Str StrLastPath(const Str &p);                                   // lastPathComponent
Str StrDelExt(const Str &p);                                     // stringByDeletingPathExtension
Str StrStripTags(const Str &s);                                  // "<size=50>Name</size>" -> "Name"
int StrInt(const Str &s);                                        // NSString.intValue: leading number, else 0
Str Utf16ToUtf8(const uint16_t *chars, int n);
bool ReadFile(const Str &path, std::vector<uint8_t> &out);
bool WriteFile(const Str &path, const void *data, size_t n);
bool FileMTime(const Str &path, double &mtime);
void MakeDirs(const Str &dir);

// ---- JSON value (the oil patterns, engine results, and the Java <-> native messages) ----
struct Json {
    enum Type { Null, Bool, Num, String, Array, Object, Floats };
    Type t = Null;
    bool b = false;
    double n = 0;
    Str s;
    std::vector<Json> a;
    std::vector<std::pair<Str, Json>> o;
    std::shared_ptr<std::vector<float>> fl;   // a float grid (NSData of floats on iOS); "base64 floats" in text

    Json() {}
    static Json Num_(double v) { Json j; j.t = Num; j.n = v; return j; }
    static Json Str_(const Str &v) { Json j; j.t = String; j.s = v; return j; }
    static Json Bool_(bool v) { Json j; j.t = Bool; j.b = v; return j; }
    static Json Arr() { Json j; j.t = Array; return j; }
    static Json Obj() { Json j; j.t = Object; return j; }
    static Json FloatsOf(std::vector<float> &&v) { Json j; j.t = Floats; j.fl = std::make_shared<std::vector<float>>(std::move(v)); return j; }

    bool isNull() const { return t == Null; }
    bool isArr() const { return t == Array; }
    bool isObj() const { return t == Object; }
    size_t size() const { return t == Array ? a.size() : t == Object ? o.size() : 0; }
    double num() const { return t == Num ? n : t == Bool ? (b ? 1 : 0) : t == String ? atof(s.c_str()) : 0; }
    int i() const { return (int)num(); }
    float f() const { return (float)num(); }
    bool truthy() const { return t == Bool ? b : t == Num ? n != 0 : t == String ? StrInt(s) != 0 || s == "true" : false; }
    Str str() const { return t == String ? s : t == Num ? Fmt("%g", n) : Str(); }
    bool has(const char *k) const { return find(k) != nullptr; }
    const Json *find(const char *k) const {
        if (t != Object) return nullptr;
        for (auto &kv : o) if (kv.first == k) return &kv.second;
        return nullptr;
    }
    const Json &operator[](const char *k) const { const Json *j = find(k); return j ? *j : Nil(); }
    const Json &operator[](size_t idx) const { return (t == Array && idx < a.size()) ? a[idx] : Nil(); }
    const Json &operator[](int idx) const { return idx >= 0 ? (*this)[(size_t)idx] : Nil(); }
    Json &set(const char *k, const Json &v) {
        if (t != Object) { t = Object; o.clear(); }
        for (auto &kv : o) if (kv.first == k) { kv.second = v; return kv.second; }
        o.emplace_back(k, v);
        return o.back().second;
    }
    Json &set(const char *k, double v) { return set(k, Num_(v)); }
    Json &set(const char *k, const Str &v) { return set(k, Str_(v)); }
    Json &set(const char *k, const char *v) { return set(k, Str_(v ? v : "")); }
    void remove(const char *k) { for (size_t q = 0; q < o.size(); q++) if (o[q].first == k) { o.erase(o.begin() + q); return; } }
    void push(const Json &v) { if (t != Array) { t = Array; a.clear(); } a.push_back(v); }
    const std::vector<Json> &items() const { static const std::vector<Json> none; return t == Array ? a : none; }

    static const Json &Nil() { static const Json n; return n; }
    static Json Parse(const Str &text, bool *ok = nullptr);
    Str Dump() const;
};

// [a, b, c, ...] of numbers
Json JNums(std::initializer_list<double> v);
