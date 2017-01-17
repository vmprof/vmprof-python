#pragma once

/**
 * Returns 0 on success
 */
int vmp_patch_callee_trampoline(const char * callee_addr);

/**
 * Return 0 on success, -1 if the trampoline is not in place.
 * Any other value indicates a fatal error!
 */
int vmp_unpatch_callee_trampoline(const char * callee_name);

int vmp_find_frameobj_on_stack(const char * callee_addr);
