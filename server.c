#include "rdma_common.h"

int main(void) {
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

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(TCP_PORT),
        .sin_addr.s_addr = INADDR_ANY
    };
    bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(listen_fd, 1);

    printf("Waiting for client...\n");
    int conn_fd = accept(listen_fd, NULL, NULL);

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
    n = read(conn_fd, &remote, sizeof(remote));
    n = write(conn_fd, &local, sizeof(local));
    close(conn_fd);
    close(listen_fd);

    if (modify_qp_to_rtr(qp, 1, remote.qp_num, remote.lid, remote.gid, use_gid)) {
        fprintf(stderr, "Failed to modify QP to RTR\n");
        return 1;
    }
    if (modify_qp_to_rts(qp)) {
        fprintf(stderr, "Failed to modify QP to RTS\n");
        return 1;
    }

    printf("Connected. Echoing packets...\n");

    struct ibv_wc wc;
    while (1) {
        post_recv(qp);

        while (ibv_poll_cq(cq, 1, &wc) == 0);
        if (wc.status != IBV_WC_SUCCESS) {
            fprintf(stderr, "CQ error: %d\n", wc.status);
            break;
        }

        uint32_t seq = ntohl(wc.imm_data);
        rdma_write(qp, mr, buf, remote.addr, remote.rkey, seq);

        while (ibv_poll_cq(cq, 1, &wc) == 0);
    }

    ibv_destroy_qp(qp);
    ibv_dereg_mr(mr);
    ibv_destroy_cq(cq);
    ibv_dealloc_pd(pd);
    ibv_close_device(ctx);
    free(buf);
    return 0;
}
