import sys
import argparse
import tempfile

import vmprof


parser = argparse.ArgumentParser(description='VMprof', prog="vmprof")

parser.add_argument('program', help='program')
parser.add_argument('args', nargs=argparse.REMAINDER, help='program arguments')
parser.add_argument('--web', nargs='?', metavar='url')
parser.add_argument('--no-cli', action='store_true', required=False)


args = parser.parse_args()

tmp = tempfile.NamedTemporaryFile()
vmprof.enable(tmp.fileno(), 1000)

try:
    sys.argv = args.args
    program = args.program
    execfile(program)
finally:
    vmprof.disable()
    stats = vmprof.read_profile(tmp.name, virtual_only=True)

    if not args.no_cli:
        vmprof.cli.show(stats)
    if args.web:
        vmprof.com.send(stats, args.program, args.args, args.web)
