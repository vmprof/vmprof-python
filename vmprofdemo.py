
import random, sys

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

def iteration_eval(expr):
	stack = []
	for item in expr:
		if ord('0') <= ord(item) <= ord('9'):
			stack.append(ord(item) - ord('0'))
		elif item == '+':
			stack.append(stack.pop() + stack.pop())
		elif item == '-':
			right = stack.pop()
			left = stack.pop()
			stack.append(left - right)
	assert len(stack) == 1
	return stack[0]

def parse_pn(text):
	stack = []
	for c in text:
		if c == '+':
			stack.append(Plus(stack.pop(), stack.pop()))
		elif c == '-':
			stack.append(Minus(stack.pop(), stack.pop()))
		elif ord('0') <= ord(c) <= ord('9'):
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

DIGITS = [chr(ord('0') + i) for i in range(10)]

def gen_exp(lgt):
	stack_depth = 0
	exp = ''
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
		for k in range(30):
			t0 = time.time()
			fuzzer(100)
			print "%.1f ms" % ((time.time() - t0) * 1000)
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