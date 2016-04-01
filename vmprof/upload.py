import sys
import json
import argparse
import six.moves.urllib.request as request


import vmprof
from vmprof import jitlog
from vmprof.stats import Stats
from vmprof.stats import EmptyProfileFile
PY3 = sys.version_info[0] >= 3


def upload(stats, name, argv, host, auth, trace_forest):

    try:
        profiles = stats.get_tree()._serialize()
    except EmptyProfileFile:
        # yes an empty profile is possible.
        # i.e. if we only want to upload the jitlog
        profiles = []

    data = {
        "VM": stats.interp,
        "profiles": profiles,
        "argv": "%s %s" % (name, argv),
        "version": 2,
    }
    if trace_forest:
        data["jitlog"] = trace_forest._serialize()
        #print json.dumps(data["jitlog"], indent=2, sort_keys=True)

    data = json.dumps(data).encode('utf-8')

    # XXX http only for now
    if host.startswith("http"):
        url = '%s/api/log/' % host.rstrip("/")
    else:
        url = 'http://%s/api/log/' % host.rstrip("/")

    headers = {'content-type': 'application/json'}

    if auth:
        headers['AUTHORIZATION'] = "Token %s" % auth

    req = request.Request(url, data, headers)

    res = request.urlopen(req)
    val = res.read()
    if PY3:
        val = val[1:-1].decode("utf-8")
    else:
        val = val[1:-1]
    return "%s/#/%s" % (host, val)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("profile")
    parser.add_argument("--web-url", default='http://vmprof.com', help='Target host')
    parser.add_argument("--web-auth", default=None, help='Authtoken for your acount on the server')
    parser.add_argument("--jitlog", action='store_true', default=None, help='The specified "profile" only contains the jitlog')
    args = parser.parse_args()

    trace_forest = None
    if args.jitlog:
        trace_forest = jitlog.read_jitlog(args.profile)
        stats = Stats([], {}, {}, "pypy")
    else:
        stats = vmprof.read_profile(args.profile)
        jitlog_path = args.profile + ".jitlog"
        if os.path.exists(jitlog_path):
            trace_forest = jitlog.read_jitlog(jitlog_path, stats=stats)
        sys.stderr.write("Compiling and uploading to %s...\n" % args.web_url)

    res = upload(stats, args.profile, [], args.web_url,
                 args.web_auth, trace_forest)
    sys.stderr.write("Available at:\n%s\n" % res)


if __name__ == '__main__':
    main()
