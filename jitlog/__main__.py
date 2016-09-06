import runpy
import sys, os
import tempfile
import argparse
from jitlog.upload import upload as jitlog_upload
from vmshare import get_url
from jitlog.parser import parse_jitlog
from jitlog import query

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

    parser.add_argument('--query', '-q', dest='query',
        help='Select traces and pretty print them. ' \
             'Example: -i <file> -q "\'my_func\' in name" ' \
             'The query API can be found on https://vmprof.readthedocs.org'
    )
    parser.add_argument('--input', '-i', dest='input',
        help='Specify the file to read from. Use with in combination with --query'
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
    output_mode_args.add_argument(
        '--upload',
        action='store_true',
        help='Upload the file provided as the program argument'
    )

    return parser

def main():
    parser = build_argparser()
    args = parser.parse_args(sys.argv[1:])
    web = args.web

    if args.input:
        assert args.query is not None, "Using -i requires you to specify -q"
        forest = parse_jitlog(args.input)
        q = query.new_unsafe_query(args.query)
        objs = q(forest)
        pretty_printer.write(sys.stdout, objs)
        sys.exit(0)

    if args.upload:
        # parse_jitlog will append source code to the binary
        forest = parse_jitlog(args.program)
        if forest.exception_raised():
            print("ERROR:", forest.exception_raised())
            sys.exit(1)
        if forest.extract_source_code_lines():
            # only copy the tags if the jitlog has no source code yet!
            forest.copy_and_add_source_code_tags()
        jitlog_upload(forest.filepath, get_url(args.web_url, "api/jitlog//"))
        sys.exit(0)

    if not _jitlog:
        if '__pypy__' in sys.builtin_module_names:
            sys.stderr.write("No _jitlog module. This PyPy version is too old!\n")
        else:
            sys.stderr.write("No _jitlog module. Use PyPy instead of CPython!\n")

    if not web:
        prof_file = args.output
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
        forest = parse_jitlog(prof_name)
        if forest.extract_source_code_lines():
            # only copy the tags if the jitlog has no source code yet!
            forest.copy_and_add_source_code_tags()
        jitlog_upload(forest.filepath, get_url(args.web_url, "api/jitlog//"))
        forest.unlink_jitlog() # free space!

main()
