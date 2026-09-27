// Syntax/ABI gate only. Never instantiate a device or execute this artifact.
#include <windows.h>
#include <d3d9.h>
template<class T>void drop(T*& p){if(p)p->Release();p=nullptr;}
void logf(const char*,...){}
struct SavedState{bool ok=true;explicit SavedState(IDirect3DDevice9*){}};
#include "celestial_disc_renderer.h"
int main(){return 0;}
