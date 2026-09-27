#!/usr/bin/env python3
# northlight-test: requires=client,stormlib,dll
"""Release gate (0.3.161): the built DLL holds no 64-byte window of any of the 36 client
liquid shader originals or their mask variants, nor of the decoded shadowblob.blp. The
game data is read from the tester's client; only counts are written. A DLL argument
(e.g. a 0.3.160 build) is scanned instead of the resolved one: a positive control.

--tree DIR scans every file under DIR (a source snapshot, see scripts/export_public.py):
- against every shader program in the client's archives and shadowblob.blp, in 64-byte windows:
  the raw bytes, and the bytes that the 0x... literals of a text file spell (each brace list as
  bytes when every literal fits a byte, else as little-endian 32-bit words).
Each hit names the file and the client file it matches.
No game, graphics device or Wine."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp; fp.use_source_modules()
import argparse
import json
import re
import struct
from pathlib import Path
from extract_water_shaders import accepted_originals,patch
from extract_signatures import variants
from world_scene_builder import Assets,decode_blp

WINDOW=64
def windows(data,step=4):
    # Skip low-entropy windows (the blob's flat runs): they identify nothing.
    return {data[i:i+WINDOW] for i in range(0,len(data)-WINDOW+1,step) if len(set(data[i:i+WINDOW]))>=8}

HEX=re.compile(rb'0[xX]([0-9a-fA-F]{1,8})[uU]?\b')
BRACES=re.compile(rb'\{(\s*(?:0[xX][0-9a-fA-F]{1,8}[uU]?\s*,\s*)+0[xX][0-9a-fA-F]{1,8}[uU]?\s*,?\s*)\}')   # a pure initializer list
def literal_words(data):
    """The bytes that the 0x... literals of a text file spell as little-endian uint32 words."""
    words=[int(m.group(1),16) for m in HEX.finditer(data)]
    return struct.pack(f'<{len(words)}I',*words) if len(words)*4>=WINDOW else b''
def literal_runs(data):
    """Per initializer list of 0x... literals: bytes if every literal fits a byte, else uint32 words."""
    runs=[]
    for body in BRACES.findall(data):
        values=[int(m.group(1),16) for m in HEX.finditer(body)]
        if len(values)<2:continue
        runs.append(bytes(values) if max(values)<=0xff else struct.pack(f'<{len(values)}I',*values))
    return runs
def blob_windows(assets):
    _,_,blob=decode_blp(assets.read('textures\\shadowblob.blp'),32)
    return windows(blob)

def scan_dll(dll):
    shaders=set()
    for _,original in accepted_originals():shaders|=windows(original)|windows(patch(original)[0])
    assets=Assets()
    try:blobs=blob_windows(assets)
    finally:assets.close()
    image=dll.read_bytes();hits={'shader':0,'blob':0}
    for i in range(len(image)-WINDOW+1):
        w=image[i:i+WINDOW]
        if w in shaders:hits['shader']+=1
        elif w in blobs:hits['blob']+=1
    report={'dll':dll.name,'dll_bytes':len(image),'shader_windows':len(shaders),'blob_windows':len(blobs),'hits':hits}
    (fp.output_dir()/'no-game-bytes.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
    if any(hits.values()):raise SystemExit('game bytes found in '+str(dll))
    print('PASS no 64-byte window of the client liquid shaders or shadowblob.blp in',dll.name)

def scan_tree(root):
    reference={};programs=0
    assets=Assets()
    try:
        for name in sorted(assets.providers):
            if not name.endswith('.bls'):continue
            data=assets.read(name)
            try:codes=[code for _,_,code in variants(data)]
            except ValueError:codes=[data]   # not a shader container this parser knows: the whole file
            programs+=len(codes)
            for code in codes:
                for w in windows(code):reference.setdefault(w,name)
        for w in blob_windows(assets):reference.setdefault(w,'textures\\shadowblob.blp')
        client=assets.fingerprint()
    finally:assets.close()
    files=sorted(p for p in root.rglob('*') if p.is_file() and '.git' not in p.relative_to(root).parts)
    hits={};scanned=0
    for path in files:
        data=path.read_bytes();scanned+=len(data);found={};runs=literal_runs(data)
        for kind,sources in (('bytes',[data]),('literals',[literal_words(data),*runs])):
            for source in sources:
                for i in range(len(source)-WINDOW+1):
                    name=reference.get(source[i:i+WINDOW])
                    if name:found.setdefault(f'{kind}:{name}',0);found[f'{kind}:{name}']+=1
        if found:hits[path.relative_to(root).as_posix()]=found
    report={'tree':root.name,'files':len(files),'bytes':scanned,'client_programs':programs,'reference_windows':len(reference),
            'client_view':client['view'],'client_archives':[a['archive'] for a in client['archives']],'hits':hits}
    (fp.output_dir()/'no-game-bytes-tree.json').write_text(json.dumps(report,indent=2)+'\n')
    for rel,found in sorted(hits.items()):print('HIT',rel,json.dumps(found))
    if hits:raise SystemExit(f'game bytes found in {len(hits)} file(s) under {root}')
    print(f'PASS no 64-byte window of {programs} client shader programs or shadowblob.blp in {len(files)} files under {root.name}')

def main():
    ap=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('dll',nargs='?',type=Path,help='a DLL to scan instead of the built one')
    ap.add_argument('--tree',type=Path,help='scan every file under this folder instead of a DLL')
    args=ap.parse_args()
    if args.tree:scan_tree(args.tree.resolve())
    else:scan_dll(args.dll or fp.dll())
if __name__=='__main__':main()
