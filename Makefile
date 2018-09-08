test: test/test_cpu.c
	gcc test/test_cpu.c -o test/test

cpu: cpu/cpu.c cpu/cpu.h
	gcc cpu/cpu.c -c
