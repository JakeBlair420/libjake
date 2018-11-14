#include <mach-o/dyld.h>

typedef struct {
    const char *path;
    int filedesc;
    size_t filesize;
    const void *map;

    struct mach_header *mach_header;
    struct symtab_command *symtab_cmd;
    struct jake_symtab_t symtab;
} *jake_symbols_t;

typedef struct {
    int msyms;
    struct nlist *symbols;
} *jake_symtab_t;

int jake_init_symbols(jake_symbols_t syms, const char *path);
int jake_discard_symbols(jake_symbols_t syms);

int find_symtab(jake_symbols_t syms);
