import functools

def queryresult(func):
    return func
#@queryresult
def traces(query, func=None):
    """
    """
    if func is None:
        return [t for t in query.forest.traces.values()]
    traces = []
    for id,trace in query.forest.traces.items():
        if func(trace):
            traces.append(trace)

    return traces

def op(query, stage='opt', name=None):
    def operation_query(trace):
        for op in trace.get_stage(stage).get_ops():
            if name is not None and name in op.opname:
                return True
        return False
    return operation_query

QUERY_FUNCS = {
    'traces': traces,
    'op': op,
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

        api_funcs = {k: functools.partial(f,self) \
                     for k,f in QUERY_FUNCS.items()}
        # ---------------------------------------------
        # SECURITY ISSUE:
        # never execute this in the server environment
        # ---------------------------------------------
        return eval(qstr, {}, api_funcs)

def new_unsafe_query(query):
    return Query(query)


