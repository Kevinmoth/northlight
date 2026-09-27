// native sun/moon/moon02 and sunGlare suppression by the WRAPPED
// (proxy) texture identity the game stores. Portable identity policy only.
#include "celestial_glare_native.h"
#include <cassert>
#include <cstdio>
#include <unordered_map>
using namespace NorthlightCelestialDisc;
int main(){
    // The game stores exposed proxies; the extension device binds raw textures.
    const std::unordered_map<std::uintptr_t,std::uintptr_t> proxies={{0x5000,0x15000},{0x6000,0x16000},{0x7000,0x17000},{0x8000,0x18000},{0x9000,0x19000}};
    // Registry::rawOf with the mirror active: a miss is 0 (never a stale address).
    auto raw=[&](std::uintptr_t exposed)->std::uintptr_t{auto f=proxies.find(exposed);return f==proxies.end()?0:f->second;};
    Identities exposed;exposed.valid=7;exposed.texture[0]=0x5000;exposed.texture[1]=0x6000;exposed.texture[2]=0x7000;
    {   // 0.3.126..0.3.164: comparing the stored (exposed) identity with the bound raw texture never matched.
        IdentityFrame frame;frame.begin(exposed);assert(frame.claimIndex(0x15000)==-1&&frame.claimIndex(0x16000)==-1);
    }
    const Identities mapped=mapIdentities(exposed,raw);
    assert(mapped.texture[0]==0x15000&&mapped.texture[1]==0x16000&&mapped.texture[2]==0x17000&&mapped.valid==7);
    {Identities empty;empty.valid=1;assert(mapIdentities(empty,raw).texture[0]==0);} // no texture: stays 0 (never looked up)
    {   // A stale/unregistered exposed value maps to 0: that body is not identified, so a
        // live raw texture at the same address can never be taken for the sun (fail open).
        Identities stale=exposed;stale.texture[0]=0x15000;const Identities m=mapIdentities(stale,raw);assert(m.texture[0]==0);
        IdentityFrame frame;frame.begin(m);assert(frame.claimIndex(0x15000)==-1&&frame.claimIndex(0)==-1&&frame.claimIndex(0x16000)==1);
        // After a mirror escape the game may hold raw objects: the miss passes through.
        auto escaped=[&](std::uintptr_t e)->std::uintptr_t{auto f=proxies.find(e);return f==proxies.end()?e:f->second;};
        assert(mapIdentities(stale,escaped).texture[0]==0x15000);
    }
    {   // Wrapped-pointer match: sun, moon and moon02 are identified by their raw binding.
        IdentityFrame frame;frame.begin(mapped);
        assert(frame.claimIndex(0x15000)==0&&frame.claimIndex(0x16000)==1&&frame.claimIndex(0x17000)==2);
        assert(frame.claimIndex(0x5000)==-1&&frame.claimIndex(0x12345)==-1&&frame.claimIndex(0)==-1);
    }
    {   // A SUPPRESSED (claimed) native draw is not an observation: the body stays
        // owned and our late disc (canFallback) is still drawn.
        IdentityFrame frame;frame.begin(mapped);
        assert(frame.claimIndex(0x15000)==0&&frame.claimIndex(0x16000)==1); // suppressed, never observe()d
        frame.finish(mapped);assert(frame.canFallback(0)&&frame.canFallback(1)&&frame.mask()==0);
        // A native draw that was NOT suppressed (effects off / no live late disc) is observed: no double sun.
        IdentityFrame shown;shown.begin(mapped);shown.observe(0x15000);shown.finish(mapped);
        assert(!shown.canFallback(0)&&shown.canFallback(1)&&shown.mask()==1);
        // Streaming change during the frame: that body's fallback is withheld (fails open to native).
        IdentityFrame changed;changed.begin(mapped);Identities later=mapped;later.texture[1]=0x19000;changed.finish(later);
        assert(changed.canFallback(0)&&!changed.canFallback(1));
        // that same streaming change AFTER the native moon was suppressed
        // must still draw our late moon (else a frame with no moon at all).
        IdentityFrame streamed;streamed.begin(mapped);assert(streamed.claimIndex(0x16000)==1); // suppressed
        streamed.finish(later);assert(!streamed.canFallback(1)&&streamed.lateDisc(1,2u)&&!streamed.lateDisc(1,0));
        // Not suppressed (native shown and observed): never a second, late moon.
        IdentityFrame seen;seen.begin(mapped);seen.observe(0x16000);seen.finish(later);assert(!seen.lateDisc(1,2u)&&!seen.lateDisc(1,0));
        IdentityFrame unfinished;unfinished.begin(mapped);assert(!unfinished.lateDisc(0,1u)); // only after the frame's final read
        assert(changed.lateDisc(0,0)&&!changed.lateDisc(2,4u)); // body 2 (moon02) is never drawn late
    }
    {   // Glare: its own identities (0x8000 sun glare, 0x9000 moon glare) are mapped the
        // same way; a glare is suppressed only for a body suppressed/owned this or last frame.
        Identities glare;glare.valid=3;glare.texture[0]=0x8000;glare.texture[1]=0x9000;
        const Identities g=mapIdentities(glare,raw);
        assert(NorthlightCelestialGlare::claim(glare,glare,0x18000,1)==-1);   // unmapped: never matches (the old bug)
        assert(NorthlightCelestialGlare::claim(g,g,0x18000,1)==0&&NorthlightCelestialGlare::claim(g,g,0x19000,2)==1);
        assert(NorthlightCelestialGlare::claim(g,g,0x18000,2)==-1&&NorthlightCelestialGlare::claim(g,g,0x18000,0)==-1); // not owned: native glare stays
        Identities moved=g;moved.texture[0]=0x19999;assert(NorthlightCelestialGlare::claim(g,moved,0x18000,1)==-1);   // changed record: fail open
    }
    std::puts("PASS F1b: exposed->raw identity map (sun, moon, moon02, glares); suppressed draws stay owned (late disc kept, also after a mid-frame identity change), stale exposed values map to 0 (no address collision), observed native draws are not doubled; glare only for owned bodies; unmapped/changed identities fail open");
}
