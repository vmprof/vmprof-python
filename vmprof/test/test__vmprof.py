import pytest
import sys
import _vmprof

def test_enable(tmpdir):
    prof = tmpdir.join('a.vmprof')
    with prof.open('w+b') as f:
        ret = _vmprof.enable(f.fileno(), 0.004)
        _vmprof.disable()
    assert ret is None

def test_enable_options(tmpdir):
    prof = tmpdir.join('a.vmprof')
    with prof.open('w+b') as f:
        ret = _vmprof.enable(f.fileno(), 0.004, lines=True)
        _vmprof.disable()
    assert ret == {}

def test_enable_options_unix_only(tmpdir):
    prof = tmpdir.join('a.vmprof')
    with prof.open('w+b') as f:
        ret = _vmprof.enable(f.fileno(), 0.004, memory=True, native=True,
                             real_time=True)
        _vmprof.disable()
    if sys.platform == 'win32':
        assert ret == {'memory': True, 'native': True, 'real_time': True}
    else:
        assert ret == {}

def test_enable_options_unknown(tmpdir):
    prof = tmpdir.join('a.vmprof')
    with prof.open('w+b') as f:
        ret = _vmprof.enable(f.fileno(), 0.004, foo=1, bar=2)
        _vmprof.disable()
    assert ret == {'foo': 1, 'bar': 2}
