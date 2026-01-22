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
- `ib_hca`: RDMA device name (e.g., mlx5_0) - use `ibv_devinfo` to list
- `ib_port`: Device port number (typically 1)
- `gid_index`: GID table index (use `ibv_devinfo -v` to find valid indices)
- `server_ip`: Server IP address (client only)

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
