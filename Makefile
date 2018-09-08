all: cpu opcodes test

test: test/test_cpu.c test/test_cpu.h cpu/cpu.h
	gcc test/test_cpu.c cpu/cpu.o cpu/opcodes.o -g -o test/test

cpu: cpu/cpu.c cpu/cpu.h
	gcc cpu/cpu.c -c -o cpu/cpu.o

opcodes: cpu/opcodes.c cpu/opcodes.h
	gcc cpu/opcodes.c -c -o cpu/opcodes.o

clean:
	rm cpu/cpu.o cpu/opcodes.o test/test
