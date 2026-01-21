# RDMA Write Ping-Pong

Minimal RDMA write latency test using ibverbs.

## Build

```bash
make
```

## Usage

On server machine:
```bash
./server
```

On client machine:
```bash
./client <server_ip>
```

## Output

```
PING server_ip: 8192 bytes
seq=1 time=12.34 us
seq=2 time=11.89 us
...
```

## Requirements

- libibverbs-dev
- RDMA-capable NIC (InfiniBand or RoCE)
