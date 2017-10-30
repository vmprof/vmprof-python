import sys
import json
import argparse
import os
import requests
import vmprof
import tempfile
from jitlog.parser import read_jitlog_data, parse_jitlog
from vmprof.stats import Stats
from vmprof.stats import EmptyProfileFile
import jitlog
import gzip
import warnings

PY3 = sys.version_info[0] >= 3

class ServiceException(Exception):
    pass

def is_errornous(json):
    return 'detail' in json

def service_exception_from_json(json):
    assert 'error' in json
    return ServiceException(json['error'])


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
        self.csrf_token = None

    def get_headers(self):
        base_headers = { 'Content-Type': 'application/json' }
        if self.auth:
            base_headers['AUTHORIZATION'] = "Token %s" % self.auth
        if self.csrf_token:
            base_headers['csrftoken'] = self.csrf_token
        return base_headers

    def get_url(self, path):
        host = self.host
        path = path.lstrip('/')
        if host.startswith("http"):
            url = '%s/%s' % (host.rstrip("/"), path)
        else:
            url = 'http://%s/%s' % (host.rstrip("/"), path)
        return url

    def stop_if_error_occured(self, response):
        if response.status_code != 200:
            sys.stderr.write("server rejected meta data." \
                             " status: %d, msg: '%s'\n" % \
                             (response.status_code, response.text))
            raise ServiceException()

    def post_new_entry(self, data={}):
        url = self.get_url('/api/runtime/new')
        headers = self.get_headers()
        bytesdata = json.dumps(data).encode('utf-8')
        response = requests.post(url, data=bytesdata, headers=headers)
        self.stop_if_error_occured(response)
        j = response.json()
        return j.get('runtime_id', None)

    def post_file(self, rid, filename, filetype, compress=False):
        if not os.path.exists(filename):
            return None

        if compress:
            filename = compress_file(filename)

        if 'vmprof.com' in self.host:
            # not sure about nginx, what does MB mean? 1MB == 1000 byte or 1024 byte?
            # take the lower bound...
            size = os.path.getsize(filename)
            if size >= 100*1000*1000: # 100 MB
                warnings.warn("WARNING: vmprof.com limits the "
                              "compressed profile file to 100 MBs. "
                              "The upload might fail!\n")
        with open(filename, 'rb') as fd:
            url = self.get_url('/api/runtime/upload/%s/%s/add' % (filetype, rid))
            headers = self.get_headers()
            headers['Content-Disposition'] = 'attachment; filename='+filename
            del headers['Content-Type']
            response = requests.post(url, headers=headers, data=fd)
            self.stop_if_error_occured(response)
            return response

    def post(self, kwargs):
        sys.stderr.write("Uploading to %s...\n" % self.host)

        argv = kwargs.get('argv', '-')
        vm = kwargs.get('VM','unknown')

        rid = self.post_new_entry({'argv': argv, 'VM': vm})
        if rid is None:
            raise ServiceException("could not create meta data for profiles")

        if Service.FILE_CPU_PROFILE in kwargs:
            filename = kwargs[Service.FILE_CPU_PROFILE]
            if os.path.exists(filename):
                sys.stderr.write(" => Uploading the cpu profile...\n")
                self.post_file(rid, filename,
                               Service.FILE_CPU_PROFILE, compress=False)
                sys.stderr.write('      ' + self.get_url('#/%s' % rid) + "\n")
        if Service.FILE_JIT_PROFILE in kwargs:
            filename = kwargs[Service.FILE_JIT_PROFILE]
            if os.path.exists(filename):
                sys.stderr.write(" => Uploading the jit log...\n")
                forest = parse_jitlog(filename)
                if forest.exception_raised():
                    sys.stderr.write(" error: %s\n" % forest.exception_raised())
                # append source code to the binary
                forest.extract_source_code_lines()
                forest.copy_and_add_source_code_tags()
                filename = forest.filepath
                response = self.post_file(rid, filename,
                               Service.FILE_JIT_PROFILE, compress=True)
                forest.unlink_jitlog()
                json = response.json()
                if 'jid' in json:
                    url = self.get_url('#/%s/traces' % json['jid'])
                else:
                    url = self.get_url('#/%s' % rid)
                sys.stderr.write('      ' + url + "\n")

        self.finalize_entry(rid)


    def finalize_entry(self, rid, data=b""):
        url = self.get_url('/api/runtime/%s/freeze/' % rid)
        headers = self.get_headers()
        response = requests.post(url, data=data, headers=headers)
        self.crsf_token = None
        if response.status_code != 200:
            sys.stderr.write("server failed to freeze these runtime profiles." \
                             " status: %d, msg: '%s'\n" % \
                                (response.status_code, response.text))
            return False
        return True

