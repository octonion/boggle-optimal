# Proof-search research plan

The initial objective is a new, faster, independently auditable certificate that no 4x4 board scores above 3,625 under the unmodified ENABLE2K word list and Vanderkam's scoring conventions. The initial engine is **not a proof of optimality**.

## Baseline and workload integrity

Vanderkam's 2025 result lists `perslatgsineters` at 3,625 and 32 symmetry classes scoring at least 3,500. His original reported 23,000 CPU hours were spent finding *all* such boards, not solely proving an incumbent of 3,625. Future comparisons should explicitly report the threshold, dictionary hash, scoring rules, hardware, and whether a complete list or only exclusion above a threshold was requested.

The `inventory_bound` function is a deliberately weak *valid* relaxation: each word is counted if its required tile counts can be supplied by already fixed cells and unfilled cells, independently of every other word. This is a correctness baseline, not the proposed primary bound. A filled board is scored exactly.

## Experimental priorities

1. **Local bound viability.** Before engineering large lookup tables, measure the lower limit on their slack at the known optimum and the 31 other high-scoring symmetry classes. If an optimistic envelope stays far above 3,625 even on completely filled high-scoring boards, reject it or strengthen it before a search.
2. **Reproducible pruning curves.** Apply every valid bound to an identical, seeded sample of partial board classes. Record bound, time per call, and eliminated-node fractions by fixed-cell count. Compare search-tree *work*, not just operations per second.
3. **Batch exact residual evaluation.** With a small set of unresolved cells, compute word-existence bitsets across all completions and sum each word once. Memoize coordinate-wise max reductions. Distinguish table build time from reuse time.
4. **Consistent small-factor bounds.** Examine weighted short-substring charging, overlapping-cell correction, variable elimination, and cost shifting. Every optimization must retain a separate mathematical proof of admissibility and an independent exact-score oracle for tests.
5. **Adaptive branching and certificates.** Use bound reductions and predicted surviving work per child; store proof certificates that are valid under explicitly stated parent constraints.

## Correctness requirements

For every partial board P, a claimed upper bound U must satisfy U(P) >= S(B) for **every** legal completion B. Passing randomly generated tests is necessary but never sufficient for the proof. For small alphabet/grid experiments, enumerate every completion and test U against the true maximum. For larger instances, audit the mathematical relaxation before allowing pruning. Evidence of pruning should be reproducible, including hash-identified dictionaries, exact thresholds, and complete coverage of search partitions.

## Architecture and performance

C++17 is the initial implementation language. The production search should support a 5950X (Zen 3) and Xeon Phi Knights Landing (68 cores, 272 threads, ~400 GB RAM). Kernels must not assume AVX-512BW or AVX-512VPOPCNTDQ on KNL. Provide a portable reference and architecture-specific versions only where measured. Share immutable tables rather than duplicating them in every thread. Include RSS, CPU time, wall time, work completed, and checkpoint integrity in benchmarks.

## Sources

- Dan Vanderkam, [A Computational Proof of the Highest-Scoring Boggle Board](https://arxiv.org/abs/2507.02117), 2025.
- Dan Vanderkam, [hybrid-boggle reference implementation](https://github.com/danvk/hybrid-boggle).
- Dan Vanderkam, [After 20 Years, the Globally Optimal Boggle Board](https://www.danvk.org/2025/04/23/boggle-solved.html), 2025.
- Rina Dechter, *Mini-Buckets: A General Scheme for Bounded Inference*, Journal of the ACM **51** (2004), 107–153.
- David Allouche et al., *Computational Protein Design as a Cost Function Network Optimization Problem*, Artificial Intelligence **212** (2014), 59–79.
