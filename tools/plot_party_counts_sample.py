#!/usr/bin/env python3
import matplotlib.pyplot as plt
from pathlib import Path

# Hardcoded sample values from test output
Ns = [2, 4, 6, 8, 10]
per_peer_ms = [1.03, 2.33, 3.64, 3.66, 5.84]
parallel_ms = [1.24, 2.03, 3.09, 3.99, 5.38]

# Pub-Sub results (broadcast+ACKs) from model/run logs for 1MB payload
# [PUB] Parties 2: avg round (broadcast+ACKs) = 1.01197 ms (payload 1MB)
# [PUB] Parties 4: avg round (broadcast+ACKs) = 1.79139 ms (payload 1MB)
# [PUB] Parties 6: avg round (broadcast+ACKs) = 2.96109 ms (payload 1MB)
# [PUB] Parties 8: avg round (broadcast+ACKs) = 3.19274 ms (payload 1MB)
# [PUB] Parties 10: avg round (broadcast+ACKs) = 4.45297 ms (payload 1MB)
pubsub_ms = [1.01197, 1.79139, 2.96109, 3.19274, 4.45297]

plt.figure(figsize=(8, 5))
plt.plot(Ns, per_peer_ms, marker='o', label='Per-peer (sequential)')
plt.plot(Ns, parallel_ms, marker='x', linestyle='--', label='Parallel')
plt.plot(Ns, pubsub_ms, marker='s', linestyle='-.', label='Pub-Sub (broadcast+ACKs)')
plt.title('Avg round time vs number of parties (1MB)')
plt.xlabel('Number of Parties (N)')
plt.ylabel('Time (ms)')
plt.grid(True, alpha=0.3)
plt.legend()
plt.tight_layout()
out = str((Path(__file__).resolve().parent / 'party_count_trends_sample.png'))
plt.savefig(out, dpi=150)
print(f'Saved {out}')
