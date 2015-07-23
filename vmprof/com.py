import json
import six.moves.urllib.request as request


def send(t, args):

    name = args.program
    base_url = args.web_url
    auth = args.web_auth


    data = {
        "profiles": t.get_tree()._serialize(),
        "argv": "%s %s" % (name, " ".join(args.args)),
        "version": 1,
    }

    data = json.dumps(data).encode('utf-8')

    # XXX http only for now
    if base_url.startswith("http"):
        url = '%s/api/log/' % base_url.rstrip("/")
    else:
        url = 'http://%s/api/log/' % base_url.rstrip("/")

    headers = {'content-type': 'application/json'}

    if auth:
        headers['AUTHORIZATION'] = "Token %s" % auth

    req = request.Request(url, data, headers)

    res = request.urlopen(req)
    val = res.read()
    return "http://" + base_url + "/#/" + val[1:-1]
