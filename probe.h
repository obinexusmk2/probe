#ifndef PROBE_H
#define PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

/* PROBE - MMUKO OS Network Probing Protocol
 * Three-state consensual question system
 * WHO | WHAT | WHEN | WHERE | WHY | HOW
 */

/* ── States ─────────────────────────────── */
typedef enum {
    PROBE_NO    = 0,
    PROBE_YES   = 1,
    PROBE_MAYBE = 2
} ProbeState;

/* ── Questions ──────────────────────────── */
typedef enum {
    PROBE_WHO   = 0,
    PROBE_WHAT  = 1,
    PROBE_WHEN  = 2,
    PROBE_WHERE = 3,
    PROBE_WHY   = 4,
    PROBE_HOW   = 5
} ProbeQuestion;

/* ── Node ───────────────────────────────── */
typedef struct {
    unsigned int  id;
    char          address[64];
    ProbeState    state;
    long long     last_seen;   /* unix timestamp */
    char          data[256];
} ProbeNode;

/* ── Result ─────────────────────────────── */
typedef struct {
    ProbeQuestion question;
    ProbeState    state;
    ProbeNode     node;
    char          detail[256];
} ProbeResult;

/* ── API ────────────────────────────────── */

/* Initialise probe system */
int  probe_init(void);

/* Ask a question about a node. Returns ProbeResult. */
ProbeResult probe_ask(ProbeQuestion q, const char *address);

/* Probe a whole network range. Results written to out[]. Returns count. */
int  probe_network(const char *cidr, ProbeResult *out, int max);

/* Resolve MAYBE - retry up to max_retries times */
ProbeState probe_resolve(ProbeResult *result, int max_retries);

/* Human-readable state string */
const char *probe_state_str(ProbeState s);

/* Human-readable question string */
const char *probe_question_str(ProbeQuestion q);

/* Cleanup */
void probe_destroy(void);

/* ── C Macro interface (? operator) ─────── */
#define PROBE(q, addr)  probe_ask(PROBE_##q, addr)
#define PROBE_IS(r)     ((r).state == PROBE_YES)
#define PROBE_NOT(r)    ((r).state == PROBE_NO)
#define PROBE_UNSURE(r) ((r).state == PROBE_MAYBE)

#ifdef __cplusplus
}
#endif

#endif /* PROBE_H */
