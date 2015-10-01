import sys
import random


class Node(object):
    def __init__(self, right, left):
        self.left = left
        self.right = right


class Digit(object):
    def __init__(self, v):
        self.v = v

    def eval(self):
        return ord(self.v) - ord('0')


class Plus(Node):
    def eval(self):
        return self.left.eval() + self.right.eval()


class Minus(Node):
    def eval(self):
        return self.left.eval() - self.right.eval()


def parse_pn(text):
    digits = [chr(ord('0') + i) for i in range(10)]

    stack = []
    for c in text:
        if c == '+':
            stack.append(Plus(stack.pop(), stack.pop()))
        elif c == '-':
            stack.append(Minus(stack.pop(), stack.pop()))
        elif c in digits:
            stack.append(Digit(c))
        else:
            assert False
    assert len(stack) == 1
    return stack[0]


def oracle(expr):
    return parse_pn(expr).eval() > 0


def find(expr):
    cur_expr = expr
    if oracle(expr):
        cur = 0
        while oracle(cur_expr):
            cur += 1
            cur_expr = cur_expr + "1-"
    else:
        cur = 0
        while not oracle(cur_expr):
            cur += 1
            cur_expr = cur_expr + "1+"
        cur = -cur + 1
    return cur


def gen_exp(lgt):
    stack_depth = 0
    exp = ''
    DIGITS = [chr(ord('0') + i) for i in range(10)]
    for i in range(lgt):
        if stack_depth > 1:
            c = random.choice(DIGITS + ['-', '+'] * 4)
        else:
            c = random.choice(DIGITS)
        if c in DIGITS:
            stack_depth += 1
        else:
            stack_depth -= 1
        exp += c
    while stack_depth > 1:
        exp += random.choice(['-', '+'])
        stack_depth -= 1
    return exp


def fuzzer(count):
    for i in range(count):
        exp = gen_exp(10)
        assert parse_pn(exp).eval() == find(exp)


if __name__ == '__main__':
    if len(sys.argv) == 2 and sys.argv[1] == 'demo':
        import time
        random.seed(42)
        l = []
        for k in range(100):
            t0 = time.time()
            fuzzer(100)
            l.append(time.time() - t0)
            print "%.1f ms" % ((time.time() - t0) * 1000)
        print "min: %.1fms max: %.1fms avg: %.1fms %.1fms" % (min(l) * 1000, max(l) * 1000,
                                                              sum(l) / len(l) * 1000, sum(l[30:]) / (len(l) - 30) * 1000)
        sys.exit(0)
    if len(sys.argv) > 1:
        count = int(sys.argv[1])
    else:
        count = 10000
    if len(sys.argv) > 2:
        random.seed(int(sys.argv[2]))
    else:
        random.seed(13)
    fuzzer(count)
