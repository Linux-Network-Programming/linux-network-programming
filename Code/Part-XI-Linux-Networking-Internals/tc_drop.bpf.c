#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/in.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>
#include <linux/pkt_cls.h>

struct icmp_hdr {
    __u8 type;
    __u8 code;
    __u16 checksum;
    __u32 rest;
};

#define ICMP_ECHO 8

SEC("tc")
int tc_drop(struct __sk_buff *skb)
{
    void *data = (void *)(long)skb->data;
    void *data_end = (void *)(long)skb->data_end;

    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end)
        return TC_ACT_OK;

    if (eth->h_proto != bpf_htons(ETH_P_IP))
        return TC_ACT_OK;

    struct iphdr *iph = (void *)(eth + 1);
    if ((void *)(iph + 1) > data_end)
        return TC_ACT_OK;

    if (iph->protocol != IPPROTO_ICMP)
        return TC_ACT_OK;

    __u32 ip_len = iph->ihl * 4;
    if (ip_len < sizeof(struct iphdr))
        return TC_ACT_OK;

    struct icmp_hdr *icmp = (void *)iph + ip_len;
    if ((void *)(icmp + 1) > data_end)
        return TC_ACT_OK;

    if (icmp->type == ICMP_ECHO)
        return TC_ACT_SHOT;

    return TC_ACT_OK;
}

char LICENSE[] SEC("license") = "GPL";