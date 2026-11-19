#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {

    char hostname[128];
    char domainname[128];

    if (gethostname(hostname, sizeof(hostname)) == -1) {
        perror("gethostname");
        exit(EXIT_FAILURE);
    }

    if (getdomainname(domainname, sizeof(domainname)) == -1) {
        perror("getdomainname");
        exit(EXIT_FAILURE);
    }

    printf("Hostname: %s\n", hostname);
    printf("Domain: %s\n", domainname);    
    return EXIT_SUCCESS;

}