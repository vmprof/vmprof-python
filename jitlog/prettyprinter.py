from colorama import init, deinit, Fore, Back, Style

class PrettyPrinter(object):
    def __enter__(self):
        pass

    def __exit__(self):
        pass

    def op(self, op):
        suffix = ''
        if op.result is not None and op.result != '?':
            suffix = "%s = " % self.var(op.result)
        descr = op.descr
        if descr is None:
            descr = ''
        else:
            descr = ', @' + self.descr(descr)
        args = [self.var(arg) for arg in op.args]
        return '%s%s(%s%s)' % (suffix, self.opname(op.opname),
                                ', '.join(args), descr)

    def trace(self, fd, trace):
        for name, stage in trace.stages.items():
            fd.write(self.stage_name(stage))
            fd.write("\n")
            for op in stage.getoperations():
                fd.write('  ' + self.op(op) + '\n')

    def var(self, var):
        return var

    def descr(self, descr):
        return descr

    def opname(self, opname):
        return opname

    def stage_name(self, stage):
        return str(stage)

class ColoredPrettyPrinter(PrettyPrinter):
    def __enter__(self):
        init()
        return self

    def __exit__(self, a, b, c):
        deinit()

    def opname(self, opname):
        return Fore.CYAN + opname + Style.RESET_ALL

    def var(self, var):
        if len(var) > 0:
            if var[0] == 'i':
                return Fore.BLUE + var + Style.RESET_ALL
            if var[0] == 'p' or var[0] == 'r':
                return Fore.GREEN + var + Style.RESET_ALL
            if var[0] == 'f':
                return Fore.BLUE + var + Style.RESET_ALL
        return Fore.RED + var + Style.RESET_ALL

