ssu_convert : main.o ssu_convert.o
	gcc main.o ssu_convert.o -o ssu_convert

main.o : main.c
	gcc -c main.c

ssu_convert.o : ssu_convert.c
	gcc -c ssu_convert.c

clean :
	rm *.o
	rm ssu_convert

