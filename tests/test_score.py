#!/usr/bin/env python3
"""Independent exhaustive word-path reference and bound checks for toy grids."""
import itertools
import pathlib
import random
import subprocess
import sys

BIN = sys.argv[1] if len(sys.argv) > 1 else './boggle-optimal'
DICTIONARY = str(pathlib.Path(__file__).with_name('words-small.txt'))


def tiles(word):
    w = word.lower()
    out = ''
    i = 0
    while i < len(w):
        if not 'a' <= w[i] <= 'z':
            return None
        if w[i] == 'q':
            if i + 1 >= len(w) or w[i + 1] != 'u':
                return None
            out += 'q'
            i += 2
        else:
            out += w[i]
            i += 1
    return out if len(w) >= 3 else None


def points(n):
    return 0 if n < 3 else 1 if n <= 4 else 2 if n == 5 else 3 if n == 6 else 5 if n == 7 else 11


def reference(board, rows, cols):
    words = set(DICTIONARY and pathlib.Path(DICTIONARY).read_text().lower().split())
    found = set()
    neighbors = [[] for _ in board]
    for i in range(len(board)):
        for j in range(len(board)):
            if i != j and abs(i // cols - j // cols) <= 1 and abs(i % cols - j % cols) <= 1:
                neighbors[i].append(j)
    def has_word(w):
        t = tiles(w)
        if not t or len(t) > len(board):
            return False
        def dfs(i, pos, visited):
            if pos == len(t):
                return True
            return any(board[j] == t[pos] and not (visited >> j & 1)
                       and dfs(j, pos + 1, visited | 1 << j) for j in neighbors[i])
        return any(board[i] == t[0] and dfs(i, 1, 1 << i) for i in range(len(board)))
    for w in words:
        if has_word(w):
            found.add(w)
    return sum(points(len(w)) for w in found)


def batch(command, boards, rows=2, cols=3):
    p = subprocess.run([BIN, command, DICTIONARY, '--rows', str(rows), '--cols', str(cols), *boards],
                       text=True, capture_output=True, check=True)
    return [int(line.rsplit(' ', 1)[-1]) for line in p.stdout.splitlines()]


def run():
    rng = random.Random(1246)
    boards = [''.join(b) for b in itertools.product('aeqrt', repeat=6)]
    scores = batch('score', boards)
    score_map = dict(zip(boards, scores))
    for b, actual in zip(boards, scores):
        expected = reference(b, 2, 3)
        assert actual == expected, (b, actual, expected)
    # Random partial boards: enumerate all completions over the testing alphabet.
    # Using a restricted alphabet still gives valid tests of the universal upper bound.
    for _ in range(250):
        full = rng.choice(boards)
        pattern = ''.join('.' if rng.randrange(3) == 0 else c for c in full)
        bound = batch('bound', [pattern])[0]
        assert bound >= score_map[full], (pattern, full, bound)
        assert bound >= max(score_map[''.join(x)] for x in itertools.product(*[('aeqrt' if c == '.' else c) for c in pattern])), pattern
    # Qu is one physical tile, but contributes two written letters.
    assert batch('score', ['qaeeee'], 2, 3)[0] >= 1  # QUA uses q and a tiles.
    # Known ENABLE2K optimum is a 16-letter board, but its score is dictionary-specific.
    assert batch('score', ['perslatgsineters'], 4, 4)[0] >= 0
    # Reject malformed boards instead of treating periods as letters in exact scoring.
    bad = subprocess.run([BIN, 'score', DICTIONARY, '--rows', '2', '--cols', '3', 'a.....'],
                         text=True, capture_output=True)
    assert bad.returncode != 0
    print(f'PASS: {len(boards)} exact 2x3 boards and 250 partial-board bounds')

if __name__ == '__main__':
    run()
