import sys
import json
import argparse
import os
import requests
import vmprof
from jitlog.parser import read_jitlog_data, parse_jitlog
from vmprof.stats import Stats
from vmprof.stats import EmptyProfileFile
from vmshare import get_url
import jitlog

PY3 = sys.version_info[0] >= 3

def upload(stats, name, argv, host, auth, forest=None):

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

    base_headers = {}
    if auth:
        base_headers = {'AUTHORIZATION': "Token %s" % auth}

    # XXX http only for now
    # upload the json profile
    headers = base_headers.copy()
    headers['Content-Type'] = 'application/json'
    url = get_url(host, "api/profile/")
    r = requests.post(url, data=data, headers=headers)
    if r.status_code != 200:
        sys.stderr.write("VMProf log: Server rejected profile. status: %d, msg: '%s'\n" % \
                (r.status_code,r.text))
        profile_checksum = ''
    else:
        profile_checksum = r.text[1:-1]
        sys.stderr.write("VMProf log: %s/#/%s\n" % (host.rstrip("/"), profile_checksum))

    if forest:
        url = get_url(host, "api/jitlog/%s/" % profile_checksum)
        jitlog.upload(forest.filepath, url)
        forest.unlink_jitlog()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("profile")
    parser.add_argument("--web-url", default='http://vmprof.com', help='Target host')
    parser.add_argument("--web-auth", default=None, help='Authtoken for your acount on the server')
    args = parser.parse_args()

    stats = vmprof.read_profile(args.profile)
    jitlog_path = args.profile + ".jitlog"
    if os.path.exists(jitlog_path):
        jitlog.upload(jitlog_path, args)
    sys.stderr.write("Compiling and uploading to %s...\n" % args.web_url)

    upload(stats, args.profile, [], args.web_url,
                 args.web_auth, None)


if __name__ == '__main__':
    main()
