# Potential Artifacts in Microbenchmarks

When benchmarking communication with very small payloads (e.g., 4B–1KB) on localhost, you may observe counterintuitive results, such as decreased time with increased data size. This can be caused by:

- **Overhead Dominance:** Fixed costs (threading, socket setup, context switching) can outweigh actual data transfer time for tiny messages.
- **System/Library Buffering:** The OS or ZeroMQ may batch or optimize larger messages, making them appear faster.
- **Measurement Noise:** Fast local tests are sensitive to system load, scheduling, and other background activity.

**Tip:** Use larger payloads (e.g., 4KB–1MB) and average over many runs to get more reliable trends.
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

### Running a specific test

You can also run a specific test executable with GoogleTest flags. For example, to run a specific test from the `test_mpc` suite 10 times:

```bash
./build/test/test_mpc --gtest_filter=MPCPartiesTest.NPartyAllToAllSumThreaded --gtest_repeat=10
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


## Simulating network conditions
sudo tc qdisc del dev lo root
 
sudo tc qdisc add dev lo root netem delay 1000ms
 