#include "dyn_stack_alloc.h"

struct DynStack {
  const uint8_t* buffer;
  uint8_t*       head;
  uint8_t*       end;
};

int dyn_stack_init(DynStack* _Stack, uint8_t* _buf, size_t _s)
{
  if (!_Stack || !_buf)
    return 1;

  *_Stack = (DynStack){
    .buffer = _buf,
    .head = _buf,
    .end = _buf + _s
  };

  return 0;
}

size_t dyn_stack_space_left(const DynStack* _Stack)
{
  if (!_Stack || !_Stack->head || !_Stack->end)
    return 0;

  return (size_t)(_Stack->end - _Stack->head);;
}

void* dyn_stack_malloc(DynStack* _Stack, size_t _memsize)
{
  if (_memsize > dyn_stack_space_left(_Stack))
    return NULL;

}


void dyn_stack_reset(DynStack* _Stack);
