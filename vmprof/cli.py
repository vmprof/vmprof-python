

def show(stats):
    print "vmprof output:"
    print "% of snapshots:  name:"
    p = stats.top_profile()
    p.sort(lambda a, b: cmp(b[2], a[2]))
    top = p[0][2]
    for k, a, v in p:
        v = "%.1f%%" % (float(v) / top * 100)
        if v == '0.0%':
            v = '<0.1%'
        if k.startswith('py:'):
            _, func_name, lineno, filename = k.split(":", 4)
            lineno = int(lineno)
            print "", v, " " * (14 - len(v)), ("%s    %s:%d" % (func_name, filename, lineno))
        else:
            print "", v, " " * (14 - len(v)), k
