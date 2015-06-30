import os
from time import time
import vmprof


class Burner:

    ITER_N = 100000

    def __init__(self):
        self._rand = 123
        self._current = self._next_rand()

    def _next_rand(self):
        # http://rosettacode.org/wiki/Linear_congruential_generator
        self._rand = (1103515245 * self._rand + 12345) & 0x7fffffff
        return self._rand

    def _iterate(self, n):
        started = time()
        for i in range(n):
            self._current ^= self._next_rand()
        ended = time()
        return float(ended - started) * 1000.

    def burn(self, ms):
        done = 0
        iters = 0
        while done < ms:
            done += self._iterate(self.ITER_N)
            iters += 1
        return iters * self.ITER_N, iters, done


def test():
    RUN_MS = 1000
    RUNTIME = 30

    print("Running for {} seconds ..\n".format(RUNTIME))

    b = Burner()
    for i in range(RUNTIME):
        t, i, d = b.burn(RUN_MS)
        print("Actual run-time: {} / Requested run-time: {}, {} iterations. Total iterations: {}".format(d, RUN_MS, i, t))


if __name__ == '__main__':

    PROFILE_FILE = 'vmprof_cpuburn.dat'

    outfd = os.open(PROFILE_FILE, os.O_RDWR | os.O_CREAT | os.O_TRUNC)
    vmprof.enable(outfd, period=0.01)
    test()
    vmprof.disable()

    print("\nProfile written to {}.".format(PROFILE_FILE))
    print("To view the profile, run: vmprofshow {}".format(PROFILE_FILE))
