#include <stdio.h>
#include <stdlib.h>
#include <net/if.h>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <interface_index>\n", argv[0]);
        return EXIT_FAILURE;
    }

    unsigned int index = (unsigned int)strtoul(argv[1], NULL, 10);
    char ifname[IF_NAMESIZE];
    if (if_indextoname(index, ifname) == NULL) {
        perror("if_indextoname");
        return EXIT_FAILURE;
    }

    printf("Index: %u\n", index);
    printf("Interface: %s\n", ifname);
    return EXIT_SUCCESS;
}