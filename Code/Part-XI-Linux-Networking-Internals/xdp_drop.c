#include <arpa/inet.h>
#include <net/if.h>
#include <linux/if_link.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    struct bpf_object *obj;
    struct bpf_program *prog;

    struct in_addr blocked_ip;
    unsigned int ifindex;
    int map_fd;
    int prog_fd;
    __u32 key = 0;
    long err;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <interface> <IPv4-address>\n", argv[0]);
        return 1;
    }

    ifindex = if_nametoindex(argv[1]);
    if (ifindex == 0) {
        perror("if_nametoindex");
        return 1;
    }

    if (inet_pton(AF_INET, argv[2], &blocked_ip) != 1) {
        fprintf(stderr, "Invalid IPv4 address\n");
        return 1;
    }

    obj = bpf_object__open_file("xdp_drop.bpf.o", NULL);
    err = libbpf_get_error(obj);
    if (err) {
        fprintf(stderr, "Error opening BPF object: %ld\n", err);
        return 1;
    }

    if (bpf_object__load(obj)) {
        perror("bpf_object__load");
        bpf_object__close(obj);
        return 1;
    }

    map_fd = bpf_object__find_map_fd_by_name(obj, "blocked_ipv4");
    if (map_fd < 0) {
        fprintf(stderr, "Cannot find map\n");
        bpf_object__close(obj);
        return 1;
    }

    if (bpf_map_update_elem(map_fd, &key, &blocked_ip.s_addr, BPF_ANY)) {
        perror("bpf_map_update_elem");
        bpf_object__close(obj);
        return 1;
    }

    prog = bpf_object__find_program_by_name(obj, "drop_src_ipv4");
    if (!prog) {
        fprintf(stderr, "Cannot find XDP program\n");
        bpf_object__close(obj);
        return 1;
    }

    prog_fd = bpf_program__fd(prog);
    if (bpf_xdp_attach(ifindex, prog_fd, XDP_FLAGS_SKB_MODE, NULL)) {
        perror("bpf_xdp_attach");
        bpf_object__close(obj);
        return 1;
    }

    printf("XDP program attached to %s\n", argv[1]);
    printf("Blocking source address: %s\n", argv[2]);
    bpf_object__close(obj);
    return 0;
}
