#pragma once
#include <stdint.h>


#define LJ_TARGET_X64 1
#define LJ_64 1

typedef struct ELFheader
{
  uint8_t emagic[4];
  uint8_t eclass;
  uint8_t eendian;
  uint8_t eversion;
  uint8_t eosabi;
  uint8_t eabiversion;
  uint8_t epad[7];
  uint16_t type;
  uint16_t machine;
  uint32_t version;
  uintptr_t entry;
  uintptr_t phofs;
  uintptr_t shofs;
  uint32_t flags;
  uint16_t ehsize;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstridx;
} ELFheader;

typedef struct ELFsectheader
{
  uint32_t name;
  uint32_t type;
  uintptr_t flags;
  uintptr_t addr;
  uintptr_t ofs;
  uintptr_t size;
  uint32_t link;
  uint32_t info;
  uintptr_t align;
  uintptr_t entsize;
} ELFsectheader;

#define ELFSECT_IDX_ABS 0xfff1

enum
{
  ELFSECT_TYPE_PROGBITS = 1,
  ELFSECT_TYPE_SYMTAB = 2,
  ELFSECT_TYPE_STRTAB = 3,
  ELFSECT_TYPE_NOBITS = 8
};

#define ELFSECT_FLAGS_WRITE 1
#define ELFSECT_FLAGS_ALLOC 2
#define ELFSECT_FLAGS_EXEC 4

typedef struct ELFsymbol
{
#if LJ_64
  uint32_t name;
  uint8_t info;
  uint8_t other;
  uint16_t sectidx;
  uintptr_t value;
  uint64_t size;
#else
  uint32_t name;
  uintptr_t value;
  uint32_t size;
  uint8_t info;
  uint8_t other;
  uint16_t sectidx;
#endif
} ELFsymbol;

enum
{
  ELFSYM_TYPE_FUNC = 2,
  ELFSYM_TYPE_FILE = 4,
  ELFSYM_BIND_LOCAL = 0 << 4,
  ELFSYM_BIND_GLOBAL = 1 << 4,
};

/* DWARF definitions. */
#define DW_CIE_VERSION 1

enum
{
  DW_CFA_nop = 0x0,
  DW_CFA_offset_extended = 0x5,
  DW_CFA_def_cfa = 0xc,
  DW_CFA_def_cfa_offset = 0xe,
  DW_CFA_offset_extended_sf = 0x11,
  DW_CFA_advance_loc = 0x40,
  DW_CFA_offset = 0x80
};

enum
{
  DW_EH_PE_udata4 = 3,
  DW_EH_PE_textrel = 0x20
};

enum
{
  DW_TAG_compile_unit = 0x11
};

enum
{
  DW_children_no = 0,
  DW_children_yes = 1
};

enum
{
  DW_AT_name = 0x03,
  DW_AT_stmt_list = 0x10,
  DW_AT_low_pc = 0x11,
  DW_AT_high_pc = 0x12
};

enum
{
  DW_FORM_addr = 0x01,
  DW_FORM_data4 = 0x06,
  DW_FORM_string = 0x08
};

enum
{
  DW_LNS_extended_op = 0,
  DW_LNS_copy = 1,
  DW_LNS_advance_pc = 2,
  DW_LNS_advance_line = 3
};

enum
{
  DW_LNE_end_sequence = 1,
  DW_LNE_set_address = 2
};

enum
{
#if LJ_TARGET_X86
  DW_REG_AX,
  DW_REG_CX,
  DW_REG_DX,
  DW_REG_BX,
  DW_REG_SP,
  DW_REG_BP,
  DW_REG_SI,
  DW_REG_DI,
  DW_REG_RA,
#elif LJ_TARGET_X64
  /* Yes, the order is strange, but correct. */
  DW_REG_AX,
  DW_REG_DX,
  DW_REG_CX,
  DW_REG_BX,
  DW_REG_SI,
  DW_REG_DI,
  DW_REG_BP,
  DW_REG_SP,
  DW_REG_8,
  DW_REG_9,
  DW_REG_10,
  DW_REG_11,
  DW_REG_12,
  DW_REG_13,
  DW_REG_14,
  DW_REG_15,
  DW_REG_RA,
#elif LJ_TARGET_ARM
  DW_REG_SP = 13,
  DW_REG_RA = 14,
#elif LJ_TARGET_PPC
  DW_REG_SP = 1,
  DW_REG_RA = 65,
  DW_REG_CR = 70,
#elif LJ_TARGET_MIPS
  DW_REG_SP = 29,
  DW_REG_RA = 31,
#else
#error "Unsupported target architecture"
#endif
};
