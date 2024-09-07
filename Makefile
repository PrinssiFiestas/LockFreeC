CFLAGS = -Wall -Wextra -Werror -Wpedantic -std=c11 -lm -lpthread

tests:
	$(CC) -o $@ tests.c $(CFLAGS)
	./tests

clean:
	rm -f *.o tests
