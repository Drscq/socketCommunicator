
import pandas as pd
import matplotlib.pyplot as plt
import io

# Paste the raw test output here
data = """
N, DataSize (B), Sequential (ms), Parallel (ms)
2, 4096, 17.4, 51.3
2, 16384, 27.8, 59.4
2, 65536, 62.9, 45.2
2, 262144, 11.3, 30.4
2, 1048576, 47.7, 64.9
4, 4096, 144.6, 161.2
4, 16384, 125.8, 166.1
4, 65536, 142.5, 140.2
4, 262144, 131.3, 99.7
4, 1048576, 126.0, 112.2
8, 4096, 191.8, 197.4
8, 16384, 196.5, 196.4
8, 65536, 175.4, 197.3
8, 262144, 195.7, 188.9
8, 1048576, 196.6, 197.3
14, 4096, 206.9, 203.1
14, 16384, 201.2, 203.6
14, 65536, 202.2, 203.6
14, 262144, 203.6, 204.4
14, 1048576, 208.3, 203.7
"""

# Read the data into a pandas DataFrame
# The first row of the string is the header, so we use it directly.
df = pd.read_csv(io.StringIO(data), skipinitialspace=True)

# Get unique data sizes for plotting
data_sizes = df['DataSize (B)'].unique()

# Create a figure with subplots for each data size
fig, axes = plt.subplots(nrows=len(data_sizes), ncols=1, figsize=(10, 5 * len(data_sizes)), sharex=True)
if len(data_sizes) == 1:
    axes = [axes] # Make it iterable if there's only one subplot

fig.suptitle('Sequential vs. Parallel Send Performance', fontsize=16)

for i, size in enumerate(data_sizes):
    ax = axes[i]
    subset = df[df['DataSize (B)'] == size]
    
    ax.plot(subset['N'], subset['Sequential (ms)'], marker='o', linestyle='-', label='Sequential')
    ax.plot(subset['N'], subset['Parallel (ms)'], marker='x', linestyle='--', label='Parallel')
    
    ax.set_title(f'Data Size: {size} Bytes')
    ax.set_ylabel('Time (ms)')
    ax.grid(True)
    ax.legend()

# Common X-axis label
axes[-1].set_xlabel('Number of Parties (N)')

plt.tight_layout(rect=[0, 0.03, 1, 0.96])

# Save the plot to a file
output_filename = 'performance_trends.png'
plt.savefig(output_filename)

print(f"Plot saved to {output_filename}")

# Plot 2: Time vs Data Size for each fixed N (to see scaling with payload)
unique_Ns = sorted(df['N'].unique())
fig2, axes2 = plt.subplots(nrows=len(unique_Ns), ncols=1, figsize=(10, 4 * len(unique_Ns)), sharex=True)
if len(unique_Ns) == 1:
    axes2 = [axes2]

fig2.suptitle('Time vs Data Size (by Number of Parties)', fontsize=16)
for i, n in enumerate(unique_Ns):
    ax = axes2[i]
    subset = df[df['N'] == n].sort_values('DataSize (B)')
    ax.plot(subset['DataSize (B)'], subset['Sequential (ms)'], marker='o', linestyle='-', label='Sequential')
    ax.plot(subset['DataSize (B)'], subset['Parallel (ms)'], marker='x', linestyle='--', label='Parallel')
    ax.set_title(f'N = {n}')
    ax.set_ylabel('Time (ms)')
    ax.grid(True)
    ax.legend()

axes2[-1].set_xlabel('Data Size (Bytes)')
plt.tight_layout(rect=[0, 0.03, 1, 0.96])

output_filename2 = 'performance_trends_by_datasize.png'
fig2.savefig(output_filename2)
print(f"Plot saved to {output_filename2}")

# Plot 3: Heatmaps for quick comparison across N and Data Sizes
seq_pivot = df.pivot(index='N', columns='DataSize (B)', values='Sequential (ms)')
par_pivot = df.pivot(index='N', columns='DataSize (B)', values='Parallel (ms)')

for pivot, title, fname in [
    (seq_pivot, 'Sequential Time Heatmap (ms)', 'performance_heatmap_sequential.png'),
    (par_pivot, 'Parallel Time Heatmap (ms)', 'performance_heatmap_parallel.png'),
]:
    fig_h, ax_h = plt.subplots(figsize=(8, 4))
    im = ax_h.imshow(pivot.values, aspect='auto', cmap='viridis')
    ax_h.set_title(title)
    ax_h.set_xlabel('Data Size (Bytes)')
    ax_h.set_ylabel('Number of Parties (N)')
    ax_h.set_xticks(range(len(pivot.columns)))
    ax_h.set_xticklabels(pivot.columns)
    ax_h.set_yticks(range(len(pivot.index)))
    ax_h.set_yticklabels(pivot.index)
    fig_h.colorbar(im, ax=ax_h, label='Time (ms)')
    fig_h.tight_layout()
    fig_h.savefig(fname)
    print(f"Plot saved to {fname}")
