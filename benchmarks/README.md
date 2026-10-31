# Reproducible bound benchmarks

Download the exact external dictionary once, then run identical workloads on both
machines. The download is pinned to an upstream commit and verified using the Git
blob hash; raw dictionary bytes are not modified or committed to this repository.

```
python3 tools/fetch_enable2k.py
make clean && make CXXFLAGS='-O3 -std=c++17 -Wall -Wextra -Wpedantic -march=native'
python3 tools/benchmark_bounds.py data/enable2k.txt --windows 4 5 6 --expect-score 3625
```

On Knights Landing, use GCC 14 and `-march=knl` instead of the local-native build.
The portable default build requires no AVX-specific instructions. No winner
between the 5950X/128 GB and Xeon Phi/400 GB is assumed.

The benchmark records dictionary and executable SHA-256 hashes, CPU/platform,
seed, every sampled partial board, every bound, table build time, query time,
workspace/table sizes, and per-depth pruning counts in `results/bounds.json`.
Use distinct output names on the two machines. Compiler version/flags, thread
count, and peak RSS should accompany any published hardware comparison; this
initial benchmark is single-threaded.

The same corpus is used for every method. It includes uniform random letters,
common-letter boards (`aeilnorst`), and partial restrictions of the incumbent.
Depths 6, 8, 10, 12, 14 and 15 are included, including easy and hard cases.
Incumbent masks are sampled without repetition. The threshold defaults to 3,625:
a bound at or below it excludes a strictly better board but does not exclude ties.
This differs from listing all boards with score at least 3,500.

Bound construction and timed evaluation are separate. `bench-bound` evaluates the
whole corpus repeatedly in one process with one compiled table. The benchmark
also runs a separate, untimed value pass for individual bound reporting; that
pass's table construction is not part of the reported query time. Report setup
and query time together when assessing a short-lived use case.

These are pruning microbenchmarks, not measurements of a full proof search and
not a comparison against Vanderkam's complete prover. GitHub Actions runs the
same pinned-dictionary benchmark and publishes its complete JSON as an artifact.

The benchmark also runs each window with `--force`, which synchronizes one
unfilled cell across all words. These variants use the same corpus and table
index, but perform extra support-mask work; compare their query costs as well as
pruning. See [the simultaneous 26-way algorithm](../docs/synchronized-bound.md).
