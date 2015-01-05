#import requests
#import urlparse
import json
#import zlib
#import base64
import tempfile
import logging

from vmprof.reader import (
    read_prof, read_ranges, read_sym_file, LibraryData
)

from vmprof.addrspace import AddressSpace
from vmprof.profiler import Profiler


class xxx_vmprof(object):

    def __init__(self, host, port=80, logger=None):
        self.host = host
        self.port = port
        self.logger = logger or self.get_default_logger()

    def get_default_logger(self):
        default_logger = logging.getLogger(self.__class__.__name__)
        default_logger.setLevel(logging.INFO)
        default_logger.addHandler(logging.StreamHandler())
        return default_logger

    def validate_args(self, *args, **kwargs):
        return True

    def __call__(self, f):
        def func(*args, **kwargs):
            if not self.validate_args(*args, **kwargs):
                return f(*args, **kwargs)

            lib = ffi.dlopen(None)
            prof_file = "%s.prof" % tempfile.mkstemp()[1]
            prof_sym_file = "%s.sym" % prof_file

            lib.vmprof_enable(prof_file, -1)
            ret = f(*args, **kwargs)
            lib.vmprof_disable()

            prof = open(prof_file, 'rb').read()
            prof_sym = open(prof_sym_file, 'rb').read()

            period, profiles, symmap = read_prof(prof)
            libs = read_ranges(symmap)

            for lib in libs:
                lib.read_object_data()
            libs.append(
                LibraryData(
                    '<virtual>',
                    0x8000000000000000L,
                    0x8fffffffffffffffL,
                    True,
                    symbols=read_sym_file(prof_sym))
            )
            addrspace = AddressSpace(libs)
            filtered_profiles, addr_set = addrspace.filter_addr(profiles)
            d = {}
            for addr in addr_set:
                name, _ = addrspace.lookup(addr | 0x8000000000000000L)
                d[addr] = name

            data = zlib.compress(json.dumps({
                'profiles': filtered_profiles, 'addresses': d
            }))

            response = requests.post(
                "http://%s:%s/api/log/" % (self.host, self.port),
                data={'data': base64.b64encode(data)}
            )

            self.logger.info("Log: %s" % response.json())

            return ret

        return func


class DjangoVMPROF(xxx_vmprof):

    def __init__(self, host, port=80, token="", logger=None):
        self.host = host
        self.port = port
        self.token = token
        self.logger = logger or self.get_default_logger()

    def validate_args(self, environ, start_response):
        qs = dict(urlparse.parse_qsl(environ.get('QUERY_STRING', '')))
        token = qs.get('vmprof')
        if token == self.token:
            return True
        return False


