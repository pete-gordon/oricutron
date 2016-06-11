#ifdef _MSC_VER
// Begin unistd.h
#include <io.h>
#include <process.h>
//#include <getopt.h>
// End unistd.h

#include <direct.h>     // Using _getcwd
#define getcwd _getcwd
#endif

//#include <stdio.h>

#include "strcasecmp.h"
