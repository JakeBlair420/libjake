#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/mman.h>

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
    bool is_swap = false;
    int header_size = 0;

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
            break;

        case MH_MAGIC:
            header_size = sizeof(struct mach_header);
            break;

        default:
            fprintf(stderr, "[libjake] Unknown magic! %x\n", magic);
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
        fprintf(stderr, "[libjake] Failed to find symtab command.\n");
        return -1;
    }



    return 0;
}
