CC = cc
CFLAGS = -std=c11 -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer -g
LDFLAGS = -fsanitize=address,undefined
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)
OUT = a.out

$(OUT): $(OBJ)
	$(CC) $(LDFLAGS) -o $(OUT) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

watch: $(OUT)
	./$(OUT)
	find . -name '*.c' -o -name '*.h' | entr -cs 'make && ./$(OUT)'

clean:
	rm -f $(OUT) $(OBJ)

test: $(OUT)
	@./tests/run.sh ./$(OUT)

.PHONY: watch clean test
