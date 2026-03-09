/* PROBE - MMUKO OS Network Probing Protocol
 * Windows-compatible build
 */

/* Windows headers MUST come first */
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0600  /* Vista+ for getaddrinfo */
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #pragma comment(lib, "ws2_32.lib")
  #define CLOSE_SOCK(s) closesocket(s)
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <errno.h>
  #define CLOSE_SOCK(s) close(s)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef _WIN32
#include <fcntl.h>
#endif
#include "probe.h"

/* ── Init / Destroy ─────────────────────── */

int probe_init(void) {
#ifdef _WIN32
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2,2), &wsa) == 0 ? 0 : -1;
#else
    return 0;
#endif
}

void probe_destroy(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

/* ── Helpers ────────────────────────────── */

const char *probe_state_str(ProbeState s) {
    switch (s) {
        case PROBE_YES:   return "YES";
        case PROBE_NO:    return "NO";
        case PROBE_MAYBE: return "MAYBE";
        default:          return "UNKNOWN";
    }
}

const char *probe_question_str(ProbeQuestion q) {
    switch (q) {
        case PROBE_WHO:   return "WHO";
        case PROBE_WHAT:  return "WHAT";
        case PROBE_WHEN:  return "WHEN";
        case PROBE_WHERE: return "WHERE";
        case PROBE_WHY:   return "WHY";
        case PROBE_HOW:   return "HOW";
        default:          return "UNKNOWN";
    }
}

/* TCP connect check with timeout */
static ProbeState tcp_ping(const char *address, int port, int timeout_ms) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(address, NULL, &hints, &res) != 0)
        return PROBE_NO;

    if (res->ai_family == AF_INET)
        ((struct sockaddr_in *)res->ai_addr)->sin_port = htons((unsigned short)port);

#ifdef _WIN32
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) { freeaddrinfo(res); return PROBE_NO; }

    /* Set non-blocking */
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);

    connect(sock, res->ai_addr, (int)res->ai_addrlen);
    freeaddrinfo(res);

    /* Wait with select */
    fd_set wset, eset;
    FD_ZERO(&wset); FD_SET(sock, &wset);
    FD_ZERO(&eset); FD_SET(sock, &eset);
    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ready = select(0, NULL, &wset, &eset, &tv);
    closesocket(sock);

    if (ready <= 0) return PROBE_NO;
    if (FD_ISSET(sock, &eset)) return PROBE_MAYBE;
    return PROBE_YES;
#else
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { freeaddrinfo(res); return PROBE_NO; }

    /* Set non-blocking */
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    connect(sock, res->ai_addr, (int)res->ai_addrlen);
    freeaddrinfo(res);

    fd_set wset;
    FD_ZERO(&wset); FD_SET(sock, &wset);
    struct timeval tv;
    tv.tv_sec  = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ready = select(sock + 1, NULL, &wset, NULL, &tv);
    close(sock);

    if (ready <= 0) return PROBE_NO;
    return PROBE_YES;
#endif
}

/* Reverse DNS */
static void resolve_hostname(const char *address, char *out, size_t len) {
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = inet_addr(address); /* inet_addr works on all platforms */
    if (sa.sin_addr.s_addr == INADDR_NONE) { out[0] = '\0'; return; }
    if (getnameinfo((struct sockaddr*)&sa, sizeof(sa),
                    out, (socklen_t)len, NULL, 0, NI_NAMEREQD) != 0)
        out[0] = '\0';
}

/* ── Core ask ───────────────────────────── */

ProbeResult probe_ask(ProbeQuestion q, const char *address) {
    ProbeResult r;
    memset(&r, 0, sizeof(r));
    r.question = q;
    strncpy(r.node.address, address, sizeof(r.node.address) - 1);
    r.node.last_seen = (long long)time(NULL);

    switch (q) {
        case PROBE_WHO:
            resolve_hostname(address, r.node.data, sizeof(r.node.data));
            r.node.state = (r.node.data[0] != '\0') ? PROBE_YES : PROBE_MAYBE;
            snprintf(r.detail, sizeof(r.detail) - 1, "WHO: %s -> %s",
                address, r.node.data[0] ? r.node.data : "unresolved");
            break;

        case PROBE_WHAT: {
            int ports[] = {80, 443, 22, 21, 3306, 5432, 8080, 0};
            char found[128] = "";
            for (int i = 0; ports[i]; i++) {
                if (tcp_ping(address, ports[i], 500) == PROBE_YES) {
                    char tmp[16];
                    snprintf(tmp, sizeof(tmp), "%d ", ports[i]);
                    strncat(found, tmp, sizeof(found) - strlen(found) - 1);
                }
            }
            r.node.state = (found[0] != '\0') ? PROBE_YES : PROBE_MAYBE;
            snprintf(r.detail, sizeof(r.detail) - 1, "WHAT: open ports [%s]",
                found[0] ? found : "none");
            break;
        }

        case PROBE_WHEN:
            r.node.state = tcp_ping(address, 80, 1000);
            snprintf(r.detail, sizeof(r.detail) - 1,
                "WHEN: %s last_seen=%lld state=%s",
                address, r.node.last_seen, probe_state_str(r.node.state));
            break;

        case PROBE_WHERE:
            r.node.state = tcp_ping(address, 80, 1000);
            if (r.node.state == PROBE_NO)
                r.node.state = tcp_ping(address, 443, 1000);
            snprintf(r.detail, sizeof(r.detail) - 1,
                "WHERE: %s reachable=%s", address, probe_state_str(r.node.state));
            break;

        case PROBE_WHY:
        case PROBE_HOW:
            r.node.state = PROBE_MAYBE;
            snprintf(r.detail, sizeof(r.detail) - 1,
                "%s: %s - requires deeper probe",
                probe_question_str(q), address);
            break;
    }

    r.state = r.node.state;
    return r;
}

/* ── Network scan ───────────────────────── */

int probe_network(const char *cidr, ProbeResult *out, int max) {
    char base[64];
    strncpy(base, cidr, sizeof(base) - 1);
    char *slash = strchr(base, '/');
    if (slash) *slash = '\0';
    char *last_dot = strrchr(base, '.');
    if (!last_dot) return 0;
    *last_dot = '\0';

    int count = 0;
    char addr[64];
    for (int i = 1; i <= 254 && count < max; i++) {
        snprintf(addr, sizeof(addr) - 1, "%s.%d", base, i);
        ProbeResult r = PROBE(WHERE, addr);
        if (r.state != PROBE_NO)
            out[count++] = r;
    }
    return count;
}

/* ── Resolve MAYBE ──────────────────────── */

ProbeState probe_resolve(ProbeResult *result, int max_retries) {
    for (int i = 0; i < max_retries; i++) {
        ProbeResult r = probe_ask(result->question, result->node.address);
        if (r.state != PROBE_MAYBE) { *result = r; return r.state; }
    }
    return PROBE_MAYBE;
}
