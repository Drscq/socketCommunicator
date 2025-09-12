#!/usr/bin/env python3
"""
Compare avg end-to-end (send+ACK) latencies between:
 - ZeroMQ Router-Dealer Mode
 - EMP-toolkit NetIOMP

Data taken from provided measurements. Produces:
 - A Markdown-like table printed to stdout
 - A PNG figure with two panels: curves and difference
"""

from dataclasses import dataclass
from typing import List
import math
import os
import sys

import matplotlib.pyplot as plt


@dataclass
class Series:
    name: str
    sizes: List[int]
    avg_ms: List[float]


def main(out_path: str = None):
    # Payload sizes in bytes (shared across both series)
    sizes = [8, 64, 512, 4096, 32768, 262144, 1048576]

    # ZeroMQ avg latencies (ms)
    zmq_avg = [
        0.235765,
        0.241718,
        0.222789,
        0.21072,
        0.255222,
        0.478296,
        0.767953,
    ]

    # NetIOMP avg latencies (ms)
    netiomp_avg = [
        0.0903905,
        0.0908006,
        0.0908711,
        0.0927651,
        0.105374,
        0.17877,
        0.399736,
    ]

    zmq = Series("ZeroMQ ROUTER/DEALER", sizes, zmq_avg)
    nio = Series("EMP NetIOMP", sizes, netiomp_avg)

    # Print a Markdown table
    print("| Size (bytes) | ZeroMQ avg (ms) | NetIOMP avg (ms) | Diff (ZMQ-NIO) ms | Ratio (ZMQ/NIO) |")
    print("|-------------:|----------------:|-----------------:|------------------:|----------------:|")
    diffs = []
    ratios = []
    for s, a, b in zip(sizes, zmq.avg_ms, nio.avg_ms):
        diff = a - b
        ratio = (a / b) if b != 0 else float("inf")
        diffs.append(diff)
        ratios.append(ratio)
        print(f"| {s:>12} | {a:>16.6f} | {b:>16.6f} | {diff:>17.6f} | {ratio:>14.2f} |")

    # Plot
    if out_path is None:
        out_path = os.path.join(os.path.dirname(__file__), "netiomp_vs_zmq_latency.png")

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(8, 7), sharex=True, gridspec_kw={"height_ratios": [3, 2]})

    # Upper: both series
    ax1.plot(sizes, zmq.avg_ms, marker='o', label=zmq.name)
    ax1.plot(sizes, nio.avg_ms, marker='o', label=nio.name)
    ax1.set_xscale('log', base=2)
    ax1.set_ylabel('Avg end-to-end latency (ms)')
    ax1.grid(True, which='both', linestyle='--', alpha=0.4)
    ax1.legend()
    ax1.set_title('NetIOMP vs ZeroMQ latency across payload sizes')

    # Lower: difference
    ax2.plot(sizes, diffs, marker='s', color='#d62728')
    ax2.axhline(0.0, color='black', linewidth=0.8)
    ax2.set_xscale('log', base=2)
    ax2.set_xlabel('Payload size (bytes, log2)')
    ax2.set_ylabel('Diff (ZMQ - NetIOMP) ms')
    ax2.grid(True, which='both', linestyle='--', alpha=0.4)

    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    print(f"\nSaved figure to: {out_path}")


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else None
    main(out)
