#include "extension_guard.h"
#include <cassert>
#include <stdexcept>
int main(){
    using namespace NorthlightExtensionGuard;
    int restored=0,forwarded=0;Fault observed=Fault::None;
    struct Restore {int& count;~Restore(){++count;}};
    auto report=[&](Fault f) noexcept {observed=f;};
    for(int mode=0;mode<4;++mode){
        observed=Fault::None;
        bool ok=run([&]{Restore cleanup{restored};if(mode==1)throw std::bad_alloc();if(mode==2)throw std::runtime_error("test");if(mode==3)throw 1;},report);
        ++forwarded;
        assert(ok==(mode==0));assert(int(observed)==mode);
        assert(restored==mode+1&&forwarded==mode+1);
    }
}
