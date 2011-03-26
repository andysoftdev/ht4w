// -*- C++ -*-
// Windows TLS functions.

DWORD LocalTLABIndex;

#include <new>

static TheCustomHeapType * initializeCustomHeap (void)
{
  // Allocate a per-thread heap.
  TheCustomHeapType * heap;
  size_t sz = sizeof(TheCustomHeapType) + sizeof(double);
  void * mh = getMainHoardHeap()->malloc(sz);
  heap = new ((char *) mh) TheCustomHeapType (getMainHoardHeap());

  // Store it in the appropriate thread-local area.
  TlsSetValue (LocalTLABIndex, heap);

  return heap;
}

inline TheCustomHeapType * getCustomHeap (void) {
  TheCustomHeapType * heap;
  heap = (TheCustomHeapType *) TlsGetValue (LocalTLABIndex);
  if (heap == NULL)  {
    heap = initializeCustomHeap();
  }
  return heap;
}

#ifndef HOARD_PRE_ACTION
#define HOARD_PRE_ACTION
#endif

#ifndef HOARD_POST_ACTION
#define HOARD_POST_ACTION
#endif

#ifndef CUSTOM_DLLNAME
#ifndef _LIB
#define CUSTOM_DLLNAME DllMain
#else
#define CUSTOM_DLLNAME HoardDllMain
#endif
#endif


//
// Intercept thread creation and destruction to flush the TLABs.
//

extern "C" {

  BOOL WINAPI CUSTOM_DLLNAME (HANDLE hinstDLL, DWORD fdwReason, LPVOID lpreserved)
  {
    static int np = HL::CPUInfo::computeNumProcessors();

    switch (fdwReason) {
      
    case DLL_PROCESS_ATTACH:
      {
	LocalTLABIndex = TlsAlloc();
	if (LocalTLABIndex == TLS_OUT_OF_INDEXES) {
	  // Not sure what to do here!
	}
	HOARD_PRE_ACTION;
	getCustomHeap();
      }
      break;
      
    case DLL_THREAD_ATTACH:
      if (np == 1) {
	// We have exactly one processor - just assign the thread to
	// heap 0.
	getMainHoardHeap()->chooseZero();
      } else {
	getMainHoardHeap()->findUnusedHeap();
      }
      getCustomHeap();
      break;
      
    case DLL_THREAD_DETACH:
      {
	// Dump the memory from the TLAB.
	getCustomHeap()->clear();
	
	TheCustomHeapType *heap
	  = (TheCustomHeapType *) TlsGetValue(LocalTLABIndex);
	
	if (np != 1) {
	  // If we're on a multiprocessor box, relinquish the heap
	  // assigned to this thread.
	  getMainHoardHeap()->releaseHeap();
	}
	
	if (heap != 0) {
	  TlsSetValue (LocalTLABIndex, 0);
	}
      }
      break;
      
    case DLL_PROCESS_DETACH:
      HOARD_POST_ACTION;
      break;
      
    default:
      return TRUE;
    }

    return TRUE;
  }

  int HoardProcessTerminates() {
    HOARD_POST_ACTION;
    return 0;
  }
}

#if defined(_LIB) && defined(_MSC_VER)

#pragma comment(linker, "/INCLUDE:__tls_used")
#pragma comment(linker, "/INCLUDE:_p_cb_hoard")
#pragma comment(linker, "/INCLUDE:_p_cb_process_term_hoard")

// extern "C" suppresses C++ name mangling so we know the symbol names
// for the linker /INCLUDE:symbol pragmas above.

extern "C" {

// This tells the linker to run these functions.
#pragma data_seg(push, old_seg)

#pragma data_seg(".CRT$XLB")
BOOL (NTAPI *p_cb_hoard)(HANDLE h, DWORD dwReason, PVOID pv) = CUSTOM_DLLNAME;
#pragma data_seg(".CRT$XTU")
int (*p_cb_process_term_hoard)(void) = HoardProcessTerminates;
#pragma data_seg(pop, old_seg)

}  // extern "C"

#endif
