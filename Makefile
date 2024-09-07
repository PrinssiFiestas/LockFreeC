CFLAGS = -Wall -Wextra -Werror -Wpedantic -lm -lpthread -std=c11

tests:
	$(CC) -o $@ tests.c $(CFLAGS)
	./tests

clean:
	rm -f *.o tests
