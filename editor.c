/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <sys/ioctl.h>
#include <sys/types.h> 
#include <termios.h>
#include <unistd.h>


/*** defines ***/
#define KILO_VERSION "0.01" 
#define CTRL_KEY(k) ((k) &0x1f) 

enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT, 
    ARROW_UP, 
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY, 
    END_KEY,
    PAGE_UP, 
    PAGE_DOWN
}; 

/*** data ***/

// struct to represent an "editor row" 
typedef struct erow {
    int size; 
    char *chars; 
} erow; 


struct editorConfig {
    int cx, cy; //cursor's x and y positions. 
    int screenrows; 
    int screencols; 
    int numrows;
    erow row; 
    struct termios orig_termios; 
}; 

//intializing an editorcongif instance. 
struct editorConfig E; 

/*** terminal ***/ 

//function that will kill the program. 
void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4); 
    write(STDOUT_FILENO, "\x1b[H", 3); 


    //outputs the error message with the string, and exits with code 1. 
    perror(s); 
    exit(1);
}

//disableRawMode will reset the terminal settings when called. 
void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) {
        die("tcsetattr"); //kill the program if terminal settings are unavailable. 
    }
}

// enableRawMode() enables raw mode (reading a single character at a time)
void enableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) die("tcsetattr"); 

    //storing the original terminal settings in orig_termios
    tcgetattr(STDIN_FILENO, &E.orig_termios); 
    //when the program exits, disableRawMode will be called. 
    atexit(disableRawMode); 

    //defining a termios struct called raw: 
    struct termios raw = E.orig_termios; 

    //turning off all input and output processing.
    //    e.g Ctrl A maps to 1. 
    //    allows us to turn on Raw mode. 
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);  
    raw.c_oflag &= ~(OPOST); 
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); 
    raw.c_cc[VMIN] = 0; //minimum amount of bytes to be read before returning. 
    raw.c_cc[VTIME] = 1; //minimum amount of time (tenths of a second) between inputs before returning. 

    //setting the configured attributes (in raw) 
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr"); 
}

// editorReadKey() reads a single key from input, and processes it. 
int editorReadKey() {
    int nread; 
    char c; 


    while((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        //kill the program if the read is unsuccesful. 
        if (nread == -1 && errno != EAGAIN) die("read"); 
    }

    //adding recognition for the arrow keys being pressed: 
    if(c == '\x1b') {
        char seq[3]; 

        if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b'; 
        if(read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b'; 

        //if there is any escape characters, return them as their WASD equivalent. 
        if(seq[0] == '[') {
            if(seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if(seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY; 
                        case '3' : return DEL_KEY; 
                        case '4': return END_KEY; 
                        case '5': return PAGE_UP; 
                        case '6' : return PAGE_DOWN; 
                        case '7' : return HOME_KEY; 
                        case '8' : return END_KEY; 
                    }
                }
            } else { 
                switch (seq[1]) {
                    case 'A' : return ARROW_UP; 
                    case 'B' : return ARROW_DOWN; 
                    case 'C' : return ARROW_RIGHT; 
                    case 'D' : return ARROW_LEFT; 
                    case 'E' : return HOME_KEY;
                    case 'F' : return END_KEY; 
                }
            }
        } else if(seq[0] == 'O') {
            switch (seq[1]) {
                case 'H' : return HOME_KEY; 
                case 'F' : return END_KEY; 
            }
        }
        return '\x1b'; 
    } else {
        return c; 
    }
}

//getCursorPosition will get the position of the cursor: 
int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0; 

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1; 

    while(i < sizeof(buf)) {
        if(read(STDIN_FILENO, &buf[i], 1) != 1) break; 
        if(buf[i] == 'R') break; 
        i ++; 
    }

    buf[i] = '\0'; 

    if(buf[0] != '\x1b' || buf[1] != '[') return -1; 
    if (sscanf(&buf[2], "%d:%d", rows, cols) != 2) return -1; 

    return 0; 

}

/*** file i/o ***/
void editorOpen() {
    //dummy code to open the editor. 
    char *line = "Hello World"; 
    ssize_t linelen = 13; 

    E.row.size = linelen; 
    E.row.chars = malloc(linelen + 1); 
    memcpy(E.row.chars, line, linelen); 
    E.row.chars[linelen] = '\0'; 
    E.numrows = 1; 
}



//getWindowSize gets the window size and updates the row and cols 
//     variables accordingly 
int getWindowSize(int *rows, int *cols) {
    struct winsize ws; 

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1; 
        return getCursorPosition(rows, cols); 
    } else {
        //setting the window sizes: 
        *cols = ws.ws_col; 
        *rows = ws.ws_row; 
        return 0; 
    }
}

/*** append buffer ***/

struct abuf {
    char *b; //string representing the buffer. 
    int len; 
}; 

#define ABUF_INIT {NULL, 0}; 

//abAppend(ab, s, len) will add len to the buffer. 
void abAppend(struct abuf *ab, const char *s, int len) {
    //using realloc we allocate memory to hold the new string. 
    char *new = realloc(ab->b, ab->len + len); 

    //if memory allocation fails, then return. 
    if(new == NULL) return; 

    //using memcpy to copy the new string to the end of the buffer. 
    memcpy(&new[ab->len], s, len); 

    //updating the struct variables as necessary. 
    ab->b = new; 
    ab->len += len; 
}

//frees the buffer. 
void abFree(struct abuf *ab) {
    free(ab->b); 
}

/*** output ***/

void editorDrawRows(struct abuf *ab) {
    int y; 

    //drawing tildes representing each new row. 
    for(y = 0; y < E.screenrows; y ++) {
        //drawing the intro message a third of the way down the screen
        if(y == E.screenrows / 3) {
            char welcome[80];  
            int welcomelen = snprintf(welcome, sizeof(welcome), "Satya's editor -- version %s", KILO_VERSION);
            
            //if the welcome message is too long - shorten it. 
            if (welcomelen > E.screencols) welcomelen = E.screencols; 

            //padding on the sides (equal on both sides.)
            int padding = (E.screencols - welcomelen) / 2; 

            //adding a row of tildes for the padding row. 
            if(padding) {
                abAppend(ab, "~", 1); 
                padding --; 
            }
            
            //adding the padding and the message. 
            while(padding --) abAppend(ab, " ", 1); 
            abAppend(ab, welcome, welcomelen); 
        } else {
            abAppend(ab, "~", 1); 
        }


        abAppend(ab, "\x1b[K", 3); 

        if(y < E.screenrows - 1) {
            abAppend(ab, "\r\n", 2);  
        } 
    }
}

void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT; 

    //the following escape sequences reset the cursor and the lines. 
    abAppend(&ab, "\x1b[?25l", 6); 
    abAppend(&ab, "\x1b[H", 3); 

    editorDrawRows(&ab);  

    //moving the cursor to the position stored 
    char buf[32]; 

    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));
    
    abAppend(&ab, "\x1b[?25h", 6); 

    write(STDOUT_FILENO, ab.b, ab.len); 

    //once we write the buffers contents to stdout, we can free it. 
    abFree(&ab); 
}

/*** input ***/

void editorMoveCursor(int key) {
    switch (key) {
        case ARROW_LEFT:
            if(E.cx != 0) {
                E.cx --;  
            }
            break; 
        case ARROW_RIGHT :
            if(E.cx != E.screencols) {
                E.cx ++; 
            }
            break;
        case ARROW_UP:
            if(E.cy != 0) {
                E.cy --; 
            } 
            break;
        case ARROW_DOWN:
            if(E.cy != E.screenrows) {
                E.cy ++; 
            }
            break; 
    }
}

// editorProcessKeypress uses editorReadKey and processes key presses 
// (e.g breaking when CTRL+q is pressed) 
void editorProcessKeypress() {
    int c = editorReadKey(); 

    switch(c) {
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
                while(times --)
                    editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN); 
            }
            break; 

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT: 
            //move the cursor based on the input. 
            editorMoveCursor(c); 
            break;  
    }
}


/*** init ***/

void initEditor() {
    E.cx = 0; 
    E.cy = 0; 
    E.numrows = 0; 
    //updating the window size variables.
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize"); 
}

int main() {
    enableRawMode(); 
    initEditor(); 
    editorOpen(); 

    while(1) {
        editorRefreshScreen(); 
        editorProcessKeypress(); 
    }

    return 0;
}

