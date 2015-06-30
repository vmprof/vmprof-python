import py
from vmprof.reader import LibraryData
from vmprof.addrspace import AddressSpace
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

    def test_tree(self):
        prof = read_profile(str(py.path.local(__file__).join(
            '..', 'test.prof')))
        tree = prof.get_tree()
        assert repr(tree) == '<Node: py:<module>:2:x.py (92) [(92, py:f:7:x.py)]>'
        values = list(tree.children.values())
        assert repr(values[0]) == '<Node: py:f:7:x.py (92) []>'
