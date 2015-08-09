import pytest
from vmprof.callgraph import SymbolicStackTrace, Frame, CallGraph, StackFrameNode
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
    root = StackFrameNode('main')
    foo1 = root[frame]
    foo2 = root[frame]
    assert foo1 is foo2
    assert foo1.frame == frame
    assert foo1.self_count == 0
    #
    foo1.self_count = 1
    foo1['bar'].self_count = 3
    assert root.cumulative_count() == 4

def test_StackFrameNode_getitem_str():
    root = StackFrameNode(Frame('main', False))
    fooframe = Frame('foo', False)
    foo1 = root[fooframe]
    foo2 = root['foo']
    assert foo1 is foo2



def test_CallGraph_add_stacktrace():
    def st(*symbols):
        stacktrace = [Frame(name, False) for name in symbols]
        return stacktrace
    #
    graph = CallGraph()
    graph.add_stacktrace(st('main', 'one', 'two-1'), count=1)
    graph.add_stacktrace(st('main', 'one', 'two-2'), count=2)
    graph.add_stacktrace(st('main', 'one'), count=1)
    #
    assert graph.root.frame == Frame('<all>', False)
    assert graph.root['main'].self_count == 0
    assert graph.root['main']['one'].self_count == 1
    assert graph.root['main']['one']['two-1'].self_count == 1
    assert graph.root['main']['one']['two-2'].self_count == 2
