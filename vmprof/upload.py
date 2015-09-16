import sys
import json
import six.moves.urllib.request as request
import click

import vmprof

def upload_v1(stats, name, argv, host, auth):
    data = {
        "VM": stats.interp,
        "profiles": stats.get_tree()._serialize(),
        "argv": "%s %s" % (name, argv),
        "version": 1,
    }

    data = json.dumps(data).encode('utf-8')
    return upload_json(data, host, auth)


def upload_v2(VM, callgraph, name, argv, host, auth):
    root = callgraph.root
    vroot = callgraph.get_virtual_root()
    data = {
        "VM": VM,
        "root": root.serialize(),
        "vroot": vroot.serialize(),
        "argv": "%s %s" % (name, argv),
        "version": 2,
    }
    data = json.dumps(data).encode('utf-8')
    return upload_json(data, host, auth)


def upload_json(data, host, auth):
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
    return "http://" + host + "/#/" + val[1:-1]



@click.command()
@click.argument('profile', type=str)
@click.option('--web-url', type=str, default='vmprof.baroquesoftware.com',
              help='Target host')
@click.option('--web-auth', type=str, default=None,
              help='Authtoken for your acount on the server')
def main(profile, web_url, web_auth): 
    stats = vmprof.read_stats(profile, virtual_only=True)
    sys.stderr.write("Compiling and uploading to %s...\n" % (web_url,))
    res = upload_v1(stats, profile, [], web_url, web_auth)
    sys.stderr.write("Available at:\n%s\n" % res)

if __name__ == '__main__':
    main()
