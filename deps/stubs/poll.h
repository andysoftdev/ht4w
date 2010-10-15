// stub for <poll.h>

#pragma once

inline void poll( int, int, int sleep_ms ) {
	::Sleep( sleep_ms );
}