#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

SEC("tracepoint/syscalls/sys_enter_openat")
int handle_openat(void *ctx)
{
    __u64 id = bpf_get_current_pid_tgid();
    __u32 pid = id >> 32;

    bpf_printk("openat called by PID %d", pid);

    return 0;
}

char LICENSE[] SEC("license") = "GPL";
