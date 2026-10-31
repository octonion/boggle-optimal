# Boggle Optimal

Independent, high-performance algorithms and certified exhaustive search for highest-scoring Boggle boards.

## Goal

Construct a **substantially faster, independently verifiable** proof of the optimal 4×4 Boggle board using the ENABLE2K dictionary. Dan Vanderkam established that `perslatgsineters` scores **3,625** and is globally optimal. We seek a new proof algorithm, not another claim to have discovered this board. A later goal is to extend the approach to unresolved word lists and 5×5 boards.

**Current status:** foundation only. The code scores complete boards exactly and includes a simple certified upper-bound baseline, but **does not yet prove global optimality**.

## Build

Requires a C++17 compiler, Make, and Python 3 for tests.

```sh
make
make test
./boggle-optimal score /path/to/enable2k.txt perslatgsineters
./boggle-optimal bound /path/to/enable2k.txt perslatgsinet....
./boggle-optimal bench /path/to/enable2k.txt perslatgsineters
```

Use the [ENABLE2K file from Vanderkam's repository](https://github.com/danvk/hybrid-boggle/tree/main/wordlists), without modifying its contents, for like-for-like scoring. The dictionary is intentionally not bundled; record its SHA-256 hash with benchmark results. Avoid comparing against `octonion/boggle`, which uses a different adapted word list and can have different tile restrictions.

Boards are lowercase row-major strings. `q` represents a `Qu` tile; word lengths for scoring count the full written `qu`. Words score 1 (3–4 letters), 2 (5), 3 (6), 5 (7), and 11 (8+); words are counted once per board irrespective of number of paths. All tiles in a word path are distinct and consecutive cells are king-move adjacent. The command supports `--rows N --cols M` (default 4×4). A partial board uses `.` for each unfilled cell.

### Validation

`make test` compares the scorer against a separate Python path-finding implementation on **all 15,625** 2×3 boards over five test symbols. It also checks the validity of the baseline partial-board bound on exhaustive completions of 250 random partial boards. These tests exercise lowercase input, Qu, duplicate dictionary entries, distinct-word scoring, and path constraints. They do **not** replace a full 4×4 certificate.

`bench` reports the throughput of 10,000 exact repeated score operations; use a larger, varied batch in future performance comparisons. No performance claim is made yet.

## Algorithms and references

See [proof-search research plan](docs/algorithms.md) for the proposed lookup-table, residual-bitmap, consistency-bound, and branch-and-bound experiments and their verification requirements. The initial inventory bound ignores adjacency and compatibility between distinct words, so it is intentionally far weaker than the sophisticated bounds used by Vanderkam.

Reference: Dan Vanderkam, [A Computational Proof of the Highest-Scoring Boggle Board](https://arxiv.org/abs/2507.02117) (2025), and the [hybrid-boggle code](https://github.com/danvk/hybrid-boggle).

## License

MIT. See [LICENSE](LICENSE).
