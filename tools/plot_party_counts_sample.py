#!/usr/bin/env python3
import matplotlib.pyplot as plt

# Hardcoded sample values from test output
Ns = [2, 4, 6, 8, 10]
per_peer_ms = [1.03, 2.33, 3.64, 3.66, 5.84]
parallel_ms = [1.24, 2.03, 3.09, 3.99, 5.38]

plt.figure(figsize=(8, 5))
plt.plot(Ns, per_peer_ms, marker='o', label='Per-peer (sequential)')
plt.plot(Ns, parallel_ms, marker='x', linestyle='--', label='Parallel')
plt.title('Avg round time vs number of parties (1MB)')
plt.xlabel('Number of Parties (N)')
plt.ylabel('Time (ms)')
plt.grid(True, alpha=0.3)
plt.legend()
plt.tight_layout()
out = 'party_count_trends_sample.png'
plt.savefig(out, dpi=150)
print(f'Saved {out}')
