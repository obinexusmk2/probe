#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "probe.h"

static void usage(const char *name) {
    printf("PROBE - MMUKO OS Network Probing Protocol\n\n");
    printf("Usage:\n");
    printf("  %s <address>              probe a single host\n", name);
    printf("  %s <address> WHO|WHAT|WHEN|WHERE|WHY|HOW\n", name);
    printf("  %s network <cidr>         scan a network range\n\n", name);
    printf("States: YES | NO | MAYBE\n");
}

static ProbeQuestion parse_question(const char *s) {
    if (!s)                   return PROBE_WHERE;
    if (!strcmp(s,"WHO"))     return PROBE_WHO;
    if (!strcmp(s,"WHAT"))    return PROBE_WHAT;
    if (!strcmp(s,"WHEN"))    return PROBE_WHEN;
    if (!strcmp(s,"WHERE"))   return PROBE_WHERE;
    if (!strcmp(s,"WHY"))     return PROBE_WHY;
    if (!strcmp(s,"HOW"))     return PROBE_HOW;
    return PROBE_WHERE;
}

int main(int argc, char *argv[]) {
    if (argc < 2) { usage(argv[0]); return 1; }

    if (probe_init() != 0) {
        fprintf(stderr, "probe_init failed\n");
        return 1;
    }

    if (strcmp(argv[1], "network") == 0) {
        const char *cidr = argc > 2 ? argv[2] : "192.168.1.0/24";
        printf("Scanning: %s\n\n", cidr);
        ProbeResult results[254];
        int count = probe_network(cidr, results, 254);
        printf("Found %d nodes:\n\n", count);
        for (int i = 0; i < count; i++)
            printf("  [%s] %s\n    %s\n\n",
                probe_state_str(results[i].state),
                results[i].node.address,
                results[i].detail);
    } else {
        const char *address = argv[1];
        if (argc == 2) {
            printf("Probing: %s\n\n", address);
            ProbeQuestion qs[] = { PROBE_WHO, PROBE_WHAT, PROBE_WHEN, PROBE_WHERE };
            for (int i = 0; i < 4; i++) {
                ProbeResult r = probe_ask(qs[i], address);
                if (r.state == PROBE_MAYBE) probe_resolve(&r, 2);
                printf("  [%s] %s\n", probe_state_str(r.state), r.detail);
            }
        } else {
            ProbeResult r = probe_ask(parse_question(argv[2]), address);
            if (r.state == PROBE_MAYBE) probe_resolve(&r, 3);
            printf("[%s] %s\n", probe_state_str(r.state), r.detail);
        }
    }

    probe_destroy();
    return 0;
}
