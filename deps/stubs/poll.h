// stub for <poll.h>

#ifdef _MSC_VER
#pragma once
#endif

#ifdef _WIN32

inline void poll( int, int, int sleep_ms ) {
  Sleep( sleep_ms );
}

#endif