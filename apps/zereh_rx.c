#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#if __has_include(<bpf/xsk.h>)
#include <bpf/xsk.h>
#define ZEREH_HAVE_XSK 1
#else
#define ZEREH_HAVE_XSK 0
#endif

#if !ZEREH_HAVE_XSK

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    fprintf(stderr, "bpf/xsk.h not found; install libbpf development headers to build AF_XDP receiver\n");
    return 1;
}

#else

#define NUM_FRAMES 4096
#define FRAME_SIZE XSK_UMEM__DEFAULT_FRAME_SIZE
#define RX_BATCH_SIZE 64

static volatile sig_atomic_t g_stop;

struct zereh_xsk_ctx {
    struct xsk_umem *umem;
    struct xsk_ring_prod fq;
    struct xsk_ring_cons cq;
    struct xsk_ring_cons rx;
    struct xsk_ring_prod tx;
    struct xsk_socket *xsk;
    void *buffer;
};

static void zereh_on_signal(int signo)
{
    (void)signo;
    g_stop = 1;
}

static int zereh_init_xsk(struct zereh_xsk_ctx *ctx, const char *ifname, uint32_t queue_id)
{
    struct xsk_umem_config umem_cfg = {
        .fill_size = NUM_FRAMES,
        .comp_size = NUM_FRAMES,
        .frame_size = FRAME_SIZE,
        .frame_headroom = 0,
        .flags = 0,
    };
    struct xsk_socket_config xsk_cfg = {
        .rx_size = 2048,
        .tx_size = 2048,
        .libbpf_flags = 0,
        .xdp_flags = 0,
        .bind_flags = XDP_USE_NEED_WAKEUP,
    };
    uint32_t idx;
    uint32_t i;

    memset(ctx, 0, sizeof(*ctx));

    if (posix_memalign(&ctx->buffer, getpagesize(), NUM_FRAMES * FRAME_SIZE) != 0) {
        perror("posix_memalign");
        return -1;
    }

    if (xsk_umem__create(&ctx->umem,
                         ctx->buffer,
                         NUM_FRAMES * FRAME_SIZE,
                         &ctx->fq,
                         &ctx->cq,
                         &umem_cfg) != 0) {
        fprintf(stderr, "xsk_umem__create failed\n");
        return -1;
    }

    if (xsk_socket__create(&ctx->xsk,
                           ifname,
                           queue_id,
                           ctx->umem,
                           &ctx->rx,
                           &ctx->tx,
                           &xsk_cfg) != 0) {
        fprintf(stderr, "xsk_socket__create failed (check XDP program + XSKMAP binding)\n");
        return -1;
    }

    if (xsk_ring_prod__reserve(&ctx->fq, NUM_FRAMES, &idx) != NUM_FRAMES) {
        fprintf(stderr, "xsk_ring_prod__reserve fill ring failed\n");
        return -1;
    }

    for (i = 0; i < NUM_FRAMES; i++) {
        *xsk_ring_prod__fill_addr(&ctx->fq, idx + i) = i * FRAME_SIZE;
    }
    xsk_ring_prod__submit(&ctx->fq, NUM_FRAMES);

    return 0;
}

static void zereh_cleanup_xsk(struct zereh_xsk_ctx *ctx)
{
    if (ctx->xsk) {
        xsk_socket__delete(ctx->xsk);
    }
    if (ctx->umem) {
        xsk_umem__delete(ctx->umem);
    }
    free(ctx->buffer);
}

int main(int argc, char **argv)
{
    struct zereh_xsk_ctx xsk;
    struct pollfd pfd;
    const char *ifname;
    uint32_t queue_id;
    uint64_t packet_count = 0;

    if (argc < 3) {
        fprintf(stderr, "Usage: %s <ifname> <queue_id>\n", argv[0]);
        return 1;
    }

    ifname = argv[1];
    queue_id = (uint32_t)strtoul(argv[2], NULL, 10);

    signal(SIGINT, zereh_on_signal);
    signal(SIGTERM, zereh_on_signal);

    if (zereh_init_xsk(&xsk, ifname, queue_id) != 0) {
        zereh_cleanup_xsk(&xsk);
        return 1;
    }

    pfd.fd = xsk_socket__fd(xsk.xsk);
    pfd.events = POLLIN;

    printf("zereh_rx listening on if=%s queue=%u\n", ifname, queue_id);

    while (!g_stop) {
        uint32_t idx_rx = 0;
        uint32_t i;
        uint32_t rcvd;

        if (poll(&pfd, 1, 1000) < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            break;
        }

        rcvd = xsk_ring_cons__peek(&xsk.rx, RX_BATCH_SIZE, &idx_rx);
        if (!rcvd) {
            continue;
        }

        for (i = 0; i < rcvd; i++) {
            const struct xdp_desc *desc = xsk_ring_cons__rx_desc(&xsk.rx, idx_rx + i);
            uint64_t addr = desc->addr;
            uint32_t idx_fq;

            packet_count++;

            while (xsk_ring_prod__reserve(&xsk.fq, 1, &idx_fq) != 1) {
                if (xsk_ring_prod__needs_wakeup(&xsk.fq)) {
                    (void)sendto(xsk_socket__fd(xsk.xsk), NULL, 0, MSG_DONTWAIT, NULL, 0);
                }
            }

            *xsk_ring_prod__fill_addr(&xsk.fq, idx_fq) = addr;
            xsk_ring_prod__submit(&xsk.fq, 1);
        }

        xsk_ring_cons__release(&xsk.rx, rcvd);

        if ((packet_count % 1024) == 0) {
            printf("zereh_rx packets=%llu\n", (unsigned long long)packet_count);
        }
    }

    printf("zereh_rx exiting packets=%llu\n", (unsigned long long)packet_count);
    zereh_cleanup_xsk(&xsk);
    return 0;
}

#endif /* ZEREH_HAVE_XSK */
