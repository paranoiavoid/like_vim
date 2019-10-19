/*
Enterをインサートモードで押したとき後ろに文字がある場合文字も改行させる
DelとBackspaceのキーコードが異なる(エラーの原因になる可能性)
*/

#include <algorithm>
#include <cstring>
#include <curses.h>
#include <string>
#include <unistd.h>
#define KEY_ESC 27  // Escにキーコードがないため定義する
#define KEY_DEL 127 // Delにキーコードがないため定義する
#define KEY_ENT 10 // テンキーでないEnterにキーコードがないため定義する

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
int line_window_width = 5; //行番号を表示するスクリーンの幅
int status_window_height = 2; //ステータス,コマンドを表示するスクリーンの高さ
int line_top = 1;    //画面の一番上の行番号
int line_max = 1;    //行が存在する最大の行番号
MODE old_mode = NOR; // mode判定に入る一つ前のモード

int input_char(void); //入力された(特殊)文字のキーコードを返す
void normal_mode(int c);
void insert_mode(int c);
void command_mode(int c);
void input_check(int c);
string command_scan(void); //コマンドモードのコマンド文字列を読み取り返す
void command_check(string str); //コマンドモードのコマンドを実行する
void line_output(void);
void mode_output(void);

WINDOW *line_screen;
WINDOW *status_screen;
WINDOW *text_screen;
WINDOW *mode_screen;

int main(void) {
    initscr(); //初期化する

    getmaxyx(stdscr, window_size_y,
             window_size_x); //ウィンドウのサイズを取得する

    line_screen = subwin(stdscr, window_size_y - status_window_height,
                         line_window_width, 0, 0);
    status_screen = subwin(stdscr, status_window_height, window_size_x,
                           window_size_y - status_window_height, 0);
    mode_screen = subwin(stdscr, 1, window_size_x,
                         window_size_y - status_window_height - 1, 0);
    text_screen =
        subwin(stdscr, window_size_y - status_window_height,
               window_size_x - line_window_width, 0, line_window_width);

    erase();

    scrollok(stdscr, true); //ウィンドウのスクロールを有効にする
    noecho();               //入力された文字を画面に表示しない
    keypad(stdscr, true);   //特殊なキーコードを使うようにする

    // wborder(line_screen, 0, 0, 0, 0, 0, 0, 0, 0);
    line_output();

    move(0, line_window_width);
    mode_output();

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
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
    } else if (c == 'a') {
        mode = INS;
        wmove(text_screen, cursor_y, ++cursor_x);
        wrefresh(text_screen);
    } else if (c == 'I') {
        mode = INS;
        cursor_x = 0;
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
    } else if (c == 'h') {
        cursor_x = max(cursor_x - 1, 0);
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
    } else if (c == 'j') {
        wmove(text_screen, ++cursor_y, cursor_x);
        wrefresh(text_screen);
    } else if (c == 'k') {
        cursor_y = max(cursor_y - 1, 0);
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
    } else if (c == 'l') {
        wmove(text_screen, cursor_y, ++cursor_x);
        wrefresh(text_screen);
    } else if (c == ':') {
        mode = COM;
        waddch(status_screen, (char)c);
        wrefresh(status_screen);
    } else if (c == 'u') {
        // scrl(1);
    } else if (c == 'd') {
        // scrl(-1);
    } else if (c == 'x') {
        wdelch(text_screen);
        wrefresh(text_screen);
    } else if (c == 'X') {
        if (cursor_x > 0) {
            wmove(text_screen, cursor_y, --cursor_x);
            wdelch(text_screen);
            wrefresh(text_screen);
        }
    } else if (c == 'O') {
        mode = INS;
        winsdelln(text_screen, 1);
        wrefresh(text_screen);
        cursor_x = 0;
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
    } else if (c == 'o') {
        mode = INS;
        wmove(text_screen, ++cursor_y, cursor_x);
        winsdelln(text_screen, 1);
        wrefresh(text_screen);
        cursor_x = 0;
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
    }
}
void insert_mode(int c) {
    getyx(text_screen, cursor_y, cursor_x);

    if (c == KEY_ESC) {
        mode = NOR;
        wmove(text_screen, cursor_y, --cursor_x);
        wrefresh(text_screen);
    } else if (c == KEY_DEL) {
        if (cursor_x > 0) {
            wmove(text_screen, cursor_y, --cursor_x);
            wdelch(text_screen);
            wrefresh(text_screen);
        }
    } else if (c == KEY_BACKSPACE) {
        if (cursor_x > 0) {
            wmove(text_screen, cursor_y, --cursor_x);
            wdelch(text_screen);
            wrefresh(text_screen);
        }
    } else if (c == KEY_ENT) {
        wmove(text_screen, ++cursor_y, cursor_x);
        winsdelln(text_screen, 1);
        wrefresh(text_screen);
        cursor_x = 0;
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
    } else {
        waddch(text_screen, (char)c);
        // winsch(text_screen,(char)c);
        wrefresh(text_screen);
    }
}

void command_mode(int c) {
    if (c == KEY_ENT) {
        mode = NOR;

        string str = command_scan();
        command_check(str);

        werase(status_screen);
        wrefresh(status_screen);
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
    } else if (c == KEY_ESC) {
        mode = NOR;

        werase(status_screen);
        wrefresh(status_screen);
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
    } else {
        waddch(status_screen, (char)c);
        wrefresh(status_screen);
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
    mode_output();
}

string command_scan(void) {
    int x, y;
    int size = 1000;
    char *tmp_str = (char *)malloc(sizeof(char) * size);
    wmove(status_screen, 0, 1);
    wrefresh(status_screen);
    int p = 0;
    p = winnstr(status_screen, tmp_str, size);

    if (p == 0) {
        exit(1);
    }

    string str = tmp_str; // char*型をstring型へ変換
    str.erase(remove(str.begin(), str.end(), ' '),
              str.end()); // string型に写した文字列から空白を全て削除

    free(tmp_str);

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

void line_output(void) {
    for (int i = 1; i <= 40; i++) {
        char tmp[10];
        snprintf(tmp, 10, "%d", i);
        mvwaddstr(line_screen, i - 1, 0, tmp);
    }
}

void mode_output(void) {
    if (old_mode != mode) {
        werase(mode_screen);
        if (mode == NOR) {
            waddstr(mode_screen, "NOR");
            wrefresh(mode_screen);
        } else if (mode == INS) {
            waddstr(mode_screen, "INS");
            wrefresh(mode_screen);
        } else if (mode == VIS) {

        } else if (mode == COM) {
            waddstr(mode_screen, "COM");
            wrefresh(mode_screen);
        }
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
    }
    old_mode = mode;
}
