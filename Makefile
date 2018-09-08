all: cpu test

test: test/test_cpu.c test/test_cpu.h cpu/cpu.h
	gcc test/test_cpu.c cpu/cpu.o -g -o test/test

cpu: cpu/cpu.c cpu/cpu.h
	gcc cpu/cpu.c -c -o cpu/cpu.o

clean:
	rm cpu/cpu.o test/test
