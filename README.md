# socketCommunicator

Minimal C++ (CMake + GoogleTest) project that experiments with ZeroMQ ROUTER/DEALER patterns and provides a latency benchmark tool.

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

## Run tests

```bash
ctest --test-dir build --output-on-failure
```

## Latency benchmark

The tool `latency_benchmark` measures the one-way time from a DEALER send to a ROUTER receive for a configurable payload.

Build the tool:

```bash
cmake --build build --target latency_benchmark -j
```

Run (defaults to 1 MiB payload, 20 iterations, localhost):

```bash
build/tools/latency_benchmark
```

Options:

- `--address <host>`: destination address (default `127.0.0.1`)
- `--base <port>`: base port used to compute per-id endpoints (default `10000`)
- `--size <bytes>`: payload size in bytes (default `1048576`)
- `--iters <n>`: number of iterations (default `20`)
- `--rtt_ms <ms>`: measured RTT in milliseconds (e.g., from `ping`)
- `--bandwidth_gbps <gbps>`: measured throughput in Gbps (e.g., from `iperf3`)

Example with 1 MiB and 5 iterations, including theoretical comparison using `ping` and `iperf3` results:

```bash
build/tools/latency_benchmark \
	--size 1048576 \
	--iters 5 \
	--rtt_ms 0.046 \
	--bandwidth_gbps 15.1
```

Output includes:

- Measured avg/median/p95 one-way latency (ms)
- Approximate throughput derived from the average (MiB/s)
- Theoretical one-way latency = RTT/2 + size/bandwidth (ms)
- Delta (measured - theoretical) and overhead percentage

Tips:

- RTT: `ping -c 5 <address>` and take the average
- Bandwidth: run `iperf3 -s` on one host, then `iperf3 -c <address> -n <total_bytes>` on the other
- For cross-host experiments, run the benchmark on the sender host and set `--address` to the receiver host