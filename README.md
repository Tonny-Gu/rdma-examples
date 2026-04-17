# RDMA Write Ping-Pong

Minimal RDMA write latency test using ibverbs.

## Build

```bash
make
```

## Usage

On server machine:
```bash
./server <ib_hca> <ib_port> <gid_index>
# Example: ./server mlx5_0 1 3
```

On client machine:
```bash
./client <ib_hca> <ib_port> <gid_index> <server_ip>
# Example: ./client mlx5_0 1 3 192.168.1.100
```

Parameters:
- `ib_hca`: RDMA device name (e.g., mlx5_0)
- `ib_port`: Device port number (typically 1)
- `gid_index`: GID table index for RoCE routing
- `server_ip`: Server IP address (client only)

## Finding Parameters with show_gids

The easiest way to find the correct parameters is using the `show_gids` command:

```bash
$ show_gids
DEV     PORT    INDEX   GID                                     IPv4            VERDEV
---     ----    -----   ---                                     ------------    ------
mlx5_0  1       0       fe80:0000:0000:0000:5200:e6ff:fea9:4f38                 v1 fabric-06
mlx5_0  1       1       fe80:0000:0000:0000:5200:e6ff:fea9:4f38                 v2 fabric-06
mlx5_0  1       2       0000:0000:0000:0000:0000:ffff:0ac8:0301 10.200.3.1      v1 fabric-06
mlx5_0  1       3       0000:0000:0000:0000:0000:ffff:0ac8:0301 10.200.3.1      v2 fabric-06
```

The columns map directly to CLI arguments:

| Column | CLI Argument | Description |
|--------|--------------|-------------|
| DEV    | `ib_hca`     | RDMA device name |
| PORT   | `ib_port`    | Device port number |
| INDEX  | `gid_index`  | GID table index |

**Choosing the right GID index:**

- Entries with **IPv4 addresses** (e.g., `10.200.3.1`) are for RoCEv1/v2 over IPv4
- Entries starting with **fe80:** are link-local IPv6 addresses
- **v1** = RoCEv1 (runs directly over Ethernet)
- **v2** = RoCEv2 (runs over UDP/IP, more commonly used)

For most setups, choose a GID index with an IPv4 address and **v2** (RoCEv2). In the example above, that would be **index 3**.

```bash
# Server (IP: 10.200.3.1)
./server mlx5_0 1 3

# Client
./client mlx5_0 1 3 10.200.3.1
```

## Output

```
Device: mlx5_0, Port: 1, GID Index: 3
PING server_ip: 8192 bytes
seq=1 time=12.34 us
seq=2 time=11.89 us
...
```

## Requirements

- libibverbs-dev
- RDMA-capable NIC (InfiniBand or RoCE)
