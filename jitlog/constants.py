# generated constants from rpython/rlib/jitlog.py
import struct
MARK_JITLOG_START = struct.pack("b", 0x10)
MARK_INPUT_ARGS = struct.pack("b", 0x11)
MARK_RESOP_META = struct.pack("b", 0x12)
MARK_RESOP = struct.pack("b", 0x13)
MARK_RESOP_DESCR = struct.pack("b", 0x14)
MARK_ASM_ADDR = struct.pack("b", 0x15)
MARK_ASM = struct.pack("b", 0x16)
MARK_TRACE = struct.pack("b", 0x17)
MARK_TRACE_OPT = struct.pack("b", 0x18)
MARK_TRACE_ASM = struct.pack("b", 0x19)
MARK_STITCH_BRIDGE = struct.pack("b", 0x1a)
MARK_START_TRACE = struct.pack("b", 0x1b)
MARK_JITLOG_COUNTER = struct.pack("b", 0x1c)
MARK_INIT_MERGE_POINT = struct.pack("b", 0x1d)
MARK_JITLOG_HEADER = struct.pack("b", 0x1e)
MARK_MERGE_POINT = struct.pack("b", 0x1f)
MARK_COMMON_PREFIX = struct.pack("b", 0x20)
MARK_ABORT_TRACE = struct.pack("b", 0x21)
MARK_SOURCE_CODE = struct.pack("b", 0x22)
MARK_REDIRECT_ASSEMBLER = struct.pack("b", 0x23)
MARK_TMP_CALLBACK = struct.pack("b", 0x24)
MARK_JITLOG_END = struct.pack("b", 0x25)
MP_INDEX = (0x4,"i")
MP_SCOPE = (0x8,"s")
MP_FILENAME = (0x1,"s")
MP_OPCODE = (0x10,"s")
MP_LINENO = (0x2,"i")
MP_STR = (0x0,"s")
MP_INT = (0x0,"i")
SEM_TYPE_NAMES = {
    0x4: "index",
    0x8: "scope",
    0x1: "filename",
    0x10: "opcode",
    0x2: "lineno",
}
