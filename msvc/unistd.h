#ifndef ORICUTRON_UNISTD_H
#define ORICUTRON_UNISTD_H

#ifdef _MSC_VER
// Begin unistd.h
#include <io.h>
#include <process.h>
//#include <getopt.h>
// End unistd.h

#include <direct.h>     // Using _getcwd
#ifndef getcwd
#define getcwd _getcwd
#endif
#endif // MSC_VER

#include "strcasecmp.h"

#endif /* ORICUTRON_UNISTD_H */
