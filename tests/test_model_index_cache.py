#!/usr/bin/env python3
# northlight-test: requires=cxx
"""Exercise bounded, fully byte-verified index plans; no real D3D/Wine/game."""
import sys; from pathlib import Path; sys.path.insert(0, str(Path(__file__).resolve().parents[1]))  # repo root
import northlight_paths as fp
from pathlib import Path
import ast,subprocess,tempfile,hashlib,json
HERE=Path(__file__).resolve().parent
def literal(file,key):return next(ast.literal_eval(n.value) for n in ast.parse(file.read_text()).body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id==key for t in n.targets))
stub=literal(HERE/'test_terrain_snapshot.py','stub');prefix=literal(HERE/'test_draw_snapshot.py','harness').split('int main()')[0]
body=r'''
int main(){Device d;Frame capture;Mesh mesh;Diagnostics why;
 constexpr unsigned n=300;std::vector<std::uint16_t> a(n),b;const unsigned source[]={0,1,2,8,9,10};for(unsigned i=0;i<n;++i)a[i]=std::uint16_t(source[i%6]);a[10]=4;b=a;b[10]=7;
 // Explicitly prove the changed two bytes fall outside EVERY fingerprint word.
 for(unsigned sample=0;sample<8;++sample){size_t offset=((a.size()*2-4)*sample)/7;assert(20+2<=offset||20>=offset+4);}
 d.ib.desc.Format=D3DFMT_INDEX16;d.ib.bytes.resize(n*2);Draw draw{D3DPT_TRIANGLELIST,0,0,12,0,n/3,true};
 auto verify=[&](const std::vector<std::uint16_t>& indices){assert(mesh.indices.size()==indices.size());for(unsigned i=0;i<indices.size();++i)for(unsigned stream=0;stream<2;++stream)for(unsigned byte=0;byte<(stream?4u:16u);++byte)
  assert(mesh.streams[stream].bytes[mesh.indices[i]*d.stride[stream]+byte]==d.vb[stream].bytes[d.offset[stream]+indices[i]*d.stride[stream]+byte]);};
 for(unsigned i=0;i<20;++i){const auto& raw=i&1?a:b;std::memcpy(d.ib.bytes.data(),raw.data(),raw.size()*2);d.vb[0].bytes[d.offset[0]+4]^=7;
  assert(capture.read(&d,&d.decl,draw,mesh,&why));verify(raw);assert(capture.indexCacheBytes()<=16u*1024u*1024u);}
 assert(capture.indexCacheMisses()==2&&capture.indexCacheHits()==18&&capture.indexCacheEntries()==2);
 const size_t readBytes=capture.bytesRead();assert(readBytes>0&&d.vb[0].locks==20&&d.ib.locks==20);
 // Returned mutable snapshots cannot alter the private index plan.
 mesh.indices[0]=999;capture.clearFrame();assert(capture.indexCacheHits()==0&&capture.indexCacheMisses()==0);
 assert(capture.read(&d,&d.decl,draw,mesh,&why));verify(a);assert(capture.indexCacheHits()==1);
 // Same full raw data with invalid min/range must not use a wider contract.
 Draw invalid=draw;invalid.minimum=1;assert(!capture.read(&d,&d.decl,invalid,mesh,&why)&&why.error==Error::IndexRange&&mesh.indices.empty());
 assert(capture.read(&d,&d.decl,draw,mesh,&why));verify(a);
 // Lock/range/budget gates still execute before a valid index cache hit.
 d.ib.failLock=true;assert(!capture.read(&d,&d.decl,draw,mesh,&why)&&why.error==Error::Lock);d.ib.failLock=false;
 d.vb[1].failUnlock=true;assert(!capture.read(&d,&d.decl,draw,mesh,&why)&&why.error==Error::Unlock);d.vb[1].failUnlock=false;
 assert(capture.read(&d,&d.decl,draw,mesh,&why));verify(a);
 // UP uses the same byte proof and remap, never identity of caller memory.
 d.decl.e.erase(d.decl.e.begin()+1);std::vector<unsigned char> up(12*d.stride[0]);for(unsigned i=0;i<up.size();++i)up[i]=std::uint8_t(i*7);
 for(unsigned round=0;round<4;++round){const auto& raw=round&1?a:b;up[4]^=1;assert(capture.readUP(&d.decl,draw,raw.data(),D3DFMT_INDEX16,up.data(),d.stride[0],mesh,&why));
  for(unsigned i=0;i<raw.size();++i)for(unsigned byte=0;byte<16;++byte)assert(mesh.streams[0].bytes[mesh.indices[i]*d.stride[0]+byte]==up[raw[i]*d.stride[0]+byte]);}
 d.decl.e={{0,4,2,0,0,0},{1,0,4,0,1,0},{0xff,0,17,0,0,0}};
 // Cache count cap (1024 since 0.3.77) with distinct valid draw contracts, then content revalidation.
 // An empty cache charges only its fixed 1024-slot table.
 capture.clearIndexCache();assert(capture.indexCacheEntries()==0&&capture.indexCacheBytes()<=16384);
 for(unsigned i=0;i<1100;++i){capture.clearFrame();draw.vertices=12+i;assert(capture.read(&d,&d.decl,draw,mesh));verify(a);assert(capture.indexCacheEntries()<=1024&&capture.indexCacheBytes()<=16u*1024u*1024u);}
 assert(capture.indexCacheEntries()==1024);capture.clearFrame();draw.vertices=12;assert(capture.read(&d,&d.decl,draw,mesh));assert(capture.indexCacheMisses()==1);
 // Memory cap (16 MiB since 0.3.77) binds before entry cap on large index plans.
 capture.clearIndexCache();constexpr unsigned large=32766;d.ib.bytes.resize(large*2);for(unsigned i=0;i<large;++i){uint16_t index=uint16_t(i%8);std::memcpy(d.ib.bytes.data()+i*2,&index,2);}draw.primitives=large/3;
 for(unsigned i=0;i<128;++i){capture.clearFrame();draw.vertices=12+i;assert(capture.read(&d,&d.decl,draw,mesh));assert(capture.indexCacheBytes()<=16u*1024u*1024u);}
 assert(capture.indexCacheEntries()>0&&capture.indexCacheEntries()<128);
 // A maximum-size INDEX32 plan (87380 primitives, ~2 MiB) now fits the 16 MiB cache and stays correct;
 // no valid draw can exceed the cache, so the oversized bypass is only a defensive path since 0.3.77.
 capture.clearIndexCache();constexpr unsigned huge=262140;d.ib.desc.Format=D3DFMT_INDEX32;d.ib.bytes.resize(huge*4);
 for(unsigned i=0;i<huge;++i){UINT index=i%8;std::memcpy(d.ib.bytes.data()+i*4,&index,4);}draw.primitives=huge/3;draw.vertices=12;capture.clearFrame();
 assert(capture.read(&d,&d.decl,draw,mesh));assert(capture.indexCacheEntries()==1&&mesh.vertexCount==8&&mesh.indices.size()==huge);for(unsigned i=0;i<huge;++i)assert(mesh.indices[i]==i%8);
 assert(d.ib.refs==1&&d.ib.locks==d.ib.unlocks);for(auto& vb:d.vb)assert(vb.refs==1&&vb.locks==vb.unlocks);
 std::puts("PASS full fingerprint-collision comparisons, alternating plans/current VB mutation, UP, mutable-output isolation, contract/failure gates, 1024-entry/16MiB eviction, maximum-size plan");
}
'''
report={}
with tempfile.TemporaryDirectory(prefix='northlight-model-index-cache-') as temp:
 p=Path(temp);(p/'d3d9.h').write_text(stub);(p/'test.cpp').write_text(prefix+body)
 for label,flags in [('native',['-O2']),('sanitizer',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
  subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror',*flags,'-I',str(p),*fp.test_include_flags(),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
  result=subprocess.run([str(p/'test')],capture_output=True,text=True);print(label+': '+result.stdout+result.stderr,end='');result.check_returncode();report[label]=True
report['source_sha256']=hashlib.sha256(fp.src('draw_snapshot.h').read_bytes()).hexdigest();report['game_launched']=False
(fp.output_dir()/'model-index-cache-validation.json').write_text(json.dumps(report,indent=2)+'\n')
