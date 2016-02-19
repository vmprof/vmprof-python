import sys
import json
import argparse
import six.moves.urllib.request as request


import vmprof
PY3 = sys.version_info[0] >= 3


def upload(stats, name, argv, host, auth):

    data = {
        "VM": stats.interp,
        "profiles": stats.get_tree()._serialize(),
        "argv": "%s %s" % (name, argv),
        "version": 1,
    }

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
    args = parser.parse_args()

    stats = vmprof.read_profile(args.profile)
    sys.stderr.write("Compiling and uploading to %s...\n" % args.web_url)

    res = upload(stats, args.profile, [], args.web_url, args.web_auth)
    sys.stderr.write("Available at:\n%s\n" % res)


if __name__ == '__main__':
    main()
