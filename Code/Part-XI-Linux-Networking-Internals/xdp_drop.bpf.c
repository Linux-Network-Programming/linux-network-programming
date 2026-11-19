#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

/* Map containing the IPv4 address to block */
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __be32);
} blocked_ipv4 SEC(".maps");

SEC("xdp")
int drop_src_ipv4(struct xdp_md *ctx)
{
    void *data = (void *)(long)ctx->data;
    void *data_end = (void *)(long)ctx->data_end;

    struct ethhdr *eth = data;

    /* Check that the Ethernet header is available */
    if ((void *)(eth + 1) > data_end)
        return XDP_PASS;

    /* Process only IPv4 packets */
    if (eth->h_proto != bpf_htons(ETH_P_IP))
        return XDP_PASS;

    struct iphdr *iph = (void *)(eth + 1);

    /* Check that the IPv4 header is available */
    if ((void *)(iph + 1) > data_end)
        return XDP_PASS;

    /* Retrieve the address to block from the map */
    __u32 key = 0;
    __be32 *blocked_ip;

    blocked_ip = bpf_map_lookup_elem(&blocked_ipv4, &key);

    if (blocked_ip && iph->saddr == *blocked_ip)
        return XDP_DROP;

    return XDP_PASS;
}

char LICENSE[] SEC("license") = "GPL";
