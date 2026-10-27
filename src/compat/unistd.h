#ifndef SF_UNISTD_H
#define SF_UNISTD_H

#ifdef _WIN32
#include <io.h>
#include <process.h>
#ifndef unlink
#define unlink _unlink
#endif
#else
#include <unistd.h>
#endif

#endif

