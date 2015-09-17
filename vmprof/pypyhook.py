import pypyjit
import json

class JITInfoWriter(object):

    def __init__(self, fname, verbose=False):
        self.fname = fname
        self.verbose = verbose
        self.f = None

    def enable(self):
        self.f = open(self.fname, 'w')
        pypyjit.set_compile_hook(self.on_compile)

    def disable(self):
        pypyjit.set_compile_hook(None)
        self.f.close()
        self.f = None

    def on_compile(self, info):
        if info.type == 'loop':
            name = 'loop #%s' % (info.loop_no)
        else:
            name = '%s for loop #%s' % (info.type, info.loop_no)

        if self.verbose:
            print '[pypyjit] compiling:', name

        loop = dict(name= name,
                    asmstart = info.asmaddr,
                    asmend = info.asmaddr + info.asmlen)
        #
        self.f.write(json.dumps(loop))
        self.f.write('\n')



