#!/usr/bin/env python3
"""Fetch the unchanged upstream ENABLE2K file from an immutable commit."""
import argparse
import hashlib
import pathlib
import urllib.request

COMMIT = '0c53977ac3cfc7513d870d8afd5ab78a6e44565f'
BLOB = '83bd20c6bc4fabe90d32cd6d84a91a27310f9499'
URL = f'https://raw.githubusercontent.com/danvk/hybrid-boggle/{COMMIT}/wordlists/enable2k.txt'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=pathlib.Path, nargs='?', default=pathlib.Path('data/enable2k.txt'))
    args = parser.parse_args()
    with urllib.request.urlopen(URL, timeout=60) as response:
        content = response.read()
    actual = hashlib.sha1(b'blob ' + str(len(content)).encode() + b'\0' + content).hexdigest()
    if actual != BLOB:
        raise SystemExit(f'Unexpected upstream blob: {actual}; expected {BLOB}')
    if args.output.exists() and args.output.read_bytes() != content:
        raise SystemExit(f'Refusing to overwrite different content: {args.output}')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(content)
    print(f'{args.output}: sha256={hashlib.sha256(content).hexdigest()} git_blob={actual}')


if __name__ == '__main__':
    main()
