# Boggle Optimal

Independent, high-performance algorithms and certified exhaustive search for highest-scoring Boggle boards.

## Goal

Construct a **substantially faster, independently verifiable** proof of the optimal 4×4 Boggle board using the ENABLE2K dictionary. Dan Vanderkam established that `perslatgsineters` scores **3,625** and is globally optimal. We seek a new proof algorithm, not another claim to have discovered this board. A later goal is to extend the approach to unresolved word lists and 5×5 boards.

**Current status:** an exact scorer, the inventory baseline, and a tested **compiled fragment-table upper bound**. The new bound checks every short fragment of a word, counts each word once, and is no weaker than the inventory baseline. This is a screening stage, **not yet a global optimality proof or a demonstrated speedup over Vanderkam's prover**.

## Build

Requires a C++17 compiler, Make, and Python 3 for tests.

```sh
make
make test
./boggle-optimal score /path/to/enable2k.txt perslatgsineters
./boggle-optimal bound /path/to/enable2k.txt --method patterns --window 5 perslatgsinet....
./boggle-optimal bench /path/to/enable2k.txt perslatgsineters
```

Use the [ENABLE2K file from Vanderkam's repository](https://github.com/danvk/hybrid-boggle/tree/main/wordlists), without modifying its contents, for like-for-like scoring. The dictionary is intentionally not bundled; record its SHA-256 hash with benchmark results. Avoid comparing against `octonion/boggle`, which uses a different adapted word list and can have different tile restrictions.

Boards are lowercase row-major strings. `q` represents a `Qu` tile; word lengths for scoring count the full written `qu`. Words score 1 (3–4 letters), 2 (5), 3 (6), 5 (7), and 11 (8+); words are counted once per board irrespective of number of paths. All tiles in a word path are distinct and consecutive cells are king-move adjacent. The command supports `--rows N --cols M` (default 4×4). A partial board uses `.` for each unfilled cell.

### Stronger bounds

The pattern index is compiled once and reused for a batch of partial boards.
Windows of 2 through 6 tiles are supported; longer windows yield tighter bounds
but require more memory. The five-tile direct index is about 57 MiB before sparse
postings; the six-tile index is about 1.5 GiB. Tables are immutable and shareable;
each worker needs only its own scratch workspace.

```sh
python3 tools/fetch_enable2k.py
python3 tools/benchmark_bounds.py data/enable2k.txt --windows 4 5 6 --expect-score 3625
```

See [bound construction and proof](docs/pattern-bound.md) and
[benchmark methodology](benchmarks/README.md). The default method remains
`inventory`; request `--method patterns` explicitly. Pattern bounds currently
support grids of at most 16 cells, while exact scoring and inventory bounds
support up to 25. A long all-wildcard path makes the fragment filter ineffective;
the implementation detects this and falls back to compact inventory checks.

### Validation

`make test` compares the scorer against a separate Python path-finding implementation on **all 15,625** 2×3 boards over five test symbols. It also checks the validity of the baseline partial-board bound on exhaustive completions of 250 random partial boards. The new bound is additionally checked on **all 4,096 partial 2×3 boards** over a three-symbol alphabet plus wildcard, at five window lengths, against exact completion maxima. Tests cover monotonicity, symmetry, Qu, absent symbols, duplicate paths, separate workspaces and generation wraparound. They do **not** replace a full 4×4 certificate.

`bench` reports JSON timings for exact scoring. `bench-bound` separately reports bound construction and batch evaluation time, checksums, memory sizes and pruning counts. No end-to-end proof-speed claim is made.

## Algorithms and references

See [proof-search research plan](docs/algorithms.md) for the proposed lookup-table, residual-bitmap, consistency-bound, and branch-and-bound experiments and their verification requirements. The initial inventory bound ignores adjacency and compatibility between distinct words, so it is intentionally far weaker than the sophisticated bounds used by Vanderkam.

Reference: Dan Vanderkam, [A Computational Proof of the Highest-Scoring Boggle Board](https://arxiv.org/abs/2507.02117) (2025), and the [hybrid-boggle code](https://github.com/danvk/hybrid-boggle).

## License

MIT. See [LICENSE](LICENSE).
