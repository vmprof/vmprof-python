import argparse
import runpy
import sys
import tempfile

import vmprof


OUTPUT_CLI = 'cli'
OUTPUT_WEB = 'web'
OUTPUT_FILE = 'file'


def create_argument_parser():
    parser = argparse.ArgumentParser(
        description='VMprof',
        prog="vmprof"
    )

    parser.add_argument(
        'program',
        help='program'
    )
    parser.add_argument(
        'args',
        nargs=argparse.REMAINDER,
        help='program arguments'
    )

    parser.add_argument(
        '--enable-nonvirtual', '-n',
        action='store_true',
        help='Report native calls'
    )
    parser.add_argument(
        '--period', '-p',
        type=int,
        default=0.001,
        help='Sampling period (in microseconds)'
    )
    parser.add_argument(
        '--web-auth',
        help='Authtoken for your acount on the server, works only when --web is used'
    )

    output_mode_args = parser.add_mutually_exclusive_group()
    output_mode_args.add_argument(
        '--web',
        metavar='url',
        help='Upload profiling stats to a remote server'
    )
    output_mode_args.add_argument(
        '--output', '-o',
        metavar='file.prof',
        type=argparse.FileType('w+b'),
        help='Save profiling data to file'
    )
    return parser


def show_stats(filename, output_mode, args):
    if output_mode == OUTPUT_FILE:
        return

    stats = vmprof.read_profile(
        filename,
        virtual_only=not args.enable_nonvirtual
    )

    if output_mode == OUTPUT_CLI:
        vmprof.cli.show(stats)
    elif output_mode == OUTPUT_WEB:
        sys.stderr.write("Compiling and uploading to %s...\n" % (args.web,))
        vmprof.com.send(stats, args)


def main():
    parser = create_argument_parser()
    args = parser.parse_args()

    if args.web:
        output_mode = OUTPUT_WEB
    elif args.output:
        output_mode = OUTPUT_FILE
    else:
        output_mode = OUTPUT_CLI

    if output_mode == OUTPUT_FILE:
        prof_file = args.output
    else:
        prof_file = tempfile.NamedTemporaryFile()

    vmprof.enable(prof_file.fileno(), args.period)

    try:
        sys.argv = [args.program] + args.args
        runpy.run_path(args.program, run_name='__main__')
    except BaseException as e:
        if not isinstance(e, (KeyboardInterrupt, SystemExit)):
            raise
    vmprof.disable()
    show_stats(prof_file.name, output_mode, args)


main()
