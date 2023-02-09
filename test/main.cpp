#include "robin/ELFWriter.hpp"
#include "robin/GdbJITSupport.h"
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

uint8_t shellcode[] = {
    /*401210:*/ 0x55,                   //push   %rbp
    /*401211:*/ 0x48, 0x89, 0xe5,       //mov    %rsp,%rbp
    /*401214:*/ 0x48, 0x89, 0x7d, 0xf8, //mov    %rdi,-0x8(%rbp)
    /*401218:*/ 0x89, 0x75, 0xf4,       //mov    %esi,-0xc(%rbp)
    /*40121b:*/ 0x48, 0x8b, 0x45, 0xf8, //mov    -0x8(%rbp),%rax
    /*40121f:*/ 0x48, 0x8b, 0x40, 0x08, //mov    0x8(%rax),%rax
    /*401223:*/ 0x48, 0x89, 0xc2,       //mov    %rax,%rdx
    /*401226:*/ 0x48, 0x8b, 0x45, 0xf8, //mov    -0x8(%rbp),%rax
    /*40122a:*/ 0x48, 0x8b, 0x00,       //mov    (%rax),%rax
    /*40122d:*/ 0x48, 0x29, 0xc2,       //sub    %rax,%rdx
    /*401230:*/ 0x48, 0x89, 0xd0,       //mov    %rdx,%rax
    /*401233:*/ 0x48, 0x89, 0xc1,       //mov    %rax,%rcx
    /*401236:*/ 0x48, 0x8b, 0x45, 0xf8, //mov    -0x8(%rbp),%rax
    /*40123a:*/ 0x8b, 0x55, 0xf4,       //mov    -0xc(%rbp),%edx
    /*40123d:*/ 0x48, 0x63, 0xd2,       //movslq %edx,%rdx
    /*401240:*/ 0x48, 0xc1, 0xe2, 0x06, //shl    $0x6,%rdx
    /*401244:*/ 0x48, 0x01, 0xd0,       //add    %rdx,%rax
    /*401247:*/ 0x48, 0x83, 0xc0, 0x70, //add    $0x70,%rax
    /*40124b:*/ 0x48, 0x89, 0x08,       //mov    %rcx,(%rax)
    /*40124e:*/ 0x90,                   //nop
    /*40124f:*/ 0x5d,                   //pop    %rbp
    /*401250:*/ 0xc3,                   //retq
    /*401251:*/ 0x90,                   //nop

    /*401210:*/ 0x55,                   //push   %rbp
    /*401211:*/ 0x48, 0x89, 0xe5,       //mov    %rsp,%rbp
    /*401214:*/ 0x48, 0x89, 0x7d, 0xf8, //mov    %rdi,-0x8(%rbp)
    /*401218:*/ 0x89, 0x75, 0xf4,       //mov    %esi,-0xc(%rbp)
    /*40121b:*/ 0x48, 0x8b, 0x45, 0xf8, //mov    -0x8(%rbp),%rax
    /*40121f:*/ 0x48, 0x8b, 0x40, 0x08, //mov    0x8(%rax),%rax
    /*401223:*/ 0x48, 0x89, 0xc2,       //mov    %rax,%rdx
    /*401226:*/ 0x48, 0x8b, 0x45, 0xf8, //mov    -0x8(%rbp),%rax
    /*40122a:*/ 0x48, 0x8b, 0x00,       //mov    (%rax),%rax
    /*40122d:*/ 0x48, 0x29, 0xc2,       //sub    %rax,%rdx
    /*401230:*/ 0x48, 0x89, 0xd0,       //mov    %rdx,%rax
    /*401233:*/ 0x48, 0x89, 0xc1,       //mov    %rax,%rcx
    /*401236:*/ 0x48, 0x8b, 0x45, 0xf8, //mov    -0x8(%rbp),%rax
    /*40123a:*/ 0x8b, 0x55, 0xf4,       //mov    -0xc(%rbp),%edx
    /*40123d:*/ 0x48, 0x63, 0xd2,       //movslq %edx,%rdx
    /*401240:*/ 0x48, 0xc1, 0xe2, 0x06, //shl    $0x6,%rdx
    /*401244:*/ 0x48, 0x01, 0xd0,       //add    %rdx,%rax
    /*401247:*/ 0x48, 0x83, 0xc0, 0x70, //add    $0x70,%rax
    /*40124b:*/ 0x48, 0x89, 0x08,       //mov    %rcx,(%rax)
    /*40124e:*/ 0x90,                   //nop
    /*40124f:*/ 0x5d,                   //pop    %rbp
    /*401250:*/ 0xc3,                   //retq
    /*401251:*/ 0x90                    //nop
};



int main()
{

  void *code = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  memcpy(code, shellcode, sizeof(shellcode));

  void *code2 = (char *)code + sizeof(shellcode) / 2;
  RobinFunctionSymbol funcs[2] = {{(void *)code, sizeof(shellcode) / 2, "end_sect"},
                                  {code2, sizeof(shellcode) / 2, "st_sect"}};
  RobinDebugLine lines[4] = {
      {code2, 7},
      {(uint8_t *)code2 + 12, 8},
      {code, 2},
      {(uint8_t *)code + 12, 3},
  };
  robin::ELFWriter writer{(uintptr_t)code, sizeof(shellcode), funcs, 2, 500, "test/example_src.c", lines, 4};
  writer.buildELFObject();
  printf("Size=%zu\n", writer.buf.getCurrentSectionDataAbsOffset());
  // FILE *f = fopen("1.o", "w");
  // fwrite(writer.buf.buffer, writer.buf.getCurrentSectionDataAbsOffset(), 1, f);
  // fclose(f);
  RobinGDBJITRegisterObject(writer.getELFObjectView(), writer.getELFObjectSize());
  getchar();
}