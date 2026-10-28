# Perceptron comparison

Custom uses 256 PC-indexed perceptrons, 30 bits of global history, signed 8-bit saturating weights, and one bias per perceptron. The score is the bias plus a weighted sum of history encoded as -1/+1. Scores >= 0 predict taken. Update on a wrong prediction or a score magnitude <= 72; then shift in the actual outcome. History starts at zero, history weights at zero, and biases at -1.

Custom state: 256 * 31 * 8 + 32-bit history + 32-bit cached score = **63,552 bits**, below the README limit of 65,792 bits. The Gshare/Tournament tables are not allocated in Custom mode. No parameter search was performed.

## Public trace results

Each trace starts a fresh process and predictor. Rates are percentages calculated from exact miss counts. The final row is the unweighted arithmetic mean of the six trace rates, matching the README's averaging method.

| Trace | Branches | Gshare:13 | Tournament:9:10:10 | Perceptron |
|---|---:|---:|---:|---:|
| fp_1.bz2 | 1546797 | 0.8253% | 0.9910% | 0.8181% |
| fp_2.bz2 | 2422049 | 1.6780% | 3.2460% | 1.1070% |
| int_1.bz2 | 3771697 | 13.8388% | 12.6222% | 7.8750% |
| int_2.bz2 | 3755315 | 0.4201% | 0.4255% | 0.2956% |
| mm_1.bz2 | 3014850 | 6.6959% | 2.5806% | 2.1937% |
| mm_2.bz2 | 2563897 | 10.1380% | 8.4832% | 7.2302% |
| **Mean** | — | **5.5993%** | **4.7248%** | **3.2533%** |

Relative reduction in mean misprediction rate: **41.90%** versus Gshare, **31.14%** versus Tournament. Custom wins on all six public traces. The six hidden traces have not been evaluated.

## Runtime

Built using the repository Makefile (`gcc -g -std=c99 -Werror`, no optimization flag). Each input was decompressed in memory before timing. Time includes process startup, input parsing, and prediction/training, but excludes decompression. Each trace/mode was repeated three times; the table sums the per-trace median elapsed times. These are local measurements, not a Gradescope time guarantee.

| Predictor | Total seconds |
|---|---:|
| gshare:13 | 1.755 |
| tournament:9:10:10 | 1.911 |
| custom | 3.071 |

## Validation and reproduction

- `make clean && make` succeeded.
- `predictor.c` compiled with `-Wall -Wextra -Werror` and the diff passed whitespace checks.
- All 30,000 predictions on a deterministic generated trace matched an independent Python integer reference, including PC collisions and changing outcomes.
- Direct assertions passed for both signed-weight saturation boundaries, initialization, history updates, and repeated initialization.
- Repeated benchmark runs produced identical miss counts.

From `src/`, build with `make`, then run each mode on each of the six traces:

```sh
bunzip2 -kc ../traces/int_1.bz2 | ./predictor --gshare:13
bunzip2 -kc ../traces/int_1.bz2 | ./predictor --tournament:9:10:10
bunzip2 -kc ../traces/int_1.bz2 | ./predictor --custom
```
