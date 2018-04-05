import os
import sys
try:
    from shutil import which
except ImportError:
    from backports.shutil_which import which

import _vmprof

from vmprof import cli

from vmprof.reader import (MARKER_NATIVE_SYMBOLS, FdWrapper,
        LogReaderState, LogReaderDumpNative)
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
        # fish the file descriptor that is still open!
        if hasattr(_vmprof, 'stop_sampling'):
            fileno = _vmprof.stop_sampling()
            if fileno >= 0:
                # TODO does fileobj leak the fd? I dont think so, but need to check 
                fileobj = FdWrapper(fileno)
                l = LogReaderDumpNative(fileobj, LogReaderState())
                l.read_all()
                if hasattr(_vmprof, 'write_all_code_objects'):
                    _vmprof.write_all_code_objects(l.dedup)
        _vmprof.disable()
    except IOError as e:
        raise Exception("Error while writing profile: " + str(e))

def _is_native_enabled(native):
    if os.name == "nt":
        if native:
            raise ValueError("native profiling is only supported on Linux & Mac OS X")
        native = False
    else:
        if native is None:
            native = True
    return native

if IS_PYPY:
    def enable(fileno, period=DEFAULT_PERIOD, memory=False, lines=False, native=None, real_time=False, warn=True):
        pypy_version_info = sys.pypy_version_info[:3]
        MAJOR = pypy_version_info[0]
        MINOR = pypy_version_info[1]
        PATCH = pypy_version_info[2]
        if not isinstance(period, float):
            raise ValueError("You need to pass a float as an argument")
        if warn and pypy_version_info < (4, 1, 0):
            raise Exception("PyPy <4.1 have various kinds of bugs, pass warn=False if you know what you're doing")
        if warn and memory:
            print("Memory profiling is currently unsupported for PyPy. Running without memory statistics.")
        if warn and lines:
            print('Line profiling is currently unsupported for PyPy. Running without lines statistics.\n')
        native = _is_native_enabled(native)
        #
        if (MAJOR, MINOR, PATCH) >= (5, 9, 0):
            _vmprof.enable(fileno, period, memory, lines, native, real_time)
            return
        if real_time:
            raise ValueError('real_time=True requires PyPy >= 5.9')
        if MAJOR >= 5 and MINOR >= 8 and PATCH >= 0:
            _vmprof.enable(fileno, period, memory, lines, native)
            return
        _vmprof.enable(fileno, period)
else:
    # CPYTHON
    def enable(fileno, period=DEFAULT_PERIOD, memory=False, lines=False, native=None, real_time=False):
        if not isinstance(period, float):
            raise ValueError("You need to pass a float as an argument")
        native = _is_native_enabled(native)
        _vmprof.enable(fileno, period, memory, lines, native, real_time)

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
