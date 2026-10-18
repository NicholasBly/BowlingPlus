"""Helpers for the checks in tools/tests: the repo root, cutting code out of the live sources, compiling and running."""
import os, subprocess, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
IOS = os.path.join(ROOT, 'src', 'Game.mm')
ANDROID = os.path.join(ROOT, 'android', 'native', 'Game.cpp')


def read(path):
    with open(path if os.path.isabs(path) else os.path.join(ROOT, path), encoding='utf-8', errors='replace') as f:
        return f.read()


def region(src, start, end):
    """The text from the line holding `start` up to (not including) `end`."""
    a = src.index(start)
    a = src.rfind('\n', 0, a) + 1
    return src[a:src.index(end, a)]


def function(src, head):
    """A whole function (from `head`, e.g. 'static float LoftVz(', to its closing brace)."""
    a = src.index(head)
    i = src.index('{', a)
    depth = 0
    while True:
        c = src[i]
        if c == '{': depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0: return src[a:i + 1] + '\n'
        i += 1


def build_and_run(cpp_text, name, here):
    out = os.path.join(here, 'build_' + name + '.cpp')
    exe = os.path.join(here, name)
    with open(out, 'w') as f: f.write(cpp_text)
    r = subprocess.run(['g++', '-std=c++17', '-O0', '-g', '-Wall', '-Wno-unused-function', '-Wno-unused-variable',
                        '-Wno-unused-but-set-variable', out, '-o', exe])
    if r.returncode: sys.exit('compile failed: ' + out)
    rc = subprocess.run([exe]).returncode
    for p in (out, exe):
        try: os.remove(p)
        except OSError: pass
    return rc


class Checks:
    def __init__(self): self.n = self.bad = 0
    def ok(self, cond, what):
        self.n += 1
        if not cond: self.bad += 1
        print(('  ok   ' if cond else '  FAIL ') + what)
    def done(self, title):
        print(f'{title}: {self.n - self.bad}/{self.n} passed')
        return 1 if self.bad else 0
