import sys
import json
import six.moves.urllib.request as request
import click

import vmprof

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
    return "http://" + host + "/#/" + val[1:-1]



@click.command()
@click.argument('profile', type=str)
@click.option('--web-url', type=str, default='vmprof.baroquesoftware.com',
              help='Target host')
@click.option('--web-auth', type=str, default=None,
              help='Authtoken for your acount on the server')
def main(profile, web_url, web_auth): 
    stats = vmprof.read_profile(profile, virtual_only=True)
    sys.stderr.write("Compiling and uploading to %s...\n" % (web_url,))
    res = upload(stats, profile, [], web_url, web_auth)
    sys.stderr.write("Available at:\n%s\n" % res)

if __name__ == '__main__':
    main()
