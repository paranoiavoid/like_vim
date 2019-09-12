#include <curses.h>

enum MODE { //現在のモードの状態
    NOR,    //ノーマルモード
    INS,    //挿入モード
    VIS,    //ビジュアルモード
    COM,    //コマンドラインモード
};

int window_size_x; //画面の横幅
int window_size_y; //画面の縦幅
int cursor_x;      //カーソルのx座標
int cursor_y;      //カーソルのy座標
MODE mode;         //現在のモード

int input_char(void) { //入力された(特殊)文字のキーコードを返す
    return getch();
}

void normal_mode(char c) {
    if (c == 'i') {
        mode = INS;
    } else if (c == 'I') {
        mode = INS;
        cursor_x++;
    }
}
void insert_mode(int c) {
    if (c == 27) { // Escキーが入力
        mode = NOR;
        cursor_x--;
    } else {
        addch((char)c);
        cursor_x++;
    }
}

void input_check(int c) {
    if (mode == NOR) {
        normal_mode(c);
    } else if (mode == INS) {
        insert_mode(c);
    } else if (mode == VIS) {

    } else if (mode == COM) {
    }
}

int main(void) {
    initscr(); //初期化する

    scrollok(stdscr, true); //ウィンドウのスクロールを有効にする
    getmaxyx(stdscr, window_size_y,
             window_size_x); //ウィンドウのサイズを取得する

    erase();

    cursor_x = 0;
    cursor_y = 0;
    noecho();             //入力された文字を画面に表示しない
    keypad(stdscr, true); //特殊なキーコードを使うようにする
    while (1) {
        move(cursor_y, cursor_x);
        input_check(input_char());
    }

    timeout(-1);
    cursor_y++;
    addstr("END");
    getch();

    endwin();
    return 0;
}
