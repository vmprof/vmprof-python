import pytest
from cStringIO import StringIO
import textwrap
from vmprof.callgraph import (TickCounter, SymbolicStackTrace, Frame, CallGraph,
                              StackFrameNode, remove_jit_hack, unpack_frame_name)
from vmprof.reader import LibraryData
from vmprof.addrspace import AddressSpace

def MyFrame(name, is_virtual):
    return Frame(name, addr=0x00, lib=None, is_virtual=is_virtual)

@pytest.fixture
def libfoo():
    return LibraryData("libfoo", 1000, 1500, is_virtual=False,
                       symbols=[(1000, "one"),
                                (1100, "two"),
                                (1200, "three")
                            ])

@pytest.fixture
def libvirt():
    return LibraryData("<virtuals>", 2000, 2500, is_virtual=True,
                       symbols=[(2000, "py:main"),
                                (2100, "py:func"),
                       ])


@pytest.fixture
def addrspace(libfoo, libvirt):
    return AddressSpace([libfoo, libvirt])


def test_TickCounter():
    c = TickCounter()
    c['foo'] = 10
    c['bar'] = 4
    assert c.total() == 14


def test_SymbolicStackTrace(addrspace, libfoo, libvirt):
    stacktrace = [0x123, 1005, 1210, 2005]
    stacktrace = SymbolicStackTrace(stacktrace, addrspace)
    assert len(stacktrace) == 4
    assert stacktrace[1] == Frame('one', 1000, libfoo, is_virtual=False)
    assert stacktrace[-1] == Frame('py:main', 2000, libvirt, is_virtual=True)
    # XXX: why do addrspace.lookup return addr+1? Is it a feature or a bug? We
    # should document it
    assert repr(stacktrace) == '[0x0000000000000124, one, three, py:main]'


def test_unpack_frame_name():
    class FakeLib(object):
        def __init__(self, name):
            self.name = name
    def unpack(name, addr=0x00, lib=None, is_virtual=False):
        frame = Frame(name, addr, lib, is_virtual)
        return unpack_frame_name(frame)

    assert unpack('py:foo:42:/tmp/a.py') == ('py:foo:42:/tmp/a.py', None, None)
    assert unpack('py:foo:42:/tmp/a.py', is_virtual=True) == ('foo', '/tmp/a.py', '42')
    assert unpack('py:foo', is_virtual=True) == ('py:foo', None, None)
    assert unpack('qsort', lib=FakeLib('libc')) == ('qsort', 'libc', None)
    assert unpack('qsort') == ('qsort', None, None)

class TestStackFrameNode:

    def test_getitem_frame(self):
        frame = MyFrame('foo', False)
        root = StackFrameNode(MyFrame('main', False))
        foo1 = root[frame]
        foo2 = root[frame]
        assert foo1 is foo2
        assert foo1.frame == frame

    def test_getitem_str(self):
        root = StackFrameNode(MyFrame('main', False))
        fooframe = MyFrame('foo', False)
        foo1 = root[fooframe]
        foo2 = root['foo']
        assert foo1 is foo2

    def test_getitem_str_virtual(self):
        root = StackFrameNode(MyFrame('main', False))
        fooframe = MyFrame('foo', True)
        foo1 = root[fooframe]
        foo2 = root['foo']
        assert foo1 is foo2
        pytest.raises(KeyError, "root['foobar']")

    def test_serialize_children_order(self):
        root = StackFrameNode(MyFrame('main', False))
        # virtual frames first
        a = MyFrame('a', True)
        root[a] # create child
        #
        # then, order by cumulative ticks
        b1 = MyFrame('b1', False)
        b2 = MyFrame('b2', False)
        root[b1].cumulative_ticks = {'C': 5, 'JIT': 10}
        root[b2].cumulative_ticks = {'C': 5, 'JIT':  8}
        #
        # finally, order by name
        c1 = MyFrame('c1', False)
        c2 = MyFrame('c2', False)
        root[c1] # create child
        root[c2] # create child
        #
        data = root.serialize()
        frames = [child['name'] for child in data['children']]
        assert frames == ['a', 'b1', 'b2', 'c1', 'c2']
        

class TestCallGraph:

    def stack(self, *symbols):
        stacktrace = [MyFrame(name, is_virtual=name.startswith('py:'))
                      for name in symbols]
        return stacktrace

    def pprint(self, tree):
        out = StringIO()
        tree.pprint(stream=out)
        return out.getvalue()

    def test_add_stacktrace(self):
        stack = self.stack
        graph = CallGraph('test')
        graph.add_stacktrace(stack('main',
                                   'init'),
                             count=1)  # no virtual frames
        #
        graph.add_stacktrace(stack('main',
                                   'py:foo',
                                   'execute_frame'),
                             count=3)  # 3 ticks on py:foo
        #
        graph.add_stacktrace(stack('main',
                                   'py:foo',
                                   'execute_frame',
                                   'py:bar',
                                   'execute_frame'),
                             count=5)  # 5 ticks on py:bar
        #
        graph.add_stacktrace(stack('main',
                                   'py:foo',
                                   'execute_frame',
                                   'jit:loop#1'),
                             count=10) # 10 ticks on py:foo
        #
        graph.add_stacktrace(stack('main',
                                   'py:foo',
                                   'execute_frame',
                                   'py:baz',
                                   'jit:loop#2'),
                             count=7)  # 5 ticks on py:baz
        #
        out = self.pprint(graph.root)
        assert out == textwrap.dedent("""\
            <all>: self{} cumulative{C: 9, JIT: 17}
              main: self{} cumulative{C: 9, JIT: 17}
                py:foo: self{} cumulative{C: 8, JIT: 17} virtual{C: 3, JIT: 10}
                  execute_frame: self{C: 3} cumulative{C: 8, JIT: 17}
                    py:baz: self{} cumulative{JIT: 7} virtual{JIT: 7}
                      jit:loop#2: self{JIT: 7} cumulative{JIT: 7}
                    py:bar: self{} cumulative{C: 5} virtual{C: 5}
                      execute_frame: self{C: 5} cumulative{C: 5}
                    jit:loop#1: self{JIT: 10} cumulative{JIT: 10}
                init: self{C: 1} cumulative{C: 1}
        """)

    def test_serialize(self):
        stack = self.stack
        graph = CallGraph('test')
        graph.add_stacktrace(stack('main',
                                   'py:foo',
                                   'execute_frame'),
                             count=3)
        #
        graph.add_stacktrace(stack('main',
                                   'py:foo',
                                   'execute_frame',
                                   'jit:loop#1'),
                             count=10) # 10 ticks on py:foo
        #
        obj = graph.root.serialize()
        assert obj == {
            'name': '<all>',
            'filename': None,
            'line': None,
            'tag': None,
            'is_virtual': False,
            'self_ticks': {},
            'cumulative_ticks': {'JIT': 10, 'C': 3},
            'virtual_ticks': None,
            'children': [
                {'name': 'main',
                 'filename': None,
                 'line': None,
                 'tag': 'C',
                 'is_virtual': False,
                 'self_ticks': {},
                 'cumulative_ticks': {'JIT': 10, 'C': 3},
                 'virtual_ticks': None,
                 'children': [
                     {'name': 'py:foo',
                      'filename': None,
                      'line': None,
                      'tag': 'C',
                      'is_virtual': True,
                      'self_ticks': {},
                      'cumulative_ticks': {'JIT': 10, 'C': 3},
                      'virtual_ticks': {'JIT': 10, 'C': 3},
                      'children': [
                          {'name': 'execute_frame',
                           'filename': None,
                           'line': None,
                           'tag': 'C',
                           'is_virtual': False,
                           'self_ticks': {'C': 3},
                           'cumulative_ticks': {'JIT': 10, 'C': 3},
                           'virtual_ticks': None,
                           'children': [
                               {'name': 'jit:loop#1',
                                'filename': None,
                                'line': None,
                                'tag': 'JIT',
                                'is_virtual': False,
                                'self_ticks': {'JIT': 10},
                                'cumulative_ticks': {'JIT': 10},
                                'virtual_ticks': None,
                                'children': []}
                           ]}
                      ]}
                 ]}
            ]}

    def test_virtual_graph(self):
        stack = self.stack
        graph = CallGraph('test')
        graph.add_stacktrace(stack('main',
                                   'py:foo',
                                   'execute_frame'),
                             count=1)
        #
        graph.add_stacktrace(stack('main',
                                   'py:foo',
                                   'execute_frame',
                                   'CALL_FUNCTION',
                                   'py:bar',
                                   'execute_frame'),
                             count=10)
        #
        graph.add_stacktrace(stack('main',
                                   'py:foo',
                                   'execute_frame',
                                   'CALL_FUNCTION',
                                   'py:baz',
                                   'execute_frame'),
                             count=1)
        #
        vroot = graph.get_virtual_root()
        out = self.pprint(vroot)
        assert out == textwrap.dedent("""\
            <all>: self{} cumulative{C: 12}
              py:foo: self{} cumulative{C: 12} virtual{C: 1}
                py:bar: self{} cumulative{C: 10} virtual{C: 10}
                py:baz: self{} cumulative{C: 1} virtual{C: 1}
        """)

    def test_virtual_graph_merge_children(self):
        stack = self.stack
        graph = CallGraph('test')
        graph.add_stacktrace(stack('main',
                                   'py:foo',
                                   'execute_frame'),
                             count=1)
        #
        graph.add_stacktrace(stack('main',
                                   'py:foo',
                                   'execute_frame',
                                   'CALL_FUNCTION',
                                   'py:bar',
                                   'execute_frame'),
                             count=10)
        #
        graph.add_stacktrace(stack('main',
                                   'py:foo',
                                   'execute_frame',
                                   'CALL_METHOD',
                                   'some_other_internal_func',
                                   'py:bar',
                                   'jit:loop1'),
                             count=100)
        #
        vroot = graph.get_virtual_root()
        out = self.pprint(vroot)
        assert out == textwrap.dedent("""\
            <all>: self{} cumulative{C: 11, JIT: 100}
              py:foo: self{} cumulative{C: 11, JIT: 100} virtual{C: 1}
                py:bar: self{} cumulative{C: 10, JIT: 100} virtual{C: 10, JIT: 100}
        """)

    def test_from_profiles(self, addrspace):
        # note that stacktraces are top-to-bottom
        stacktrace1 = [1101, 2000, 1001] # two, py:main, one
        stacktrace2 = [1201, 2101, 1101, 2000, 1001] # three, py:func, two, py:main, one
        #
        profiles = [
          # (stacktrace, count, thread_id)
            (stacktrace1, 5, 0),
            (stacktrace2, 2, 0),
        ]
        #
        graph = CallGraph.from_profiles('test', addrspace, profiles)
        out = self.pprint(graph.root)
        assert out == textwrap.dedent("""\
            <all>: self{} cumulative{C: 7}
              one: self{} cumulative{C: 7}
                py:main: self{} cumulative{C: 7} virtual{C: 5}
                  two: self{C: 5} cumulative{C: 7}
                    py:func: self{} cumulative{C: 2} virtual{C: 2}
                      three: self{C: 2} cumulative{C: 2}
        """)

    def test_remove_jit_hack(self):
        START = 0x02
        END = 0x01
        py_main = 0x7000000000000001
        py_foo = 0x7000000000000002
        stacktrace = [10,
                      py_main,
                      20,
                      START,
                      py_main, # this is removed
                      30,
                      40,
                      END,
                      50,
                      py_main,
                      60,
                      START,
                      py_foo,  # this is not removed
                      70,
                      80,
                      END,
                      90]
        stacktrace = remove_jit_hack(stacktrace)
        assert stacktrace == [10, py_main, 20, 30, 40, 50, py_main, 60, py_foo, 70, 80, 90]
