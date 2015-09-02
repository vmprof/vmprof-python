import py
from vmprof.reader import LibraryData
from vmprof.addrspace import AddressSpace
from vmprof import read_stats, Stats

class TestLibraryData(object):

    def test_lookup(self):
        d = LibraryData("lib", 1200, 1300)
        d.symbols = [(1234, "a"), (1250, None), (1260, "b")]
        #
        py.test.raises(KeyError, "d.lookup(1199)")
        assert d.lookup(1200) == (1200, "0x00000000000004b0:lib")
        assert d.lookup(1233) == (1233, "0x00000000000004d1:lib")
        assert d.lookup(1234) == (1234, "a")
        assert d.lookup(1240) == (1234, "a")
        assert d.lookup(1249) == (1234, "a")
        py.test.raises(KeyError, "d.lookup(1250)")
        py.test.raises(KeyError, "d.lookup(1259)")
        assert d.lookup(1260) == (1260, "b")
        assert d.lookup(1299) == (1260, "b")
        py.test.raises(KeyError, "d.lookup(1300)")


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

    def test_JIT_symbols(self):
        d = LibraryData("lib", 1234, 1300)
        d.symbols = [(1234, "a"), (1260, "b")]
        JIT_symbols = LibraryData("<JIT>", 1400, 1500)
        JIT_symbols.symbols = [(1400, "jit loop 1"), (1450, None),
                               (1480, "jit loop 2"), (1500, None)]
        addr = AddressSpace([d])
        addr.JIT_symbols = JIT_symbols
        #
        assert addr.lookup(1250) == ("a", 1234, False, d)
        assert addr.lookup(1400) == ("jit loop 1", 1400, False, JIT_symbols)
        assert addr.lookup(1449) == ("jit loop 1", 1400, False, JIT_symbols)
        assert addr.lookup(1490) == ("jit loop 2", 1480, False, JIT_symbols)

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

    def test_tree(self):
        prof = read_stats(str(py.path.local(__file__).join(
            '..', 'test.prof')))
        tree = prof.get_tree()
        assert repr(tree) == '<Node: py:<module>:2:x.py (92) [(92, py:f:7:x.py)]>'
        values = list(tree.children.values())
        assert repr(values[0]) == '<Node: py:f:7:x.py (92) []>'

    def test_wsgi_tree(self):
        prof = read_stats(str(py.path.local(__file__).join(
            '..', 'wsgi.prof')))
        tree = prof.get_tree()
        assert repr(tree) == '<Node: py:__call__:162:/vmprof/vmprof-test/.env/local/lib/python2.7/site-packages/django/core/handlers/wsgi.py (367) [(367, py:get_response:94:/vmprof/vmprof-test/.env/local/lib/python2.7/site-packages/django/core/handlers/base.py)]>'
        values = list(tree.children.values())
        assert repr(values[0]) == '<Node: py:get_response:94:/vmprof/vmprof-test/.env/local/lib/python2.7/site-packages/django/core/handlers/base.py (367) [(365, py:index:23:/vmprof/vmprof-test/app/main.py), (2, py:resolve:360:/vmprof/vmprof-test/.env/local/lib/python2.7/site-packages/django/core/urlresolvers.py)]>'
