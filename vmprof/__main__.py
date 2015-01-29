
import vmprof, sys, tempfile

tmp = tempfile.NamedTemporaryFile()
vmprof.enable(tmp.fileno(), 1000)
try:
    sys.argv = sys.argv[1:]
    prog_name = sys.argv[0]
    execfile(prog_name)
finally:
    vmprof.disable()
    stats = vmprof.read_profile(tmp.name)
    print "VMProf profiler output:"
    print "number of traces:     name:"
    for k, v in stats.top_profile():
        print "", v, " " * (18 - len(str(v))), k
