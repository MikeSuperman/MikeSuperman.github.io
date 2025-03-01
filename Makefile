object = main.c
target = main

$(target):$(object)
	gcc -g $^ -o $@ -lpthread

*.o:*.c
	gcc $< -c

PHONY:clean

clean:
	rm $(target) 
