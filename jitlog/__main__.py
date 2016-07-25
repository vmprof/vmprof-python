import runpy
import sys, os
import tempfile
import argparse
from jitlog.upload import upload as jitlog_upload
from vmprofservice import get_url

try:
    import _jitlog
except ImportError:
    _jitlog = None

def build_argparser():
    # TODO merge with arguments of vmprof.cli
    parser = argparse.ArgumentParser(
        description='Jitlog',
        prog="jitlog"
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
        '--web-auth',
        help='Authtoken for your acount on the server, works only when --web is used'
    )

    parser.add_argument(
        '--web-url',
        metavar='url',
        default='http://vmprof.com',
        help='Provide URL instead of the default vmprof.com)'
    )
    output_mode_args = parser.add_mutually_exclusive_group()
    output_mode_args.add_argument(
        '--web',
        action='store_true',
        help='Upload profiling stats to a remote server (defaults to vmprof.com)'
    )
    output_mode_args.add_argument(
        '--output', '-o',
        metavar='file.prof',
        type=argparse.FileType('w+b'),
        help='Save profiling data to file'
    )

    return parser

def main():
    parser = build_argparser()
    args = parser.parse_args(sys.argv[1:])
    web = args.web

    if not _jitlog:
        if '__pypy__' in sys.builtin_module_names:
            sys.stderr.write("No _jitlog module. This PyPy version is too old!\n")
        else:
            sys.stderr.write("No _jitlog module. Use PyPy instead of CPython!\n")

    if not web:
        prof_file = args.output
        prof_name = prof_file.name
    else:
        prof_file = tempfile.NamedTemporaryFile(delete=False)
        prof_name = prof_file.name


    fd = os.open(prof_name, os.O_WRONLY | os.O_TRUNC | os.O_CREAT)
    _jitlog.enable(fd)

    try:
        sys.argv = [args.program] + args.args
        sys.path.insert(0, os.path.dirname(args.program))
        runpy.run_path(args.program, run_name='__main__')
    except BaseException as e:
        if not isinstance(e, (KeyboardInterrupt, SystemExit)):
            raise
    # not need to close fd, will be here
    _jitlog.disable()

    if web:
        jitlog_upload(prof_name, get_url(args.web_url, "api/jitlog//"))
        os.unlink(prof_name)

main()
