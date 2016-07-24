import runpy
import sys, os
import tempfile

try:
    import _jitlog
except ImportError:
    _jitlog = None

def upload_stats(stats, forest, args):
    import vmprof.upload
    name = args.program
    argv = " ".join(args.args)
    host = args.web_url
    auth = args.web_auth
    #
    sys.stderr.write("Compiling and uploading to %s...\n" % (args.web_url,))
    vmprof.upload.upload(stats, name, argv, host, auth, forest)

def build_argparser():
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

    if args.web:
        output_mode = OUTPUT_WEB
    elif args.output:
        output_mode = OUTPUT_FILE

    if output_mode == OUTPUT_FILE:
        prof_file = args.output
        prof_name = prof_file.name
    else:
        prof_file = tempfile.NamedTemporaryFile(delete=False)
        prof_name = prof_file.name


    vmprof.enable(prof_file.fileno(), args.period, args.mem, args.lines)
    fd = os.open(prof_name, os.O_WRONLY | os.O_TRUNC | os.O_CREAT)
    _jitlog.enable(fd)
    # not need to close fd, will be closed in _jitlog.disable()

    try:
        sys.argv = [args.program] + args.args
        sys.path.insert(0, os.path.dirname(args.program))
        runpy.run_path(args.program, run_name='__main__')
    except BaseException as e:
        if not isinstance(e, (KeyboardInterrupt, SystemExit)):
            raise
    _jitlog.disable()
    if output_mode == OUTPUT_WEB:
        upload(prof_name, output_mode, args)
    if output_mode != OUTPUT_FILE:
        os.unlink(prof_name)

main()
