CFLAGS = -Wall -Wextra -pedantic -std=c99 

Text_Editor:Text_Editor.c
	gcc Text_Editor.c -o Text_Editor

clean:
	rm -rf Text_Editor