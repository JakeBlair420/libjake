
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <mach-o/nlist.h>

#include <sys/mman.h>
#include <sys/stat.h>

#include "common.h"
#include "img.h"

int jake_init_image(jake_img_t img, const char *path)
{
    if (access(path, F_OK) != 0)
    {
        return -1;
    }

    img->path = path;

    struct stat s = { 0 };
    if (stat(path, &s))
    {
        return -1;
    }

    img->filesize = s.st_size;

    img->filedesc = img4_reopen(file_open(img->path, O_RDONLY),NULL,0);
    if (img->filedesc == 0)
    {
        return -1;
    }

	int ret = img->filedesc->ioctl(img->filedesc,IOCTL_MEM_GET_DATAPTR,&img->map,&img->mapsize);

    if (ret != 0)
    {
        jake_discard_image(img);

        return -1;
    }

    img->mach_header = (struct mach_header *)img->map;

    /* might fail */
    jake_find_symtab(img);

    return 0;
}

int jake_discard_image(jake_img_t img)
{
    if (img->filedesc >= 0)
    {
		img->filedesc->close(img->filedesc);
    }

    return 0;
}

bool jake_is_swap_img(jake_img_t img)
{
    uint32_t magic = img->mach_header->magic;

    return (magic == MH_CIGAM) || (magic == MH_CIGAM_64);
}

bool jake_is_64bit_img(jake_img_t img)
{
    uint32_t magic = img->mach_header->magic;

    if (jake_is_swap_img(img))
    {
        magic = ntohl(magic);
    }

    return magic == MH_MAGIC_64;
}

struct load_command **jake_find_load_cmds(jake_img_t img, int command)
{
    int header_size = 0;

    bool is_swap = jake_is_swap_img(img);

    uint32_t magic = img->mach_header->magic;

    if (is_swap)
    {
        magic = ntohl(magic);
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
            LOG("Unknown magic! %x", magic);
            return NULL;
    }

    int matching_cmd_count = 0;

    struct load_command *cmd = (struct load_command *)((uintptr_t)img->map + header_size);

    for (int i = 0; i < img->mach_header->ncmds; i++)
    {
        if (cmd->cmd == command)
        {
            matching_cmd_count++;
        }

        cmd = (struct load_command *)((uintptr_t)cmd + cmd->cmdsize);
    }

    /* no matching commands */
    if (matching_cmd_count == 0)
    {
        return NULL;
    }
    ++matching_cmd_count;
    /* holds array of pointers to load commands */
    struct load_command **lc_array = (struct load_command **)malloc(matching_cmd_count * sizeof(struct load_command *));

    /* reset back to start */
    cmd = (struct load_command *)((uintptr_t)img->map + header_size);

    int curr_cmd_index = 0;
    for (int i = 0; i < img->mach_header->ncmds; i++)
    {
        if (cmd->cmd == command)
        {
            lc_array[curr_cmd_index++] = cmd;
        }

        cmd = (struct load_command *)((uintptr_t)cmd + cmd->cmdsize);
    }
    lc_array[curr_cmd_index++] = NULL;
    return lc_array;
}

struct load_command *jake_find_load_cmd(jake_img_t img, int command)
{
    /* we'll find all matching load command and return the first match (if present) */
    struct load_command **lc_array = jake_find_load_cmds(img, command);

    if (lc_array == NULL)
    {
        return NULL;
    }

    struct load_command *load_cmd = lc_array[0];

    free(lc_array);

    return load_cmd;
}

int jake_find_symtab(jake_img_t img)
{
    struct load_command *symtab_cmd = jake_find_load_cmd(img, LC_SYMTAB);

    bool is_64 = jake_is_64bit_img(img);

    int nlist_size = is_64 ? sizeof(struct nlist_64) : sizeof(struct nlist);

    img->symtab_cmd = (struct symtab_command *)symtab_cmd;

    if (img->symtab_cmd == NULL)
    {
        LOG("Failed to find symtab command.");
        return -1;
    }

    img->symtab = malloc(sizeof(jake_symtab));

    img->symtab->nsyms = img->symtab_cmd->nsyms;
    img->symtab->symbols = malloc(img->symtab->nsyms * sizeof(nlist_global));

    for (int i = 0; i < img->symtab->nsyms; i++)
    {
        nlist_global *sym = &img->symtab->symbols[i];
        struct nlist *item = (struct nlist *)(img->map + img->symtab_cmd->symoff + (i * nlist_size));

        sym->n_name = (char *)(img->map + img->symtab_cmd->stroff + item->n_un.n_strx);
        sym->n_type = item->n_type;
        sym->n_sect = item->n_sect;
        sym->n_desc = item->n_desc;

        sym->n_value = is_64 ? *(uint64_t *)&item->n_value : item->n_value;
    }

    return 0;
}

uint64_t jake_find_symbol(jake_img_t img, const char *name)
{
    if (img->symtab == NULL)
    {
        return 0x0;
    }

    if (img->symtab->nsyms == 0)
    {
        return 0x0;
    }

    if (img->symtab->symbols == NULL)
    {
        return 0x0;
    }

    for (int i = 0; i < img->symtab->nsyms; i++)
    {
        nlist_global *sym = &img->symtab->symbols[i];

        if (strcmp(sym->n_name, name) == 0)
        {
            return sym->n_value;
        }
    }

    return 0x0;
}

uint64_t jake_fileoff_to_vaddr(jake_img_t img, uint64_t fileoff)
{
    uint64_t found_value = 0x0;
    struct load_command **seg_array    = jake_find_load_cmds(img, LC_SEGMENT   );
    struct load_command **seg_array_64 = jake_find_load_cmds(img, LC_SEGMENT_64);
	struct load_command **iter = seg_array;

    /* lookup in LC_SEGMENT's first */
    if (seg_array != NULL)
    {
        for (struct load_command *cmd = *iter++; cmd; cmd = *iter++)
        {
            struct segment_command *seg = (struct segment_command *)cmd;

            if ((fileoff >= seg->fileoff) &&
                (fileoff < (seg->fileoff + seg->filesize)))
            {
                found_value = seg->vmaddr + (fileoff - seg->fileoff);
                goto out;
            }
        }
    }

    /* lookup in LC_SEGMENT_64's */
    if (seg_array_64 != NULL)
    {
		iter = seg_array_64;
        for (struct load_command *cmd = *iter; cmd; cmd = *iter++)
        {
            struct segment_command_64 *seg = (struct segment_command_64 *)cmd;

            if ((fileoff >= seg->fileoff) &&
				(fileoff < (seg->fileoff + seg->filesize)))
            {
                found_value = seg->vmaddr + (fileoff - seg->fileoff);
                goto out;
            }
        }
    }

out:;

    if (seg_array)
    {
        free(seg_array);
    }

    if (seg_array_64)
    {
        free(seg_array_64);
    }

    return found_value;
}

uint64_t jake_vaddr_to_fileoff(jake_img_t img, uint64_t vaddr)
{
	uint64_t found_value = 0x0;
    struct load_command **seg_array    = jake_find_load_cmds(img, LC_SEGMENT   );
    struct load_command **seg_array_64 = jake_find_load_cmds(img, LC_SEGMENT_64);
	struct load_command **iter = seg_array;

    /* lookup in LC_SEGMENT's first */
    if (seg_array != NULL)
    {
        for (struct load_command *cmd = *iter++; cmd; cmd = *iter++)
        {
            struct segment_command *seg = (struct segment_command *)cmd;

            if ((vaddr >= seg->vmaddr) &&
                (vaddr < (seg->vmaddr + seg->filesize)))
            {
                found_value = seg->fileoff + (vaddr - seg->vmaddr);
				goto out;
            }
        }
    }

    /* lookup in LC_SEGMENT_64's */
    if (seg_array_64 != NULL)
    {
		iter = seg_array_64;
        for (struct load_command *cmd = *iter; cmd; cmd = *iter++)
        {
            struct segment_command_64 *seg = (struct segment_command_64 *)cmd;

            if ((vaddr >= seg->vmaddr) &&
                (vaddr < (seg->vmaddr + seg->filesize)))
            {
                found_value = seg->fileoff + (vaddr - seg->vmaddr);
				goto out;
            }
        }
    }

out:;
	if (seg_array) {
		free(seg_array);
	}
	if (seg_array_64) {
		free(seg_array_64);
	}
    return found_value;
}
