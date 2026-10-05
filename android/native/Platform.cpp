#include "Platform.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <sys/stat.h>

namespace bfdetail {
Str VFmt(const char *fmt, ...) {
    if (!fmt) return Str();
    char stackBuf[512];
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int n = vsnprintf(stackBuf, sizeof(stackBuf), fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return Str(); }
    if ((size_t)n < sizeof(stackBuf)) { va_end(ap2); return Str(stackBuf, (size_t)n); }
    Str out((size_t)n, '\0');
    vsnprintf(&out[0], (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return out;
}
}

bool StrHas(const Str &s, const Str &sub) { return s.find(sub) != Str::npos; }
bool StrHasPrefix(const Str &s, const Str &p) { return s.size() >= p.size() && s.compare(0, p.size(), p) == 0; }
bool StrHasSuffix(const Str &s, const Str &p) { return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0; }
Str StrLower(const Str &s) { Str o(s); for (auto &c : o) c = (char)tolower((unsigned char)c); return o; }
Str StrTrim(const Str &s, const char *chars) {
    size_t a = s.find_first_not_of(chars);
    if (a == Str::npos) return Str();
    size_t b = s.find_last_not_of(chars);
    return s.substr(a, b - a + 1);
}
std::vector<Str> StrSplit(const Str &s, const Str &sep) {
    std::vector<Str> out;
    if (sep.empty()) { out.push_back(s); return out; }
    size_t p = 0;
    for (;;) {
        size_t q = s.find(sep, p);
        if (q == Str::npos) { out.push_back(s.substr(p)); break; }
        out.push_back(s.substr(p, q - p));
        p = q + sep.size();
    }
    return out;
}
std::vector<Str> StrSplitWS(const Str &s) {
    std::vector<Str> out;
    Str cur;
    for (char c : s) {
        if (isspace((unsigned char)c)) { if (!cur.empty()) out.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}
Str StrJoin(const std::vector<Str> &parts, const Str &sep) {
    Str o;
    for (size_t i = 0; i < parts.size(); i++) { if (i) o += sep; o += parts[i]; }
    return o;
}
Str StrLastPath(const Str &p) { size_t k = p.find_last_of('/'); return k == Str::npos ? p : p.substr(k + 1); }
Str StrDelExt(const Str &p) {
    size_t slash = p.find_last_of('/'), dot = p.find_last_of('.');
    if (dot == Str::npos || (slash != Str::npos && dot < slash)) return p;
    return p.substr(0, dot);
}
Str StrStripTags(const Str &s) {
    Str o;
    bool in = false;
    for (char c : s) {
        if (c == '<') in = true;
        else if (c == '>' && in) in = false;
        else if (!in) o += c;
    }
    return StrTrim(o);
}
int StrInt(const Str &s) { return atoi(s.c_str()); }

Str Utf16ToUtf8(const uint16_t *c, int n) {
    Str o;
    o.reserve((size_t)n);
    for (int i = 0; i < n; i++) {
        uint32_t cp = c[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < n && c[i + 1] >= 0xDC00 && c[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (c[i + 1] - 0xDC00);
            i++;
        }
        if (cp < 0x80) o += (char)cp;
        else if (cp < 0x800) { o += (char)(0xC0 | (cp >> 6)); o += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { o += (char)(0xE0 | (cp >> 12)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
        else { o += (char)(0xF0 | (cp >> 18)); o += (char)(0x80 | ((cp >> 12) & 0x3F)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
    }
    return o;
}

bool ReadFile(const Str &path, std::vector<uint8_t> &out) {
    FILE *f = fopen(path.c_str(), "rb");
    if (!f) return false;
    out.clear();
    uint8_t buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.insert(out.end(), buf, buf + n);
    fclose(f);
    return true;
}
bool WriteFile(const Str &path, const void *data, size_t n) {
    Str tmp = path + ".tmp";
    FILE *f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, n, f) == n;
    ok = fclose(f) == 0 && ok;
    if (ok) ok = rename(tmp.c_str(), path.c_str()) == 0;
    if (!ok) remove(tmp.c_str());
    return ok;
}
bool FileMTime(const Str &path, double &mtime) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    mtime = st.st_mtim.tv_sec + st.st_mtim.tv_nsec * 1e-9 + st.st_size * 1e-12;   // size too, so a same-second rewrite counts
    return true;
}
void MakeDirs(const Str &dir) {
    Str cur;
    for (const Str &part : StrSplit(dir, "/")) {
        if (part.empty()) { cur += "/"; continue; }
        cur += part;
        mkdir(cur.c_str(), 0700);
        cur += "/";
    }
}

Json JNums(std::initializer_list<double> v) {
    Json j = Json::Arr();
    for (double x : v) j.push(Json::Num_(x));
    return j;
}

// ---- JSON text ----
static const char *kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static Str B64(const uint8_t *p, size_t n) {
    Str o;
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)p[i] << 16 | (i + 1 < n ? (uint32_t)p[i + 1] << 8 : 0) | (i + 2 < n ? p[i + 2] : 0);
        o += kB64[(v >> 18) & 63];
        o += kB64[(v >> 12) & 63];
        o += i + 1 < n ? kB64[(v >> 6) & 63] : '=';
        o += i + 2 < n ? kB64[v & 63] : '=';
    }
    return o;
}
static std::vector<uint8_t> UnB64(const Str &s) {
    std::vector<uint8_t> out;
    uint32_t v = 0;
    int bits = 0;
    for (char c : s) {
        const char *k = strchr(kB64, c);
        if (!k || !c) continue;
        v = v << 6 | (uint32_t)(k - kB64);
        bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back((uint8_t)(v >> bits)); }
    }
    return out;
}

static void DumpStr(const Str &s, Str &o) {
    o += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n"; break;
            case '\r': o += "\\r"; break;
            case '\t': o += "\\t"; break;
            default:
                if (c < 0x20) o += Fmt("\\u%04x", c);
                else o += (char)c;
        }
    }
    o += '"';
}

static void DumpTo(const Json &j, Str &o) {
    switch (j.t) {
        case Json::Null: o += "null"; break;
        case Json::Bool: o += j.b ? "true" : "false"; break;
        case Json::Num:
            if (std::isfinite(j.n) && j.n == floor(j.n) && fabs(j.n) < 1e15) o += Fmt("%lld", (long long)j.n);
            else if (std::isfinite(j.n)) o += Fmt("%.9g", j.n);
            else o += "0";
            break;
        case Json::String: DumpStr(j.s, o); break;
        case Json::Array:
            o += '[';
            for (size_t i = 0; i < j.a.size(); i++) { if (i) o += ','; DumpTo(j.a[i], o); }
            o += ']';
            break;
        case Json::Object:
            o += '{';
            for (size_t i = 0; i < j.o.size(); i++) {
                if (i) o += ',';
                DumpStr(j.o[i].first, o);
                o += ':';
                DumpTo(j.o[i].second, o);
            }
            o += '}';
            break;
        case Json::Floats: {   // little-endian float32, base64, tagged so the reader knows
            o += "{\"$f32\":\"";
            if (j.fl && !j.fl->empty()) o += B64((const uint8_t *)j.fl->data(), j.fl->size() * sizeof(float));
            o += "\"}";
            break;
        }
    }
}
Str Json::Dump() const { Str o; DumpTo(*this, o); return o; }

namespace {
struct Parser {
    const char *p, *e;
    bool ok = true;
    void ws() { while (p < e && isspace((unsigned char)*p)) p++; }
    bool lit(const char *w) { size_t n = strlen(w); if ((size_t)(e - p) >= n && memcmp(p, w, n) == 0) { p += n; return true; } return false; }
    static void PutUtf8(Str &o, uint32_t cp) {
        if (cp < 0x80) o += (char)cp;
        else if (cp < 0x800) { o += (char)(0xC0 | (cp >> 6)); o += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { o += (char)(0xE0 | (cp >> 12)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
        else { o += (char)(0xF0 | (cp >> 18)); o += (char)(0x80 | ((cp >> 12) & 0x3F)); o += (char)(0x80 | ((cp >> 6) & 0x3F)); o += (char)(0x80 | (cp & 0x3F)); }
    }
    Str str() {
        Str o;
        if (p >= e || *p != '"') { ok = false; return o; }
        p++;
        while (p < e && *p != '"') {
            char c = *p++;
            if (c != '\\') { o += c; continue; }
            if (p >= e) { ok = false; break; }
            char x = *p++;
            switch (x) {
                case 'n': o += '\n'; break;
                case 't': o += '\t'; break;
                case 'r': o += '\r'; break;
                case 'b': o += '\b'; break;
                case 'f': o += '\f'; break;
                case 'u': {
                    if (e - p < 4) { ok = false; return o; }
                    uint32_t cp = (uint32_t)strtoul(Str(p, 4).c_str(), nullptr, 16);
                    p += 4;
                    if (cp >= 0xD800 && cp <= 0xDBFF && e - p >= 6 && p[0] == '\\' && p[1] == 'u') {
                        uint32_t lo = (uint32_t)strtoul(Str(p + 2, 4).c_str(), nullptr, 16);
                        if (lo >= 0xDC00 && lo <= 0xDFFF) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); p += 6; }
                    }
                    PutUtf8(o, cp);
                    break;
                }
                default: o += x;
            }
        }
        if (p < e) p++; else ok = false;
        return o;
    }
    Json val(int depth) {
        Json j;
        ws();
        if (p >= e || depth > 64) { ok = false; return j; }
        if (*p == '{') {
            p++;
            j = Json::Obj();
            ws();
            if (p < e && *p == '}') { p++; return j; }
            while (ok) {
                ws();
                Str k = str();
                ws();
                if (p >= e || *p != ':') { ok = false; break; }
                p++;
                Json v = val(depth + 1);
                j.o.emplace_back(k, v);
                ws();
                if (p < e && *p == ',') { p++; continue; }
                if (p < e && *p == '}') { p++; break; }
                ok = false;
            }
            if (j.o.size() == 1 && j.o[0].first == "$f32" && j.o[0].second.t == Json::String) {   // a float grid
                std::vector<uint8_t> raw = UnB64(j.o[0].second.s);
                std::vector<float> f(raw.size() / sizeof(float));
                if (!f.empty()) memcpy(f.data(), raw.data(), f.size() * sizeof(float));
                return Json::FloatsOf(std::move(f));
            }
            return j;
        }
        if (*p == '[') {
            p++;
            j = Json::Arr();
            ws();
            if (p < e && *p == ']') { p++; return j; }
            while (ok) {
                j.a.push_back(val(depth + 1));
                ws();
                if (p < e && *p == ',') { p++; continue; }
                if (p < e && *p == ']') { p++; break; }
                ok = false;
            }
            return j;
        }
        if (*p == '"') return Json::Str_(str());
        if (lit("true")) return Json::Bool_(true);
        if (lit("false")) return Json::Bool_(false);
        if (lit("null")) return j;
        char *end = nullptr;
        double d = strtod(p, &end);
        if (end == p) { ok = false; return j; }
        p = end;
        return Json::Num_(d);
    }
};
}

Json Json::Parse(const Str &text, bool *okOut) {
    Parser ps{ text.data(), text.data() + text.size() };
    Json j = ps.val(0);
    if (okOut) *okOut = ps.ok;
    return ps.ok ? j : Json();
}
