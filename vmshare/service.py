import sys
import json
import argparse
import os
import requests
import vmprof
from jitlog.parser import read_jitlog_data, parse_jitlog
from vmprof.stats import Stats
from vmprof.stats import EmptyProfileFile
import jitlog

PY3 = sys.version_info[0] >= 3

class ServiceException(Exception):
    pass


def compress_file(filename):
    fileno, name = tempfile.mkstemp(suffix='.zip')
    os.close(fileno)
    with open(filename, 'rb') as fd:
        with gzip.open(name, 'wb') as zipfd:
            while True:
                chunk = fd.read(1024)
                if not chunk:
                    break
                zipfd.write(chunk)
    return name

class Service(object):
    FILE_CPU_PROFILE = 'cpu'
    FILE_MEM_PROFILE = 'mem'
    FILE_JIT_PROFILE = 'jit'

    def __init__(self, host, auth=None):
        self.host = host
        self.auth = auth
        self.runtime_id = None

    def get_headers(self):
        base_headers = { 'Content-Type': 'application/json' }
        if self.auth:
            base_headers = {'AUTHORIZATION': "Token %s" % self.auth}
        return base_headers

    def get_url(self, path):
        host = self.host
        path = path.lstrip('/')
        if host.startswith("http"):
            url = '%s/%s' % (host.rstrip("/"), path)
        else:
            url = 'http://%s/%s' % (host.rstrip("/"), path)
        return url

    def post_new_entry(self, data={}):
        url = self.get_url('/api/runtime/new')
        headers = self.get_headers()
        bytesdata = json.dumps(data).encode('utf-8')
        response = requests.post(url, data=bytesdata, headers=headers)
        if response.status_code != 200:
            sys.stderr.write("server rejected meta data." \
                             " status: %d, msg: '%s'\n" % \
                             (response.status_code, response.text))
            return None
        return r.json['rid']

    def post_file(self, rid, filename, filetype, compress=False):
        if not os.path.exists(filename):
            return False
        origfile = filename
        if compress:
            filename = compress_file(filename)
        with open(filename, 'rb') as fd:
            url = self.get_url('/api/runtime/upload/%s/%s/add' % (filetype, rid))
            files = { 'file': fd }
            headers = self.get_headers()
            response = requests.post(url, data=b"", headers=headers, files=files)
            if response.status_code != 200:
                sys.stderr.write("server rejected file." \
                        " status: %d, msg: '%s'\n" % \
                        (response.status_code, response.text))
                raise ServiceException()
        return True

    def post(self, data, **kwargs):
        sys.stderr.write("Uploading to %s...\n" % self.host)

        argv = kwargs['argv'] if 'argv' in kwargs else None

        rid = self.post_new_entry({'argv': argv})
        if rid is None:
            raise ServiceException("could not create meta data for profiles")

        if Service.FILE_CPU_PROFILE in kwargs:
            filename = kwargs[Service.FILE_CPU_PROFILE]
            if os.path.exists(filename):
                sys.stderr.write(" => uploading cpu profile...\n")
                self.post_file(rid, filename,
                               Service.FILE_CPU_PROFILE, compress=False)
        elif Service.FILE_MEM_PROFILE in kwargs:
            filename = kwargs[Service.FILE_MEM_PROFILE]
            if os.path.exists(filename):
                sys.stderr.write(" => uploading mem profile...\n")
                self.post_file(rid, filename,
                               Service.FILE_MEM_PROFILE, compress=False)
        elif Service.FILE_JIT_PROFILE in kwargs:
            filename = kwargs[Service.FILE_JIT_PROFILE]
            if os.path.exists(filename):
                sys.stderr.write(" => uploading jit profile...\n")
                forest = parse_jitlog(filename)
                if forest.exception_raised():
                    sys.stderr.write(" error: %s\n" % forest.exception_raised())
                # append source code to the binary
                forest.extract_source_code_lines()
                forest.copy_and_add_source_code_tags()
                filename = self.filepath
                self.post_file(rid, filename,
                               Service.FILE_JIT_PROFILE, compress=False)
                forest.unlink_jitlog()

        self.finalize_entry(rid)

    def finalize_entry(self, rid, data=b""):
        url = self.get_url('/api/runtime/%s/freeze' % rid)
        headers = self.get_headers()
        response = requests.post(url, data=data, headers=headers)
        if r.status_code != 200:
            sys.stderr.write("server failed to freeze these runtime profiles." \
                             " status: %d, msg: '%s'\n" % \
                                (r.status_code, r.text))
            return False
        return True

