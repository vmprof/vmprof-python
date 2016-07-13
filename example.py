import vmprof
def test_1():
    return [a for a in range(1000000)]


def test_2():
    return [b for b in range(10000000)]


def main():
    test_1()
    test_2()

    return test_1() + test_2()


with vmprof.profile("perf.data"):
    main()
with vmprof.profile("perf.data.gz", ["gzip", "-2"]):
    main()
