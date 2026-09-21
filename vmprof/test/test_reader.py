
import io
import struct, pytest
from vmprof import reader
from vmprof.reader import (FileReadError, MARKER_HEADER, MARKER_STACKTRACE,
        MARKER_TRAILER, MARKER_VIRTUAL_IP, VERSION_SAMPLE_TIME,
        VERSION_TIMESTAMP, PROFILE_REAL_TIME)
from vmprof.profiler import read_profile
from vmprof.test.test_run import (read_one_marker, read_header,
        BufferTooSmallError, FileObjWrapper)

class FileObj(object):
    def __init__(self, lst=None):
        self.s = b''
        if lst is None:
            return
        for item in lst:
            if isinstance(item, int):
                item = struct.pack('l', item)
            self.write(item)

    def read(self, count):
        if self.s:
            s = self.s[:count]
            self.s = self.s[count:]
            return s # might be incomplete
        return b''

    def write(self, s):
        self.s += s

    def tell(self):
        return 0

def test_fileobj():
    f = FileObj()
    f.write(b'foo')
    f.write(b'bar')
    assert f.read(2) == b'fo'
    assert f.read(2) == b'ob'
    assert f.read(4) == b'ar'
    assert f.read(1) == b''

def test_fileobj_wrapper():
    f1 = FileObj([b"123456"])
    fw = FileObjWrapper(f1)
    assert fw.read(4) == b"1234"
    exc = pytest.raises(BufferTooSmallError, fw.read, 4)
    f1.write(b"789")
    fw = FileObjWrapper(f1, exc.value.get_buf())
    assert fw.read(3) == b'123'
    assert fw.read(4) == b'4567'
    assert fw.read(2) == b'89'


MS = 10**6 # nanoseconds in a millisecond

def build_profile(samples, period_usec=1000, version=VERSION_SAMPLE_TIME,
                  mode=0):
    """ A 64-bit little-endian profile of (trace, thread_id, timestamp_ns)
    samples, written the way src/vmprof_unix.c writes it. The timestamp
    is only written for version >= VERSION_SAMPLE_TIME.
    """
    word = lambda v: struct.pack('<q', v)
    out = [word(0), word(3), word(0), word(period_usec), word(0)]
    out.append(MARKER_HEADER + struct.pack('!h', version) +
               struct.pack('B', mode) + struct.pack('B', 4) + b'test')
    for addr, name in [(1, b'foo'), (2, b'bar'), (3, b'baz')]:
        out.append(MARKER_VIRTUAL_IP + word(addr) + word(len(name)) + name)
    for trace, thread_id, timestamp in samples:
        out.append(MARKER_STACKTRACE + word(1) + word(len(trace)))
        for addr in reversed(trace):
            out.append(word(addr))
        out.append(word(thread_id))
        if version >= VERSION_SAMPLE_TIME:
            out.append(struct.pack('<q', timestamp))
    out.append(MARKER_TRAILER + word(0) + word(0) + b'\x00' * 8)
    return io.BytesIO(b''.join(out))

def weights(stats):
    return [p[1] for p in stats.profiles]

def test_sample_weights():
    # 1 ms period, one thread. The third signal was lost, the last one
    # arrived early: the sample after the gap is worth two periods and
    # an early sample is never worth less than one.
    stats = read_profile(build_profile([
        ([1, 2], 7, 0),
        ([1, 2], 7, 1 * MS),
        ([1, 3], 7, 3 * MS),
        ([1, 2], 7, 3 * MS + MS // 2),
    ]))
    assert weights(stats) == [1.0, 1.0, 2.0, 1.0]
    assert stats.n_samples == 4
    assert stats.expected_samples == 5.0
    assert stats.get_lost_fraction() == pytest.approx(0.2)
    assert stats.lost_time == 0.0
    tree = stats.get_tree()
    assert tree.count == 5.0
    assert tree.children[2].count == 3.0
    assert tree.children[3].count == 2.0
    assert dict(stats.top_profile()) == {'foo': 5.0, 'bar': 3.0, 'baz': 2.0}
    assert repr(tree) == '<Node: foo (5) [(3, bar), (2, baz)]>'

def test_sample_weights_fractional():
    stats = read_profile(build_profile([
        ([1], 7, 0),
        ([1], 7, 5 * MS // 2),
    ]))
    assert weights(stats) == [1.0, 2.5]
    assert repr(stats.get_tree()) == '<Node: foo (3.5) []>'

def test_sample_weights_old_version():
    # no timestamps in the file: every sample is worth one period
    stats = read_profile(build_profile([
        ([1, 2], 7, 0),
        ([1, 2], 7, 5 * MS),
    ], version=VERSION_TIMESTAMP))
    assert weights(stats) == [1, 1]
    assert stats.get_lost_fraction() == 0.0
    assert stats.get_tree().count == 2

def test_sample_weights_capped():
    # the gap is capped at max_sample_gap, the excess ends up in lost_time
    profile = build_profile([
        ([1], 7, 0),
        ([1], 7, 30 * MS),
    ])
    stats = read_profile(profile, max_sample_gap=0.010)
    assert weights(stats) == [1.0, 10.0]
    assert stats.lost_time == pytest.approx(0.020)
    profile.seek(0)
    stats = read_profile(profile)
    assert weights(stats) == [1.0, 30.0]
    assert stats.lost_time == 0.0

def test_sample_weights_two_threads():
    # thread 7 and thread 8 both miss the signal at 1 ms
    samples = [
        ([1], 7, 0),
        ([2], 8, MS // 10),
        ([1], 7, 2 * MS),
        ([2], 8, 2 * MS + MS // 10),
    ]
    # cpu time mode: one process-wide timer, the timestamp is process cpu
    # time, so the gap is measured against the previous sample of any thread
    stats = read_profile(build_profile(samples))
    assert weights(stats) == [1.0, 1.0, 1.9, 1.0]
    # real time mode: every thread gets its own signal per period, so the
    # gap is measured per thread
    stats = read_profile(build_profile(samples, mode=PROFILE_REAL_TIME))
    assert weights(stats) == [1.0, 1.0, 2.0, 2.0]
    assert stats.get_lost_fraction() == pytest.approx(1.0 - 4.0 / 6.0)

def test_sample_weights_out_of_order():
    # buffers of different threads can be written out of order; a sample
    # that is older than the previous one is worth one period and does
    # not move the clock backwards
    stats = read_profile(build_profile([
        ([1], 7, 2 * MS),
        ([2], 8, 1 * MS),
        ([1], 7, 3 * MS),
    ]))
    assert weights(stats) == [1.0, 1.0, 1.0]

