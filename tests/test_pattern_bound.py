#!/usr/bin/env python3
"""Exhaustive reduced-alphabet admissibility, monotonicity and symmetry tests."""
import functools
import itertools
import pathlib
import random
import subprocess
import sys
import tempfile

import test_score as reference

BIN = str(pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else './boggle-optimal').resolve())


def main():
    rng = random.Random(451800)
    tiles = {'aaaba', 'ababa', 'bababa', 'qqq', 'qa', 'qaq', 'aqqb', 'bbbbbb'}
    for n in range(2, 8):
        tiles.update(''.join(t) for t in itertools.product('abq', repeat=n) if rng.random() < 0.18)
    tokens = [t.replace('q', 'qu') for t in sorted(tiles)]
    tokens += ['QUA', 'qua', 'qat', '123', 'a', 'at', 'a' * 26]
    boards = [''.join(t) for t in itertools.product('abq', repeat=6)]
    patterns = [''.join(t) for t in itertools.product('.abq', repeat=6)]
    with tempfile.TemporaryDirectory() as folder:
        dictionary = pathlib.Path(folder) / 'words.txt'
        dictionary.write_text('\n'.join(tokens) + '\n')
        reference.DICTIONARY = str(dictionary)

        def run(command, data, *options):
            result = subprocess.run([BIN, command, str(dictionary), '--rows', '2', '--cols', '3',
                                     '--stdin', *options], input='\n'.join(data) + '\n',
                                    text=True, capture_output=True, check=True)
            return [int(line.rsplit(' ', 1)[1]) for line in result.stdout.splitlines()]

        scores = dict(zip(boards, run('score', boards)))
        for board in boards:
            assert scores[board] == reference.reference(board, 2, 3), board

        @functools.lru_cache(None)
        def maximum(pattern):
            if '.' not in pattern:
                return scores[pattern]
            i = pattern.index('.')
            return max(maximum(pattern[:i] + c + pattern[i + 1:]) for c in 'abq')

        previous = dict(zip(patterns, run('bound', patterns, '--method', 'inventory')))
        improved = 0
        for window in range(2, 7):
            bounds = dict(zip(patterns, run('bound', patterns, '--method', 'patterns', '--window', str(window))))
            for pattern, bound in bounds.items():
                assert maximum(pattern) <= bound <= previous[pattern], (window, pattern, bound, maximum(pattern))
                improved += bound < previous[pattern]
                # Restricting a wildcard cannot increase this bound.
                for i, cell in enumerate(pattern):
                    if cell == '.':
                        for letter in 'abq':
                            child = pattern[:i] + letter + pattern[i + 1:]
                            assert bounds[child] <= bound, (pattern, child)
                horizontal = pattern[:3][::-1] + pattern[3:][::-1]
                vertical = pattern[3:] + pattern[:3]
                assert bounds[horizontal] == bounds[vertical] == bound
            previous = bounds
        assert improved > 0
        # A letter absent from the dictionary alphabet must invalidate paths
        # containing it, not act as a wildcard or alias another letter.
        unknown = ['z.....', 'zzzzzz', 'abz...', 'zqb...']
        bounds = run('bound', unknown, '--method', 'patterns')
        for p, bound in zip(unknown, bounds):
            for completion in itertools.product(*['abq' if c == '.' else c for c in p]):
                assert reference.reference(''.join(completion), 2, 3) <= bound
        invalid = [('--window', '7'), ('--window', '-1'), ('--window', '3x'),
                   ('--rows', '2147483647'), ('--cols', '0'), ('--table-mib', '0'),
                   ('--method', 'missing'), ('--repeat', '0')]
        for options in invalid:
            result = subprocess.run([BIN, 'bound', str(dictionary), '--rows', '2', '--cols', '3',
                                     '--method', 'patterns', *options, '......'], capture_output=True)
            assert result.returncode != 0, options
    print(f'PASS: {len(boards)} independent scores; {len(patterns)} partial boards x 5 window lengths; '
          f'{improved} strict bound improvements; all completions, monotonicity, symmetries and invalid input')


if __name__ == '__main__':
    main()
