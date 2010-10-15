// stub for <readline/readline.h>

#pragma once

#ifdef _WIN32

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

inline char *readline(const char *prompt) {
	const int buflen = 0x1000;
	printf("%s", prompt);
	char *p = (char*)malloc(buflen);
	return gets_s(p, buflen);
}

#ifdef __cplusplus
}
#endif

#endif

