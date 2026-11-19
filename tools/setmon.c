/* setmon — set a wifi iface mode via WEXT ioctl (needs CFG80211_WEXT).
 * usage: setmon <iface> [monitor|managed]   (default: monitor)
 * Prints the mode read back. Bring the iface DOWN before, UP after.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/wireless.h>

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <iface> [monitor|managed]\n", argv[0]); return 2; }
    const char *ifn = argv[1];
    int want = IW_MODE_MONITOR; /* 6 */
    if (argc >= 3 && strcmp(argv[2], "managed") == 0) want = IW_MODE_INFRA; /* 2 */

    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) { perror("socket"); return 1; }

    struct iwreq wrq;
    memset(&wrq, 0, sizeof(wrq));
    strncpy(wrq.ifr_name, ifn, IFNAMSIZ - 1);
    wrq.u.mode = want;
    if (ioctl(s, SIOCSIWMODE, &wrq) < 0) { perror("SIOCSIWMODE"); return 1; }

    memset(&wrq, 0, sizeof(wrq));
    strncpy(wrq.ifr_name, ifn, IFNAMSIZ - 1);
    if (ioctl(s, SIOCGIWMODE, &wrq) < 0) { perror("SIOCGIWMODE"); return 1; }
    printf("%s mode = %d  (2=managed, 6=monitor) -> %s\n", ifn, wrq.u.mode,
           wrq.u.mode == IW_MODE_MONITOR ? "MONITOR OK" :
           wrq.u.mode == IW_MODE_INFRA ? "managed" : "other");
    return wrq.u.mode == want ? 0 : 3;
}
