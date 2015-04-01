import json
import urllib2


def send(t, name, argv, base_url):

    data = {
        "profiles": t,
        "argv": "%s %s" % (name, " ".join(argv))
    }

    data = json.dumps(data)

    # XXX http only for now
    if base_url.startswith("http"):
        url = '%s/api/log/' % base_url.rstrip("/")
    else:
        url = 'http://%s/api/log/' % base_url.rstrip("/")

    request = urllib2.Request(
        url, data, {'content-type': 'application/json'}
    )

    urllib2.urlopen(request)
