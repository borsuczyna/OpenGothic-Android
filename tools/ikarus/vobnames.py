#!/usr/bin/env python3
"""Which vob classes do the MEM_SearchVobByName / MEM_SearchAllVobsByName names of a mod belong to.

Static check, nothing is executed: the constant names are taken from the source of the scripts, the class of
the vob is read from the nearest class header of the binary ZEN. The compatibility layer mirrors only the
movers (zCMover), so every other class is a name that a script searches and the layer cannot return.
"""
import argparse
import bisect
import collections
import re
import sys
from pathlib import Path

SEARCH = re.compile(r'MEM_SEARCH(ALL)?VOBS?BY(?:NAME)?\(([^)]*)\)', re.I)
HEADER = re.compile(rb'\[% ([A-Za-z0-9_:]+) \d+ \d+\]')
TOKEN  = re.compile(rb'[A-Z0-9_]')
FUNC   = re.compile(r'^func\s+\w+\s+(\w+)\s*\(', re.M | re.I)


def functions(source):
    """Body of every function, the key is the upper case name."""
    ret = {}
    for path in source.glob('*.d'):
        text = path.read_text(encoding='latin-1')
        for m in FUNC.finditer(text):
            start = text.find('{', m.end())
            if start < 0:
                continue
            depth, i = 0, start
            while i < len(text):
                if text[i] == '{':
                    depth += 1
                elif text[i] == '}':
                    depth -= 1
                    if depth == 0:
                        break
                i += 1
            ret.setdefault(m.group(1).upper(), text[m.start():i + 1])
    return ret


def searched(funcs):
    """name -> callers, and the callers that pass a variable instead of a constant."""
    names, dynamic = collections.defaultdict(set), {}
    for fn, body in funcs.items():
        for m in SEARCH.finditer(body):
            arg = m.group(2).strip()
            lit = re.fullmatch(r'"([^"]*)"', arg)
            if lit:
                names[lit.group(1).upper()].add(fn)
            else:
                dynamic.setdefault(fn, []).append(arg)
    return names, dynamic


def classes(world, names):
    """name -> Counter of the class headers that precede the name in the world."""
    data = world.read_bytes()
    upper = data.upper()
    heads = [(m.start(), m.group(1).decode().split(':')[0]) for m in HEADER.finditer(data)]
    starts = [h[0] for h in heads]
    ret = {}
    for name in names:
        key, pos = name.encode('latin-1', 'replace'), 0
        while True:
            i = upper.find(key, pos)
            if i < 0:
                break
            pos = i + 1
            if TOKEN.match(upper[i - 1:i]) or TOKEN.match(upper[i + len(key):i + len(key) + 1]):
                continue
            k = bisect.bisect_right(starts, i) - 1
            if k >= 0 and i - starts[k] <= 900:
                ret.setdefault(name, collections.Counter())[heads[k][1]] += 1
    return ret


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mod', type=Path, required=True, help='directory with the .d source of the scripts')
    parser.add_argument('--worlds', type=Path, required=True, help='directory with the extracted *.ZEN (binary)')
    parser.add_argument('--pattern', default='*.ZEN')
    args = parser.parse_args()
    sys.stdout.reconfigure(errors='backslashreplace')

    names, dynamic = searched(functions(args.mod))
    print(f'constant names: {len(names)}, callers with a variable name: {sorted(dynamic)}')

    found = collections.defaultdict(collections.Counter)
    for world in sorted(args.worlds.glob(args.pattern)):
        for name, count in classes(world, names).items():
            found[name].update(count)

    missing = sorted(n for n in names if n not in found)
    other   = sorted(n for n in names if n in found and 'zCMover' not in found[n])
    print(f'in a world: {len(found)}, not in any: {len(missing)}, with a mover: {len(found) - len(other)}, without: {len(other)}')
    for name in missing:
        print(f'  not found  {name}  <- {sorted(names[name])}')
    for name in other:
        print(f'  no mover   {name} {dict(found[name])} <- {sorted(names[name])}')


if __name__ == '__main__':
    main()
