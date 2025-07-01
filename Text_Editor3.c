/*** includes ***/
#include <termios.h>  
#include <unistd.h>
#include <stdlib.h> 
#include <ctype.h> 
#include <stdio.h>  
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
 
/*** defines ***/
#define KILO_VERSION "0.0.1"
 
#define CTRL_KEY(k) ((k) & 0x1f)
 
enum editorKey {
	ARROW_LEFT = 1000,
	ARROW_RIGHT ,
	ARROW_UP ,
	ARROW_DOWN,
	DEL_KEY,  //映射Delete键
	HOME_KEY,  //映射Home键
  	END_KEY,  //映射End键
	PAGE_UP,  //映射Page Up键
	PAGE_DOWN  //映射Page Down键
};
 
/*** 数据区 ***/
 
struct editorConfig {
	int cx, cy;
	int screenrows;  //窗口行数
	int screencols;  //窗口列数
	struct termios orig_termios;
};
struct editorConfig E;
 
/*** 终端区***/
 
void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);  //清屏
    write(STDOUT_FILENO, "\x1b[H", 3);   //重定位光标到左上角
 
    perror(s);
 
    exit(1);
 
}
 
void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}
 
void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);
    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
 
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
 
    raw.c_lflag &= ~(ECHO | IEXTEN | ISIG|ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
 
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}
 
int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
	if (c == '\x1b') {  //转义序列开始
 
		char seq[3];
		if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
		if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
 
		if (seq[0] == '[') {
			if (seq[1] >= '0' && seq[1] <= '9') {
				if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
				if (seq[2] == '~') {
					switch (seq[1]) {
						case '1': return HOME_KEY;//转义序列有多种<esc>[1~, <esc>[7~, <esc>[H或者 <esc>OH
						case '3': return DEL_KEY;//转义序列是<esc>[3~
						case '4': return END_KEY;//转义序列有多种<esc>[4~, <esc>[8~, <esc>[F,或<esc>OF
						case '5': return PAGE_UP;//转义序列是<esc>[5~
						case '6': return PAGE_DOWN;//转义序列是<esc>[6~
						case '7': return HOME_KEY;
						case '8': return END_KEY;
					}
				}
			} else {
				switch (seq[1]) {
					case 'A': return ARROW_UP;
					case 'B': return ARROW_DOWN;
					case 'C': return ARROW_RIGHT;
					case 'D': return ARROW_LEFT;
					case 'H': return HOME_KEY;
					case 'F': return END_KEY;
				}
			}
		} else if (seq[0] == 'O') {
			switch (seq[1]) {
				case 'H': return HOME_KEY;//转义序列有多种<esc>[1~, <esc>[7~, <esc>[H或者 <esc>OH
				case 'F': return END_KEY;
			}
		}
 
		return '\x1b';
	} else {
		return c;  //非转义序列，返回字符的Ascii码值
	}
}
 
int getCursorPosition(int *rows, int *cols) {
	char buf[32];  //缓存输出
	unsigned int i = 0;
	if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1; 
	while (i < sizeof(buf) - 1) {  //解析转义序列27[rows;colsR
		if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
		if (buf[i] == 'R') break;
		i++;
	}
	buf[i] = '\0';  
	if (buf[0] != '\x1b' || buf[1] != '[') return -1;
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
	return 0;
}
 
int getWindowSize(int *rows, int *cols) {  //获取窗口的大小(行和列)
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;  
		return getCursorPosition(rows, cols);  //成功求出窗口大小
	} else {  //库函数有效
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}
 
/*** 可变长缓冲区 ***/
struct abuf {
	char *b;  //缓冲区首地址
	int len;  //缓冲区长度
};
 
#define ABUF_INIT {NULL, 0}
 
void abAppend(struct abuf *ab, const char *s, int len) {
	char *new = realloc(ab->b, ab->len + len);
	if (new == NULL) return;  //分配失败
	memcpy(&new[ab->len], s, len);  //在原字符串ab后拼接字符串s
	ab->b = new;
	ab->len += len;
}
 
void abFree(struct abuf *ab) {
	free(ab->b);
}
 
/*** output ***/
void editorDrawRows(struct abuf *ab ,int argc  ,char *Filename) {
	int y;
    if(argc == 1){
  	    for (y = 0; y < E.screenrows; y++) {
    	    if (y == E.screenrows / 3) {
      		    char welcome[80];  //保存欢迎信息
      		    int welcomelen = snprintf(welcome, sizeof(welcome),"Hello World!");
      		    if (welcomelen > E.screencols) welcomelen = E.screencols;  //不能超过屏幕列数
      		    int padding = (E.screencols - welcomelen) / 2;  //留白左右各一半
      		    if (padding) {
        		abAppend(ab, "~", 1);
        		padding--;
      		    }
      		    while (padding--) abAppend(ab, " ", 1);
      		    abAppend(ab, welcome, welcomelen);
		    } else {
      		abAppend(ab, "~", 1);
    	    }
    	    abAppend(ab, "\x1b[K", 3);
 
    	    if (y < E.screenrows - 1) {
      		    abAppend(ab, "\r\n", 2);
    	    }
        }    
  	}
      else if (argc == 2) {
        FILE *file = fopen(Filename, "r");
        if (!file) die("fopen");
        
        char *erow[100] = {0};  // 初始化为NULL
        char buffer[256];
        int line_count = 0;
        
        while (line_count < 100 && fgets(buffer, sizeof(buffer), file)) {
            buffer[strcspn(buffer, "\n")] = 0;  // 移除换行符
            
            erow[line_count] = malloc(strlen(buffer) + 1);
            if (!erow[line_count]) die("malloc");
            strcpy(erow[line_count], buffer);
            
            line_count++;
        }
        fclose(file);
        
        // 显示文件内容
        for (int y = 0; y < E.screenrows || y < line_count; y++) {
            if (erow[y]) {
                abAppend(ab, erow[y], strlen(erow[y]));
            }
            abAppend(ab, "\x1b[K", 3);
            if (y < E.screenrows - 3) abAppend(ab, "\r\n", 2);
            if(y == E.screenrows - 2)
            {
                char filetitle[30];  //保存file信息
      		    int filetitlelen = snprintf(filetitle, sizeof(filetitle),"filetitle:%s",Filename);
                abAppend(ab, filetitle, filetitlelen);
                abAppend(ab,"    ",4);
                char row[30];  //保存file信息
      		    int rowlen = snprintf(row, sizeof(row),"row:%d",E.cy);
                abAppend(ab, row, rowlen);
                abAppend(ab,"    ",4);
                char col[30]; 
                int collen = snprintf(col, sizeof(col),"col:%d",E.cx);
                abAppend(ab, col, collen);
                abAppend(ab, "\x1b[K", 3);
                abAppend(ab, "\r\n", 2);
            }
        }
        
        // 释放内存
        for (int i = 0; i < line_count; i++) {
            free(erow[i]);
        }
    }
}
 
void editorRefreshScreen(int argc,char *Filename) {
	struct abuf ab = ABUF_INIT;
 
	abAppend(&ab, "\x1b[?25l", 6);
	abAppend(&ab, "\x1b[H", 3);
 
	editorDrawRows(&ab,argc,Filename);
 
	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
	abAppend(&ab, buf, strlen(buf));
 
	abAppend(&ab, "\x1b[?25h", 6);
 
	write(STDOUT_FILENO, ab.b, ab.len);
	abFree(&ab);
}
 
/*** input ***/
void editorMoveCursor(int key) {
	switch (key) {
		case ARROW_LEFT:
			if (E.cx != 0) {  
				E.cx--;
			}
            else if(  E.cx == 0 && E.cy != 0){
            E.cx = E.screencols - 1;
            E.cy--;
            }
			break;
		case ARROW_RIGHT:
			if (E.cx != E.screencols - 1) {  
				E.cx++;
			}
            else if(E.cx == E.screencols - 1 && E.cy != E.screenrows - 1){
                E.cx = 0;
                E.cy++;
            }
			break;
		case ARROW_UP:
			if (E.cy != 0) {  
				E.cy--;
			}
			break;
		case ARROW_DOWN:
			if (E.cy != E.screenrows - 1) {  
				E.cy++;
			}
			break;
	}
}
 
void editorProcessKeypress() {
    int c = editorReadKey();
    switch (c) {
        case CTRL_KEY('q'):
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;
		
		case HOME_KEY:
			E.cx = 0;
			break;
		case END_KEY:
			E.cx = E.screencols - 1;
			break;
 
		case PAGE_UP:
		case PAGE_DOWN:
		{
			int times = E.screenrows;
			while (times--)
			editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
		}
		break;
 
		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			editorMoveCursor(c);
			break;
    }
}
 
/*** 初始区 ***/
 
void initEditor() {
	E.cx = 0;
	E.cy = 0;
	if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main(int argc, char *argv[]) {
    enableRawMode();
    initEditor();
    while (1) {
        editorRefreshScreen(argc, argv[1]);
        editorProcessKeypress();
    }
    return 0;
}