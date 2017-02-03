import runpy
import platform
import sys, os
import tempfile
import vmprof
from vmshare.service import Service
try:
    import _jitlog
except ImportError:
    _jitlog = None

OUTPUT_CLI = 'cli'
OUTPUT_WEB = 'web'
OUTPUT_FILE = 'file'

def show_stats(filename, output_mode, args):
    if output_mode == OUTPUT_FILE:
        return
    elif output_mode == OUTPUT_CLI:
        stats = vmprof.read_profile(filename)
        vmprof.cli.show(stats)
    elif output_mode == OUTPUT_WEB:
        host, auth = args.web_url, args.web_auth
        service = Service(host, auth)
        service.post({ Service.FILE_CPU_PROFILE: filename,
                       Service.FILE_JIT_PROFILE: filename + '.jit',
                       'argv': ' '.join(sys.argv[:]),
                       'VM': platform.python_implementation() })

def main():
    args = vmprof.cli.parse_args(sys.argv[1:])

    # None means default on this platform
    native = None
    if args.no_native:
        native = False
    if args.web:
        output_mode = OUTPUT_WEB
    elif args.output:
        output_mode = OUTPUT_FILE
    else:
        output_mode = OUTPUT_CLI

    if output_mode == OUTPUT_FILE:
        prof_file = args.output
        prof_name = prof_file.name
    else:
        prof_file = tempfile.NamedTemporaryFile(delete=False)
        prof_name = prof_file.name


    vmprof.enable(prof_file.fileno(), args.period, args.mem,
                  args.lines, native=native)
    if args.jitlog and _jitlog:
        fd = os.open(prof_name + '.jit', os.O_WRONLY | os.O_TRUNC | os.O_CREAT)
        _jitlog.enable(fd)
    # invoke the user program:
    try:
        sys.argv = [args.program] + args.args
        sys.path.insert(0, os.path.dirname(args.program))
        runpy.run_path(args.program, run_name='__main__')
    except BaseException as e:
        if not isinstance(e, (KeyboardInterrupt, SystemExit)):
            raise
    #
    vmprof.disable()
    if args.jitlog and _jitlog:
        _jitlog.disable()

    prof_file.close()
    show_stats(prof_name, output_mode, args)
    if output_mode != OUTPUT_FILE:
        os.unlink(prof_name)

main()
