#pragma once
#include <stddef.h>
#include <stdint.h>

struct RobinFunctionSymbol
{
  void *addr;
  size_t size;
  const char *name;
};

struct RobinDebugLine
{
  void *addr;
  int lineNumber;
};

struct RobinELFWriter;

#define ROBIN_WRITER_SIZE 128

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize an ELFWriter using the buffer provided by \p writer
 * @param writer The buffer to hold the writer object
 * @param writerSize the size of the writer object. Can be obtained by ROBIN_WRITER_SIZE
 * @return The initialized writer. Should point to the same location of the parameter `writer`
 * */
RobinELFWriter* RobinELFWriterInit(void* writer, size_t writerSize, uintptr_t machineCodeAddr,
                  size_t machineCodeSize, RobinFunctionSymbol *funcSymbols, size_t numFuncSymbols, size_t bufferSize,
                  const char *srcFileName, RobinDebugLine *lineNumbers,
                  size_t numLineNumbers);

// build the in-memory ELF object. The built object is stored in the writer. Returns the ELF object size (bytes)
size_t RobinELFWriterBuild(RobinELFWriter* writer);

// copy the in-memory ELF object to a buffer. The buffer must be larger than the byte size returned by RobinELFWriterBuild()
void RobinELFWriterCopy(RobinELFWriter* writer, void* buf);

void RobinELFWriterDestory(RobinELFWriter* writer);

#ifdef __cplusplus
}
#endif