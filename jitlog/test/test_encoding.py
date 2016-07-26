# coding: utf-8
import sys
import py
from jitlog.objects import (TraceForest, MergePoint, FlatOp)
from jitlog import constants as const

PY3 = sys.version_info[0] >= 3

@py.test.mark.parametrize('encoding,text,decoded',
    [('ascii', b'a!1%$', u'a!1%$')])
def test_merge_point_extract_source_code(encoding,text,decoded,tmpdir):
    forest = TraceForest(1)
    trace = forest.add_trace('loop', 0, 0)
    trace.start_mark(const.MARK_TRACE_OPT)
    file = tmpdir.join("file"+encoding+".py")
    l = []
    l.append(b"# coding: ")
    l.append(encoding.encode(encoding))
    l.append(b"\r\n")
    l.append(b"print(\"")
    l.append(text)
    l.append(b"\")")
    l.append(b"\r\n")
    file.write(b''.join(l))
    trace.add_instr(MergePoint({0x1: str(file), 0x2: 2}))
    forest.extract_source_code_lines()
    assert forest.source_lines[str(file)][2] == (0, "print(\"" + decoded + "\")")
