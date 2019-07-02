#include <string.h> // memmem
#include <stdlib.h> // exit

#include "common.h"
#include "symbols.h"

#ifndef LIBJAKE_PATCHFINDER
#define LIBJAKE_PATCHFINDER
#define FAILED_TO_FIND() exit(1)
uint64_t libjake_find_str(jake_symbols_t syms, char * str);
uint64_t find_xref(jake_symbols_t syms,uint64_t start_addr,uint64_t xref_address);
uint64_t find_trustcache(jake_symbols_t syms);
uint64_t find_swapprefix(jake_symbols_t syms);
uint64_t find_realhost(jake_symbols_t syms);
uint64_t find_zonemap(jake_symbols_t syms);
#define P_LOG_DBG(fmt, args ...) LOG(fmt,##args)
#endif
