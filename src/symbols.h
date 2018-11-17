#include <mach-o/dyld.h>

typedef struct jake_symtab* jake_symtab_t;
typedef struct jake_symbols* jake_symbols_t;

typedef struct jake_symbols {
    const char *path;
    int filedesc;
    size_t filesize;
    const void *map;

    struct mach_header *mach_header;
    struct symtab_command *symtab_cmd;
    jake_symtab_t symtab;
} jake_symbols;

typedef struct jake_symtab {
    int nsyms;
    struct nlist_global *symbols;
} jake_symtab;

typedef struct nlist_global {
    char *n_name;
    uint8_t n_type;
    uint8_t n_sect;
    int16_t n_desc;
    uint64_t n_value;
} nlist_global;

int jake_init_symbols(jake_symbols_t syms, const char *path);
int jake_discard_symbols(jake_symbols_t syms);

int find_symtab(jake_symbols_t syms);
uint64_t find_symbol(jake_symbols_t syms, const char *name);
