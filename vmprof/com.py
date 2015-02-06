import json
import urllib2


def send(stats, argv):

    data = {
        "profiles": [(map(str, a), b) for a, b in stats.profiles],
        "addresses": stats.adr_dict,
        "argv": " ".join(argv)
    }

    data = json.dumps(data)

    url = 'http://vmprof.baroquesoftware.com/api/log/'
    url = 'http://10.0.0.1:8000/api/log/'

    request = urllib2.Request(
        url, data, {'content-type': 'application/json'}
    )

    try:
        urllib2.urlopen(request)
    except Exception as e:


        import pdb; pdb.set_trace()
        pass

