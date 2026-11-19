/* setmon_nl — set a wifi iface type via nl80211 (for drivers that reject WEXT monitor).
 * usage: setmon_nl <iface> [monitor|managed]   (default monitor)
 * Bring the iface DOWN before, UP after.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <net/if.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>
#include <linux/nl80211.h>

#define BUFSZ 8192

static void put_attr(char *buf, int *off, int type, const void *data, int len) {
    struct nlattr *a = (struct nlattr *)(buf + *off);
    a->nla_type = type;
    a->nla_len = NLA_HDRLEN + len;
    memcpy((char *)a + NLA_HDRLEN, data, len);
    *off += NLA_ALIGN(a->nla_len);
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <iface> [monitor|managed]\n", argv[0]); return 2; }
    const char *ifn = argv[1];
    unsigned iftype = NL80211_IFTYPE_MONITOR;            /* 6 */
    if (argc >= 3 && !strcmp(argv[2], "managed")) iftype = NL80211_IFTYPE_STATION; /* 2 */
    unsigned ifindex = if_nametoindex(ifn);
    if (!ifindex) { perror("if_nametoindex"); return 1; }

    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
    if (fd < 0) { perror("socket"); return 1; }
    struct sockaddr_nl sa; memset(&sa, 0, sizeof sa); sa.nl_family = AF_NETLINK;
    if (bind(fd, (struct sockaddr *)&sa, sizeof sa) < 0) { perror("bind"); return 1; }

    char buf[BUFSZ]; int off;
    struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
    struct genlmsghdr *g;

    /* 1) resolve nl80211 family id */
    memset(buf, 0, sizeof buf);
    nlh->nlmsg_type = GENL_ID_CTRL; nlh->nlmsg_flags = NLM_F_REQUEST;
    nlh->nlmsg_seq = 1; nlh->nlmsg_pid = getpid();
    g = (struct genlmsghdr *)NLMSG_DATA(nlh); g->cmd = CTRL_CMD_GETFAMILY; g->version = 1;
    off = NLMSG_LENGTH(GENL_HDRLEN);
    put_attr(buf, &off, CTRL_ATTR_FAMILY_NAME, "nl80211", 8);
    nlh->nlmsg_len = off;
    if (send(fd, buf, off, 0) < 0) { perror("send getfamily"); return 1; }

    char rb[BUFSZ]; int rl = recv(fd, rb, sizeof rb, 0);
    if (rl < 0) { perror("recv getfamily"); return 1; }
    struct nlmsghdr *rh = (struct nlmsghdr *)rb;
    if (rh->nlmsg_type == NLMSG_ERROR) { fprintf(stderr, "getfamily NLMSG_ERROR\n"); return 1; }
    int famid = -1;
    struct genlmsghdr *rg = (struct genlmsghdr *)NLMSG_DATA(rh);
    int alen = rh->nlmsg_len - NLMSG_LENGTH(GENL_HDRLEN);
    struct nlattr *at = (struct nlattr *)((char *)rg + GENL_HDRLEN);
    while (alen > (int)NLA_HDRLEN) {
        if (at->nla_type == CTRL_ATTR_FAMILY_ID)
            famid = *(unsigned short *)((char *)at + NLA_HDRLEN);
        int l = NLA_ALIGN(at->nla_len);
        at = (struct nlattr *)((char *)at + l); alen -= l;
    }
    if (famid < 0) { fprintf(stderr, "nl80211 family not found\n"); return 1; }

    /* 2) NL80211_CMD_SET_INTERFACE */
    memset(buf, 0, sizeof buf);
    nlh->nlmsg_type = famid; nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
    nlh->nlmsg_seq = 2; nlh->nlmsg_pid = getpid();
    g = (struct genlmsghdr *)NLMSG_DATA(nlh); g->cmd = NL80211_CMD_SET_INTERFACE; g->version = 0;
    off = NLMSG_LENGTH(GENL_HDRLEN);
    put_attr(buf, &off, NL80211_ATTR_IFINDEX, &ifindex, 4);
    put_attr(buf, &off, NL80211_ATTR_IFTYPE, &iftype, 4);
    nlh->nlmsg_len = off;
    if (send(fd, buf, off, 0) < 0) { perror("send set_interface"); return 1; }

    rl = recv(fd, rb, sizeof rb, 0);
    if (rl < 0) { perror("recv set_interface"); return 1; }
    rh = (struct nlmsghdr *)rb;
    if (rh->nlmsg_type == NLMSG_ERROR) {
        struct nlmsgerr *e = (struct nlmsgerr *)NLMSG_DATA(rh);
        if (e->error == 0) { printf("OK: %s -> %s via nl80211\n", ifn, iftype == NL80211_IFTYPE_MONITOR ? "MONITOR" : "managed"); return 0; }
        fprintf(stderr, "SET_INTERFACE error %d (%s)\n", e->error, strerror(-e->error));
        return 3;
    }
    printf("sent (unexpected reply type %d)\n", rh->nlmsg_type);
    return 0;
}
