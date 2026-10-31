# Compiled fragment-table upper bound

This is a tested screening bound, not a complete 4x4 optimality prover and not a
claim to outperform Vanderkam's orderly-tree method. It strengthens this
repository's inventory baseline while replacing full-word path enumeration with
short-path lookups. Its major limitation is described below.

## Admissibility

Normalize a word to tile symbols, replacing every written `qu` by one `q` tile.
Scoring still uses written length. For window length `k`, let `G_k(w)` contain all
contiguous tile substrings of length `min(k, |w|)`. Reversed substrings are
identified because board adjacency is undirected. Repeated substrings count once.

For partial board `P`, let `I(w,P)` mean that the word fits the grid and its tile
inventory: `sum_c max(0, demand_w(c) - fixed_P(c)) <= blanks(P)`. Let `Q(g,P)`
mean that some simple board path can spell substring `g` on some completion of
`P`. Define

```
U_k(P) = sum_w points(w) * I(w,P) * product_{g in G_k(w)} Q(g,P).
```

If a completion contains `w`, it satisfies the inventory condition and contains
every contiguous fragment of `w` on a simple path. Thus every actual word's score
appears in the sum. Each word contributes once, regardless of how many paths or
fragments it has. This proves `score(B) <= U_k(P)` for every completion `B`.

Increasing `k` cannot increase the bound: each shorter fragment is contained in
one of the longer fragments, unless the complete word is already being tested.
Fixing an additional cell cannot increase the bound either. The bound is no
larger than the inventory sum. The public CLI scores complete boards exactly,
matching the baseline's complete-board behavior.

This is NOT sufficient for a word to occur: its fragments may require mutually
inconsistent completions, or mutually incompatible placements. Different words
can also disagree. No statistical assumptions enter the bound or pruning rule.

## Lookup representation

1. Intern the dictionary's distinct fragments. Retain each word's fragment IDs,
   sparse tile counts, and score.
2. For each fragment, enumerate its at most `2^k` fixed-letter/wildcard patterns.
   Canonicalize reversals and deduplicate patterns before indexing them.
3. Store a direct base-`(alphabet_size + 1)` index into compressed, contiguous
   lists of compatible fragment IDs. Wildcards are a separate final digit.
4. Precompute the grid's simple paths through at most `k` distinct cells, retaining
   one orientation of each path. There are 4,484 such paths of lengths 2 through 5
   on the 4x4 king graph.
5. At query time, turn these paths into pattern indexes. A small scratch hash set
   prevents duplicate patterns from rescanning their lists. Generation markers
   record which fragments remain possible.
6. Check each word's requirements and inventory once, adding its score once.

Only dictionary-relevant symbols enter the index. A fixed symbol outside that
alphabet invalidates any fragment path containing it; it is not a wildcard.
`Qu` words occupying two tiles are included when their written length is at
least three. Words occupying more cells than the grid are excluded.

The direct index requires `4 * sum_{j=2}^k (A+1)^j` bytes, plus the sparse postings.
For 26 symbols this is about 57 MiB at `k=5` and 1.5 GiB at `k=6`. This is not a
dense bitset of all words for every pattern. The postings contain at most `2^k`
entries per distinct fragment, before reversal deduplication.

`PatternBound` is immutable after construction. Give each thread its own
`Workspace`; do not concurrently reuse one workspace. Tables can be shared
between threads without duplication. Dictionary/trie storage, word requirements,
geometry and workspace allocations are additional to the reported `table_bytes`.
The `--table-mib` budget limits the direct index plus postings, not total RSS.

## A useful shortcut and a hard limitation

If the unfilled cells contain a simple path of length `k`, every fragment of
length at most `k` is possible. In that case this bound is exactly the inventory
bound: larger lookup throughput cannot improve its pruning power.

A 65,536-entry byte table for a 4x4 grid detects this case from the unfilled-cell
mask. It is constructed by marking length-`k` path masks and taking subset
closure. The query then skips fragment matching and uses compact inventory
checks. An entirely unfilled board returns the precomputed dictionary score sum.

Consequently this is principally a middle/late-search screening stage. Longer
windows are tighter but have much larger indexes and more geometric paths. Do
not extrapolate a local speedup into an end-to-end optimality-proof speedup.
Synchronizing the remaining letter choices across words, or compiling exact
small residual problems, is still needed for difficult surviving classes.

## Usage and verification

```
./boggle-optimal bound data/enable2k.txt --method patterns --window 5 perslatgsine....
printf '%s\n' perslatgsine.... aaaaaaaaaaaa.... |
  ./boggle-optimal bound data/enable2k.txt --method patterns --stdin
```

`--window` accepts 2 through 6. Lookup geometry currently supports at most 16
cells; exact scoring and the inventory baseline still support up to 25 cells.
The default method remains `inventory` for compatibility and to avoid unexpected
large table construction. Tables are compiled once per process, not per query.

Tests include an independent word-by-word solver on all 729 complete 2x3 boards
over `a,b,q`, and all 4,096 partial boards over `.a,b,q`, for each of five window
lengths. Exact completion maxima, inventory dominance, window monotonicity,
restriction monotonicity, reflections, absent dictionary symbols, Qu, repeated
words, generation wraparound, and separate workspaces are checked. The original
15,625-board reference comparison and 250 baseline-bound checks also remain.

## Relationship to existing work

Vanderkam's sum/union bound counts words that can occur on some completion.
This fragment filter is a cheaper necessary-condition relaxation of that word
feasibility test, not a new proof that sum/union is sufficient. His orderly-tree
bounds additionally synchronize letter choices, which this implementation does
not do. See [the research plan](algorithms.md) and
[Vanderkam's methodology](https://www.danvk.org/2025/02/13/boggle2025.html).
