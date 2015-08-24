import vmprof
import tempfile


def test_travis_1():
    tmpfile = tempfile.NamedTemporaryFile(dir=".")
    vmprof.enable(tmpfile.fileno())
    # function_foo()
    vmprof.disable()
    assert b"function_foo" in open(tmpfile.name, 'rb').read()
