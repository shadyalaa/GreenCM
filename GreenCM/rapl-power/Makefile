CFLAGS=-g

all: rapl_lib_static power_gadget

rapl_lib_static: 
	gcc $(CFLAGS) -lm -c msr.c cpuid.c rapl.c
	ar rcs librapl.a msr.o cpuid.o rapl.o /home/shady/x86_energy/build/libx86_energy.so

power_gadget: 
	gcc $(CFLAGS) power_gadget.c -I. -L. -o power_gadget ./librapl.a -lm

clean: 
	rm -f librapl.a msr.o cpuid.o rapl.o 
