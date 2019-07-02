#ifndef IMG_H
#define IMG_H

#include <mach-o/dyld.h>
#include <vfs.h>

typedef struct jake_img* jake_img_t;
typedef struct jake_symtab* jake_symtab_t;

typedef struct jake_img {
    const char *path;
    FHANDLE filedesc;
    size_t filesize;
    const void *map;
	size_t mapsize;

    struct mach_header *mach_header;
    struct symtab_command *symtab_cmd;
    jake_symtab_t symtab;
} jake_img;

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

int jake_init_image(jake_img_t img, const char *path);
int jake_discard_image(jake_img_t img);

bool jake_is_swap_img(jake_img_t img);
bool jake_is_64bit_img(jake_img_t img);

struct load_command **jake_find_load_cmds(jake_img_t img, int command);
struct load_command *jake_find_load_cmd(jake_img_t img, int command);

int jake_find_symtab(jake_img_t img);
uint64_t jake_find_symbol(jake_img_t img, const char *name);

uint64_t jake_fileoff_to_vaddr(jake_img_t img, uint64_t fileoff);
uint64_t jake_vaddr_to_fileoff(jake_img_t img, uint64_t vaddr);

#endif
