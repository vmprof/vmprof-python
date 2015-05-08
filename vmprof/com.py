import json
import urllib2


def send(t, args):

    name = args.program
    base_url = args.web
    auth = args.web_auth


    data = {
        "profiles": t.get_tree().flatten()._serialize(),
        "argv": "%s %s" % (name, " ".join(args.args)),
        "version": 1,
    }

    data = json.dumps(data)

    # XXX http only for now
    if base_url.startswith("http"):
        url = '%s/api/log/' % base_url.rstrip("/")
    else:
        url = 'http://%s/api/log/' % base_url.rstrip("/")

    headers = {'content-type': 'application/json'}

    if auth:
        headers['AUTHORIZATION'] = "Token %s" % auth

    request = urllib2.Request(url, data, headers)

    urllib2.urlopen(request)
