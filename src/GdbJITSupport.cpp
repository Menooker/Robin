#include "robin/GdbJITSupport.h"
#include "robin/ELFWriter.hpp"
#include "robin/ELFApi.h"
#include <stdlib.h>
#include <string.h>

#ifndef ROBIN_DONT_DEFINE_GDB_SYMBOLS
GDBJITdesc __jit_debug_descriptor = {
    1, GDBJIT_NOACTION, NULL, NULL};
/* GDB sets a breakpoint at this function. */
extern "C" void __attribute__((noinline)) __jit_debug_register_code()
{
    __asm__ __volatile__("");
};

#else
extern GDBJITdesc __jit_debug_descriptor;
#endif

GDBJITentryobj* RobinGDBJITRegisterObject(void* elfObj, size_t objSize)
{
  /* Allocate memory for GDB JIT entry and ELF object. */
  size_t sz = (size_t)(sizeof(GDBJITentryobj) + objSize);
  GDBJITentryobj *eo = (GDBJITentryobj *)malloc(sz);
  memcpy(eo->obj, elfObj, objSize);
  eo->sz = sz;
  /* Link new entry to chain and register it. */
  eo->entry.prev_entry = NULL;
  eo->entry.next_entry = __jit_debug_descriptor.first_entry;
  if (eo->entry.next_entry)
    eo->entry.next_entry->prev_entry = &eo->entry;
  eo->entry.symfile_addr = (const char *)&eo->obj;
  eo->entry.symfile_size = objSize;
  __jit_debug_descriptor.first_entry = &eo->entry;
  __jit_debug_descriptor.relevant_entry = &eo->entry;
  __jit_debug_descriptor.action_flag = GDBJIT_REGISTER;
  __jit_debug_register_code();
  return eo;
}

void RobinGDBJITUnregisterObject(GDBJITentryobj* eo)
{
  if (eo) {
    if (eo->entry.prev_entry)
      eo->entry.prev_entry->next_entry = eo->entry.next_entry;
    else
      __jit_debug_descriptor.first_entry = eo->entry.next_entry;
    if (eo->entry.next_entry)
      eo->entry.next_entry->prev_entry = eo->entry.prev_entry;
    __jit_debug_descriptor.relevant_entry = &eo->entry;
    __jit_debug_descriptor.action_flag = GDBJIT_UNREGISTER;
    __jit_debug_register_code();
    free(eo);
  }
}


