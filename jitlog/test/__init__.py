import py
import platform

if platform.system() == 'Windows':
    """ this is a low hanging fruit! there should be little work
        to get this going """
    py.test.skip("jitlog is not supported on windows (yet)")
