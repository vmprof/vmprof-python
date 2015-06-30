import os
import pytest

from vmprof.reader import read_ranges, read_prof, LibraryData
from vmprof.addrspace import AddressSpace

RANGES = """0400000-005a1000 r-xp 00000000 08:01 5389781                            /home/fijal/.virtualenvs/cffi3/bin/python
007a0000-007a1000 r--p 001a0000 08:01 5389781                            /home/fijal/.virtualenvs/cffi3/bin/python
007a1000-007dd000 rw-p 001a1000 08:01 5389781                            /home/fijal/.virtualenvs/cffi3/bin/python
007dd000-007ed000 rw-p 00000000 00:00 0
020b3000-1f7d5000 rw-p 00000000 00:00 0                                  [heap]
7f9dafd4c000-7f9dafeff000 r-xp 00000000 08:01 296555                     /lib/x86_64-linux-gnu/libcrypto.so.1.0.0
7f9dafeff000-7f9db00fe000 ---p 001b3000 08:01 296555                     /lib/x86_64-linux-gnu/libcrypto.so.1.0.0
7f9db00fe000-7f9db0119000 r--p 001b2000 08:01 296555                     /lib/x86_64-linux-gnu/libcrypto.so.1.0.0
7f9db0119000-7f9db0124000 rw-p 001cd000 08:01 296555                     /lib/x86_64-linux-gnu/libcrypto.so.1.0.0
7f9db0124000-7f9db0128000 rw-p 00000000 00:00 0
7fffc971b000-7fffc973c000 rw-p 00000000 00:00 0                          [stack]
7fffc97fe000-7fffc9800000 r-xp 00000000 00:00 0                          [vdso]
ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0                  [vsyscall]
"""

FAKE_NM = """0000000000004ff0 t _ufc_dofinalperm_r
00000000000057f0 t _ufc_doit_r
0000000000218180 t _ufc_foobar
0000000000004de0 T _ufc_mk_keytab_r
0000000000005230 t _ufc_output_conversion_r
0000000000004790 t _ufc_setup_salt_r
000000000020a140 T _ufc_tables_lock
"""


here = pytest.fixture(lambda: os.path.dirname(__file__))


def test_read_ranges():
    def fake_reader(name):
        return FAKE_NM

    ranges = read_ranges(RANGES)
    assert len(ranges) == 13
    assert ranges[0].name == '/home/fijal/.virtualenvs/cffi3/bin/python'
    assert ranges[-3].name == '[stack]'
    assert ranges[-5].name == '/lib/x86_64-linux-gnu/libcrypto.so.1.0.0'
    assert ranges[-4].name == '0'
    symbols = ranges[-5].read_object_data(reader=fake_reader)
    s = ranges[-5].start
    assert symbols == [(s + 18320, '_ufc_setup_salt_r'),
                       (s + 19936, '_ufc_mk_keytab_r'),
                       (s + 20464, '_ufc_dofinalperm_r'),
                       (s + 21040, '_ufc_output_conversion_r'),
                       (s + 22512, '_ufc_doit_r'),
                       (s + 2138432, '_ufc_tables_lock'),
                       (s + 2195840, '_ufc_foobar')]


def test_read_profile(here):

    prof_path = os.path.join(here, 'test.prof')
    prof_content = open(prof_path, 'rb')

    period, profiles, symbols, ranges, interp_name = read_prof(prof_content)

    assert ranges[5].name == '/lib/x86_64-linux-gnu/liblzma.so.5.0.0'
    assert ranges[5].start == 140682901667840
    assert ranges[5].end == 140682901803008

    # symbols contains all kinds of crap, reverse it
    sym_dict = {}
    for k, v in symbols:
        sym_dict[v] = k
    assert 'py:f:7:x.py' in sym_dict
    assert 'py:g:4:x.py' in sym_dict
    assert 'py:<module>:2:x.py' in sym_dict

    addrspace = AddressSpace([
        LibraryData(
            'virtual',
            0x7000000000000000,
            0x7000000000010000,
            True,
            symbols=symbols)
    ])
    name, start_addr, is_virtual, _ = addrspace.lookup(sym_dict['py:<module>:2:x.py'])
    assert name == 'py:<module>:2:x.py'
    assert is_virtual
