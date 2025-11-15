#include <bpf/libbpf.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "monitor.skel.h"

struct nfa_key {
    __u32 current_state;
    __u32 input_id;
};

struct nfa_value {
    __u32 next_state;
    __u32 is_final_state;
};

static volatile bool stop = false;

static void int_handler(int sig) {
    stop = true;
}

int load_nfa_rules(struct nfa_monitor *skel) {
    FILE *f;
    char line[256];
    int line_num = 0;
    int rule_count = 0;

    f = fopen("nfa_table.dat", "r");
    if (!f) {
        fprintf(stderr, "ERROR: Failed to open nfa_table.dat: %s\n", strerror(errno));
        return -1;
    }

    const struct bpf_map *map = skel->maps.nfa_transition_map;

    while (fgets(line, sizeof(line), f)) {
        line_num++;
        if (line[0] == '#' || line[0] == '\n')
            continue;

        struct nfa_key key = {};
        struct nfa_value value = {};
        int ret = sscanf(line, "%u %u %u %u", &key.current_state, &key.input_id, &value.next_state, &value.is_final_state);

        if (ret != 4) {
            fprintf(stderr, "Warning: Skipping malformed line %d: %s", line_num, line);
            continue;
        }

        ret = bpf_map__update_elem(map, &key, sizeof(key), &value, sizeof(value), BPF_ANY);
        if (ret != 0) {
            fprintf(stderr, "ERROR: Failed to upload rule (line %d): %s\n", line_num, strerror(errno));
            fclose(f);
            return -1;
        }
        rule_count++;
    }

    fclose(f);
    printf("Loader: Successfully loaded %d nfa rules into kernel.\n", rule_count);
    return 0;
}

int main(int argc, char **argv) {
    struct nfa_monitor *skel;
    int err;

    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);

    skel = nfa_monitor__open_and_load();
    if (!skel) {
        fprintf(stderr, "ERROR: Failed to open BPF skeleton\n");
        return 1;
    }

    err = load_nfa_rules(skel);
    if (err) {
        fprintf(stderr, "ERROR: Failed to load nfa rules.\n");
        goto cleanup;
    }

    err = nfa_monitor__attach(skel);
    if (err) {
        fprintf(stderr, "ERROR: Failed to attach BPF skeleton: %s\n", strerror(-err));
        goto cleanup;
    }

    printf("eBPF monitor loaded and attached to sys_dummy. Press Ctrl+C to exit.\n");
    while (!stop) {
        sleep(1);
    }

cleanup:
    nfa_monitor__destroy(skel);
    printf("\neBPF monitor detached and unloaded.\n");
    return -err;
}
