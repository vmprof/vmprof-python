import sys
import json
import argparse
import six.moves.urllib.request as request


import vmprof
from vmprof.log.parser import read_jitlog_data, parse_jitlog
from vmprof.stats import Stats
from vmprof.stats import EmptyProfileFile
PY3 = sys.version_info[0] >= 3


def upload(stats, name, argv, host, auth):

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
    data = json.dumps(data).encode('utf-8')

    upload_data(host, "api/log/", data, auth=auth)

def upload_data(host, path, data, auth=None):
    # XXX http only for now
    if host.startswith("http"):
        url = '%s/api/%s' % (host.rstrip("/"), path)
    else:
        url = 'http://%s/%s' % (host.rstrip("/"), path)

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

def upload_jitlog(jitlog, args):
    data = read_jitlog_data(args.jitlog)
    trace_forest = parse_jitlog(data)
    upload_to(args.web_url, "api/jitlog/", data, auth=args.web_auth)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("profile")
    parser.add_argument("--web-url", default='http://vmprof.com', help='Target host')
    parser.add_argument("--web-auth", default=None, help='Authtoken for your acount on the server')
    parser.add_argument("--jitlog", action='store_true', default=None, help='The specified "profile" only contains the jitlog')
    args = parser.parse_args()

    trace_forest = None
    if args.jitlog:
        upload_jitlog(args.jitlog, args)
    else:
        stats = vmprof.read_profile(args.profile)
        jitlog_path = args.profile + ".jitlog"
        if os.path.exists(jitlog_path):
            upload_jitlog(jitlog_path, args)
        sys.stderr.write("Compiling and uploading to %s...\n" % args.web_url)

    res = upload(stats, args.profile, [], args.web_url,
                 args.web_auth, trace_forest)
    sys.stderr.write("Available at:\n%s\n" % res)


if __name__ == '__main__':
    main()
