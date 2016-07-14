import os
import runpy
import subprocess
import sys
import tempfile

import vmprof
from vmprof.log.parser import parse_jitlog


OUTPUT_CLI = 'cli'
OUTPUT_WEB = 'web'
OUTPUT_FILE = 'file'


def show_stats(filename, output_mode, args):
    if output_mode == OUTPUT_FILE:
        return

    stats = vmprof.read_profile(filename)
    forest = None
    jitlog_filename = filename + '.jitlog'
    if os.path.exists(jitlog_filename):
        forest = parse_jitlog(jitlog_filename)

    if output_mode == OUTPUT_CLI:
        vmprof.cli.show(stats)
    elif output_mode == OUTPUT_WEB:
        upload_stats(stats, forest, args)


def upload_stats(stats, forest, args):
    import vmprof.upload
    name = args.program
    argv = " ".join(args.args)
    host = args.web_url
    auth = args.web_auth
    #
    sys.stderr.write("Compiling and uploading to %s...\n" % (args.web_url,))
    vmprof.upload.upload(stats, name, argv, host, auth, forest)


def main():
    args = vmprof.cli.parse_args(sys.argv[1:])
    proc = None

    if args.web:
        output_mode = OUTPUT_WEB
    elif args.output:
        output_mode = OUTPUT_FILE
    else:
        output_mode = OUTPUT_CLI

    if output_mode == OUTPUT_FILE:
        prof_file = args.output
        prof_name = prof_file.name
        fileno = prof_file.fileno()
        if args.gzip:
            cmd = ['/usr/bin/gzip', "-" + str(args.gzip)]
            proc = subprocess.Popen(cmd, bufsize=-1,
                                    stdin=subprocess.PIPE,
                                    stdout=prof_file.fileno(),
                                    close_fds=True)
            fileno = proc.stdin.fileno()
    else:
        prof_file = tempfile.NamedTemporaryFile(delete=False)
        prof_name = prof_file.name
        fileno = prof_file.fileno()

    if args.jitlog:
        assert hasattr(vmprof, 'enable_jitlog'), "note: jitlog is only available on pypy"

    vmprof.enable(fileno, args.period, args.mem)
    if args.jitlog:
        # note that this file descr is then handled by jitlog
        fd = os.open(prof_name + '.jitlog', os.O_WRONLY | os.O_TRUNC | os.O_CREAT)
        vmprof.enable_jitlog(fd)

    try:
        sys.argv = [args.program] + args.args
        sys.path.insert(0, os.path.dirname(args.program))
        runpy.run_path(args.program, run_name='__main__')
    except BaseException as e:
        if not isinstance(e, (KeyboardInterrupt, SystemExit)):
            raise
    vmprof.disable()
    if args.jitlog and hasattr(vmprof, 'disable_jitlog'):
        vmprof.disable_jitlog(fd)
    if proc:
        proc.stdin.close()
        proc.wait()
    prof_file.close()
    show_stats(prof_name, output_mode, args)
    if output_mode != OUTPUT_FILE:
        os.unlink(prof_name)


main()
