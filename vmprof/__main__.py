
import vmprof, sys, tempfile

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
    stats = vmprof.read_profile(tmp.name, virtual_only=False)
    print "VMProf profiler output:"
    print "number of traces: name:"
    p = stats.top_profile()
    p.sort(lambda a, b: cmp(b[1], a[1]))
    for k, v in p:
        if k.startswith(' '):
            _, func_name, lineno, filename = k.split(":", 4)
            lineno = int(lineno)
            print "", v, " " * (14 - len(str(v))), ("%s    %s:%d" %
                                  (func_name, filename, lineno))
        else:
            print "", v, " " * (14 - len(str(v))), k
