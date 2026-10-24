// Minimal headless check: load the .vst3 DLL, call InitDll + GetPluginFactory, list classes.
// Does not instantiate audio; host-level validation still has to happen in a DAW.
#include <windows.h>
#include <cstdio>

struct PFactoryInfo { char vendor[64]; char url[256]; char email[128]; int flags; };
struct PClassInfo  { unsigned char cid[16]; int cardinality; char category[32]; char name[64]; };
struct IPluginFactory {  // vtable layout of the VST3 IPluginFactory interface (FUnknown + 4 methods)
    virtual long __stdcall queryInterface (const void*, void**) = 0;
    virtual unsigned long __stdcall addRef() = 0;
    virtual unsigned long __stdcall release() = 0;
    virtual long __stdcall getFactoryInfo (PFactoryInfo*) = 0;
    virtual int  __stdcall countClasses() = 0;
    virtual long __stdcall getClassInfo (int, PClassInfo*) = 0;
};

int wmain (int argc, wchar_t** argv)
{
    if (argc < 2) { std::printf ("usage: smoke <path.vst3 dll>\n"); return 2; }
    HMODULE m = LoadLibraryW (argv[1]);
    if (! m) { std::printf ("FAIL LoadLibrary err=%lu\n", GetLastError()); return 1; }
    auto init = (bool (*)()) GetProcAddress (m, "InitDll");
    auto getFactory = (IPluginFactory* (*)()) GetProcAddress (m, "GetPluginFactory");
    if (! init || ! getFactory || ! init()) { std::printf ("FAIL exports/InitDll\n"); return 1; }
    IPluginFactory* f = getFactory();
    if (! f) { std::printf ("FAIL GetPluginFactory null\n"); return 1; }
    PFactoryInfo fi {}; f->getFactoryInfo (&fi);
    std::printf ("vendor=%s classes=%d\n", fi.vendor, f->countClasses());
    for (int i = 0; i < f->countClasses(); ++i) { PClassInfo ci {}; f->getClassInfo (i, &ci); std::printf ("  [%s] %s\n", ci.category, ci.name); }
    f->release();
    auto exitDll = (bool (*)()) GetProcAddress (m, "ExitDll"); if (exitDll) exitDll();
    std::printf ("OK\n");
    return 0;
}
