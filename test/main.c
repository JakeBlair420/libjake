
#include <stdio.h>
#include <unistd.h>

#include "img.h"

int main(int argc, char *argv[])
{
    int ret = 0;

    printf("testing libjake...\n");

    if (argc < 2)
    {
        printf("usage: %s [target]\n", argv[0]);
        return 0;
    }

    jake_img img = { 0 };

    const char *bin_path = argv[1];
    if (access(bin_path, F_OK) != 0)
    {
        printf("failed to find file '%s'!\n", bin_path);
        goto out;
    }

    ret = jake_init_image(&img, bin_path);
    if (ret != 0)
    {
        printf("failed to open file '%s'!\n", bin_path);
        goto out;
    }

    printf("opened image: %p\n", &img);

    printf("img is swap: %d\n", jake_is_swap_img(&img));
    printf("img is 64bit: %d\n", jake_is_64bit_img(&img));

    ret = jake_find_symtab(&img);
    if (ret != 0)
    {
        printf("failed to find symtab.\n");
    }
    else
    {
        /* found symtab */
        printf("found %d symbols\n", img.symtab->nsyms);

        int sym_cnt = (img.symtab->nsyms < 10) ? img.symtab->nsyms : 10;

        printf("dumping %d symbols...\n", sym_cnt);
        for (int i = 0 ; i < sym_cnt; i++)
        {
            nlist_global sym_entry = img.symtab->symbols[i];

            printf("symbol %s = %llx\n", sym_entry.n_name, sym_entry.n_value);
        }
    }

    printf("dumping segments...\n");
    for (struct load_command **cmd_ptr = img.load_commands; *cmd_ptr != NULL; cmd_ptr++)
    {
        struct load_command *cmd = *cmd_ptr;

        if (cmd->cmd != LC_SEGMENT_64) continue;

        struct segment_command_64 *seg = (struct segment_command_64 *)cmd;

        printf("fileoff: [%llx - %llx] vaddr: [%llx - %llx]\n", seg->fileoff, seg->fileoff + seg->filesize, seg->vmaddr, seg->vmaddr + seg->filesize);
    }
    
    uint64_t vaddr = jake_fileoff_to_vaddr(&img, 0);
    printf("vaddr = %llx\n", vaddr);

    uint64_t fileoff = jake_vaddr_to_fileoff(&img, vaddr);
    printf("fileoff = %llx\n", fileoff);


    printf("done!\n");

out:;

    return 0;
}
