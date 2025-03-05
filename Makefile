object = main.c
target = main
files = my.txt

$(target):$(object)
	gcc -g $^ -o $@ -lpthread

*.o:*.c
	gcc $< -c

PHONY:clean

clean:
	rm $(target) $(files)
