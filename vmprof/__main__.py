import runpy
import sys
import tempfile

import vmprof


OUTPUT_CLI = 'cli'
OUTPUT_WEB = 'web'
OUTPUT_FILE = 'file'


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
        upload_stats(stats, args)


def upload_stats(stats, args):
    import vmprof.upload
    name = args.program
    argv = " ".join(args.args)
    host = args.web_url
    auth = args.web_auth
    #
    sys.stderr.write("Compiling and uploading to %s...\n" % (args.web_url,))
    res = vmprof.upload.upload(stats, name, argv, host, auth)
    sys.stderr.write("Available at:\n%s\n" % res)


def main():
    args = vmprof.cli.parse_args(sys.argv[1:])

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
