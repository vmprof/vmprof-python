import six

class Stats(object):
    def __init__(self, profiles, adr_dict=None, jit_frames=None, interp=None):
        self.profiles = profiles
        self.adr_dict = adr_dict
        self.functions = {}
        self.generate_top()
        if jit_frames is None:
            jit_frames = set()
        if interp is None:
            interp = 'pypy' # why not
        self.interp = interp
        self.jit_frames = jit_frames

    def display(self, no):
        prof = self.profiles[no][0]
        return [self._get_name(elem) for elem in prof]

    def generate_top(self):
        for profile in self.profiles:
            current_iter = {}
            for addr in profile[0]:
                if addr not in current_iter:  # count only topmost
                    self.functions[addr] = self.functions.get(addr, 0) + 1
                    current_iter[addr] = None

    def top_profile(self):
        return [(self._get_name(k), v) for (k, v) in six.iteritems(self.functions)]

    def _get_name(self, addr):
        if self.adr_dict is not None:
            try:
                return self.adr_dict[addr]
            except KeyError:
                if addr in self.jit_frames:
                    name = 'jit:' + hex(addr)
                    self.adr_dict[addr] = name
                    return name
                raise

        return addr

    def function_profile(self, top_function):
        """ Show functions that we call (directly or indirectly) under
        a given addr
        """
        result = {}
        total = 0
        for profile in self.profiles:
            current_iter = {}  # don't count twice
            counting = False
            for addr in profile[0]:
                if counting:
                    if addr in current_iter:
                        continue
                    current_iter[addr] = None
                    result[addr] = result.get(addr, 0) + 1
                else:
                    if addr == top_function:
                        counting = True
                        total += 1
        result = result.items()
        result.sort(lambda a, b: cmp(a[1], b[1]))
        return result, total

    def get_tree(self):
        top_addr = self.profiles[0][0][0]
        top = Node(top_addr, self._get_name(top_addr))
        top.count = len(self.profiles)
        for profile in self.profiles:
            cur = top
            for i in range(1, len(profile[0])):
                addr = profile[0][i]
                cur = cur.add_child(addr, self._get_name(addr))
        # get the first "interesting" node, that is after vmprof and pypy
        # mess
        while True:
            if top.name.startswith('py:<module>') and 'vmprof/__main__.py' not in top.name:
                return top
            if len(top.children) > 1:
                # pick the biggest one in case of branches in vmprof
                next = None
                count = -1
                for c in top.children.values():
                    if c.count > count:
                        count = c.count
                        next = c
                top = next
            else:
                top = top['']

class Node(object):
    """ children is a dict of addr -> Node
    """
    _self_count = None
    flat = False
    
    def __init__(self, addr, name):
        self.children = {}
        self.name = name
        self.addr = addr
        self.count = 1 # starts at 1

    def __getitem__(self, item):
        if isinstance(item, int):
            return self.children[item]
        for v in self.children.values():
            if item in v.name:
                return v
        raise KeyError

    def update_meta_from(self, c, no_jit=False):
        for elem, value in six.iteritems(c.meta):
            if elem != 'jit':
                self.meta[elem] = self.meta.get(elem, 0) + value

    def flatten(self):
        if self.flat:
            return self
        new = Node(self.addr, self.name)
        new.meta = {}
        new.jit_codes = {}
        new_children = {}
        new.count = self.count
        for addr, c in six.items(self.children):
            c = c.flatten()
            if c.name.startswith('meta'):
                name = c.name[5:]
                new.meta[name] = new.meta.get(name, 0) + c.count
                new.update_meta_from(c)
                assert not c.children
            elif c.name.startswith('jit'):
                new.update_meta_from(c, no_jit=True)
                new.meta['jit'] = new.meta.get('jit', 0) + c.count
            else:
                new_children[addr] = c # normal
        new.children = new_children
        new.flat = True
        return new

    def as_json(self):
        import json
        return json.dumps(self.flatten()._serialize())

    def _serialize(self):
        chld = [ch._serialize() for ch in six.itervalues(self.children)]
        # if we don't make str() of addr here, JS does its
        # int -> float -> int losy convertion without
        # any warning
        return [self.name, str(self.addr), self.count, self.meta, chld]
    
    def _rec_count(self):
        c = 1
        for x in six.itervalues(self.children):
            c += x._rec_count()
        return c

    def walk(self, callback):
        callback(self)
        for c in six.itervalues(self.children):
            c.walk(callback)

    def cumulative_meta(self, d=None):
        if d is None:
            d = {}
        for c in six.itervalues(self.children):
            c.cumulative_meta(d)
        for k, v in six.iteritems(self.meta):
            d[k] = d.get(k, 0) + v
        return d

    def _filter(self, count):
        # XXX make a copy
        for key, c in self.children.items():
            if c.count < count:
                del self.children[key]
            else:
                c._filter(count)

    def get_self_count(self):
        if self._self_count is not None:
            return self._self_count
        self._self_count = self.count
        for elem in six.itervalues(self.children):
            self._self_count -= elem.count
        return self._self_count

    self_count = property(get_self_count)

    def add_child(self, addr, name):
        try:
            next = self.children[addr]
            next.count += 1
        except KeyError:
            next = Node(addr, name)
            self.children[addr] = next
        return next

    def __repr__(self):
        items = self.children.items()
        items.sort()
        child_str = ", ".join([("(%d, %s)" % (v.count, v.name))
                               for k, v in items])
        return '<Node: %s (%d) [%s]>' % (self.name, self.count, child_str)
