vmprof is a delicate piece of software. Following considerations should
be present when developing it.

Supported platform combinations (all combinations are supported):

* pypy (>=4.1), cpython

* windows, os x and linux

CPython should be tested on both TeamCity and Travis, PyPy is more
patchy since there is never a new enough version on either. Since PyPy
only exercises the pure python part, please test it each time you change
and interface between `_vmprof` and `vmprof`.

## Signals

On OS X and Linux we handle signal handlers. This means that we have
to be very very careful at what we can and cannot do. Notably, we can't
use malloc, locks or refcounting in any of the signal handlers. Python data
should be read-only and we should be prepared to read garbage or NULL
from anything.

On windows we use an external thread, so it's an imperative we freeze
the thread we're inspecting. It's possible that the interpreter state
handling is not thread safe in a way it should be, investigate how we
can improve it.
