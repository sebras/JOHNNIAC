all: johnniac

run: johnniac
	./johnniac

johnniac: johnniac.c
	gcc -g -O0 -fno-omit-frame-pointer -o johnniac johnniac.c -lreadline

.PHONY: clean
clean:
	rm johnniac
