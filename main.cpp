#include <algorithm>
#include <cstring>
#include <curses.h>
#include <string>
#include <unistd.h>
#define KEY_ESC 27 // Escにキーコードがないため定義する

using std::max;
using std::min;
using std::string;

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

int input_char(void); //入力された(特殊)文字のキーコードを返す
void normal_mode(int c);
void insert_mode(int c);
void command_mode(int c);
void input_check(int c);
string command_scan(void); //コマンドモードのコマンド文字列を読み取り返す
void command_check(string str); //コマンドモードのコマンドを実行する

int main(void) {
    initscr(); //初期化する

    scrollok(stdscr, true); //ウィンドウのスクロールを有効にする
    getmaxyx(stdscr, window_size_y,
             window_size_x); //ウィンドウのサイズを取得する

    erase();

    noecho();             //入力された文字を画面に表示しない
    keypad(stdscr, true); //特殊なキーコードを使うようにする

    while (1) {
        input_check(input_char());
    }

    timeout(-1);
    endwin();
    return 0;
}

int input_char(void) { //入力された(特殊)文字のキーコードを返す
    return getch();
}

void normal_mode(int c) {
    if (c == 'i') {
        mode = INS;
    } else if (c == 'a') {
        mode = INS;
        getyx(stdscr, cursor_y, cursor_x);
        move(cursor_y, ++cursor_x);
    } else if (c == 'I') {
        mode = INS;
        getyx(stdscr, cursor_y, cursor_x);
        cursor_x = 0;
        move(cursor_y, cursor_x);
    } else if (c == 'h') {
        getyx(stdscr, cursor_y, cursor_x);
        cursor_x = max(cursor_x - 1, 0);
        move(cursor_y, cursor_x);
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
    if (c == '\n') {
        mode = NOR;

        string str = command_scan();
        command_check(str);

        deleteln();
        move(cursor_y, cursor_x);
    } else if (c == KEY_ESC) {
        mode = NOR;
        deleteln();
        move(cursor_y, cursor_x);
    } else {
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

string command_scan(void) {
    int x, y;
    int size = 1000;
    char *tmp_str = (char *)malloc(sizeof(char) * size);
    getyx(stdscr, y, x);
    move(y, 1);
    int p = 0;
    p = innstr(tmp_str, size);

    if (p == 0) {
        exit(1);
    }

    string str = tmp_str; // char*型をstring型へ変換
    str.erase(remove(str.begin(), str.end(), ' '),
              str.end()); // string型に写した文字列から空白を全て削除

    /*
    move(2, 0);
    addstr(tmp_str);
    move(3, 0);
    addstr(str.c_str());
    */

    free(tmp_str);

    move(y, x);

    return str;
}

void command_check(string str) {
    if (str == "q") {
        endwin();
        exit(0);
    } else if (str == "wq") {
        endwin();
        exit(0);
    } else if (str == "q!") {
        endwin();
        exit(0);
    }
}
