#include "stack.h"

#include <libunwind.h>

#include "hotpatch/khash.h"
typedef uint64_t ptr_t;
KHASH_MAP_INIT_INT64(ptr, char*);
static khash_t(ptr) * ip_symbol_lookup = 0;

static int vmp_native_traces_enabled = 0;
static int vmp_native_traces_sp_offset = -1;
static ptr_t *vmp_ranges = NULL;
static size_t vmp_range_count = 0;


int vmp_walk_and_record_python_stack(PyFrameObject *frame, void ** result,
                                     int max_depth)
{
    void *ip, *sp;
    unw_cursor_t cursor;
    unw_context_t uc;
    unw_proc_info_t pip;

    int ret = unw_init_local(&cursor, &uc);
    if (ret < 0) {
        // could not initialize lib unwind cursor and context
        return -1;
    }

    PyFrameObject * top_most_frame = frame;
    PyFrameObject * compare_frame;
    int depth = 0;
    while (depth < max_depth) {
        if (!vmp_native_enabled()) {
            if (top_most_frame == NULL) {
                break;
            }
            // TODO add line profiling
            sp = (void*)CODE_ADDR_TO_UID(top_most_frame->f_code);
            result[depth++] = sp;
            top_most_frame = top_most_frame->f_back;
            continue;
        }
        unw_get_proc_info(&cursor, &pip);

        if (unw_get_reg(&cursor, UNW_REG_SP, (unw_word_t*)&sp) < 0) {
            // could not retrieve
            break;
        }

        if ((void*)pip.start_ip == PyEval_EvalFrameEx) {
            // yes we found one stack entry of the python frames!
            compare_frame = vmp_get_virtual_ip(sp);
            if (compare_frame != top_most_frame) {
                // uh we are screwed! the ip indicates we are have context
                // to a PyEval_EvalFrameEx function, but when we tried to retrieve
                // the stack located py frame it has a different address than the
                // current top_most_frame
                result[depth++] = (void*)-1;
                break;
            }
            sp = (void*)CODE_ADDR_TO_UID(top_most_frame->f_code);
            result[depth++] = sp;
            top_most_frame = top_most_frame->f_back;
        } else if (vmp_ignore_ip(pip.start_ip)) {
            // this is an instruction pointer that should be ignored,
            // (that is any function name in the mapping range of
            //  cpython, but of course not extenstions in site-packages))
        } else {
            ip = (void*)((ptr_t)pip.start_ip | 0x1);
            // mark native routines with the first bit set,
            // this is possible because compiler align to 8 bytes.
            // TODO need to check if this is possible on other 
            // compiler than e.g. gcc/clang too?
            //
            char * name = (char*)malloc(64);
            unw_word_t off = 0;
            int ret;
            if (unw_get_proc_name(&cursor, name, 64, &off) != 0) {
                khiter_t it;
                it = kh_get(ptr, ip_symbol_lookup, (ptr_t)ip);
                if (it == kh_end(ip_symbol_lookup)) {
                    it = kh_put(ptr, ip_symbol_lookup, (ptr_t)ip, &ret);
                    result[depth++] = ip;
                    kh_value(ip_symbol_lookup, it) = name;
                } else {
                    free(name);
                }
            } else {
                free(name);
            }
        }

        if (unw_step(&cursor) <= 0) {
            break;
        }
    }

    return depth;
}


void *vmp_get_virtual_ip(char* sp) {
    PyFrameObject *f = *(PyFrameObject **)(sp + vmp_native_sp_offset());
    return (void *)CODE_ADDR_TO_UID(f->f_code);
}

int vmp_native_enabled(void) {
    return vmp_native_traces_enabled;
}

int vmp_native_sp_offset(void) {
    return vmp_native_traces_sp_offset;
}

const char * vmp_get_symbol_for_ip(void * ip) {
    if ((((ptr_t)ip) & 0x1) == 0) {
        return NULL;
    }
    khiter_t it = kh_get(ptr, ip_symbol_lookup, (ptr_t)ip);
    if (it == kh_end(ip_symbol_lookup)) {
        return NULL;
    }

    return kh_value(ip_symbol_lookup, it);
}

#ifdef __unix__
int vmp_read_vmaps(const char * name) {

    FILE * fd = fopen(name, "rb");
    if (fd == NULL) {
        return 0;
    }
    char * saveptr;

    // assumptions to be verified:
    // 1) /proc/self/maps is ordered ascending by start address
    // 2) libraries that contain the name 'python' are considered
    //    candidates in the mapping to be ignored
    // 3) libraries containing site-packages are not considered
    //    candidates

    ssize_t size;
    vmp_range_count = 10;
    vmp_ranges = malloc(vmp_range_count*2);
    ptr_t * cursor = vmp_ranges;
    cursor[0] = 0;
    while ((size = getline(&line, &n, fd)) >= 0) {
        char * start_hex = strtok_r(line, "-", &saveptr);
        char * start_hex_end = saveptr;
        char * end_hex = strtok_r(NULL, " ", &saveptr);
        char * end_hex_end = saveptr;
        // skip over flags, ...
        strotk_r(NULL, " ", &saveptr);
        strotk_r(NULL, " ", &saveptr);
        strotk_r(NULL, " ", &saveptr);
        strotk_r(NULL, " ", &saveptr);

        char * name = saveptr;
        if (strstr(name, "python") != NULL && \
            strstr(name, "site-packages") == NULL) {
            // realloc if the chunk is to small
            ptrdiff_t diff = (cursor - vmp_ranges);
            if (diff/2 + 2 <= vmp_range_count) {
                vmp_ranges = realloc(vmp_ranges, vmp_range_count*2);
                vmp_range_count *= 2;
                cursor = vmp_ranges + diff;
            }

            ptr_t start = strtoll(start_hex, start_hex_end, 16);
            ptr_t end = strtoll(end_hex, end_hex_end, 16);
            if (cursor[0] == start) {
                cursor[0] = end;
            } else {
                if (cursor != vmp_ranges) {
                    // not pointing to the first entry
                    cursor++;
                }
                cursor[0] = start;
                cursor[1] = end;
                cursor++;
            }
        }
    }

    fclose(fd);
}
#endif

int vmp_native_enable(int offset) {
    vmp_native_traces_enabled = 1;
    vmp_native_traces_sp_offset = 1;
    ip_symbol_lookup = kh_init(ptr);

#ifdef __unix__
    vmp_read_vmaps("/proc/self/maps");
#endif
// TODO MAC use mach task interface to extract the same information
}

void *vmp_ip_ignore(void * ip)
{
    ptr_t * l = vmp_ranges;
    ptr_t * r = vmp_ranges + vmp_range_count/2;
    int i = vmp_binary_search_ranges(ip, l, r);
    if (i == -1) {
        return 0;
    }

    ptr_t v = l[i];
    ptr_t v2 = l[i+1];
    return v <= (ptr_t)ip && (ptr_t)ip < v2;
}

int vmp_binary_search_ranges(ptr_t ip, ptr_t * l, ptr_t * r)
{
    ptr_t * ol = l;
    ptr_t * or = r;
    while (1) {
        ptrdiff_t i = (r-l)/2;
        if (i == 0) {
            if (l == ol && *l > ip) {
                // at the start
                return -1;
            if (l == or && *l < ip) {
                // at the end
                return -1;
            } else {
                // we found the lower bound
                i = l - ol;
                return i;
            }
        }
        ptr_t * m = l + i;
        if (ip < *m) {
            r = m;
        } else {
            l = m;
        }

    }
    return -1;
}
