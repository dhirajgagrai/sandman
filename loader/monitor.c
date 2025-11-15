#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

#define STATE_START 0
#define STATE_FINAL 69420

#define SIGKILL 9

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 10240);
    __type(key, __u32);
    __type(value, __u32);
} nfa_state_map SEC(".maps");

struct nfa_key {
    __u32 current_state;
    __u32 input_id;
};

struct nfa_value {
    __u32 next_state;
    __u32 is_final_state;
};

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 10240);
    __type(key, struct nfa_key);
    __type(value, struct nfa_value);
} nfa_transition_map SEC(".maps");

SEC("tracepoint/syscalls/sys_enter_dummy")
int on_dummy_syscall(struct trace_event_raw_sys_enter *ctx) {

    __u32 pid = bpf_get_current_pid_tgid() >> 32;

    int input_id = (int)ctx->args[0];

    __u32 current_state = STATE_START;
    __u32 next_state = STATE_START;

    __u32 *state_ptr = bpf_map_lookup_elem(&nfa_state_map, &pid);
    if (state_ptr) {
        current_state = *state_ptr;
    }

    if (current_state == STATE_FINAL) {
        bpf_printk("MONITOR: PID %d - Transition after Final State\n", pid);
        bpf_send_signal(SIGKILL);
        bpf_map_delete_elem(&nfa_state_map, &pid);

        return -2;
    }

    struct nfa_key key = {};
    key.current_state = current_state;
    key.input_id = input_id;

    struct nfa_value *transition;
    transition = bpf_map_lookup_elem(&nfa_transition_map, &key);

    if (!transition) {
        bpf_printk("MONITOR: PID %d - Invalid Transition from %u, Input: %u\n", pid, current_state, input_id);
        bpf_send_signal(SIGKILL);
        bpf_map_delete_elem(&nfa_state_map, &pid);

        return -1;
    }

    __u32 is_final_state = transition->is_final_state;
    if (is_final_state) {
        next_state = STATE_FINAL;
    } else {
        next_state = transition->next_state;
    }

    bpf_printk("MONITOR: PID %d - Transition: %u -> %u, Input: %u\n", pid, current_state, next_state, input_id);

    bpf_map_update_elem(&nfa_state_map, &pid, &next_state, BPF_ANY);

    return 0;
}

char LICENSE[] SEC("license") = "GPL";
