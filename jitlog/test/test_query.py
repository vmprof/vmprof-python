from jitlog.objects import (TraceForest, FlatOp)
from jitlog import constants as c
from jitlog import query

class TestQueries(object):
    def q(self, forest, text):
        return query.new_unsafe_query(text)(forest)

    def test_query_empty_forest(self):
        f = TraceForest(3, is_32bit=False, machine='s390x')
        assert self.q(f, '') == None
        assert self.q(f, 'traces()') == []

    def test_query_small_forest(self):
        f = TraceForest(3, is_32bit=False, machine='s390x')
        #
        t = f.add_trace('loop', 0, 0, 'jd')
        stage = t.start_mark(c.MARK_TRACE_OPT)
        stage.append_op(FlatOp(0, 'load', [], '?'))
        #
        t2 = f.add_trace('loop', 1, 1, 'jd')
        stage = t2.start_mark(c.MARK_TRACE_OPT)
        stage.append_op(FlatOp(0, 'store', [], '?'))
        #
        assert len(f.traces) == 2
        assert self.q(f, '') == None
        assert self.q(f, 'traces(op(name="load"))') == [t]
