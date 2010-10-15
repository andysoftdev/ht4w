// stub for <sys/uio.h>

#pragma once

#include <io.h>

#ifdef __cplusplus
extern "C" {
#endif

struct iovec {
	void *iov_base;
	size_t iov_len;
};

#ifdef __cplusplus
}
#endif
