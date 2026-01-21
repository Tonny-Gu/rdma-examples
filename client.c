#include "rdma_common.h"
#include <sys/time.h>

static double get_time_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1e6 + tv.tv_usec;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <server_ip>\n", argv[0]);
        return 1;
    }
    const char *server_ip = argv[1];

    struct ibv_device **dev_list = ibv_get_device_list(NULL);
    if (!dev_list) {
        fprintf(stderr, "Failed to get device list\n");
        return 1;
    }
    struct ibv_context *ctx = ibv_open_device(dev_list[0]);
    ibv_free_device_list(dev_list);
    if (!ctx) {
        fprintf(stderr, "Failed to open RDMA device\n");
        return 1;
    }

    struct ibv_port_attr port_attr;
    if (ibv_query_port(ctx, 1, &port_attr)) {
        fprintf(stderr, "Failed to query port\n");
        return 1;
    }

    struct ibv_pd *pd = ibv_alloc_pd(ctx);
    if (!pd) {
        fprintf(stderr, "Failed to allocate PD\n");
        return 1;
    }
    struct ibv_cq *cq = ibv_create_cq(ctx, CQ_SIZE, NULL, NULL, 0);
    if (!cq) {
        fprintf(stderr, "Failed to create CQ\n");
        return 1;
    }

    void *buf = malloc(BUF_SIZE);
    if (!buf) {
        fprintf(stderr, "Failed to allocate buffer\n");
        return 1;
    }
    memset(buf, 'A', BUF_SIZE);
    struct ibv_mr *mr = ibv_reg_mr(pd, buf, BUF_SIZE,
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
    if (!mr) {
        fprintf(stderr, "Failed to register MR\n");
        return 1;
    }

    struct ibv_qp_init_attr qp_attr = {
        .send_cq = cq, .recv_cq = cq,
        .cap = { .max_send_wr = 4, .max_recv_wr = 4, .max_send_sge = 1, .max_recv_sge = 1 },
        .qp_type = IBV_QPT_RC
    };
    struct ibv_qp *qp = ibv_create_qp(pd, &qp_attr);
    if (!qp) {
        fprintf(stderr, "Failed to create QP\n");
        return 1;
    }
    if (modify_qp_to_init(qp, 1)) {
        fprintf(stderr, "Failed to modify QP to INIT\n");
        return 1;
    }

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(TCP_PORT)
    };
    inet_pton(AF_INET, server_ip, &addr.sin_addr);

    if (connect(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    union ibv_gid my_gid;
    ibv_query_gid(ctx, 1, 0, &my_gid);
    int use_gid = (port_attr.link_layer == IBV_LINK_LAYER_ETHERNET);

    struct conn_data local = {
        .addr   = (uintptr_t)buf,
        .rkey   = mr->rkey,
        .qp_num = qp->qp_num,
        .lid    = port_attr.lid
    };
    memcpy(local.gid, &my_gid, 16);

    struct conn_data remote;
    ssize_t n __attribute__((unused));
    n = write(sock_fd, &local, sizeof(local));
    n = read(sock_fd, &remote, sizeof(remote));
    close(sock_fd);

    if (modify_qp_to_rtr(qp, 1, remote.qp_num, remote.lid, remote.gid, use_gid)) {
        fprintf(stderr, "Failed to modify QP to RTR\n");
        return 1;
    }
    if (modify_qp_to_rts(qp)) {
        fprintf(stderr, "Failed to modify QP to RTS\n");
        return 1;
    }

    printf("PING %s: %d bytes\n", server_ip, BUF_SIZE);

    struct ibv_wc wc;
    uint32_t seq = 0;

    while (1) {
        seq++;
        post_recv(qp);

        double t_start = get_time_us();
        rdma_write(qp, mr, buf, remote.addr, remote.rkey, seq);

        while (ibv_poll_cq(cq, 1, &wc) == 0);
        if (wc.status != IBV_WC_SUCCESS) {
            fprintf(stderr, "Send error: %d\n", wc.status);
            break;
        }

        while (ibv_poll_cq(cq, 1, &wc) == 0);
        if (wc.status != IBV_WC_SUCCESS) {
            fprintf(stderr, "Recv error: %d\n", wc.status);
            break;
        }

        double t_end = get_time_us();
        printf("seq=%u time=%.2f us\n", seq, t_end - t_start);

        sleep(1);
    }

    ibv_destroy_qp(qp);
    ibv_dereg_mr(mr);
    ibv_destroy_cq(cq);
    ibv_dealloc_pd(pd);
    ibv_close_device(ctx);
    free(buf);
    return 0;
}
