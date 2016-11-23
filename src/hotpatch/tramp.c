#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sysexits.h>
#include <sys/mman.h>
#include <err.h>
#include <assert.h>
#include <unistd.h>

#include "arch.h"
#include "bin_api.h"
#include "util.h"

#define FREELIST_INLINES 3

/*
 * trampoline specific stuff
 */
static struct tramp_st2_entry *tramp_table;
static size_t tramp_size;   // number of tramps in tramp_table
static size_t tramp_length; // size, in bytes, of each tramp

/*
 * inline trampoline specific stuff
 */
static size_t inline_tramp_size;
static struct inline_tramp_st2_entry *inline_tramp_table;
struct memprof_config memprof_config;


void init_memprof_config_base(void) {
  memset(&memprof_config, 0, sizeof(memprof_config));
  memprof_config.offset_heaps_slot_limit = SIZE_MAX;
  memprof_config.offset_heaps_slot_slot = SIZE_MAX;
  memprof_config.pagesize = getpagesize();
  assert(memprof_config.pagesize);
}


void
create_tramp_table(void)
{
  size_t i;
  void *region, *ent, *inline_ent;
  size_t tramp_sz = 0, inline_tramp_sz = 0;

  ent = arch_get_st2_tramp(&tramp_sz);
  tramp_length = tramp_sz;
  inline_ent = arch_get_inline_st2_tramp(&inline_tramp_sz);
  assert(ent && inline_ent);

  region = bin_allocate_page();
  if (region == MAP_FAILED)
    errx(EX_SOFTWARE, "Failed to allocate memory for stage 1 trampolines.");

  tramp_table = region;
  inline_tramp_table = region + memprof_config.pagesize / 2;

  for (i = 0; i < (memprof_config.pagesize / 2) / tramp_sz; i++) {
    memcpy(tramp_table + i, ent, tramp_sz);
  }

  for (i = 0; i < (memprof_config.pagesize / 2) / inline_tramp_sz; i++) {
    memcpy(inline_tramp_table + i, inline_ent, inline_tramp_sz);
  }
}

void*
insert_tramp(const char *trampee, void *tramp, size_t *out_tramp_size)
{
  void *trampee_addr = bin_find_symbol(trampee, NULL, 1);
  void *tramp_addr;
  int inline_ent = inline_tramp_size;

  if (trampee_addr == NULL) {
      errx(EX_SOFTWARE, "Failed to locate required symbol %s", trampee);
  } else {
    tramp_table[tramp_size].addr = tramp;
    tramp_addr = &tramp_table[tramp_size];
    if (bin_update_image(trampee, tramp_addr, NULL) != 0)
      errx(EX_SOFTWARE, "Failed to insert tramp for %s", trampee);
    tramp_size++;
    if (out_tramp_size != NULL)
        *out_tramp_size = tramp_length;
    return tramp_addr;
  }
}
