
static ptrdiff_t vmprof_unw_get_custom_offset(void* ip) {
	// overloaded in places like pypy
	return -1;
}
