import sys
import tempfile
import vmprof

if len(sys.argv) == 1:
    print "Usage: python -m vmprof <program> <program args>"
    sys.exit(1)

tmp = tempfile.NamedTemporaryFile()
vmprof.enable(tmp.fileno(), 1000)
try:
    sys.argv = sys.argv[1:]
    prog_name = sys.argv[0]
    execfile(prog_name)
finally:
    vmprof.disable()
    stats = vmprof.read_profile(tmp.name, virtual_only=True)

    vmprof.cli.show(stats)
    vmprof.com.send(stats, sys.argv)
