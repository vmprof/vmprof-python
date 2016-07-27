import json
import zlib

import py
import six

import vmprof
from vmprof.stats import Node, Stats, JittedCode, AssemblerCode

def test_tree_basic():
    profiles = [([1, 2], 1, 1),
                ([1, 2], 1, 1)]
    stats = Stats(profiles, adr_dict={1: 'foo', 2: 'bar'})
    tree = stats.get_tree()
    assert tree == Node(1, 'foo', 2, {2: Node(2, 'bar', 2)})
    assert repr(tree) == '<Node: foo (2) [(2, bar)]>'

    profiles = [([1, 2], 1, 1),
                ([1, 3], 1, 1)]
    stats = Stats(profiles, adr_dict={1: 'foo', 2: 'bar', 3: 'baz'})
    tree = stats.get_tree()
    assert tree == Node(1, 'foo', 2, {
        2: Node(2, 'bar', 1),
        3: Node(3, 'baz', 1)})

def test_tree_jit():
    profiles = [([1], 1, 1),
                ([1, AssemblerCode(100), JittedCode(1)], 1, 1)]
    stats = Stats(profiles, adr_dict={1: 'foo'})
    tree = stats.get_tree()
    assert tree == Node(1, 'foo', 2)
    assert tree.meta['jit'] == 1

def test_read_simple():
    py.test.skip("think later")
    lib_cache = get_or_write_libcache('simple_nested.pypy.prof')
    path = py.path.local(__file__).join('..', 'simple_nested.pypy.prof')
    stats = vmprof.read_profile(path, virtual_only=True,
                                include_extra_info=True, lib_cache=lib_cache)
    tree = stats.get_tree()
    main_name = 'py:<module>:2:foo.py'
    foo_name = 'py:foo:6:foo.py'
    bar_name = 'py:bar:2:foo.py'
    assert tree['foo'].name == foo_name
    assert tree['foo'].meta['jit'] == 19
    assert tree['foo']['bar'].name == bar_name
    assert tree['foo']['bar'].meta['jit'] == 101
    assert tree['foo'].jitcodes == {140523638277712: 120}
    assert tree['foo']['bar'].jitcodes == {140523638275600: 27,
                                           140523638276656: 3}
    assert not tree['foo']['bar'].children
    assert tree['foo']['bar'].meta['gc:minor'] == 2
    data = json.loads(tree.as_json())
    main_addr = str(tree.addr)
    foo_addr = str(tree['foo'].addr)
    bar_addr = str(tree['foo']['bar'].addr)
    expected = [main_name, main_addr, 120, {}, [
        [foo_name, foo_addr, 120, {'jit': 19, 'gc:minor': 2}, [
            [bar_name, bar_addr, 101, {'gc:minor': 2, 'jit': 101}, []]]]]]
    assert data == expected
