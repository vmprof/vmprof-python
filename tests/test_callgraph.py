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

    def test_add_stacktrace(self):
        def stack(*symbols):
            stacktrace = [Frame(name, is_virtual=name.startswith('py:'))
                          for name in symbols]
            return stacktrace
        #
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
        out = StringIO()
        graph.root.pprint(stream=out)
        out = out.getvalue()
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
