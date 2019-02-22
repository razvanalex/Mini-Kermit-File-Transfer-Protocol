all: build 

build: ksender kreceiver miniKermit

ksender: ksender.o link_emulator/lib.o miniKermit.o
	gcc -g ksender.o link_emulator/lib.o miniKermit.o -o ksender -lm

kreceiver: kreceiver.o link_emulator/lib.o miniKermit.o
	gcc -g kreceiver.o link_emulator/lib.o miniKermit.o -o kreceiver -lm

miniKermit: miniKermit.o 
	gcc -g -c miniKermit.c -lm

.c.o: 
	gcc -Wall -g -c $? 

clean:
	-rm -f *.o ksender kreceiver 
