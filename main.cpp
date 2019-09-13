#include <curses.h>

#define KEY_ESC 27 // Escにキーコードがないため定義する

enum MODE { //現在のモードの状態
    NOR,    //ノーマルモード
    INS,    //挿入モード
    VIS,    //ビジュアルモード
    COM,    //コマンドラインモード
};

int window_size_x; //画面の横幅
int window_size_y; //画面の縦幅
//カーソルの座標はインサートモード以外では直接は動かさない
//他のモードの入力から戻ってきたときに座標を保持しておくため
int cursor_x = 0; //インサートモードにおけるカーソルのx座標
int cursor_y = 0; //インサートモードにおけるカーソルのy座標
MODE mode;        //現在のモード

int input_char(void) { //入力された(特殊)文字のキーコードを返す
    return getch();
}

void normal_mode(int c) {
    if (c == 'i') {
        mode = INS;
    } else if (c == 'I') {
        mode = INS;
        getyx(stdscr, cursor_y, cursor_x);
        move(cursor_y, ++cursor_x);
    } else if (c == ':') {
        mode = COM;
        move(window_size_y - 2, 0);
        addch((char)c);
    }
}
void insert_mode(int c) {
    if (c == KEY_ESC) {
        mode = NOR;
        getyx(stdscr, cursor_y, cursor_x);
        move(cursor_y, --cursor_x);
    } else {
        addch((char)c);
    }
}

void command_mode(int c) {
    if (c == KEY_ESC) {
        mode = NOR;
        deleteln();
        move(cursor_y, cursor_x);
    }
    /*
    else if (c == KEY_ENTER) {
        mode = NOR;
        deleteln();
        move(cursor_y, cursor_x);
    }
    */
    else {
        addch((char)c);
    }
}
void input_check(int c) {
    if (mode == NOR) {
        normal_mode(c);
    } else if (mode == INS) {
        insert_mode(c);
    } else if (mode == VIS) {

    } else if (mode == COM) {
        command_mode(c);
    }
}

int main(void) {
    initscr(); //初期化する

    scrollok(stdscr, true); //ウィンドウのスクロールを有効にする
    getmaxyx(stdscr, window_size_y,
             window_size_x); //ウィンドウのサイズを取得する

    erase();

    noecho();             //入力された文字を画面に表示しない
    keypad(stdscr, true); //特殊なキーコードを使うようにする

    while (1) {
        // move(cursor_y, cursor_x);
        input_check(input_char());
    }

    timeout(-1);
    cursor_y++;
    addstr("END");
    getch();

    endwin();
    return 0;
}
