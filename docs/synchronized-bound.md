# One-cell synchronization without 26 bound traversals

The fragment filter in [pattern-bound.md](pattern-bound.md) lets different words
choose different letters at a wildcard. The optional `--force` layer removes
that relaxation at one selected cell **simultaneously for every word**.

For an unfilled cell `i`, define

```
V_i(P) = max_{a in a..z} U_k(P[i=a]).
```

Here `U_k` is the raw fragment/inventory sum, including on complete boards. Every
completion uses one of those letters, so `max score(P) <= V_i(P) <= U_k(P)`.
The CLI continues to score a complete input board exactly. Internally, a raw
fragment bound on a complete board may exceed its score when words exceed `k`.

## Compute every child bound in one pass

For each fragment `g`, compute a 26-bit mask `A_g` of letters allowed at `i` by
some compatible occurrence of `g`:

- An occurrence avoiding `i` supports all 26 letters.
- An occurrence through `i` supports the fragment's letter at that position.
- OR masks over occurrences, including both orientations when compatible.

For a word, intersect its fragment masks. This enforces the same chosen letter
in every required fragment of that word. Also intersect with its inventory mask.
If its tile shortfall is below the number of blanks, every letter passes the
inventory check. If equal, only letters currently in deficit pass. A larger
shortfall makes the word impossible.

Add the word's score once to each supported letter's score bin. Words supporting
all letters are accumulated in a shared scalar instead of making 26 additions.
The maximum of the resulting 26 bins is exactly `V_i(P)`, not an approximation to
it. Full-word geometry beyond the fragments is still relaxed.

This computes 26 raw child bounds using **one short-path pass and one word pass**,
not 26 independent recursive evaluations. The implementation exposes the entire
26-entry array as `PatternBound::letter_bounds`, useful for later branch ordering.

## Orientation and storage

A posting packs the fragment ID and two orientation bits into its existing
32-bit slot. Symmetric wildcard patterns can support both directions even if the
complete fragment is asymmetric. The canonical location of the selected cell is
part of the scratch hash key; deduplicating only by pattern would be incorrect.
Fragment letters are retained in six-byte arrays. Each workspace adds one
32-bit support mask per fragment. The direct index and posting counts do not grow.

A wildcard `k`-path avoiding `i` makes every fragment possible for every chosen
letter. The subset-closure table detects this and skips geometric processing;
inventory synchronization can still strengthen the parent bound.

## Choice of cell and tests

`--force` chooses an unfilled cell of largest king-graph degree, breaking ties by
row-major index. The priority is independent of the window and of fixed letters,
which preserves monotonicity under additional fixed cells and increasing window
length. It need not preserve reflection symmetry when several cells tie.
`--force-cell N` chooses an explicit zero-based unfilled cell.

```
./boggle-optimal bound data/enable2k.txt --method patterns --window 5 --force perslatgsine....
```

The tests compare this bound with exact completion maxima on all 4,096 partial
2x3 boards at five window lengths, check both monotonicities and dominance over
the raw filter, and compare the simultaneous 26 scores with 26 separate raw
bound evaluations. The latter includes unknown alphabet symbols, asymmetric
fragments, symmetric wildcard patterns, and generation-counter wraparound.

This strengthens the unsynchronized fragment filter at one cell. It does not
synchronize all remaining letters or necessarily dominate exact word-union bounds. The benchmark reports its extra
cost and pruning separately; a tighter bound is not automatically a faster full
search. An adaptive proof engine should apply it only to surviving classes where
it saves more downstream work than it costs.
