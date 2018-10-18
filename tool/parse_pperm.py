#!/usr/bin/env python3

import argparse
import os
import re
import sys
from typing import List, Optional

from argparse import ArgumentParser

USAGE = ""

DESC = """\
Reconstructs valid GAP partial permutation construction statements from their
string representations passed to this program via stdin."""

PPERM = r'^((?:\[\d+,\d+(?:,\d+)*\])*)((?:\(\d+(?:,\d+)*\))*)$'
PPERM_IDENTITY = r'^<identity partial perm on \[ (\d+(?:, \d+)*) \]>$'


def list_str_to_int(lst: str) -> List[int]:
    return list(map(lambda x: int(x.strip()), lst.split(',')))


def pperm_identity_constructor(domain: List[int]) -> str:
    return 'PartialPermOp((), {})'.format(domain)


def pperm_constructor(chains: Optional[List[List[int]]],
                      cycles: Optional[List[List[int]]]) -> str:
    mapping = {}

    def map_to(x: int, y: int):
        if x in mapping:
            fmt = "ambigous image given for domain element ''"
            raise ValueError(fmt.format(x))

        mapping[x] = y

    for chain in chains:
        for x, y in zip(chain[:-1], chain[1:]):
            map_to(x, y)

    for cycle in cycles:
        for x, y in zip(cycle, cycle[1:] + [cycle[0]]):
            map_to(x, y)

    domain = []
    image = []

    for x, y in mapping.items():
        domain.append(x)
        image.append(y)

    return 'PartialPermOp({}, {})'.format(domain, image)


if __name__ == '__main__':
    # program name
    progname = os.path.basename(__file__)

    # parse arguments
    def formatter_class(prog):
        return argparse.RawTextHelpFormatter(prog, max_help_position=80)

    parser = ArgumentParser(description=DESC, formatter_class=formatter_class)

    parser.add_argument('-f', '--infile',
                        help="read from IN_FILE instead of stdin")

    parser.add_argument('-o', '--outfile',
                        help="write to OUT_FILE instead of stdout")

    parser.add_argument('-s', '--sort', action='store_true',
                        help="sort result alphabetically")

    args = parser.parse_args()

    # parse input
    matcher_pperm_identity = re.compile(PPERM_IDENTITY)
    matcher_pperm = re.compile(PPERM)

    istream = open(args.infile, 'r') if args.infile is not None else sys.stdin

    if args.outfile is not None:
        open(args.outfile, 'w').close()

    result = []

    listmode = False
    for i, line in enumerate(istream):
        line = line.strip()

        if i == 0:
            if line.startswith('['):
                listmode = True

                line = line.replace(', (', '; (')
                line = line.replace(', [', '; [')
                line = line.replace(', <', '; <')
        else:
            if listmode:
                fmt = "{}: error: can only parse one input line in list mode\n{}"
                print(fmt.format(progname, line), file=sys.stderr)

                sys.exit(1)

        if listmode:
            pperms = [pperm.strip() for pperm in line[1:-1].split(';')]
        else:
            pperms = [line]

        for pperm in pperms:
            match_pperm_identity = matcher_pperm_identity.match(pperm)
            if match_pperm_identity is None:

                match_pperm = matcher_pperm.match(pperm)
                if match_pperm is None:
                    fmt = "{}: error: failed to parse input line:\n{}"
                    print(fmt.format(progname, line), file=sys.stderr)

                    sys.exit(1)

                chains, cycles = match_pperm.groups()

                if chains:
                    chains = [list_str_to_int(chain)
                              for chain in chains[1:-1].split('][')]

                if cycles:
                    cycles = [list_str_to_int(cycles)
                              for cycles in cycles[1:-1].split(')(')]

                result.append(pperm_constructor(chains, cycles))

            else:
                domain = list_str_to_int(match_pperm_identity.group(1))

                result.append(pperm_identity_constructor(domain))

    if istream != sys.stdin:
        istream.close()

    if args.sort:
        result = sorted(result)

    if listmode:
        result = '[ ' + ',\n  '.join(result) + ' ]'
    else:
        result = ',\n'.join(result)

    if args.outfile is not None:
        with open(args.outfile, 'r+', encoding='UTF-8') as outfile:
            outfile.write(result + '\n')

    else:
        print(result)
