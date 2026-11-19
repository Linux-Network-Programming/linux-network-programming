#include <stdio.h>
#include <stdlib.h>
#include <net/if.h>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <interface>\n", argv[0]);
        return EXIT_FAILURE;
    }

    unsigned int index = if_nametoindex(argv[1]);

    if (index == 0) {
        perror("if_nametoindex");
        return EXIT_FAILURE;
    }

    printf("Interface: %s\n", argv[1]);
    printf("Index: %u\n", index);

    return EXIT_SUCCESS;
}