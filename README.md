# Robin - JIT compiler helpers

Currently it includes
 * In-memory ELF (DWARF) symbol file generator
 * Dynamically registering symbols and debug lines to GDB - this helps to enable you JIT-compiled code to be debugged by GDB

## ELF-DWARF symbol file generator

Here is an example to show how we can generate an in memory ELF object with DWARF debug info. Assume we have JIT'd a module which contains two functions. The base pointer of the module is given in `base`. And the function pointers are given in `func1` and `func2` below. The size of the module and the functions are also given. The memory address range `[base, base+size_base)` should contain all functions' memory ranges defined by `[funcN, funcN+lenN)`.

```c++
#include "robin/ELFWriter.hpp"

void generate(void* func1, size_t len1, void* func2, size_t len2, void* base, void* size_base) {
    RobinFunctionSymbol funcs[2] = {{func1, len1, "end_sect"},
                                    {func2, len2, "st_sect"}};
    RobinDebugLine lines[4] = {
        {(uint8_t *)func2, 7},
        {(uint8_t *)func2 + 12, 8},
        {(uint8_t *)func1, 2},
        {(uint8_t *)func1 + 12, 3},
    };
    robin::ELFWriter writer{(uintptr_t)base, size_base, funcs, 2, 500, "test/example_src.c", lines, 4};
    writer.buildELFObject();
    printf("ELF file Size=%zu\n", writer.getELFObjectSize());
    auto pELFObj = writer.getELFObjectView();
    // use the pELFObj
    ...
}
```

The above example defines two functions in DWARF: the first function is named "end_sect" and its address is `func1`. Its size in bytes is `len1`. The second function is named "st_sect" and its address is `func2`. Its size in bytes is `len2`. These infomation is passed via the `RobinFunctionSymbol` struct:

```c++
struct RobinFunctionSymbol
{
  void *addr;
  size_t size;
  const char *name;
};
```

The example also maps 4 positions in the code to 4 lines in source code. It is done by the `RobinDebugLine` struct:

```c++
struct RobinDebugLine
{
  void *addr;
  int lineNumber;
};
```

The `addr` field records the position in the machine code, and the `lineNumber` fields records the line number in the source code. The path to the source file is later given in `ELFWriter` constructor.

The example creates a `ELFWriter` object with code:

```c++
robin::ELFWriter writer{(uintptr_t)base, size_base, funcs, 2, 500, "test/example_src.c", lines, 4};
```

The prototype of the constructor is

```c++
ELFWriter(uintptr_t machineCodeAddr,
            size_t machineCodeSize, RobinFunctionSymbol *funcSymbols, size_t numFuncSymbols, size_t bufferSize,
            const char *srcFileName, RobinDebugLine *lineNumbers,
            size_t numLineNumbers)
```

The parameters `machineCodeAddr` and `machineCodeSize` are the base pointer of all functions given in `funcSymbols` and the buffer size of `machineCodeAddr`. The parameters `funcSymbols`, `numFuncSymbols`, `lineNumbers` and `numLineNumbers` has been discussed above. The parameter `bufferSize` is the internal buffer size for the `ELFWriter` to hold the generated ELF file. A too small buffer size will let `ELFWriter` re-allocate memory frequently. And a too large buffer size will waste memory. A reasonable buffer size may range from a few hunderds to a few thousands, based on the number of `funcSymbols` and `lineNumbers`. The parameter `srcFileName` is the source path for the `lineNumbers`.

After creating a `ELFWriter` object, the example builds the ELF in-memory file with:

```c++
writer.buildELFObject();
printf("ELF file Size=%zu\n", writer.getELFObjectSize());
auto pELFObj = writer.getELFObjectView();
```

The `buildELFObject` method of `ELFWriter` generates the ELF file in its internal memory buffer. The `getELFObjectSize` method returns the size of generated ELF file in bytes. The `getELFObjectView` method returns the pointer to the generated ELF file object. The methods `getELFObjectSize` and `getELFObjectView` should be called after `buildELFObject` method is called. 

Now users can further use the `pELFObj` in GDB debug info or dumping to file.

## Registering debug info into GDB

Once you generete the in-memory ELF file, you can register it in GDB debugger at the run time. After registering, your JIT'd code can be reconginized by GDB. The APIs to use are:

```c++
GDBJITentryobj* RobinGDBJITRegisterObject(void* elfObj, size_t objSize);
void RobinGDBJITUnregisterObject(GDBJITentryobj* eo);
```

`RobinGDBJITRegisterObject` takes 2 arguments. The first is the in-memory ELF object pointer. The second is the size of the object. If you are using Robin to generate ELF debug info, you can pass the results of `getELFObjectView()` and `getELFObjectSize()` from `ELFWriter` to this function. `RobinGDBJITRegisterObject` returns the pointer to the GDB-internal structure that idendities the ELF object. You can later pass it to `RobinGDBJITUnregisterObject` to unload the ELF object from GDB.

**Note** The `RobinGDBJIT*` functions are not thread-safe. They will modify the globally shared linked-list `__jit_debug_descriptor` as is required by the protocol of GDB. Consider to put calls to `RobinGDBJIT*` functions in a critical section to avoid multi-threading issues.

You can pass `ELFWriter` generated object into this function:

```c++
#include "robin/GdbJITSupport.h"
...
  robin::ELFWriter writer{(uintptr_t)code, sizeof(shellcode), funcs, 2, 500, "test/example_src.c", lines, 4};
  writer.buildELFObject();
  // save the "pobj" to unregister in GDB later
  auto pobj = RobinGDBJITRegisterObject(writer.getELFObjectView(), writer.getELFObjectSize());
  ...
  // unregister the ELF file in GDB
  RobinGDBJITUnregisterObject(pobj);
...
```

A important note is that, if you need to link `GdbJITSupport.cpp` with other libraries that supports GDB symbol registration (e.g. LLVM JIT engines), you need to pass `-DROBIN_DONT_DEFINE_GDB_SYMBOLS=1` when building `GdbJITSupport.cpp` with C++ compilers.

## How to build

This project requires C++11. You also need to provide the include path of `robin` with `-I` switch in C++ compiler:

For example:

```
g++ -std=c++11 -g /path/to/robin/src/robin.cpp /path/to/robin/src/GdbJITSupport.cpp test/main.cpp -I/path/to/robin/src -o ./test/rb
```

You also need to consider the `-DROBIN_DONT_DEFINE_GDB_SYMBOLS=1` option (see the above section)

## Acknowledgements

The ELF generator and GDB registering was originally derived from LuaJIT project.