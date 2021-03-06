
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
    int ret = 0;

    memset(img,0,sizeof(jake_img)); // make sure we set everything to zero otherwise we might touch stuff in the error path that was uninited
    
    if (access(path, F_OK) != 0)
    {
        LOG("Could not find file '%s'", path);
        goto fail;
    }

    img->path = path;

    struct stat s = { 0 };
    ret = stat(img->path, &s);
    if (ret != 0)
    {
        LOG("Failed to stat file '%s'", img->path);
        goto fail;
    }

    img->filesize = s.st_size;

    if (img->filesize < 100)
    {
        LOG("Filesize too small: %zu bytes", img->filesize);
        goto fail;
    }

    img->filedesc = open(img->path, O_RDONLY);
    if (img->filedesc < 0)
    {
        LOG("Failed to open the image");
        goto fail;
    }

    char file_hdr[100] = { 0 };
    read(img->filedesc, &file_hdr, sizeof(file_hdr));

    uint32_t file_magic = *(uint32_t *)&file_hdr;

    /* TODO: Parse the file as ASN.1 to check if it's IMG4 */
    char *img4_val = strstr((const char *)&file_hdr, "IMG4");

    if (img4_val != NULL) /* Is img4 */
    {
        close(img->filedesc);
        img->filedesc = 0;

        img->filehandle = file_open(img->path, O_RDONLY);
        if (img->filehandle == NULL)
        {
            LOG("Failed to open img4 file");
            goto fail;
        }

        img->filehandle = img4_reopen(img->filehandle, NULL, 0);
        if (img->filehandle == NULL)
        {
            LOG("Failed to open img4");
            goto fail;
        }

        ret = img->filehandle->ioctl(img->filehandle, IOCTL_MEM_GET_DATAPTR, &img->map, &img->mapsize);
        if (ret != 0)
        {
            LOG("Failed to map img4");
            goto fail;
        }
    }
    else if (file_magic == MH_MAGIC ||
             file_magic == MH_MAGIC_64)
    {
        img->map = mmap(NULL, img->filesize, PROT_READ, MAP_PRIVATE, img->filedesc, 0);
        if (img->map == MAP_FAILED)
        {
            LOG("Failed to map image");
            goto fail;
        }
    }
    else if (file_magic == 0x646c7964) // dyld shared cache
    {
        img->map = mmap(NULL, img->filesize, PROT_READ, MAP_PRIVATE, img->filedesc, 0);
        if (img->map == MAP_FAILED)
        {
            LOG("Failed to map image");
            goto fail;
	}
        LOG("Mapped the dyld cache but didn't parsed the symtab only partial functionality present");
        return 0;
    }
    else
    {
        LOG("Unknown file type! Magic: %x", file_magic);
        goto fail;
    }

#if 0
#define kASN1TagSEQUENCE        16 //0x10
#define kASN1TagIA5String       22 //0x16

    typedef struct {
        uint8_t tagNumber : 5,
                isConstructed : 1,
                tagClass : 2;
    } asn1Tag;

    typedef struct {
        uint8_t len : 7,
                isLong : 1;
    } asn1Length;

    typedef struct{
        size_t dataLen;
        size_t sizeBytes;
    } asn1ElemLen;

    asn1Tag *tag = (asn1Tag *)&file_hdr;

    if (tag->tagNumber != kASN1TagSEQUENCE)
    {
        LOG("not a sequence");
    }
    else
    {
        LOG("is a sequence");
    
        asn1Length *length = (asn1Length *)tag++;
        LOG("length = %d %d", length->len, length->isLong);

        asn1ElemLen elemLen = {
            .dataLen = length->len,
            .sizeBytes = 1,
        };

        asn1Tag *tag2 = (asn1Tag *)tag++;
        LOG("tagNumber = %x", tag2->tagNumber);
        if ((tag2->tagNumber  | kASN1TagIA5String) == 0)
        {
            LOG("not a string");
            goto fail;
        }
        else LOG("is a string");
    }
#endif 

    img->mach_header = (struct mach_header *)img->map;

    /* parse all mach-o segments and cache them */
    int header_size = 0;
    uint32_t magic = img->mach_header->magic;

    switch (magic)
    {
        case MH_MAGIC_64:
            header_size = sizeof(struct mach_header_64);
            break;

        case MH_MAGIC:
            header_size = sizeof(struct mach_header);
            break;

        default: 
            LOG("Unknown magic: %x", magic);
            goto fail;
    }

    img->load_commands = (struct load_command **)malloc((img->mach_header->ncmds + 1) * sizeof(struct load_command *));

    struct load_command *cmd = (struct load_command *)((uintptr_t)img->map + header_size);

    for (int i = 0; i < img->mach_header->ncmds; i++)
    {
        img->load_commands[i] = cmd;

        cmd = (struct load_command *)((uintptr_t)cmd + cmd->cmdsize);
    
        if ((uintptr_t)cmd > (uintptr_t)img->map + img->filesize)
        {
            LOG("Load commands are out of bounds! Last command size: %x", cmd->cmdsize);
            goto fail;
        }
    }

    img->load_commands[img->mach_header->ncmds - 1] = NULL;

    /* might fail */
    jake_find_symtab(img);

    return 0;

fail:;
    jake_discard_image(img);
    return -1;
}

int jake_discard_image(jake_img_t img)
{
    if (img->filehandle != NULL)
    {
		img->filehandle->close(img->filehandle);
    }

    if (img->filedesc != 0)
    {
        if (img->map != NULL)
        {
            munmap((void *)img->map, img->filesize);
        }

        close(img->filedesc);
    }

    if (img->load_commands != NULL)
    {
        free(img->load_commands);
    }

    if (img->symtab != NULL)
    {
        if (img->symtab->symbols != NULL)
        {
            free(img->symtab->symbols);
        }

        free(img->symtab);
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

struct load_command *jake_find_load_cmd(jake_img_t img, int command)
{
    for (int i = 0; i < img->mach_header->ncmds; i++)
    {
        struct load_command *cmd = img->load_commands[i];

        if (cmd->cmd == command)
        {
            return cmd;
        }
    }

    return NULL;
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
    for (struct load_command **cmd_ptr = img->load_commands; *cmd_ptr != NULL; cmd_ptr++)
    {
        struct load_command *cmd = *cmd_ptr;

        if (cmd->cmd == LC_SEGMENT)
        {
            struct segment_command *seg = (struct segment_command *)cmd;

            if ((fileoff >= seg->fileoff) &&
                (fileoff < (seg->fileoff + seg->filesize)))
            {
                return seg->vmaddr + (fileoff - seg->fileoff);
            }
        }

        if (cmd->cmd == LC_SEGMENT_64)
        {
            struct segment_command_64 *seg = (struct segment_command_64 *)cmd;

            if ((fileoff >= seg->fileoff) &&
                (fileoff < (seg->fileoff + seg->filesize)))
            {
                return seg->vmaddr + (fileoff - seg->fileoff);
            }
        }
    }

    return 0x0;
}

uint64_t jake_vaddr_to_fileoff(jake_img_t img, uint64_t vaddr)
{
    for (struct load_command **cmd_ptr = img->load_commands; *cmd_ptr != NULL; cmd_ptr++)
    {
        struct load_command *cmd = *cmd_ptr;

        if (cmd->cmd == LC_SEGMENT)
        {
            struct segment_command *seg = (struct segment_command *)cmd;

            if ((vaddr >= seg->vmaddr) &&
                (vaddr < (seg->vmaddr + seg->filesize)))
            {
                return seg->fileoff + (vaddr - seg->vmaddr);   
            }
        }

        if (cmd->cmd == LC_SEGMENT_64)
        {
            struct segment_command_64 *seg = (struct segment_command_64 *)cmd;

            if ((vaddr >= seg->vmaddr) &&
                (vaddr < (seg->vmaddr + seg->filesize)))
            {
                return seg->fileoff + (vaddr - seg->vmaddr);   
            }
        }
    }

    return 0x0;
}
