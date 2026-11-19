#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]) {

    if (argc < 2) {
        fprintf(stderr, "Usage: %s hostname [domain_name]", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (sethostname(argv[1], strlen(argv[1])) == -1) {
        perror("sethostname");
        exit(EXIT_FAILURE);
    }    
    
    if (argc > 2) {
        if (setdomainname(argv[2], strlen(argv[2])) == -1){
            perror("setdomainname");
            exit(EXIT_FAILURE);
        }
    }

    char hostname[128];
    if (gethostname(hostname, sizeof(hostname)) == -1) {
        perror("gethostname");
        exit(EXIT_FAILURE);
    }
    printf("Hostname: %s\n", hostname);

    if(argc > 2) {
        char domainname[128];
        if (getdomainname(domainname, sizeof(domainname)) == -1) {
            perror("getdomainname");
            exit(EXIT_FAILURE);
        }
        printf("Domain: %s", domainname);
    }

    return EXIT_SUCCESS;
}