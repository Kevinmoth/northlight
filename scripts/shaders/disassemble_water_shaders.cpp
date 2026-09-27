// Offline bytecode inspection only: loads d3dcompiler, never D3D9 or a device.
// 0.3.161: the mask variants are not embedded any more; validate_water_shaders.py
// writes them (patch() of the tester's client originals) as <hash>.bin into
// argv[1], and each is disassembled to <hash>.asm in argv[2].
#include <windows.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <string>
#include <vector>
int main(int argc,char**argv){
    if(argc!=3)return 2;HMODULE module=LoadLibraryA("d3dcompiler_47.dll");if(!module)return 3;
    using Function=HRESULT(WINAPI*)(const void*,SIZE_T,UINT,const char*,ID3DBlob**);
    auto disassemble=reinterpret_cast<Function>(GetProcAddress(module,"D3DDisassemble"));if(!disassemble)return 4;
    WIN32_FIND_DATAA found;HANDLE search=FindFirstFileA((std::string(argv[1])+"\\*.bin").c_str(),&found);if(search==INVALID_HANDLE_VALUE)return 7;
    unsigned count=0;
    do{
        std::string name=found.cFileName,stem=name.substr(0,name.size()-4);
        FILE*in=std::fopen((std::string(argv[1])+"\\"+name).c_str(),"rb");if(!in)return 8;
        std::vector<unsigned char> code;unsigned char buffer[4096];size_t n;while((n=std::fread(buffer,1,sizeof buffer,in))>0)code.insert(code.end(),buffer,buffer+n);std::fclose(in);
        ID3DBlob* output=nullptr;HRESULT hr=disassemble(code.data(),code.size(),0,nullptr,&output);if(FAILED(hr)||!output){std::fprintf(stderr,"Rejected %s hr=%08lx\n",stem.c_str(),(unsigned long)hr);return 5;}
        FILE*f=std::fopen((std::string(argv[2])+"\\"+stem+".asm").c_str(),"wb");if(!f)return 6;std::fwrite(output->GetBufferPointer(),1,output->GetBufferSize(),f);std::fclose(f);output->Release();++count;
    }while(FindNextFileA(search,&found));
    FindClose(search);
    std::printf("Disassembled %u exact liquid mask shaders; no graphics device created.\n",count);FreeLibrary(module);return 0;
}
