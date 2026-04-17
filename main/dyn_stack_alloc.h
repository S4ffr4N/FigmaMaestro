#ifndef __DYN_STACK_ALLOC_H__
#define __DYN_STACK_ALLOC_H__

#include <stdlib.h>
#include <stdint.h>

/* TODO: Test this */

typedef struct DynStack DynStack;

/* Init DynStack to _buf
 * Size of _buf on stack must be same as _s
 * For instance declare _buf as uint8_t buf[1024] then _s must be 1024*/
int dyn_stack_init(DynStack* _Stack, uint8_t* _buf, size_t _s);

/* Allocate _memsize bytes to DynStack
 * Returns pointer to start of allocation */
void* dyn_stack_alloc(DynStack* _Stack, size_t _memsize);

/* Check the amount of bytes currently allocated */
size_t dyn_stack_space_left(const DynStack* _Stack);

/* Free specific stack space */
void dyn_stack_free(DynStack* _Stack);

/* Clear all allocations */
void dyn_stack_reset(DynStack* _Stack);


#endif
