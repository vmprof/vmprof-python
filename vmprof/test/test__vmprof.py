import pytest
import sys
import _vmprof

def enable_and_disable(fname, *args, **kwargs):
    with fname.open('w+b') as f:
        try:
            if not kwargs:
                # try hard to pass kwargs == NULL, instead of {}
                return _vmprof.enable(f.fileno(), *args)
            else:
                return _vmprof.enable(f.fileno(), *args, **kwargs)
        finally:
            _vmprof.stop_sampling()
            _vmprof.disable()

def test_enable(tmpdir):
    prof = tmpdir.join('a.vmprof')
    ret = enable_and_disable(prof, 0.004)
    assert ret is None

def test_enable_options(tmpdir):
    prof = tmpdir.join('a.vmprof')
    ret = enable_and_disable(prof, 0.004, lines=True)
    assert ret == {}

def test_enable_options_unix_only(tmpdir):
    prof = tmpdir.join('a.vmprof')
    ret = enable_and_disable(prof, 0.004, memory=True, native=True,
                             real_time=True)
    if sys.platform == 'win32':
        assert ret == {'memory': True, 'native': True, 'real_time': True}
    else:
        assert ret == {}

def test_enable_options_unknown(tmpdir):
    prof = tmpdir.join('a.vmprof')
    ret = enable_and_disable(prof, 0.004, foo=1, bar=2)
    assert ret == {'foo': 1, 'bar': 2}
