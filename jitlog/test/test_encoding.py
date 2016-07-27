# coding: utf-8
import sys
import py
from jitlog.objects import (TraceForest, MergePoint, FlatOp)
from jitlog import constants as const

PY3 = sys.version_info[0] >= 3

@py.test.mark.parametrize('encoding,text,decoded,bom',
    [('ascii',   b'a!1%$', u'a!1%$', None),
     ('utf-8',   b"\x41\xE2\x89\xA2\xCE\x91\x2E", u'A≢Α.', None),
     ('latin-1', b'\xDCber', u'Über', None),
    ])
def test_merge_point_extract_source_code(encoding,text,decoded,bom,tmpdir):
    forest = TraceForest(1)
    trace = forest.add_trace('loop', 0, 0)
    trace.start_mark(const.MARK_TRACE_OPT)
    file = tmpdir.join("file"+encoding+".py")
    l = []
    if bom:
        l.append(bom)
    l.append("# coding: ".encode(encoding))
    l.append(encoding.encode(encoding))
    l.append("\r\n".encode(encoding))
    l.append("print(\"".encode(encoding))
    l.append(text)
    l.append("\")".encode(encoding))
    l.append("\r\n".encode(encoding))
    file.write_binary(b''.join(l))
    trace.add_instr(MergePoint({0x1: str(file), 0x2: 2}))
    forest.extract_source_code_lines()
    line = forest.source_lines[str(file)][2]
    if PY3:
        assert line == (0, "print(\"" + decoded + "\")")
    else:
        assert line == (0, "print(\"" + text + "\")")
