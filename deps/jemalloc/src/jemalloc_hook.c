#if defined(_LIB) && defined(_MSC_VER)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>
#include <locale.h>
#include "jemalloc.h"

// This is an unused but exported symbol that we can use to tell the
// MSVC linker to bring in jemalloc, via the /INCLUDE linker flag.
// This function exports the symbol "__jemalloc".
#ifndef _WIN64
void _jemalloc() { }
#else
void __jemalloc() { }
#endif

#define BYTES_TO_STORE 5
#define IAX86_NEARJMP_OPCODE 0xe9
#define MakeIAX86Offset(to,from) ((unsigned)((char*)(to)-(char*)(from)) - BYTES_TO_STORE)
#define PATCH_SIZE(n) ( sizeof(n)/sizeof(n[0]) )

#ifndef _DEBUG

static const char *je_crt_libnames[] = { 
//"MSVCR71.DLL", 
//"MSVCR80.DLL", 
//"MSVCR90.DLL", 
 "MSVCR100.DLL",
 0
};

#else

static const char *je_crt_libnames[] = { 
 0
};

#endif

typedef struct _PATCH
{
  const char *import;  // import name of patch routine
  FARPROC replacement;  // pointer to replacement function
  FARPROC original;  // pointer to original function
  unsigned char codebytes[BYTES_TO_STORE];  // original code storage
  HMODULE module;
} PATCH;

BOOL malloc_init_hard(void);
size_t je_msize(const void *ptr);
void* je_expand(void *ptr, size_t newsize);

static void je_exit(int code);
static void je__exit(int code);
static _onexit_t je_onexit(_onexit_t);
void je_patch_in(PATCH *patch, int count);
static void je_patch_out(PATCH *patch, int count);
static void je_patch_it(PATCH *patch);
static void je_unpatch_it(PATCH *patch);

static PATCH exit_patches[] = 
{

  // exit routines

  {"exit",            (FARPROC) je_exit,      0, 0},
  {"_exit",           (FARPROC) je__exit,     0, 0},
  {"_onexit",         (FARPROC) je_onexit,    0, 0}
};

static PATCH alloc_patches[] = 
{

  // operator new, new[], delete, delete[].

#ifdef _WIN64

  {"??2@YAPEAX_K@Z",  (FARPROC) je_malloc,    0, 0},
  {"??_U@YAPEAX_K@Z", (FARPROC) je_malloc,    0, 0},

#else

  {"??2@YAPAXI@Z",    (FARPROC) je_malloc,    0, 0},
  {"??_U@YAPAXI@Z",   (FARPROC) je_malloc,    0, 0},

#endif

  // comment from tcmalloc

  // Ideally we should patch the nothrow versions of new/delete, but
  // at least in msvcrt, nothrow-new machine-code is of a type we
  // can't patch.  Since these are relatively rare, I'm hoping it's ok
  // not to patch them.

  // the nothrow variants new, new[].

  //{"??2@YAPAXIABUnothrow_t@std@@@Z",  (FARPROC) je_malloc, 0, 0},
  //{"??_U@YAPAXIABUnothrow_t@std@@@Z", (FARPROC) je_malloc, 0, 0},

  {"malloc",          (FARPROC) je_malloc,   0, 0},
  {"calloc",          (FARPROC) je_calloc,   0, 0},
  {"_calloc_crt",     (FARPROC) je_calloc,   0, 0},
  {"valloc",          (FARPROC) je_valloc,   0, 0},
  {"realloc",         (FARPROC) je_realloc,  0, 0},
  {"_recalloc",       (FARPROC) je_recalloc, 0, 0},
  {"_expand",         (FARPROC) je_expand,   0, 0},
  {"_msize",          (FARPROC) je_msize,    0, 0},
  {"strndup",         (FARPROC) je_strndup,  0, 0},
  {"strdup",          (FARPROC) je_strdup,   0, 0},
};

static PATCH free_patches[] = 
{

  {"free",	(FARPROC) je_free,         0, 0},

  // operator new, new[], delete, delete[].

#ifdef _WIN64

  {"??3@YAXPEAX@Z",   (FARPROC) je_free,      0, 0},
  {"??_V@YAXPEAX@Z",  (FARPROC) je_free,      0, 0},

#else

  {"??3@YAXPAX@Z",    (FARPROC) je_free,      0, 0},
  {"??_V@YAXPAX@Z",   (FARPROC) je_free,      0, 0},

#endif

  // comment from tcmalloc

  // Ideally we should patch the nothrow versions of new/delete, but
  // at least in msvcrt, nothrow-new machine-code is of a type we
  // can't patch.  Since these are relatively rare, I'm hoping it's ok
  // not to patch them.

  // the nothrow variants delete, delete[].

  //{"??3@YAXPAXABUnothrow_t@std@@@Z",  (FARPROC) je_free, 0, 0},
  //{"??_V@YAXPAXABUnothrow_t@std@@@Z", (FARPROC) je_free, 0, 0},
};

// Intercept the exit functions
#define JE_MAX_EXIT_FUNCTIONS 255
static int exitCount = 0;
//static TheLockType exitLock;
typedef void (*exit_t)(int status);
static _onexit_t _onexit_buf[JE_MAX_EXIT_FUNCTIONS];

BOOL je_ctrl_handler( DWORD fdwCtrlType )
{ 
  switch( fdwCtrlType )
  { 
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
      return __je_term();
    case CTRL_BREAK_EVENT:
      return FALSE;
    case CTRL_LOGOFF_EVENT:
      return FALSE;
    case CTRL_SHUTDOWN_EVENT:
      return FALSE;

    default:
      return FALSE;
  } 
} 

int __cdecl __je_entry(void) {
  SetConsoleCtrlHandler((PHANDLER_ROUTINE) je_ctrl_handler, TRUE);
  malloc_init_hard();
  je_patch_in(exit_patches, PATCH_SIZE(exit_patches));
  je_patch_in(alloc_patches, PATCH_SIZE(alloc_patches));
  je_patch_in(free_patches, PATCH_SIZE(free_patches));
  setlocale(LC_ALL, 0);
  return 0;
}

int __cdecl __je_term(void) {
  je_patch_out(free_patches, PATCH_SIZE(free_patches));
  je_patch_out(alloc_patches, PATCH_SIZE(alloc_patches));
  je_patch_out(exit_patches, PATCH_SIZE(exit_patches));
  return 0;
}

#ifndef _WIN64
#pragma comment(linker, "/INCLUDE:_p_cb_je_init")
#pragma comment(linker, "/INCLUDE:_p_cb_je_term")
#else
#pragma comment(linker, "/INCLUDE:p_cb_je_init")
#pragma comment(linker, "/INCLUDE:p_cb_je_term")
#endif

// This tells the linker to run these functions.
#pragma data_seg(push, old_seg)

#pragma data_seg(".CRT$XCY")
int (__cdecl *p_cb_je_init)(void) = __je_entry;

#pragma data_seg(".CRT$XTU")
int (*p_cb_je_term)(void) = __je_term;

#pragma data_seg(pop, old_seg)

void je_exit(int code) {
  exit_t fp;
  {
    //HL::Guard<TheLockType> g (exitLock);
    while (exitCount > 0) {
      exitCount--;
      (_onexit_buf[exitCount])();
    }
    je_unpatch_it(&exit_patches[0]);
    //__je_term();
    fp = (exit_t)exit_patches[0].original;
  }
  (*fp)(code);
}

void je__exit(int code) {
  exit_t fp;
  {
    //HL::Guard<TheLockType> g (exitLock);
    //je_unpatch_it(&exit_patches[1]);
    //__je_term();
    fp = (exit_t)exit_patches[1].original;
  }
  (*fp)(code);
}

_onexit_t je_onexit(_onexit_t function) {
  if (function) {
    //HL::Guard<TheLockType> g (exitLock);
    if (exitCount < JE_MAX_EXIT_FUNCTIONS) {
      _onexit_buf[exitCount] = function;
      exitCount++;
    }
    else
      function = 0;
  }
  return function;
}

void je_patch_it(PATCH *patch)
{
  unsigned char *patchloc;

  // Change rights on CRT Library module to execute/read/write.

  MEMORY_BASIC_INFORMATION mbi_thunk;
  VirtualQuery((void*)patch->original, &mbi_thunk, 
    sizeof(MEMORY_BASIC_INFORMATION));
  VirtualProtect(mbi_thunk.BaseAddress, mbi_thunk.RegionSize, 
    PAGE_EXECUTE_READWRITE, &mbi_thunk.Protect);

  // Patch CRT library original routine
  // save original code bytes for exit restoration
  // write jmp <patch_routine> (at least 5 bytes long) to original
  CopyMemory(patch->codebytes, patch->original, sizeof(patch->codebytes));
  patchloc = (unsigned char*)patch->original;
  *patchloc++ = IAX86_NEARJMP_OPCODE;
  *(unsigned*)patchloc = MakeIAX86Offset(patch->replacement, patch->original);

  // Reset CRT library code to original page protection
  VirtualProtect(mbi_thunk.BaseAddress, mbi_thunk.RegionSize, 
    mbi_thunk.Protect, &mbi_thunk.Protect);
}

void je_unpatch_it(PATCH *patch)
{
  if(patch->import) {
    // Change rights on CRT Library module to execute/read/write
    MEMORY_BASIC_INFORMATION mbi_thunk;
    VirtualQuery((void*)patch->original, &mbi_thunk, 
      sizeof(MEMORY_BASIC_INFORMATION));
    VirtualProtect(mbi_thunk.BaseAddress, mbi_thunk.RegionSize, 
      PAGE_EXECUTE_READWRITE, &mbi_thunk.Protect);

    // Patch CRT library original routine
    // save original code bytes for exit restoration
    // write jmp <patch_routine> (at least 5 bytes long) to original
    CopyMemory(patch->original, patch->codebytes, sizeof(patch->codebytes));

    // Reset CRT library code to original page protection
    VirtualProtect(mbi_thunk.BaseAddress, mbi_thunk.RegionSize, 
      mbi_thunk.Protect, &mbi_thunk.Protect);

    // Mark as unpatched
    patch->import = 0;
  }
}

void je_patch_in(PATCH *patch, int count) {
  int i, j;
  HMODULE module;

  for (i = 0; je_crt_libnames[i]; i++) {
    module = GetModuleHandle(je_crt_libnames[i]);
    if (module) {
      for (j = 0; j < count; j++, patch++) {
        if (patch->original = GetProcAddress(module, patch->import)) {
          patch->module = module;
          je_patch_it(patch);
        }
      }
    }
  }
}

void je_patch_out(PATCH *patch, int count) {
  int j;
  for (j = 0; j < count; j++, patch++) {
    je_unpatch_it(patch);
  }
}

#endif
