import io
import os
import sys
import subprocess
import struct
try:
    from shutil import which
except ImportError:
    from backports.shutil_which import which

from . import cli

import _vmprof

from vmprof.reader import read_prof, MARKER_NATIVE_SYMBOLS
from vmprof.stats import Stats
from vmprof.profiler import Profiler, read_profile


PY3  = sys.version_info[0] >= 3
IS_PYPY = '__pypy__' in sys.builtin_module_names

# it's not a good idea to use a "round" default sampling period, else we risk
# to oversample periodic tasks which happens to run at e.g. 100Hz or 1000Hz:
# http://www.solarisinternals.com/wiki/index.php/DTrace_Topics_Hints_Tips#profile-1001.2C_profile-997.3F
#
# To avoid the problem, we use a period which is "almost" but not exactly
# 1000Hz
DEFAULT_PERIOD = 0.00099

def disable():
    try:
        _vmprof.disable()
        _gzip_finish()
    except IOError as e:
        raise Exception("Error while writing profile: " + str(e))

def _is_native_enabled(native):
    if os.name == "nt":
        if native:
            raise ValueError("native profiling is only supported on Linux & Mac OS X")
        native = False
    else:
        # TODO native should be enabled by default?
        if native is None:
            native = True
    return native

if IS_PYPY:
    def enable(fileno, period=DEFAULT_PERIOD, memory=False, lines=False, native=None, warn=True):
        if not isinstance(period, float):
            raise ValueError("You need to pass a float as an argument")
        if warn and sys.pypy_version_info[:3] < (4, 1, 0):
            raise Exception("PyPy <4.1 have various kinds of bugs, pass warn=False if you know what you're doing")
        if warn and memory:
            print("Memory profiling is currently unsupported for PyPy. Running without memory statistics.")
        if warn and lines:
            print('Line profiling is currently unsupported for PyPy. Running without lines statistics.\n')
        native = _is_native_enabled(native)
        gz_fileno = _gzip_start(fileno)
        _vmprof.enable(gz_fileno, period, memory, lines, native)
else:
    # CPYTHON
    def enable(fileno, period=DEFAULT_PERIOD, memory=False, lines=False, native=None):
        if not isinstance(period, float):
            raise ValueError("You need to pass a float as an argument")
        gz_fileno = _gzip_start(fileno)
        native = _is_native_enabled(native)
        _vmprof.enable(gz_fileno, period, memory, lines, native)

    def dump_native_symbols(fileno):
        # native symbols cannot be resolved in the signal handler.
        # it would take far too long. Thus this method should be called
        # just after the sampling finished and before the file descriptor
        # is closed.

        # called from C with the fileno that has been used for this profile
        # duplicates are avoided if this function is only called once for a profile
        fileobj = io.open(fileno, mode='rb', closefd=False)
        fileobj.seek(0)
        _, profiles, _, _, _, _, _ = read_prof(fileobj)

        duplicates = set()
        fileobj = io.open(fileno, mode='ab', closefd=False)

        for profile in profiles:
            addrs = profile[0]
            for addr in addrs:
                if addr in duplicates:
                    continue
                duplicates.add(addr)
                if addr & 0x1 and addr > 1:
                    name, lineno, srcfile = _vmprof.resolve_addr(addr)
                    if name == "" and srcfile == '-':
                        name = "<native symbol 0x%x>" % addr

                    str = "n:%s:%d:%s" % (name, lineno, srcfile)
                    if PY3:
                        str = str.encode()
                    out = [MARKER_NATIVE_SYMBOLS, struct.pack("l", addr),
                           struct.pack("l", len(str)),
                           str]
                    fileobj.write(b''.join(out))

    def sample_stack_now():
        """ Helper utility mostly for tests, this is considered
            private API.

            It will return a list of stack frames the python program currently
            walked.
        """
        stackframes = _vmprof.sample_stack_now()
        assert isinstance(stackframes, list)
        return stackframes

    def resolve_addr(addr):
        """ Private API, returns the symbol name of the given address.
            Only considers linking symbols found by dladdr.
        """
        return _vmprof.resolve_addr(addr)


_gzip_proc = None

def _gzip_start(fileno):
    """Spawn a gzip subprocess that writes compressed profile data to `fileno`.

    Return the subprocess' input fileno.
    """
    # XXX During the sprint in munich we found several issues
    # on bigger applications running vmprof. For instance:
    # coala or some custom medium sized scripts.
    return fileno
    #
    # Prefer system gzip and fall back to Python's gzip module
    if which("gzip"):
        gzip_cmd = ["gzip", "-", "-4"]
    else:
        gzip_cmd = ["python", "-u", "-m", "gzip"]
    global _gzip_proc
    _gzip_proc = subprocess.Popen(gzip_cmd, stdin=subprocess.PIPE,
                                  stdout=fileno, bufsize=-1,
                                  close_fds=(sys.platform != "win32"))
    if _gzip_proc.returncode is not None:
        # oh, the gzip process has terminated already?
        _gzip_proc = None
        return fileno # proceed without compressing the object
    return _gzip_proc.stdin.fileno()

def _gzip_finish():
    global _gzip_proc
    if _gzip_proc is not None:
        _gzip_proc.stdin.close()
        returncode = _gzip_proc.wait()
        assert returncode == 0, \
               "return code was non zero: %d" % returncode
        _gzip_proc = None
