CFLAGS = -Wall -g -O1

CC = gcc

mush2: mush2.o
	$(CC) $(CFLAGS) -L ~pn-cs357/Given/Mush/lib64 -o mush2 mush2.o -lmush

mush2.o: mush2.c
	$(CC) $(CFLAGS) -I ~pn-cs357/Given/Mush/include -c mush2.c

clean: 
	rm *.o mush2
