 #include <string.h> // memmem

#include "common.h"
#include "symbols.h"

#ifndef LIBJAKE_PATCHFINDER
#define LIBJAKE_PATCHFINDER
#define libjake_find_str(syms,str) ((uint64_t)memmem(syms->map,syms->mapsize,str,sizeof(str))-((uint64_t)syms->map)+LIBJAKE_KERNEL_BASE)
uint64_t find_xref(jake_symbols_t syms,uint64_t start_addr,uint64_t xref_address);
uint64_t find_trustcache(jake_symbols_t syms);
uint64_t find_swapprefix(jake_symbols_t syms);
uint64_t find_realhost(jake_symbols_t syms);
uint64_t find_zonemap(jake_symbols_t syms);
#define P_LOG_DBG(fmt, args ...) LOG(fmt,##args)
#endif
