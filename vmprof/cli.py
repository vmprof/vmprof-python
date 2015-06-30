from __future__ import print_function

def show(stats):
    p = stats.top_profile()
    if not p:
        print("no stats")
        return

    p.sort(key=lambda x: x[1], reverse=True)
    top = p[0][1]

    max_len = max([_namelen(e[0]) for e in p])

    print(" vmprof output:")
    print(" %:      name:" + " " * (max_len - 3) + "location:")

    for k, v in p:
        v = "%.1f%%" % (float(v) / top * 100)
        if v == '0.0%':
            v = '<0.1%'
        if k.startswith('py:'):
            _, func_name, lineno, filename = k.split(":", 3)
            lineno = int(lineno)
            print(" %s %s %s:%d" % (v.ljust(7), func_name.ljust(max_len + 1), filename, lineno))
        else:
            print(" %s %s" % (v.ljust(7), k.ljust(max_len + 1)))

def _namelen(e):
    if e.startswith('py:'):
        return len(e.split(':')[1])
    else:
        return len(e)

