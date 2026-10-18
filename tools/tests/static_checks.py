#!/usr/bin/env python3
"""Checks that need no compiler: the code both platforms share is byte-identical, every setting is read, saved and
reachable the same way on iOS and Android, the version is the same everywhere, the workflows parse and point at
files that exist, and every Python tool compiles.
Usage (from the repo root):  python3 tools/tests/static_checks.py"""
import glob, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from common import ROOT, read, region, Checks

c = Checks()
ios, android = read('src/Game.mm'), read('android/native/Game.cpp')

# ---- shared native code: these sections are the same text on both platforms (per-platform bits go through macros
# above them). A new shared section goes in this list.
SHARED = [
    ('// ---- 1.7.5: ball stats off, loft, game modes, ramps & obstacles ----', 'static void OilTick() {'),
    ('// ---- Arsenal order (1.7.5)', '// Arsenal search'),
    ('// ---- pins keep their turn (Practice) ----', '// ---- your own pin image'),
    ("// ---- engine methods for the two parts below", "// ---- the sweeper's banner stays in front of the bar"),
    ('// ---- realistic pin physics (Practice) ----', "// ---- the game's own settings, for the game modes"),
    ('// ---- 1) mirror fix ----', '// ---- 3) invisible oil'),
]
for a, b in SHARED:
    try:
        x, y = region(ios, a, b), region(android, a, b)
    except ValueError:
        c.ok(False, f'shared block "{a[8:50]}...": markers not found on both platforms'); continue
    if x != y:
        xl, yl = x.split('\n'), y.split('\n')
        line = next((i for i in range(min(len(xl), len(yl))) if xl[i] != yl[i]), min(len(xl), len(yl)))
        c.ok(False, f'shared block "{a[8:50]}..." differs from its line {line + 1}:\n         iOS:     {xl[line] if line < len(xl) else "(end)"}\n         Android: {yl[line] if line < len(yl) else "(end)"}')
    else:
        c.ok(True, f'shared block "{a[8:50]}..." identical ({x.count(chr(10))} lines)')

# ---- settings
eng_ios, eng_and = read('src/Engine.mm'), read('android/native/Engine.cpp')
ios_load = set(re.findall(r'd\[@"(\w+)"\]', region(eng_ios, 'void BFLoadConfig(void) {', 'void BFSaveConfig(void) {')))
ios_save = set(re.findall(r'@"(\w+)":\s*@', region(eng_ios, 'void BFSaveConfig(void) {', '[[NSUserDefaults standardUserDefaults] setObject:d')))
and_save = set(re.findall(r'd\.set\("(\w+)"', region(eng_and, 'Json BFConfigJson(void) {', 'static void Clamp() {')))
and_set = set(re.findall(r'k == "(\w+)"', region(eng_and, 'bool BFConfigSet(', 'void BFLoadConfig(void) {')))
java = ''.join(read(p) for p in glob.glob(os.path.join(ROOT, 'android/java/com/bowlingplus/*.java')))
java_keys = set(re.findall(r'Config\.(?:b|d|i|set)\("(\w+)"', java)) | set(re.findall(r'toggle\(\w+, "(\w+)"', java))
for arr in re.findall(r'String\[\] keys = \{([^}]*)\}', read('android/java/com/bowlingplus/Menu.java')):   # the obstacle kinds' switches
    java_keys |= set(re.findall(r'"(\w+)"', arr))
LEGACY_IOS = {'privacy'}                     # read once to carry an old setting over, never saved
ANDROID_ONLY = {'logHosts', 'fbWebLogin'}    # Android-only features
c.ok(ios_save <= ios_load, 'iOS: every saved setting is read back' + ('' if ios_save <= ios_load else f' (not read: {sorted(ios_save - ios_load)})'))
c.ok(ios_load - LEGACY_IOS <= ios_save, 'iOS: every setting read is saved' + ('' if ios_load - LEGACY_IOS <= ios_save else f' (not saved: {sorted(ios_load - LEGACY_IOS - ios_save)})'))
c.ok(and_save <= and_set, 'Android: every saved setting is read back' + ('' if and_save <= and_set else f' (not read: {sorted(and_save - and_set)})'))
c.ok(and_set <= and_save, 'Android: every setting the menu can set is saved' + ('' if and_set <= and_save else f' (not saved: {sorted(and_set - and_save)})'))
c.ok(java_keys <= and_set, f'Android: every setting the Java side uses ({len(java_keys)}) reaches the native side' + ('' if java_keys <= and_set else f' (unknown to native: {sorted(java_keys - and_set)})'))
c.ok(ios_save == and_save - ANDROID_ONLY, f'the same {len(ios_save)} settings on both platforms' + ('' if ios_save == and_save - ANDROID_ONLY else f' (iOS only: {sorted(ios_save - and_save)}, Android only: {sorted(and_save - ANDROID_ONLY - ios_save)})'))
fields = lambda s: re.findall(r'^\s+(?:bool|float|int|uint16_t|uint32_t|int32_t)\s+([\w, ]+);', region(s, 'typedef struct {', '} BFConfig;'), re.M)
fi, fa = fields(read('src/BFShared.h')), fields(read('android/native/BFShared.h'))
fa = [f for f in fa if f not in ('logHosts', 'fbWebLogin')]   # (the Android-only settings)
c.ok(fi == fa, f'BFConfig has the same {len(fi)} fields, in the same order, on both platforms' + ('' if fi == fa else f' (iOS {fi} vs Android {fa})'))

# ---- version
v = re.search(r'^Version:\s*(\S+)', read('control'), re.M).group(1)
found = {
    'control': v,
    'src/BFShared.h': re.search(r'#define BF_VERSION @"([^"]+)"', read('src/BFShared.h')).group(1),
    'android/native/BFShared.h': re.search(r'#define BF_VERSION "([^"]+)"', read('android/native/BFShared.h')).group(1),
    'BF.java': re.search(r'VERSION = "([^"]+)"', read('android/java/com/bowlingplus/BF.java')).group(1),
    'CHANGELOG.md (newest)': re.search(r'^## \[([^\]]+)\]', read('CHANGELOG.md'), re.M).group(1),
}
c.ok(len(set(found.values())) == 1, 'version ' + ', '.join(f'{k} {x}' for k, x in found.items()))
c.ok(f'**{v}:**' in read('README.md'), f'README has a line for {v}')

# ---- workflows: they parse, and the scripts and files they run exist
try:
    import yaml
except ImportError:
    yaml = None
for wf in sorted(glob.glob(os.path.join(ROOT, '.github/workflows/*.yml'))):
    name = os.path.relpath(wf, ROOT)
    text = read(wf)
    if yaml:
        try:
            doc = yaml.safe_load(text)
            c.ok(isinstance(doc, dict) and 'jobs' in doc, f'{name} parses')
        except Exception as e:
            c.ok(False, f'{name} does not parse: {e}')
    refs = set(re.findall(r'(?<![\w/.-])((?:tools|android|src|layout|Port)/[\w./-]+\.(?:py|sh|json|plist|mm|cpp|h))', text))
    missing = sorted(r for r in refs if not os.path.exists(os.path.join(ROOT, r)))
    c.ok(not missing, f'{name}: the {len(refs)} repo files it names exist' + (f' (missing: {missing})' if missing else ''))
if not yaml: print('  (PyYAML not installed: workflow parsing skipped)')

# ---- every Python tool compiles
bad = []
for p in glob.glob(os.path.join(ROOT, 'tools/**/*.py'), recursive=True):
    if '__pycache__' in p: continue
    try: compile(read(p), p, 'exec')
    except SyntaxError: bad.append(os.path.relpath(p, ROOT))
c.ok(not bad, 'every Python tool compiles' + (f' (not: {bad})' if bad else ''))

sys.exit(c.done('static checks'))
