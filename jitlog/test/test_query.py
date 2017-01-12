from jitlog.objects import (TraceForest, FlatOp, MergePoint)
from jitlog import constants as c
from jitlog import query

class TestQueries(object):
    def q(self, forest, text):
        return query.new_unsafe_query(text)(forest)

    def test_query_empty_forest(self):
        f = TraceForest(3, is_32bit=False, machine='s390x')
        assert self.q(f, '') == None
        assert self.q(f, 'loops & bridges') == []

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
        assert self.q(f, 'op("load")') == [t]

    def test_query_loops_and_bridges(self):
        f = TraceForest(3, is_32bit=False, machine='s390x')
        #
        t = f.add_trace('loop', 0, 0, 'jd')
        t2 = f.add_trace('bridge', 1, 1, 'jd')
        #
        assert len(f.traces) == 2
        assert self.q(f, 'loops') == [t]
        assert self.q(f, 'bridges') == [t2]

    def test_filter(self):
        from jitlog.query import loops, bridges, Filter
        f = TraceForest(3, is_32bit=False, machine='s390x')
        loop = f.add_trace('loop', 0, 0, 'su')
        bridge = f.add_trace('bridge', 1, 1, 'shi')
        assert loops._filter(loop)
        assert not loops._filter(bridge)
        assert not bridges._filter(loop)
        assert bridges._filter(bridge)
        r = loops | bridges
        assert isinstance(r, Filter)
        assert r._filter(loop)
        assert r._filter(bridge)

    def test_func_filter(self):
        from jitlog.query import FuncFilter as func
        f = TraceForest(3, is_32bit=False, machine='s390x')
        loop = f.add_trace('loop', 0, 0, 'su')
        stage = loop.start_mark(c.MARK_TRACE)
        stage.append_op(MergePoint({ c.MP_SCOPE[0]: '_sake_in_a_glass' }))
        assert not func('hello')._filter(loop)
        assert func('sake')._filter(loop)
        assert not func('s_a_k_e')._filter(loop)

