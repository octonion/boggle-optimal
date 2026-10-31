#!/usr/bin/env python3
"""Compare bounds on identical seeded partial boards; keep setup and query costs separate."""
import argparse
import hashlib
import json
import math
import pathlib
import platform
import random
import statistics
import subprocess

INCUMBENT = 'perslatgsineters'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dictionary', type=pathlib.Path)
    parser.add_argument('--binary', type=pathlib.Path, default=pathlib.Path('./boggle-optimal'))
    parser.add_argument('--samples', type=int, default=64, help='Samples per family and depth (incumbent masks capped)')
    parser.add_argument('--seed', type=int, default=451800)
    parser.add_argument('--windows', type=int, nargs='+', default=[4, 5])
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--threshold', type=int, default=3625)
    parser.add_argument('--expect-score', type=int)
    parser.add_argument('--output', type=pathlib.Path, default=pathlib.Path('results/bounds.json'))
    args = parser.parse_args()
    if args.samples < 1 or args.repeats < 1 or any(w < 2 or w > 6 for w in args.windows):
        parser.error('Positive samples/repeats and window lengths 2..6 required')
    binary = str(args.binary.resolve())
    rng = random.Random(args.seed)
    corpus = []
    for family, alphabet in [('uniform', 'abcdefghijklmnopqrstuvwxyz'), ('common', 'aeilnorst'), ('incumbent', '')]:
        for fixed in [6, 8, 10, 12, 14, 15]:
            target = min(args.samples, math.comb(16, fixed)) if family == 'incumbent' else args.samples
            seen = set()
            while len(seen) < target:
                board = list(INCUMBENT) if family == 'incumbent' else [rng.choice(alphabet) for _ in range(16)]
                for i in rng.sample(range(16), 16 - fixed):
                    board[i] = '.'
                pattern = ''.join(board)
                if pattern not in seen:
                    seen.add(pattern)
                    corpus.append({'family': family, 'fixed': fixed, 'pattern': pattern})
    stdin = '\n'.join(row['pattern'] for row in corpus) + '\n'
    digest = hashlib.sha256(args.dictionary.read_bytes()).hexdigest()
    score_run = subprocess.run([binary, 'score', str(args.dictionary), INCUMBENT], text=True, capture_output=True, check=True)
    incumbent_score = int(score_run.stdout.split()[-1])
    if args.expect_score is not None and incumbent_score != args.expect_score:
        raise AssertionError((incumbent_score, args.expect_score))
    report = {'dictionary_sha256': digest, 'binary_sha256': hashlib.sha256(args.binary.read_bytes()).hexdigest(),
              'platform': platform.platform(), 'seed': args.seed, 'threshold': args.threshold,
              'incumbent_score': incumbent_score, 'samples': corpus, 'variants': []}
    cpuinfo = pathlib.Path('/proc/cpuinfo')
    if cpuinfo.exists():
        report['cpu'] = next((line.split(':', 1)[1].strip() for line in cpuinfo.read_text().splitlines()
                              if line.startswith('model name')), 'unknown')
    baseline = None
    previous = {}
    raw_bounds = {}
    for window, force in [(None, False), *[(w, f) for w in sorted(set(args.windows)) for f in (False, True)]]:
        method = 'inventory' if window is None else 'patterns'
        options = ['--method', method, '--stdin'] + ([] if window is None else ['--window', str(window)])
        if force:
            options.append('--force')
        bound_run = subprocess.run([binary, 'bound', str(args.dictionary), *options], input=stdin,
                                   text=True, capture_output=True, check=True)
        values = [int(line.rsplit(' ', 1)[1]) for line in bound_run.stdout.splitlines()]
        assert len(values) == len(corpus)
        if baseline is None:
            baseline = values
        assert all(a <= b for a, b in zip(values, baseline))
        if force in previous:
            assert all(a <= b for a, b in zip(values, previous[force]))
        previous[force] = values
        if force:
            assert all(a <= b for a, b in zip(values, raw_bounds[window]))
        else:
            raw_bounds[window] = values
        assert all(value >= incumbent_score for row, value in zip(corpus, values) if row['family'] == 'incumbent')
        bench = subprocess.run([binary, 'bench-bound', str(args.dictionary), *options,
                                '--repeat', str(args.repeats), '--threshold', str(args.threshold)],
                               input=stdin, text=True, capture_output=True, check=True)
        timing = json.loads(bench.stdout)
        assert timing['checksum'] == sum(values) * args.repeats
        groups = []
        for family in ['uniform', 'common', 'incumbent']:
            for fixed in [6, 8, 10, 12, 14, 15]:
                selected = [v for row, v in zip(corpus, values) if row['family'] == family and row['fixed'] == fixed]
                groups.append({'family': family, 'fixed': fixed, 'samples': len(selected),
                               'median_bound': statistics.median(selected),
                               'pruned': sum(v <= args.threshold for v in selected)})
        variant = {'name': method if window is None else f'patterns-{window}' + ('-forced' if force else ''), 'timing': timing, 'groups': groups, 'bounds': values}
        report['variants'].append(variant)
        print(json.dumps({k: v for k, v in variant.items() if k != 'bounds'}), flush=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({'output': str(args.output), 'dictionary_sha256': digest,
                      'incumbent_score': incumbent_score, 'cpu': report.get('cpu', 'unknown')}))


if __name__ == '__main__':
    main()
