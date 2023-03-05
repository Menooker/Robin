import sys

expected = ["Dump of assembler code for function st_sect", "7",
            "int a=c+1;", "push   %rbp", "8", "int b=a+2;", "-0x8(%rbp),%eax", "mov    %rcx,(%rax)",
            "Dump of assembler code for function end_sect", "2", "int a=c+1;", "push   %rbp", "3", "int b=a+2;", "mov    -0x8(%rbp),%eax",
            "Breakpoint 2, ", "No symbol \"st_sect\" ", "No symbol \"end_sect\" "]

idx = 0

def find_next():
    global idx
    if expected[idx] in line:
        idx+=1
        if idx == len(expected):
            exit(0)
        find_next()

for line in sys.stdin:
    sys.stdout.write(line)
    find_next()
if idx == len(expected):
    exit(0)
print("Expected:", expected[idx], "   idx=", idx)
exit(1)
