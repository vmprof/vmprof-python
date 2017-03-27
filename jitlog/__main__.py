import runpy
import sys, os
import tempfile
import argparse
from jitlog.upload import upload as jitlog_upload
from jitlog.parser import parse_jitlog
from jitlog import query
from vmshare.service import Service

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

    parser.add_argument('--query', '-q', dest='query', default=None,
        help='Select traces and pretty print them. ' \
             'The query API can be found on https://vmprof.readthedocs.org'
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

    if args.query is not None:
        from jitlog import prettyprinter as pp
        sys.stderr.write("Parsing jitlog...")
        sys.stderr.flush()
        forest = parse_jitlog(args.program)
        sys.stderr.write("done\n")
        query_str = args.query or "traces()"
        q = query.new_unsafe_query(query_str)
        objs = q(forest)
        color = True
        pp_clazz = pp.ColoredPrettyPrinter if color else pp.PrettyPrinter
        with pp_clazz() as ppinst:
            for trace in objs:
                ppinst.trace(sys.stdout, trace)
        if args.query is None:
            sys.stderr.write("-" * 10 + '\n')
            sys.stderr.write("Display the jitlog with an empty query (defaults to -q 'traces()'). "
                    "Add -q 'your query' if you want to narrow down the output\n")
        sys.exit(0)

    if args.upload:
        host, auth = args.web_url, args.web_auth
        service = Service(host, auth)
        service.post({ Service.FILE_JIT_PROFILE: args.program })
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
        host, auth = args.web_url, args.web_auth
        service = Service(host, auth)
        service.post({ Service.FILE_JIT_PROFILE: forest.filepath })
        forest.unlink_jitlog() # free space!

main()
