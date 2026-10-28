# CSE240A Branch Predictors

Branch prediction simulator with Gshare, Tournament, and a custom online perceptron predictor. Predictor implementations are in `src/predictor.c`; the supplied driver reads branch PCs and outcomes from traces.

The original assignment requirements are preserved in [README_ASSIGNMENT.md](README_ASSIGNMENT.md). `README.txt` is an older assignment version with different grading details.

## Build and run

```sh
cd src
make
bunzip2 -kc ../traces/int_1.bz2 | ./predictor --custom
bunzip2 -kc ../traces/int_1.bz2 | ./predictor --gshare:13
bunzip2 -kc ../traces/int_1.bz2 | ./predictor --tournament:9:10:10
```

An uncompressed trace can be passed as a file argument. Add `--verbose` to print each prediction. Each trace record contains a hexadecimal PC and an outcome (`0` = not taken, `1` = taken):

```text
0x40d7f9 0
0x40d81e 1
```

## Predictor design

- **Gshare:** XOR the PC with global history to index a table of 2-bit saturating counters.
- **Tournament:** Combine global and per-PC local-history predictors with a 2-bit chooser indexed by global history. Update the chooser when the two predictions disagree.
- **Custom perceptron:** Use the low 8 PC bits to select one of 256 perceptrons. Each has 30 signed 8-bit history weights and one signed 8-bit bias.

For Custom, encode each of the last 30 outcomes as `x[i] = +1` for taken and `-1` for not taken. Compute:

```text
y = bias + sum(weight[i] * x[i])
prediction = taken if y >= 0, otherwise not taken
```

On a wrong prediction or low confidence (`abs(y) <= 72`), update the bias by `t` and each weight by `t * x[i]`, where `t` is the actual outcome encoded as -1/+1. Weights saturate at [-128, 127]. Training uses the history from prediction; the actual outcome is then shifted into the least significant history bit. History and history weights start at zero; biases start at -1 to initially predict not taken.

The predictor learns online after each branch. Prediction needs only integer additions and subtractions. Long-history correlations are its main advantage; linear decision boundaries and collisions between PCs sharing a perceptron are limitations.

### Custom storage budget

| State | Size |
|---|---:|
| Weights and biases | 256 × 31 × 8 = 63,488 bits |
| Global history register | 32 bits |
| Cached prediction score | 32 bits |
| **Total Custom state** | **63,552 bits (7,944 bytes)** |
| Assignment limit | 65,792 bits (8,224 bytes) |

The other predictors' tables are not allocated in Custom mode. Dimensions, history mask, and training threshold are compile-time constants.

## Experimental results

The baselines use the assignment configurations: `gshare:13` and `tournament:9:10:10`. Each run starts with a fresh predictor. Results below are misprediction rates on the six supplied traces; the mean is the arithmetic mean of the six rates, rather than a branch-count-weighted rate.

| Trace | Gshare | Tournament | Perceptron |
|---|---:|---:|---:|
| fp_1 | 0.8253% | 0.9910% | **0.8181%** |
| fp_2 | 1.6780% | 3.2460% | **1.1070%** |
| int_1 | 13.8388% | 12.6222% | **7.8750%** |
| int_2 | 0.4201% | 0.4255% | **0.2956%** |
| mm_1 | 6.6959% | 2.5806% | **2.1937%** |
| mm_2 | 10.1380% | 8.4832% | **7.2302%** |
| **Mean** | **5.5993%** | **4.7248%** | **3.2533%** |

Perceptron wins on all six public traces, reducing the mean misprediction rate by **41.90% versus Gshare** and **31.14% versus Tournament**. No parameter search was performed; the six hidden traces have not been evaluated.

Local elapsed time across the six traces was **1.755 s** for Gshare, **1.911 s** for Tournament, and **3.071 s** for Perceptron. These totals sum the per-trace medians of three runs using the default Makefile, without compiler optimization flags. Timing includes process startup and trace parsing, and excludes decompression. Custom improves accuracy at a higher execution cost in this simulator.

See [PERCEPTRON_RESULTS.md](PERCEPTRON_RESULTS.md) for branch counts, measurement details, and validation results.

## Validation

The project builds with the supplied Makefile, and `predictor.c` passes `-Wall -Wextra -Werror`. All 30,000 predictions on a deterministic synthetic trace matched an independent Python reference. Additional assertions checked weight saturation, initialization, history updates, and reinitialization. Repeated public-trace runs produced identical miss counts.
