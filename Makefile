CFLAGS = -Wall -Wextra -Werror -Wpedantic -lm -lpthread -std=c11

gpc.o: gpc.c gpc.h
	$(CC) -o $@ -c $< -O3 -lm -lpthread -flto -D_GNU_SOURCE

tests: tests.c lfc.h gpc.o
	$(CC) -o $@ $< gpc.o -ggdb3 -gdwarf $(CFLAGS)
	./$@

release_tests: tests.c lfc.h gpc.o
	$(CC) -o $@ $< gpc.o -O3 -DNDEBUG -D_GNU_SOURCE -flto $(CFLAGS)
	./$@

cpp_tests: tests.c lfc.h gpc.o
	$(CXX) -o $@ $< gpc.o -Wall -Wextra -Werror -Wpedantic -O3 -DNDEBUG -std=c++11
	./$@

clean:
	rm -f *.o tests release_tests cpp_tests
