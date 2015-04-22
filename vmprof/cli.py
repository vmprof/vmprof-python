

def show(stats):
    p = stats.top_profile()
    if not p:
        print "no stats"
        return
    print "vmprof output:"
    print "% of snapshots:  name:"
    p.sort(lambda a, b: cmp(b[1], a[1]))
    top = p[0][1]
    for k, v in p:
        v = "%.1f%%" % (float(v) / top * 100)
        if v == '0.0%':
            v = '<0.1%'
        if k.startswith('py:'):
            _, func_name, lineno, filename = k.split(":", 3)
            lineno = int(lineno)
            print "", v, " " * (14 - len(v)), ("%s    %s:%d" % (func_name, filename, lineno))
        else:
            print "", v, " " * (14 - len(v)), k
