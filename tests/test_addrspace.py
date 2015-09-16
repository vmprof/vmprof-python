import py
from vmprof.reader import LibraryData
from vmprof.addrspace import AddressSpace, JittedVirtual, JitAddr,\
     BlackholeWarmupFrame, VirtualFrame
from vmprof import read_profile, Stats


class TestAddrSpace(object):
    def test_lookup(self):
        d = LibraryData("lib", 1234, 1300)
        d.symbols = [(1234, "a"), (1260, "b")]
        d2 = LibraryData("lib2", 1400, 1500)
        d2.symbols = []
        addr = AddressSpace([d, d2])
        fn, _, is_virtual, _ = addr.lookup(1350)
        assert fn == '0x0000000000000547'  # outside of range
        fn, _, is_virtual, _ = addr.lookup(1250)
        assert fn == "a"

    def test_filter_profiles(self):
        d = LibraryData("lib", 12, 20)
        d.symbols = [(12, "lib:a"), (15, "lib:b")]
        d2 = LibraryData("<virtual>", 1000, 1500, True, symbols=[
            (1000, "py:one"), (1010, "py:two"),
            ])
        addr_space = AddressSpace([d, d2])
        profiles = [([12, 17, 1007], 1),
                    ([12, 12, 12], 1),
                    ([1000, 1020, 17], 1)]
        profiles = addr_space.filter(profiles)
        assert profiles == [
            (["py:one"], 1),
            (["py:two", "py:one"], 1),
            ]
        p = Stats(profiles)
        assert p.functions == {"py:one": 2, "py:two": 1}
        assert p.function_profile("py:two") == ([('py:one', 1)], 1)

    def test_filter_jit(self):
        l = LibraryData("lib", 100, 200, True)
        l.symbols = [(112, "py:one"), (113, "py:two")]
        addr_space = AddressSpace([l])
        r = addr_space.filter_addr([
            ([213, 0x1, 111, 112, 0x2], 1, 1)
            ], interp_name='pypy')
        assert r[0] == [([JitAddr(214), JittedVirtual(113), JittedVirtual(112)], 1, 1)]
        r = addr_space.filter_addr([
            ([111, 213, 0x1, 111, 112, 0x2], 1, 1)
            ], interp_name='pypy')
        assert r[0] == [([JitAddr(214), JittedVirtual(113), JittedVirtual(112)], 1, 1)]

    def test_filter_jit_2(self):
        l = LibraryData("python", 100, 200, True)
        l.symbols = [(101, 'py:one'), (105, 'py:two'), (107, 'py:three')]
        addr_space = AddressSpace([l])
        r = addr_space.filter_addr([
            ([0x2222, 0x1111, 0x1, 100, 104, 0x2, 0x3333,
            0x1, 104, 106, 0x2], 1, 1)
            ], interp_name='pypy')
        assert r[0][0][0] == [JitAddr(0x3334), JittedVirtual(107),
                              JitAddr(0x1112),
                              JittedVirtual(105), JittedVirtual(101)]

    def test_filter_gc_frames(self):
        l = LibraryData('foo', 100, 200, True)
        l.symbols = [(101, 'py:one'), (105, 'py:two'), (107, 'py:three')]
        l2 = LibraryData("binary", 1000, 2000, False)
        l2.symbols = [(1500, "pypy_g_resume_in_blackhole")]
        addr_space = AddressSpace([l, l2])
        r = addr_space.filter_addr([
            ([1, 1, 1, 1500, 101, 105], 1, 1)])
        assert r[0][0][0] == [VirtualFrame(105), VirtualFrame(101), BlackholeWarmupFrame(1500)]

    def test_tree(self):
        prof = read_profile(str(py.path.local(__file__).join(
            '..', 'test.prof')))
        tree = prof.get_tree()
        assert repr(tree) == '<Node: py:<module>:2:x.py (92) [(92, py:f:7:x.py)]>'
        values = list(tree.children.values())
        assert repr(values[0]) == '<Node: py:f:7:x.py (92) []>'

    def test_wsgi_tree(self):
        prof = read_profile(str(py.path.local(__file__).join(
            '..', 'wsgi.prof')))
        tree = prof.get_tree()
        assert repr(tree) == '<Node: py:__call__:162:/vmprof/vmprof-test/.env/local/lib/python2.7/site-packages/django/core/handlers/wsgi.py (367) [(367, py:get_response:94:/vmprof/vmprof-test/.env/local/lib/python2.7/site-packages/django/core/handlers/base.py)]>'
        values = list(tree.children.values())
        assert repr(values[0]) == '<Node: py:get_response:94:/vmprof/vmprof-test/.env/local/lib/python2.7/site-packages/django/core/handlers/base.py (367) [(365, py:index:23:/vmprof/vmprof-test/app/main.py), (2, py:resolve:360:/vmprof/vmprof-test/.env/local/lib/python2.7/site-packages/django/core/urlresolvers.py)]>'
