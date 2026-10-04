// Reads the text layout the game's 48 patterns and Kegel's own ".txt" export share (plain C++ so it can be
// tested on its own). Fixed line positions (checked against all 48 of the game's pattern files):
//   line 1 name / series, 10 microliters per board, 12 distance (ft), 13 reverse brush drop (ft),
//   then 15-line columns: forward start 14, stop 29, loads 44, speed 59, end distance 136;
//   reverse start 75, stop 90, loads 105, speed 120, end distance 211.
// Line 0 is -1 in some files and a pattern id in others, so it isn't checked.
#pragma once
#include <string>
#include <vector>
#include <cstdlib>

struct KegelFileStep { int start, stop, loads, speed; float end; };
struct KegelFile {
    bool ok = false;
    int feet = 0, drop = 0, ul = 50;
    std::string name;
    std::vector<KegelFileStep> fwd, rev;
};

static inline std::string KegelTrim(const std::string &s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) b--;
    return s.substr(a, b - a);
}

static inline KegelFile KegelParseLines(const std::vector<std::string> &L) {
    KegelFile f;
    if (L.size() < 226) return f;
    auto at = [&](size_t i) -> std::string { return i < L.size() ? KegelTrim(L[i]) : std::string(); };
    static const size_t base[2][5] = { { 14, 29, 44, 59, 136 }, { 75, 90, 105, 120, 211 } };
    for (int d = 0; d < 2; d++) {
        for (size_t i = 0; i < 15; i++) {
            std::string st = at(base[d][0] + i), sp = at(base[d][3] + i);
            if (st.empty() || sp.empty()) break;
            KegelFileStep k = { atoi(st.c_str()), atoi(at(base[d][1] + i).c_str()), atoi(at(base[d][2] + i).c_str()),
                                atoi(sp.c_str()), (float)atof(at(base[d][4] + i).c_str()) };
            if (k.start < 1 || k.stop > 39 || k.stop < k.start || k.speed <= 0) return f;     // not this layout
            (d == 0 ? f.fwd : f.rev).push_back(k);
        }
    }
    f.feet = atoi(at(12).c_str());
    f.drop = atoi(at(13).c_str());
    f.ul = atoi(at(10).c_str());
    if (f.ul < 5 || f.ul > 150) f.ul = 50;
    f.name = at(1);
    f.ok = !f.fwd.empty() && f.feet >= 15 && f.feet <= 60;
    return f;
}
