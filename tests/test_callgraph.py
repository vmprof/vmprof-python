import pytest
from cStringIO import StringIO
import textwrap
from vmprof.callgraph import (TickCounter, SymbolicStackTrace, Frame, CallGraph,
                              StackFrameNode)
from vmprof.reader import LibraryData
from vmprof.addrspace import AddressSpace

@pytest.fixture
def addrspace():
    lib = LibraryData("libfoo", 1000, 1500, is_virtual=False,
                      symbols=[(1000, "one"),
                               (1100, "two"),
                               (1200, "three")
                           ])
    virtlib = LibraryData("<virtuals>", 2000, 2500, is_virtual=True,
                          symbols=[(2000, "py:main"),
                                   (2100, "py:func"),
                               ])
    return AddressSpace([lib, virtlib])


def test_TickCounter():
    c = TickCounter()
    c['foo'] = 10
    c['bar'] = 4
    assert c.total() == 14


def test_SymbolicStackTrace(addrspace):
    stacktrace = [0x123, 1005, 1210, 2005]
    stacktrace = SymbolicStackTrace(stacktrace, addrspace)
    assert len(stacktrace) == 4
    assert stacktrace[1] == Frame('one', is_virtual=False)
    assert stacktrace[-1] == Frame('py:main', is_virtual=True)
    # XXX: why do addrspace.lookup return addr+1? Is it a feature or a bug? We
    # should document it
    assert repr(stacktrace) == '[0x0000000000000124, one, three, py:main]'

def test_StackFrameNode():
    frame = Frame('foo', False)
    root = StackFrameNode(Frame('main', False))
    foo1 = root[frame]
    foo2 = root[frame]
    assert foo1 is foo2
    assert foo1.frame == frame


def test_StackFrameNode_getitem_str():
    root = StackFrameNode(Frame('main', False))
    fooframe = Frame('foo', False)
    foo1 = root[fooframe]
    foo2 = root['foo']
    assert foo1 is foo2

def test_StackFrameNode_getitem_str_virtual():
    root = StackFrameNode(Frame('main', False))
    fooframe = Frame('foo', True)
    foo1 = root[fooframe]
    foo2 = root['foo']
    assert foo1 is foo2
    pytest.raises(KeyError, "root['foobar']")


class TestCallGraph:

    def stack(self, *symbols):
        stacktrace = [Frame(name, is_virtual=name.startswith('py:'))
                      for name in symbols]
        return stacktrace

    def pprint(self, tree):
        out = StringIO()
        tree.pprint(stream=out)
        return out.getvalue()

    def test_add_stacktrace(self):
        stack = self.stack
        graph = CallGraph()
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
                init: self{C: 1} cumulative{C: 1}
                py:foo: self{} cumulative{C: 8, JIT: 17} virtual{C: 3, JIT: 10}
                  execute_frame: self{C: 3} cumulative{C: 8, JIT: 17}
                    py:baz: self{} cumulative{JIT: 7} virtual{JIT: 7}
                      jit:loop#2: self{JIT: 7} cumulative{JIT: 7}
                    jit:loop#1: self{JIT: 10} cumulative{JIT: 10}
                    py:bar: self{} cumulative{C: 5} virtual{C: 5}
                      execute_frame: self{C: 5} cumulative{C: 5}
        """)

    def test_serialize(self):
        stack = self.stack
        graph = CallGraph()
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
            'frame': '<all>',
            'tag': 'C',
            'is_virtual': False,
            'self_ticks': {},
            'cumulative_ticks': {'JIT': 10, 'C': 3},
            'virtual_ticks': None,
            'children': [
                {'frame': 'main',
                 'tag': 'C',
                 'is_virtual': False,
                 'self_ticks': {},
                 'cumulative_ticks': {'JIT': 10, 'C': 3},
                 'virtual_ticks': None,
                 'children': [
                     {'frame': 'py:foo',
                      'tag': 'Python',
                      'is_virtual': True,
                      'self_ticks': {},
                      'cumulative_ticks': {'JIT': 10, 'C': 3},
                      'virtual_ticks': {'JIT': 10, 'C': 3},
                      'children': [
                          {'frame': 'execute_frame',
                           'tag': 'C',
                           'is_virtual': False,
                           'self_ticks': {'C': 3},
                           'cumulative_ticks': {'JIT': 10, 'C': 3},
                           'virtual_ticks': None,
                           'children': [
                               {'frame': 'jit:loop#1',
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
        graph = CallGraph()
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
        graph = CallGraph()
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
