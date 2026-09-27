#pragma once
// Keep the previous session's log (northlight-renderer.prev.log) before the new
// session truncates northlight-renderer.log. Runs once, before the first open.
// Every step is one read/rename/attribute call that never waits on another
// process; any failure only restores the old behaviour (truncate) and can
// never fail DLL load or rendering. Fs is a Win32 adapter in renderer.cpp and
// a fake/POSIX adapter in the native tests.
// 0.3.144: the DLL is also loaded by a short-lived probe instance at every game
// start (it logs the version line but never creates a device), in either order
// with the game. Only a log that recorded a real session (a CreateDevice line)
// is rotated, and a log another live instance still holds is left alone, so a
// probe stub can never replace the real previous session. Nothing is copied or
// deleted.
namespace NorthlightLogRotation {
enum class Result {Missing,Moved,Replaced,Stub,InUse,Failed};
inline const char* name(Result r){
    switch(r){case Result::Missing:return "none";case Result::Moved:return "moved";case Result::Replaced:return "replaced-readonly";
              case Result::Stub:return "kept-previous-stub";case Result::InUse:return "kept-previous-in-use";default:return "failed";}
}
// A log recorded a real session if its first bytes contain a device creation.
inline bool recordedSession(const char* head,unsigned long bytes){
    static const char marker[]="\nCreateDevice ";const unsigned long n=sizeof(marker)-1;
    for(unsigned long i=0;i+n<=bytes;++i){unsigned long k=0;while(k<n&&head[i+k]==marker[k])++k;if(k==n)return true;}
    return false;
}
template<class Fs,class Path> Result rotate(Fs& fs,const Path& current,const Path& previous){
    if(!fs.exists(current))return Result::Missing;
    // A probe instance's stub (or an unreadable log) never replaces the previous session.
    if(!fs.session(current))return Result::Stub;
    // Atomic rename, replacing an older previous log.
    if(fs.move(current,previous))return Result::Moved;
    // A read-only previous log refuses replacement: clear only that attribute, once.
    if(fs.readOnly(previous)&&fs.makeWritable(previous)&&fs.move(current,previous))return Result::Replaced;
    // The current log is held by another live instance (or cannot be moved):
    // keep the previous log as it is; the new session truncates as before.
    return Result::InUse;
}
}
