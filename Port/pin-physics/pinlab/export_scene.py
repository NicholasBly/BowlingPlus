#!/usr/bin/env python3
"""Exports the game's pin physics scene for pinlab (scene.txt): one lane pin's colliders (PinsPhys/PinUnity1: five
convex hulls + two capsules, with their physics materials), and every static box collider near the lane
(ScenePhysics/...), with materials. Reads the game's own files with UnityPy (pip install UnityPy).
Usage:  python3 export_scene.py <the game's Data folder (with level1, sharedassets1.assets)> scene.txt"""
import sys, logging, numpy as np, UnityPy
logging.disable(logging.CRITICAL)
D = sys.argv[1].rstrip('/') + '/'
env = UnityPy.load(D + 'level1', D + 'sharedassets1.assets')
trs, go_of, tr_of, names, gos = {}, {}, {}, {}, {}
lvl = [o for o in env.objects if o.assets_file.name == 'level1']
for o in lvl:
    if o.type.name in ('Transform', 'RectTransform'):
        t = o.read(); trs[o.path_id] = t; go_of[o.path_id] = t.m_GameObject.path_id; tr_of[t.m_GameObject.path_id] = o.path_id
    elif o.type.name == 'GameObject':
        g = o.read(); names[o.path_id] = g.m_Name; gos[o.path_id] = g
byid = {o.path_id: o for o in lvl}
def parent(t): p = trs[t].m_Father.path_id; return p if p in trs else None
def path(t):
    out = []
    while t: out.append(names.get(go_of[t], '?')); t = parent(t)
    return '/'.join(reversed(out))
def world(t):
    ch = []
    while t: ch.append(t); t = parent(t)
    M = np.eye(4)
    for c in reversed(ch):
        T = trs[c]; p, r, s = T.m_LocalPosition, T.m_LocalRotation, T.m_LocalScale
        x, y, z, w = r.x, r.y, r.z, r.w
        R = np.array([[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)], [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)], [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]])
        L = np.eye(4); L[:3, :3] = R @ np.diag([s.x, s.y, s.z]); L[:3, 3] = [p.x, p.y, p.z]; M = M @ L
    return M
def active(t):
    while t:
        if not gos[go_of[t]].m_IsActive: return False
        t = parent(t)
    return True
def material(pp):
    if not pp or not pp.get('m_PathID'): return None
    for o in env.objects:
        if o.path_id == pp['m_PathID'] and o.type.name in ('PhysicsMaterial', 'PhysicMaterial'):
            if pp['m_FileID'] == 0 and o.assets_file.name != 'level1': continue
            return o.read_typetree()
def quat(m):
    t = np.trace(m)
    if t > 0: s = np.sqrt(t + 1) * 2; return [(m[2,1]-m[1,2])/s, (m[0,2]-m[2,0])/s, (m[1,0]-m[0,1])/s, 0.25*s]
    i = int(np.argmax(np.diag(m)))
    if i == 0: s = np.sqrt(1+m[0,0]-m[1,1]-m[2,2])*2; return [0.25*s, (m[0,1]+m[1,0])/s, (m[0,2]+m[2,0])/s, (m[2,1]-m[1,2])/s]
    if i == 1: s = np.sqrt(1+m[1,1]-m[0,0]-m[2,2])*2; return [(m[0,1]+m[1,0])/s, 0.25*s, (m[1,2]+m[2,1])/s, (m[0,2]-m[2,0])/s]
    s = np.sqrt(1+m[2,2]-m[0,0]-m[1,1])*2; return [(m[0,2]+m[2,0])/s, (m[1,2]+m[2,1])/s, 0.25*s, (m[1,0]-m[0,1])/s]
S = lambda n: str(n).replace(' ', '_')
out = open(sys.argv[2], 'w'); mats = {}
pin = [t for t in trs if path(t) == 'PinsPhys/PinUnity1'][0]
inv = np.linalg.inv(world(pin))
def walk(t):
    for c in gos[go_of[t]].m_Components:
        o = byid.get(c.component.path_id if hasattr(c, 'component') else c.path_id)
        if not o or o.type.name not in ('MeshCollider', 'CapsuleCollider'): continue
        r = o.read_typetree(); m = material(r.get('m_Material')); mats[S(m['m_Name'])] = m
        M = inv @ world(t)
        if o.type.name == 'MeshCollider':
            mo = [x for x in env.objects if x.path_id == r['m_Mesh']['m_PathID'] and x.type.name == 'Mesh' and x.assets_file.name == 'sharedassets1.assets'][0].read()
            V = np.array([[float(v) for v in l.split()[1:4]] for l in mo.export().splitlines() if l.startswith('v ')]); V[:, 0] *= -1   # the exporter flips X
            W = (M @ np.c_[V, np.ones(len(V))].T).T[:, :3]
            out.write('hull %s %s %d\n' % (S(names[go_of[t]]), S(m['m_Name']), len(W)))
            for v in W: out.write('%.6f %.6f %.6f\n' % tuple(v))
        else:
            s = np.linalg.norm(M[:3, 0]); c0 = (M @ np.r_[[r['m_Center'][a] for a in 'xyz'], 1])[:3]
            out.write('capsule %s %s %.6f %.6f %.6f %.6f %.6f %d\n' % (S(names[go_of[t]]), S(m['m_Name']), r['m_Radius'] * s, r['m_Height'] * s, *c0, r['m_Direction']))
    for ch in [c.path_id for c in trs[t].m_Children]: walk(ch)
walk(pin)
for o in lvl:
    if o.type.name != 'BoxCollider': continue
    r = o.read_typetree(); t = tr_of.get(r['m_GameObject']['m_PathID'])
    if not t or not path(t).startswith('ScenePhysics/') or r.get('m_IsTrigger') or not r.get('m_Enabled', True) or not active(t): continue
    W = world(t); sc = np.linalg.norm(W[:3, :3], axis=0); R = W[:3, :3] / sc
    if np.linalg.det(R) < 0: R[:, 0] *= -1
    c0 = (W @ np.r_[[r['m_Center'][a] for a in 'xyz'], 1])[:3]; half = np.abs(np.array([r['m_Size'][a] for a in 'xyz']) * sc) / 2
    ext = np.abs(R) @ half
    if c0[0]+ext[0] < -2 or c0[0]-ext[0] > 2 or c0[1]+ext[1] < 10 or c0[1]-ext[1] > 30: continue
    m = material(r.get('m_Material')) or {'m_Name': 'none', 'm_StaticFriction': 0.6, 'm_DynamicFriction': 0.6, 'm_Bounciness': 0, 'm_FrictionCombine': 0, 'm_BounceCombine': 0}
    mats[S(m['m_Name'])] = m
    out.write('box %s %s %.5f %.5f %.5f %.5f %.5f %.5f %.6f %.6f %.6f %.6f\n' % (S(path(t).split('/')[-1]), S(m['m_Name']), *c0, *half, *quat(R)))
mats.setdefault('Ball', {'m_StaticFriction': 0.3, 'm_DynamicFriction': 0.3, 'm_Bounciness': 0.65, 'm_FrictionCombine': 1, 'm_BounceCombine': 1})
for n, m in mats.items():
    out.write('material %s %.4f %.4f %.4f %d %d\n' % (n, m['m_StaticFriction'], m['m_DynamicFriction'], m['m_Bounciness'], m['m_FrictionCombine'], m['m_BounceCombine']))
out.close(); print('wrote', sys.argv[2])
