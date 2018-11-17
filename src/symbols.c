#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <mach-o/nlist.h>

#include <sys/stat.h>
#include <sys/mman.h>

#include "common.h"
#include "symbols.h"

int jake_init_symbols(jake_symbols_t syms, const char *path)
{
    if (access(path, F_OK) != 0)
    {
        return -1;
    }

    syms->path = path;
    
    struct stat s = { 0 };
    if (stat(path, &s))
    {
        return -1;
    }

    syms->filesize = s.st_size;
    
    syms->filedesc = open(syms->path, O_RDONLY);
    if (syms->filedesc < 0)
    {
        return -1;
    }

    syms->map = mmap(NULL, s.st_size, PROT_READ, MAP_PRIVATE, syms->filedesc, 0);

    if (syms->map == MAP_FAILED)
    {
        jake_discard_symbols(syms);

        return -1;
    }

    syms->mach_header = (struct mach_header *)syms->map;

    if (find_symtab(syms) != 0)
    {
        jake_discard_symbols(syms);

        return -1;
    }

    return 0;
}

int jake_discard_symbols(jake_symbols_t syms)
{
    if (syms->map != NULL &&
        syms->map != MAP_FAILED)
    {
        munmap((void *)syms->map, syms->filesize);
    }
    
    if (syms->filedesc >= 0)
    {
        close(syms->filedesc);
    }

    return 0;
}

int find_symtab(jake_symbols_t syms)
{
    bool is_64, is_swap = false;
    int header_size = 0, nlist_size = 0;

    uint32_t magic = syms->mach_header->magic;

    if (magic == MH_CIGAM ||
        magic == MH_CIGAM_64)
    {
        magic = ntohl(magic);

        is_swap = true;
    }
    
    switch (magic)
    {
        case MH_MAGIC_64:
            header_size = sizeof(struct mach_header_64);
            nlist_size = sizeof(struct nlist_64);
            is_64 = true;
            break;

        case MH_MAGIC:
            header_size = sizeof(struct mach_header);
            nlist_size = sizeof(struct nlist);
            is_64 = false;
            break;

        default:
            LOG("Unknown magic! %x", magic);
            return -1;
    }

    struct load_command *cmd = (struct load_command *)((uintptr_t)syms->map + header_size);

    for (int i = 0; i < syms->mach_header->ncmds; i++)
    {
        if (cmd->cmd == LC_SYMTAB)
        {
            syms->symtab_cmd = (struct symtab_command *)cmd;
            break;
        }

        cmd = (struct load_command *)((uintptr_t)cmd + cmd->cmdsize);
    }

    if (syms->symtab_cmd == NULL)
    {
        LOG("Failed to find symtab command.");
        return -1;
    }

    syms->symtab = malloc(sizeof(jake_symtab));

    syms->symtab->nsyms = syms->symtab_cmd->nsyms;
    syms->symtab->symbols = malloc(syms->symtab->nsyms * sizeof(nlist_global));

    for (int i = 0; i < syms->symtab->nsyms; i++)
    {
        nlist_global *sym = &syms->symtab->symbols[i];
        struct nlist *item = (struct nlist *)(syms->map + syms->symtab_cmd->symoff + (i * nlist_size));

        sym->n_name = (char *)(syms->map + syms->symtab_cmd->stroff + item->n_un.n_strx);
        sym->n_type = item->n_type;
        sym->n_sect = item->n_sect;
        sym->n_desc = item->n_desc;

        sym->n_value = is_64 ? *(uint64_t *)&item->n_value : item->n_value;
    }

    return 0;
}

uint64_t find_symbol(jake_symbols_t syms, const char *name)
{
    if (syms->symtab == NULL)
    {
        return 0x0;
    }

    if (syms->symtab->nsyms == 0)
    {
        return 0x0;
    }

    if (syms->symtab->symbols == NULL)
    {
        return 0x0;
    }

    for (int i = 0; i < syms->symtab->nsyms; i++)
    {
        nlist_global *sym = &syms->symtab->symbols[i];

        if (strcmp(sym->n_name, name) == 0)
        {
            return sym->n_value;
        }
    }

    return 0x0;
}
