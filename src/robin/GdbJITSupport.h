#pragma once
#include <stdint.h>
#include <stddef.h>

/* GDB JIT actions. */
enum
{
  GDBJIT_NOACTION = 0,
  GDBJIT_REGISTER,
  GDBJIT_UNREGISTER
};

/* GDB JIT entry. */
typedef struct GDBJITentry
{
  struct GDBJITentry *next_entry;
  struct GDBJITentry *prev_entry;
  const char *symfile_addr;
  uint64_t symfile_size;
} GDBJITentry;

/* GDB JIT descriptor. */
typedef struct GDBJITdesc
{
  uint32_t version;
  uint32_t action_flag;
  GDBJITentry *relevant_entry;
  GDBJITentry *first_entry;
} GDBJITdesc;

/* Combined structure for GDB JIT entry and ELF object. */
typedef struct GDBJITentryobj
{
  GDBJITentry entry;
  size_t sz;
  char obj[0];
} GDBJITentryobj;

#ifdef __cplusplus
extern "C"
{
#endif
  GDBJITentryobj *RobinGDBJITRegisterObject(void *elfObj, size_t objSize);
  void RobinGDBJITUnregisterObject(GDBJITentryobj *eo);

#ifdef __cplusplus
}
#endif