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

from vmprof.reader import MARKER_NATIVE_SYMBOLS
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

def disable(only_needed=False):
    try:
        if IS_PYPY:
            # for now only_needed is not supported, we need to
            # copy the sources for pypy and update the code heren
            # (with version check)
            _vmprof.disable()
        else:
            _vmprof.disable(only_needed)
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
    def enable(fileno, period=DEFAULT_PERIOD, memory=False, lines=False, native=None, real_time=False, warn=True):
        pypy_version_info = sys.pypy_version_info[:3]
        if not isinstance(period, float):
            raise ValueError("You need to pass a float as an argument")
        if warn and pypy_version_info < (4, 1, 0):
            raise Exception("PyPy <4.1 have various kinds of bugs, pass warn=False if you know what you're doing")
        if warn and memory:
            print("Memory profiling is currently unsupported for PyPy. Running without memory statistics.")
        if warn and lines:
            print('Line profiling is currently unsupported for PyPy. Running without lines statistics.\n')
        # TODO fixes currently released pypy's
        native = _is_native_enabled(native)
        gz_fileno = _gzip_start(fileno)
        #
        # use the actual version number as soon as we implement real_time in pypy
        if pypy_version_info >= (999, 0, 0):
            _vmprof.enable(gz_fileno, period, memory, lines, native, real_time)
        if pypy_version_info >= (5, 8, 0):
            _vmprof.enable(gz_fileno, period, memory, lines, native)
        else:
            _vmprof.enable(gz_fileno, period) # , memory, lines, native)
else:
    # CPYTHON
    def enable(fileno, period=DEFAULT_PERIOD, memory=False, lines=False, native=None, real_time=False):
        if not isinstance(period, float):
            raise ValueError("You need to pass a float as an argument")
        gz_fileno = _gzip_start(fileno)
        native = _is_native_enabled(native)
        _vmprof.enable(gz_fileno, period, memory, lines, native, real_time)

    def sample_stack_now(skip=0):
        """ Helper utility mostly for tests, this is considered
            private API.

            It will return a list of stack frames the python program currently
            walked.
        """
        stackframes = _vmprof.sample_stack_now(skip)
        assert isinstance(stackframes, list)
        return stackframes

    def resolve_addr(addr):
        """ Private API, returns the symbol name of the given address.
            Only considers linking symbols found by dladdr.
        """
        return _vmprof.resolve_addr(addr)


def insert_real_time_thread():
    """ Inserts a thread into the list of threads to be sampled in real time mode.
        When enabling real time mode, the caller thread is inserted automatically.
        Returns the number of registered threads, or -1 if we can't insert thread.
    """
    return _vmprof.insert_real_time_thread()

def remove_real_time_thread():
    """ Removes a thread from the list of threads to be sampled in real time mode.
        When disabling in real time mode, *all* threads are removed automatically.
        Returns the number of registered threads, or -1 if we can't remove thread.
    """
    return _vmprof.remove_real_time_thread()


def is_enabled():
    """ Indicates if vmprof has already been enabled for this process.
        Returns True or False. None is returned if the state is unknown.
    """
    if hasattr(_vmprof, 'is_enabled'):
        return _vmprof.is_enabled()
    raise NotImplementedError("is_enabled is not implemented on this platform")

def get_profile_path():
    """ Returns the absolute path for the file that is currently open.
        None is returned if the backend implementation does not implement that function,
        or profiling is not enabled.
    """
    if hasattr(_vmprof, 'get_profile_path'):
        return _vmprof.get_profile_path()
    raise NotImplementedError("get_profile_path not implemented on this platform")

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
