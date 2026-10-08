import re

from vmprof.show import CallgrindPrinter
from vmprof.stats import Node


def build_tree():
    """ A tree covering python, native and JIT frames.

    <module>  100 samples, 10 of them its own
    `-- work   80 samples, 50 of them its own
        `-- JIT code  30
    `-- memcpy 10, native and without a filename
    """
    root = Node(1, 'py:<module>:1:prog.py', 100)
    work = root.add_child(2, 'py:work:10:prog.py', 80)
    work.add_child(3, 'jit:0x1234', 30)
    root.add_child(4, 'n:memcpy:0:-', 10)
    return root


def write_callgrind(tmpdir, tree):
    path = str(tmpdir.join('out.callgrind'))
    printer = CallgrindPrinter(output=path)
    printer._show(tree)
    with open(path) as fd:
        return fd.read()


def parse_callgrind(text):
    """ Minimal callgrind reader: self cost per fn and the call edges. """
    self_cost = {}
    edges = {}
    pos = None
    callee = None
    for line in text.splitlines():
        if line.startswith('fl='):
            filename = line[3:]
        elif line.startswith('fn='):
            pos = (filename, line[3:])
            self_cost.setdefault(pos, 0)
            callee = None
        elif line.startswith('cfl='):
            cfilename = line[4:]
        elif line.startswith('cfn='):
            callee = (cfilename, line[4:])
        elif line.startswith('calls='):
            pass
        elif re.match(r'^\d+ \d+$', line):
            cost = int(line.split()[1])
            if callee is not None:
                edges[(pos, callee)] = cost
                callee = None
            else:
                self_cost[pos] += cost
    return self_cost, edges


def test_callgrind_header(tmpdir):
    text = write_callgrind(tmpdir, build_tree())
    assert text.startswith('# callgrind format\n')
    assert 'positions: line\n' in text
    assert 'events: Periods\n' in text
    assert 'summary: 100\n' in text


def test_callgrind_self_cost(tmpdir):
    self_cost, _ = parse_callgrind(write_callgrind(tmpdir, build_tree()))
    assert self_cost == {
        ('prog.py', '<module>'): 10,
        ('prog.py', 'work'): 50,
        ('[jit]', '0x1234'): 30,
        ('???', 'memcpy'): 10,
    }
    # the summary has to match what the body adds up to
    assert sum(self_cost.values()) == 100


def test_callgrind_edges_carry_inclusive_cost(tmpdir):
    _, edges = parse_callgrind(write_callgrind(tmpdir, build_tree()))
    assert edges == {
        (('prog.py', '<module>'), ('prog.py', 'work')): 80,
        (('prog.py', '<module>'), ('???', 'memcpy')): 10,
        (('prog.py', 'work'), ('[jit]', '0x1234')): 30,
    }


def test_callgrind_every_callee_is_defined(tmpdir):
    self_cost, edges = parse_callgrind(write_callgrind(tmpdir, build_tree()))
    for _, callee in edges:
        assert callee in self_cost


def test_callgrind_folds_repeated_functions(tmpdir):
    """ A function reached by two paths is reported once, with costs summed. """
    root = Node(1, 'py:<module>:1:prog.py', 100)
    left = root.add_child(2, 'py:left:10:prog.py', 60)
    right = root.add_child(3, 'py:right:20:prog.py', 40)
    # same addr, so the same function, under two different callers
    left.add_child(4, 'py:leaf:30:prog.py', 50)
    right.add_child(4, 'py:leaf:30:prog.py', 30)

    self_cost, edges = parse_callgrind(write_callgrind(tmpdir, root))
    assert self_cost[('prog.py', 'leaf')] == 80
    assert edges[(('prog.py', 'left'), ('prog.py', 'leaf'))] == 50
    assert edges[(('prog.py', 'right'), ('prog.py', 'leaf'))] == 30


def test_callgrind_rounds_fractional_weights(tmpdir):
    """ Sample weights are floats once the profile carries timestamps.

    Costs are rounded on the way out, so the summary is the sum of what
    was actually written rather than the unrounded total: the tools check
    the former, and a rounding drift of a sample does not matter here.
    """
    root = Node(1, 'py:<module>:1:prog.py', 10.4)
    root.add_child(2, 'py:work:10:prog.py', 4.6)

    text = write_callgrind(tmpdir, root)
    for line in text.splitlines():
        if re.match(r'^\d+ ', line):
            assert re.match(r'^\d+ \d+$', line), line

    self_cost, _ = parse_callgrind(text)
    summary = int(re.search(r'^summary: (\d+)$', text, re.M).group(1))
    assert summary == sum(self_cost.values())
