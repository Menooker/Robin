#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "robin/ELFDef.hpp"
#include "robin/ELFWriter.hpp"
#include "robin/GdbJITSupport.h"
#include <new>

/* Minimal list of sections for the in-memory ELF object. */
enum
{
  GDBJIT_SECT_NULL,
  GDBJIT_SECT_text,
  //GDBJIT_SECT_eh_frame,
  GDBJIT_SECT_shstrtab,
  GDBJIT_SECT_strtab,
  GDBJIT_SECT_symtab,
  GDBJIT_SECT_debug_info,
  GDBJIT_SECT_debug_abbrev,
  GDBJIT_SECT_debug_line,
  GDBJIT_SECT__MAX
};

enum
{
  GDBJIT_SYM_UNDEF,
  GDBJIT_SYM_FILE,
  GDBJIT_SYM_FUNC,
  GDBJIT_SYM__MAX
};

/* In-memory ELF object. */
typedef struct GDBJITobj
{
  ELFheader hdr;                        /* ELF header. */
  ELFsectheader sect[GDBJIT_SECT__MAX]; /* ELF sections. */
  ELFsymbol sym[0];                     /* ELF symbol table. */
} GDBJITobj;

/* Template for in-memory ELF header. */
static const ELFheader elfhdr_template = {
    .emagic = {0x7f, 'E', 'L', 'F'},
    .eclass = LJ_64 ? 2 : 1,
    .eendian = 1 /*little endian*/,
    .eversion = 1,
#if LJ_TARGET_LINUX
    .eosabi = 0, /* Nope, it's not 3. */
#elif defined(__FreeBSD__)
    .eosabi = 9,
#elif defined(__NetBSD__)
    .eosabi = 2,
#elif defined(__OpenBSD__)
    .eosabi = 12,
#elif (defined(__sun__) && defined(__svr4__))
    .eosabi = 6,
#else
    .eosabi = 0,
#endif
    .eabiversion = 0,
    .epad = {0, 0, 0, 0, 0, 0, 0},
    .type = 1,
#if LJ_TARGET_X86
    .machine = 3,
#elif LJ_TARGET_X64
    .machine = 62,
#elif LJ_TARGET_ARM
    .machine = 40,
#elif LJ_TARGET_PPC
    .machine = 20,
#elif LJ_TARGET_MIPS
    .machine = 8,
#else
#error "Unsupported target architecture"
#endif
    .version = 1,
    .entry = 0,
    .phofs = 0,
    .shofs = offsetof(GDBJITobj, sect),
    .flags = 0,
    .ehsize = sizeof(ELFheader),
    .phentsize = 0,
    .phnum = 0,
    .shentsize = sizeof(ELFsectheader),
    .shnum = GDBJIT_SECT__MAX,
    .shstridx = GDBJIT_SECT_shstrtab};

namespace robin
{

  ELFObjectBuffer::ELFObjectBuffer(size_t numSymbols, size_t bufferSize) : numSymbols(numSymbols), bufferSize(bufferSize)
  {
    buffer = (uint8_t *)malloc(sizeof(GDBJITobj) + bufferSize);
    cur = data();
    bufStart = cur;
    /* Fill in ELF header and clear structures. */
    memcpy(&obj()->hdr, &elfhdr_template, sizeof(ELFheader));
    memset(&obj()->sect, 0, sizeof(ELFsectheader) * GDBJIT_SECT__MAX);
    memset(&obj()->sym, 0, sizeof(ELFsymbol) * numSymbols);
  }

  ELFObjectBuffer::~ELFObjectBuffer()
  {
    free(buffer);
  }
  uint8_t *ELFObjectBuffer::data() { return (uint8_t *)&obj()->sym[numSymbols]; }

  uint8_t *ELFObjectBuffer::reserve(size_t sz)
  {
    if (cur + sz > buffer + bufferSize)
    {
      intptr_t curDiff = cur - buffer;
      intptr_t bufStartDiff = bufStart - buffer;
      buffer = (uint8_t *)realloc(buffer, bufferSize * 2);
      bufferSize *= 2;
      cur = curDiff + buffer;
      bufStart = bufStartDiff + buffer;
    }
    return cur;
  }

  /* Add a zero-terminated string. */
  uint32_t ELFObjectBuffer::addStr(const char *str)
  {
    size_t len = strlen(str);
    uint8_t *p = reserve(len + 1);
    uint32_t ofs = (uint32_t)getCurrentSectionDataOffset();
    do
    {
      *p++ = (uint8_t)*str;
    } while (*str++);
    cur = p;
    return ofs;
  }

  /* Append a decimal number. */
  void ELFObjectBuffer::addDecimalNum(uint32_t n)
  {
    // UINT32_MAX = 4294967295U
    reserve(11);
    addDecimalNumImpl(n);
  }

  /* Add a ULEB128 value. */
  void ELFObjectBuffer::addULEB128(uint32_t v)
  {
    reserve(5);
    uint8_t *p = cur;
    for (; v >= 0x80; v >>= 7)
      *p++ = (uint8_t)((v & 0x7f) | 0x80);
    *p++ = (uint8_t)v;
    cur = p;
  }

  /* Add a SLEB128 value. */
  void ELFObjectBuffer::addSLEB128(int32_t v)
  {
    reserve(5);
    uint8_t *p = cur;
    for (; (uint32_t)(v + 0x40) >= 0x80; v >>= 7)
      *p++ = (uint8_t)((v & 0x7f) | 0x80);
    *p++ = (uint8_t)(v & 0x7f);
    cur = p;
  }

  void ELFObjectBuffer::addDecimalNumImpl(uint32_t n)
  {
    if (n >= 10)
    {
      uint32_t m = n / 10;
      n = n % 10;
      addDecimalNumImpl(m);
    }
    *cur++ = '0' + n;
  }

  ELFWriter::ELFWriter(uintptr_t machineCodeAddr,
                       size_t machineCodeSize, RobinFunctionSymbol *funcSymbols, size_t numFuncSymbols, size_t bufferSize,
                       const char *srcFileName, RobinDebugLine *lineNumbers,
                       size_t numLineNumbers) : machineCodeAddr{machineCodeAddr}, machineCodeSize{machineCodeSize}, funcSymbols{funcSymbols},
                                                numFuncSymbols{numFuncSymbols}, buf{numFuncSymbols + 2, bufferSize}, srcFileName{srcFileName}, lineNumbers{lineNumbers}, numLineNumbers{numLineNumbers}
  {
  }

#define DALIGNNOP(s)             \
  while ((uintptr_t)p & ((s)-1)) \
  *p++ = DW_CFA_nop
#define DSECT(name, stmt)                                          \
  {                                                                \
    uint32_t *szp_##name = (uint32_t *)p;                          \
    p += 4;                                                        \
    stmt                                                           \
        *szp_##name = (uint32_t)((p - (uint8_t *)szp_##name) - 4); \
  }

  void ELFWriter::dumpSectorHeader()
  {
    ELFsectheader *sect;
    buf.dump('\0'); /* Empty string at start of string table. */

#define SECTDEF(id, tp, al)                  \
  sect = &buf.obj()->sect[GDBJIT_SECT_##id]; \
  sect->name = buf.addStr("." #id);          \
  sect->type = ELFSECT_TYPE_##tp;            \
  sect->align = (al)

    SECTDEF(text, NOBITS, 16);
    sect->flags = ELFSECT_FLAGS_ALLOC | ELFSECT_FLAGS_EXEC;
    sect->addr = machineCodeAddr;
    sect->ofs = 0;
    sect->size = machineCodeSize;

    // SECTDEF(eh_frame, PROGBITS, sizeof(uintptr_t));
    // sect->flags = ELFSECT_FLAGS_ALLOC;

    SECTDEF(shstrtab, STRTAB, 1);
    SECTDEF(strtab, STRTAB, 1);

    SECTDEF(symtab, SYMTAB, sizeof(uintptr_t));
    sect->ofs = offsetof(GDBJITobj, sym);
    sect->size = sizeof(ELFsymbol) * buf.numSymbols;
    sect->link = GDBJIT_SECT_strtab;
    sect->entsize = sizeof(ELFsymbol);
    //https://segmentfault.com/a/1190000016834180
    sect->info = GDBJIT_SYM_FILE + 1; // the index of first non-local symbol

    SECTDEF(debug_info, PROGBITS, 1);
    SECTDEF(debug_abbrev, PROGBITS, 1);
    SECTDEF(debug_line, PROGBITS, 1);

#undef SECTDEF
  }

  void ELFWriter::dumpSymtab()
  {
    // https://refspecs.linuxbase.org/elf/gabi4+/ch4.symtab.html
    ELFsymbol *symtab = buf.obj()->sym;
    ELFsymbol *sym = &symtab[GDBJIT_SYM_FILE];

    buf.dump('\0'); /* Empty string at start of string table. */

    // the symbol 0 should have zero filled data
    // start of the first symbol
    sym->name = buf.addStr(srcFileName);
    sym->sectidx = ELFSECT_IDX_ABS;
    sym->info = ELFSYM_TYPE_FILE | ELFSYM_BIND_LOCAL;

    int currentSlot = GDBJIT_SYM_FILE + 1;
    for (size_t i = 0; i < numFuncSymbols; i++, currentSlot++)
    {
      sym = &symtab[currentSlot];
      sym->name = buf.addStr(funcSymbols[i].name);
      // this document says that we may use absolute addr by using IDX_ABS
      sym->sectidx = GDBJIT_SECT_text;
      sym->value = (uintptr_t)funcSymbols[i].addr - machineCodeAddr;
      sym->size = funcSymbols[i].size;
      sym->info = ELFSYM_TYPE_FUNC | ELFSYM_BIND_GLOBAL;
    }
  }

  void ELFWriter::writeDataLength(size_t offsetOfLength, size_t startOffset)
  {
    uint32_t *plength = (uint32_t *)buf.getSectionDataByAbsOffset(offsetOfLength);
    *plength = buf.getCurrentSectionDataAbsOffset() - startOffset - 4;
  }

  /* Initialize .debug_info section. */
  void ELFWriter::dumpDebuginfo()
  {
    size_t dataStart = buf.getCurrentSectionDataAbsOffset();
    // reserve the 4 bytes at the beginning of the data chunk for the length of it
    buf.dump<uint32_t>(0);               /*length*/
    buf.dump<uint16_t>(2);               /* DWARF version. */
    buf.dump<uint32_t>(0);               /* Abbrev offset. */
    buf.dump<int8_t>(sizeof(uintptr_t)); /* Pointer size. */

    buf.addULEB128(1);                           /* Abbrev #1: DW_TAG_compile_unit. */
    buf.addStr(srcFileName);                     /* DW_AT_name. */
    buf.dump(machineCodeAddr);                   /* DW_AT_low_pc. */
    buf.dump(machineCodeAddr + machineCodeSize); /* DW_AT_high_pc. */
    buf.dump<uint32_t>(0);                       /* DW_AT_stmt_list. */

    writeDataLength(dataStart, dataStart);
  }

  void ELFWriter::dumpDebugAbbrev()
  {
    /* Abbrev #1: DW_TAG_compile_unit. */
    buf.addULEB128(1);
    buf.addULEB128(DW_TAG_compile_unit);
    buf.dump<char>(DW_children_no);
    buf.addULEB128(DW_AT_name);
    buf.addULEB128(DW_FORM_string);
    buf.addULEB128(DW_AT_low_pc);
    buf.addULEB128(DW_FORM_addr);
    buf.addULEB128(DW_AT_high_pc);
    buf.addULEB128(DW_FORM_addr);
    buf.addULEB128(DW_AT_stmt_list);
    buf.addULEB128(DW_FORM_data4);
    buf.dump<char>(0);
    buf.dump<char>(0);
  }

  //https://wiki.osdev.org/DWARF
  typedef struct __attribute__((packed))
  {
    uint32_t length;
    uint16_t version;
    uint32_t header_length;
    uint8_t min_instruction_length;
    uint8_t default_is_stmt;
    int8_t line_base;
    uint8_t line_range;
    uint8_t opcode_base;
    uint8_t std_opcode_lengths[0];
  } DebugLineHeader;

  void ELFWriter::dumpLine(int op, uint32_t sz)
  {
    buf.dump<uint8_t>(DW_LNS_extended_op);
    buf.addULEB128(1 + sz);
    buf.dump<uint8_t>(op);
  }

  void ELFWriter::dumpDebugline()
  {
    size_t startPtr = buf.getCurrentSectionDataAbsOffset();
    buf.dump<uint32_t>(0); /*reserved length*/
    buf.dump<uint16_t>(2); /* DWARF version. */
    {
      size_t headerPtr = buf.getCurrentSectionDataAbsOffset();
      buf.dump<uint32_t>(0);                      /*reserved header length*/
      buf.dump<uint8_t>(1);                       /* Minimum instruction length. */
      buf.dump<uint8_t>(1);                       /* is_stmt. */
      buf.dump<int8_t>(0);                        /* Line base for special opcodes. */
      buf.dump<uint8_t>(2);                       /* Line range for special opcodes. */
      buf.dump<uint8_t>(DW_LNS_advance_line + 1); /* Opcode base at DW_LNS_advance_line+1. */
      buf.dump<uint8_t>(0);
      buf.dump<uint8_t>(1);
      buf.dump<uint8_t>(1); /* Standard opcode lengths. */
      /* Directory table. */
      buf.dump<uint8_t>(0);
      /* File name table. */
      buf.addStr(srcFileName);
      buf.addULEB128(0);
      buf.addULEB128(0);
      buf.addULEB128(0);
      buf.dump<uint8_t>(0);
      writeDataLength(headerPtr, headerPtr);
    }
    dumpLine(DW_LNE_set_address, sizeof(uintptr_t));
    buf.dump(machineCodeAddr);
    uint8_t *curAddr = (uint8_t *)machineCodeAddr;
    int curLine = 1;

    // buf.dump<uint8_t>(DW_LNS_advance_pc);
    // buf.addULEB128(machineCodeSize);
    for (size_t i = 0; i < numLineNumbers; i++)
    {
      RobinDebugLine *dl = &lineNumbers[i];
      uint8_t *addr = (uint8_t *)dl->addr;
      if (addr >= curAddr)
      {
        buf.dump<uint8_t>(DW_LNS_advance_pc);
        buf.addULEB128(addr - curAddr);
      }
      else
      {
        dumpLine(DW_LNE_set_address, sizeof(uintptr_t));
        buf.dump(addr);
      }
      buf.dump<uint8_t>(DW_LNS_advance_line);
      buf.addSLEB128(dl->lineNumber - curLine);
      buf.dump<uint8_t>(DW_LNS_copy);

      curLine = dl->lineNumber;
      curAddr = addr;
    }
    dumpLine(DW_LNE_set_address, sizeof(uintptr_t));
    buf.dump(machineCodeAddr + machineCodeSize);
    dumpLine(DW_LNE_end_sequence, 0);
    writeDataLength(startPtr, startPtr);
  }

  void ELFWriter::startSection(int sect)
  {
    buf.obj()->sect[sect].ofs = buf.startSection();
  }

  void ELFWriter::endSection(int sect)
  {
    buf.obj()->sect[sect].size = buf.getCurrentSectionDataOffset();
  }

#define SECTALIGN(p, a) \
  ((p) = (uint8_t *)(((uintptr_t)(p) + ((a)-1)) & ~(uintptr_t)((a)-1)))

  /* Build in-memory ELF object. */
  size_t ELFWriter::buildELFObject()
  {
    startSection(GDBJIT_SECT_shstrtab);
    dumpSectorHeader();
    endSection(GDBJIT_SECT_shstrtab);

    startSection(GDBJIT_SECT_strtab);
    dumpSymtab();
    endSection(GDBJIT_SECT_strtab);

    startSection(GDBJIT_SECT_debug_info);
    dumpDebuginfo();
    endSection(GDBJIT_SECT_debug_info);

    startSection(GDBJIT_SECT_debug_abbrev);
    dumpDebugAbbrev();
    endSection(GDBJIT_SECT_debug_abbrev);

    startSection(GDBJIT_SECT_debug_line);
    dumpDebugline();
    endSection(GDBJIT_SECT_debug_line);

    return buf.getCurrentSectionDataAbsOffset();
  }

  void ELFWriter::copyELFObject(void *target)
  {
    memcpy(target, buf.obj(), buf.getCurrentSectionDataAbsOffset());
  }

} // namespace robin

RobinELFWriter *RobinELFWriterInit(char *writter, size_t writterSize, uintptr_t machineCodeAddr,
                                   size_t machineCodeSize, RobinFunctionSymbol *funcSymbols, size_t numFuncSymbols, size_t bufferSize,
                                   const char *srcFileName, RobinDebugLine *lineNumbers,
                                   size_t numLineNumbers)
{
  static_assert(sizeof(robin::ELFWriter) <= ROBIN_WRITER_SIZE, "Wrong ROBIN_WRITER_SIZE");
  if (writterSize < sizeof(robin::ELFWriter))
  {
    return nullptr;
  }
  robin::ELFWriter *ret = new (writter) robin::ELFWriter{machineCodeAddr, machineCodeSize, funcSymbols,
                                                         numFuncSymbols, bufferSize, srcFileName, lineNumbers, numLineNumbers};
  return (RobinELFWriter *)ret;
}

// build the in-memory ELF object. The built object is stored in the writer. Returns the ELF object size (bytes)
size_t RobinELFWriterBuild(RobinELFWriter *writer)
{
  return ((robin::ELFWriter *)writer)->buildELFObject();
}

// copy the in-memory ELF object to a buffer. The buffer must be larger than the byte size returned by RobinELFWriterBuild()
void RobinELFWriterCopy(RobinELFWriter *writer, void *buf)
{
  return ((robin::ELFWriter *)writer)->copyELFObject(buf);
}

void RobinELFWriterDestory(RobinELFWriter *writer)
{
  using namespace robin;
  ((ELFWriter *)writer)->~ELFWriter();
}
