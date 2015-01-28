#if !defined(TRAMP__)
#define TRAMP__

void init_memprof_config_base(void);

/*
 * create_tramp_table - create the trampoline tables.
 */
void
create_tramp_table(void);

/*
 * insert_tramp - insert a trampoline.
 *
 * Given:
 *  - trampee: function in which we want to install the trampoline.
 *  - tramp:   pointer to the function to be called from the trampoline.
 *  - tramp_size: output arg, if given it will contain the size of the trampoline
 *
 * This function is responsible for installing the requested trampoline
 * at the location of "trampee".  This results in tramp() being called
 * whenever trampee() is executed.
 */
void*
insert_tramp(const char *trampee, void *tramp, size_t* out_tramp_size);
#endif
