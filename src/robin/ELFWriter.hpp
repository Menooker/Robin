#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ELFApi.h"

struct GDBJITobj;

namespace robin
{

    struct ELFObjectBuffer
    {
    private:
        uint8_t *cur = nullptr;      // the current position for data buffer write
        uint8_t *bufStart = nullptr; // the start position in data buffer for the current section
    public:
        size_t numSymbols;
        size_t bufferSize;
        uint8_t *buffer;

        ELFObjectBuffer(size_t numSymbols, size_t bufferSize);

        ELFObjectBuffer(const ELFObjectBuffer &) = delete;
        ELFObjectBuffer(ELFObjectBuffer &&) = delete;
        ~ELFObjectBuffer();
        GDBJITobj *obj() { return (GDBJITobj *)buffer; }
        uint8_t *data();

        uint8_t *reserve(size_t sz);

        size_t getCurrentSectionDataOffset()
        {
            return (size_t)(cur - bufStart);
        }

        size_t getCurrentSectionDataAbsOffset()
        {
            return (size_t)(cur - buffer);
        }

        uint8_t *getSectionDataByAbsOffset(size_t off)
        {
            return buffer + off;
        }

        uintptr_t startSection()
        {
            bufStart = cur;
            return getCurrentSectionDataAbsOffset();
        }

        /* Add a zero-terminated string. */
        uint32_t addStr(const char *str);

        /* Append a decimal number. */
        void addDecimalNum(uint32_t n);

        /* Add a ULEB128 value. */
        void addULEB128(uint32_t v);

        /* Add a SLEB128 value. */
        void addSLEB128(int32_t v);

        template <typename T>
        void dump(T x)
        {
            reserve(sizeof(T));
            *(T *)cur = x;
            cur += sizeof(T);
        }

    private:
        void addDecimalNumImpl(uint32_t n);
    };

    struct ELFWriter
    {
        uintptr_t machineCodeAddr;
        size_t machineCodeSize;
        RobinFunctionSymbol *funcSymbols;
        size_t numFuncSymbols;
        ELFObjectBuffer buf;
        const char *srcFileName;
        RobinDebugLine *lineNumbers;
        size_t numLineNumbers;

        ELFWriter(uintptr_t machineCodeAddr,
                  size_t machineCodeSize, RobinFunctionSymbol *funcSymbols, size_t numFuncSymbols, size_t bufferSize,
                  const char *srcFileName, RobinDebugLine *lineNumbers,
                  size_t numLineNumbers);

        void dumpSectorHeader();

        void dumpSymtab();

        void writeDataLength(size_t offsetOfLength, size_t startOffset);

        /* Initialize .debug_info section. */
        void dumpDebuginfo();
        void dumpDebugAbbrev();

        void dumpLine(int op, uint32_t sz);
        void dumpDebugline();
        void startSection(int sect);
        void endSection(int sect);

        /* Build in-memory ELF object. Returns the object size*/
        size_t buildELFObject();

        // gets a view of the generated object. The lifetime is owned by the current writer object
        GDBJITobj *getELFObjectView()
        {
            return buf.obj();
        }
        size_t getELFObjectSize()
        {
            return buf.getCurrentSectionDataAbsOffset();
        }

        // copy the generated object to the buffer. The buffer must be larger than getELFObjectSize()
        void copyELFObject(void *target);
    };

}