import functools



class Filter(object):
    def __and__(self, o):
        assert isinstance(o, Filter)
        return AndFilter(self, o)

    def __or__(self, o):
        assert isinstance(o, Filter)
        return OrFilter(self, o)

    def _filter(self, trace):
        return True

class BinaryFilter(Filter):
    def __init__(self, left, right):
        self.left = left
        self.right = right

class AndFilter(BinaryFilter):
    def _filter(self, trace):
        return self.left._filter(trace) and self.right._filter(trace)

class OrFilter(BinaryFilter):
    def _filter(self, trace):
        if self.left._filter(trace):
            return True
        if self.right._filter(trace):
            return True
        return False

class OpFilter(Filter):
    def __init__(self, name):
        self.name = name

    def _filter(self, trace):
        name = self.name
        for stage in trace.stages.keys():
            for op in trace.get_stage(stage).get_ops():
                if name in op.opname:
                    return True
        return False

class FuncFilter(Filter):
    def __init__(self, name):
        self.name = name

    def _filter(self, trace):
        name = self.name
        stage = trace.get_stage('noopt')
        if not stage:
            return False

        for mp in stage.get_merge_points():
            if name in mp.get_scope():
                return True

        return False

class LoopFilter(Filter):
    def _filter(self, trace):
        return trace.type == 'loop'

class BridgeFilter(Filter):
    def _filter(self, trace):
        return trace.type == 'bridge'

loops = LoopFilter()

bridges = BridgeFilter()


QUERY_API = {
    # functions
    'op': OpFilter,
    'func': FuncFilter,
    # variable filters without state
    'loops': loops,
    'bridges': bridges,
}


class Query(object):
    def __init__(self, text):
        self.query_str = text
        self.forest = None

    def __call__(self, forest):
        self.forest = forest
        return self.evaluate(forest, self.query_str)

    def evaluate(self, forest, qstr):
        if qstr is None or qstr.strip() == '':
            return None

        api = {}
        for k,f in QUERY_API.items():
            api[k] = f
        # ---------------------------------------------
        # SECURITY ISSUE:
        # never execute this in the server environment
        # ---------------------------------------------
        f = eval(qstr, {}, api)
        return [t for t in forest.traces.values() if f._filter(t)]

def new_unsafe_query(query):
    return Query(query)


