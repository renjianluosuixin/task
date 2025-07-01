/*** includes ***/
#include <termios.h>  
#include <unistd.h>
#include <stdlib.h> 
#include <ctype.h> 
#include <stdio.h>  
#include <errno.h>
 
/*** 数据区 ***/
 
struct termios orig_termios;
 
/*** 终端区***/
 
void die(const char *s) {
    perror(s);
    /*
    函数原型:
        void perror(const char *s);
    参数解释:
        参数是一个指向常量字符的指针，通常用于提供自定义的错误描述信息。
    函数功能:
        将上一个系统调用发生的错误输出到标准错误流（stderr）。
    */
    exit(1);
    /*
    函数原型:
        void exit(int status);
    参数解释:
        整数参数status表示程序的终止状态码。一般约定，返回值为 0 表示程序正常终止。
        非零值表示程序异常终止，其中具体的非零值可以用于表示不同的错误或状态。
    函数功能:
        终止程序的执行，并返回一个整数值作为状态码。
    */
}
 
void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}
 
void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /*
    当设置了BRKINT标志时，终端会将BREAK条件（通常由发送设备产生）视为中断。
    清除这个标志意味着BREAK条件不会中断程序的执行。
    当设置了INPCK标志时，终端会启用奇偶校验。
    清除这个标志后，奇偶校验将被禁用。
    当设置了ISTRIP标志时，终端会将输入字符的第 8 位清除，保留低 7 位。
    清除这个标志后，输入字符的所有 8 位都会被保留。
    */
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    /*
    设置c_cflag 字段中的字符大小标志 CS8，它指定了字符大小为 8 位。
    CS8 表示字符有 8 位，这是标准的字符大小设置.
    */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    /*
    c_cc 数组是一个特殊的数组，用于存储终端特殊字符的控制信息。
    VMIN 是其中的一个索引常量，代表了在非规范模式下，read 函数读取字符的最小数量限制。
    将其设置为 0 表示在读取时不需要等待任何字符，即立即返回。
    VTIME 是 c_cc 数组中的另一个索引常量，代表在非规范模式下，read 函数读取的字符之间的超时时间。
    将其设置为 1 表示当没有可用字符时，read 函数会等待 100 毫秒（1/10 秒）来接收字符，超过这个时间将返回 EOF（文件结束符,0）。
    总的作用是将串行通信设置为非阻塞模式。
    通过将 VMIN 设置为 0，read 函数立即返回，无需等待字符输入。
    而将 VTIME 设置为 1，read 函数在没有可用字符时会等待 100 毫秒，超过这个时间将返回 EOF。
    */
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}
 
/*** 初始区 ***/
 
int main() {
    enableRawMode();
    while (1) {
        char c = '\0';
        if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
        /*
        errno 是一个全局变量，用于保存最近发生的错误代码。
        EAGAIN 是一个错误代码，表示资源暂时不可用（例如，在非阻塞模式下没有可用的输入）。
        这个条件确保发生的错误不是由于资源暂时不可用而导致的,即本文不认为这是个错误。
        */
        if (iscntrl(c)) {
            printf("%d\r\n", c);
        } else {
            printf("%d ('%c')\r\n", c, c);
        }
    if (c == 'q') break;
  }
  return 0;
}