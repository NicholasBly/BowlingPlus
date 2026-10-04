#!/usr/bin/env python3
"""Extract the game's 48 built-in Kegel pattern files (OilDescription._source TextAssets) from the decrypted IPA's
`Data/Raw/AssetBundles/data` bundle into ./files/NN.txt, in the game's own order, and print each pattern's name,
distance and volume as the game records them. Needs `pip install UnityPy`.

    python3 extract_game_patterns.py /path/to/Payload/BowlingbyJasonBelmonte.app/Data/Raw/AssetBundles/data

Then build the shared-parser check (tools/oil_verify/all48.cpp) and run it on files/*.txt: all 48 parse, and the
Kegel-accurate model gives clean grids for every one.
"""
import os, sys, UnityPy
env = UnityPy.load(sys.argv[1]); objs = {o.path_id: o for o in env.objects}
oils = None
for o in env.objects:
    if o.type.name != 'MonoBehaviour': continue
    try: tt = o.read_typetree()
    except Exception: continue
    if 'Oils' in tt: oils = tt['Oils']; break
os.makedirs('files', exist_ok=True)
for i, e in enumerate(oils):
    d = objs[e['_source']['m_PathID']].read()
    s = d.m_Script if isinstance(d.m_Script, str) else bytes(d.m_Script).decode('utf-8', 'replace')
    open('files/%02d.txt' % i, 'w', encoding='utf-8', newline='').write(s)
    print('%02d  %-30s distance %s ft  volume %s mL' % (i, e['LongName'].strip(), e['Distance'], e['Volume']))
