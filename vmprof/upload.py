import gzip
import sys
import json
import argparse
import os
import requests
import tempfile
import vmprof
from vmprof.log.parser import read_jitlog_data, parse_jitlog
from vmprof.stats import Stats
from vmprof.stats import EmptyProfileFile

PY3 = sys.version_info[0] >= 3

def compress_file(filename):
    fileno, name = tempfile.mkstemp(prefix='jit', suffix='.log.zip')
    os.close(fileno)
    with open(filename, 'rb') as fd:
        with gzip.open(name, 'wb') as zipfd:
            while True:
                chunk = fd.read(1024)
                if not chunk:
                    break
                zipfd.write(chunk)
    return name


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
    profile_checksum = r.text[1:-1]
    sys.stderr.write("VMProf log: %s/#/%s\n" % (host.rstrip("/"), profile_checksum))

    # upload jitlog
    if forest:
        filename = compress_file(forest.filepath)
        with open(filename, 'rb') as fd:
            url = get_url(host, "api/jitlog/%s/" % profile_checksum)
            r = requests.post(url, files={ 'file': fd })
            checksum = r.text[1:-1]
            sys.stderr.write("PyPy JIT log: %s/#/%s/traces\n" % (host.rstrip("/"), checksum))

        # TODO remove temp file?

def get_url(host, path):
    if host.startswith("http"):
        url = '%s/%s' % (host.rstrip("/"), path)
    else:
        url = 'http://%s/%s' % (host.rstrip("/"), path)
    return url


def upload_jitlog(jitlog, args):
    host = args.web_url
    if jitlog.endswith("zip"):
        filename = jitlog
    else:
        filename = compress_file(jitlog)
    with open(filename, 'rb') as fd:
        url = get_url(host, "api/jitlog//")
        r = requests.post(url, files={ 'file': fd })
        checksum = r.text[1:-1]
        sys.stderr.write("PyPy JIT log: %s/#/%s/traces\n" % (host.rstrip("/"), checksum))

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("profile")
    parser.add_argument("--web-url", default='http://vmprof.com', help='Target host')
    parser.add_argument("--web-auth", default=None, help='Authtoken for your acount on the server')
    parser.add_argument("--jitlog", default=None,
                        help='The specified "profile" only contains the jitlog')
    args = parser.parse_args()

    trace_forest = None
    if args.jitlog:
        jitlog_path = args.jitlog
        upload_jitlog(jitlog_path, args)
    else:
        stats = vmprof.read_profile(args.profile)
        jitlog_path = args.profile + ".jitlog"
        if os.path.exists(jitlog_path):
            upload_jitlog(jitlog_path, args)
        sys.stderr.write("Compiling and uploading to %s...\n" % args.web_url)

        upload(stats, args.profile, [], args.web_url,
                     args.web_auth, trace_forest)


if __name__ == '__main__':
    main()
