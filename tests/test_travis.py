import time
import vmprof
import tempfile


def function_foo():
    t0 = time.time()
    t = time.time() - t0 < 0.5
    while t:
        t = time.time() - t0 < 0.5
        print time.time() - t0


def test_travis_1():
    tmpfile = tempfile.NamedTemporaryFile(dir=".")
    vmprof.enable(tmpfile.fileno())
    function_foo()
    vmprof.disable()
    assert tmpfile.name
