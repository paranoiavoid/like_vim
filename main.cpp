/*
line_outputのlimの範囲がずれている可能性がある
右端まで文字が表示されるとそれ以降入力しても文字が消えるのを解消する
DelとBackspaceのキーコードが異なる(エラーの原因になる可能性)
*/

#include <algorithm>
#include <cstring>
#include <curses.h>
#include <string>
#include <unistd.h>
#include <vector>
#define KEY_ESC 27  // Escにキーコードがないため定義する
#define KEY_DEL 127 // Delにキーコードがないため定義する
#define KEY_ENT 10 // テンキーでないEnterにキーコードがないため定義する
#define MAX_LINE 100000 //テキストの最大行数

using std::max;
using std::min;
using std::string;
using std::vector;

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
int line_top = 1;              //画面の一番上の行番号
int line_max = 1;              //行が存在する最大の行番号
vector<string> text(MAX_LINE); //テキストを保存しておく二次元文字配列
vector<int> text_size(MAX_LINE, 0); //各行のテキストの文字数を管理
string nor_com; //ノーマルモードのコマンドを格納
//ノーマルモードのコマンドをリスト化
vector<string> nor_com_list = {"i", "a", "I", "A", "h", "j", "k",  "l", ":",
                               "u", "d", "x", "X", "O", "o", "bb", "$", "0"};

int input_char(void); //入力された(特殊)文字のキーコードを返す
void normal_mode(int c);
void insert_mode(int c);
void command_mode(int c);
void input_check(int c);
string command_scan(void); //コマンドモードのコマンド文字列を読み取り返す
void command_check(string str); //コマンドモードのコマンドを実行する
void line_output(void);
void mode_output(void);
string text_scan(void);
bool normal_command_check(
    void); //ノーマルモードのコマンドが現時点で存在する可能性があるか判定
int now_line(void); //今何行目のテキストにカーソルが乗っているか判定

WINDOW *line_screen;
WINDOW *status_screen;
WINDOW *text_screen;
WINDOW *mode_screen;

int main(void) {
    initscr(); //初期化する

    getmaxyx(stdscr, window_size_y,
             window_size_x); //ウィンドウのサイズを取得する

    line_screen = subwin(stdscr, window_size_y - status_window_height - 1,
                         line_window_width, 0, 0);
    status_screen = subwin(stdscr, status_window_height, window_size_x,
                           window_size_y - status_window_height, 0);
    mode_screen = subwin(stdscr, 1, window_size_x,
                         window_size_y - status_window_height - 1, 0);
    text_screen =
        subwin(stdscr, window_size_y - status_window_height - 1,
               window_size_x - line_window_width, 0, line_window_width);

    erase();

    scrollok(stdscr, true); //ウィンドウのスクロールを有効にする
    noecho();               //入力された文字を画面に表示しない
    keypad(stdscr, true);   //特殊なキーコードを使うようにする
    scrollok(text_screen, true);
    scrollok(line_screen, true);

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

    nor_com.push_back((char)c);
    if (normal_command_check() == false) {
        nor_com = "";
        if (c == ':') {
            mode = COM;
            werase(status_screen);
            waddch(status_screen, (char)c);
            wrefresh(status_screen);
        }
        return;
    }

    if (nor_com == "i") {
        mode = INS;
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
        nor_com = "";
    } else if (nor_com == "a") {
        mode = INS;
        if (text_size[now_line()] == 0) {
            wmove(text_screen, cursor_y, cursor_x);
        } else {
            wmove(text_screen, cursor_y, ++cursor_x);
        }
        wrefresh(text_screen);
        nor_com = "";
    } else if (nor_com == "I") {
        mode = INS;
        cursor_x = 0;
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
        nor_com = "";
    } else if (nor_com == "A") {
        mode = INS;
        cursor_x = text_size[now_line()];
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
        nor_com = "";
    } else if (nor_com == "h") {
        cursor_x = max(cursor_x - 1, 0);
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
        nor_com = "";
    } else if (nor_com == "j") {
        if (cursor_y < line_max - line_top) {
            if (text_size[now_line() + 1] <= cursor_x) {
                cursor_x = max(text_size[now_line() + 1] - 1, 0);
            }
            wmove(text_screen, ++cursor_y, cursor_x);
            wrefresh(text_screen);
        }
        nor_com = "";
    } else if (nor_com == "k") {
        if (now_line() > 1) {
            if (text_size[now_line() - 1] <= cursor_x) {
                cursor_x = max(text_size[now_line() - 1] - 1, 0);
            }
        }
        cursor_y = max(cursor_y - 1, 0);
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
        nor_com = "";
    } else if (nor_com == "l") {
        if (text_size[now_line()] >= cursor_x + 2) {
            wmove(text_screen, cursor_y, ++cursor_x);
            wrefresh(text_screen);
        }
        nor_com = "";
    } else if (nor_com == ":") {
        mode = COM;
        waddch(status_screen, (char)c);
        wrefresh(status_screen);
        nor_com = "";
    } else if (nor_com == "u") {
        if (line_top < line_max) {
            wmove(text_screen, 0, 0);
            text[line_top++] = text_scan();
            wscrl(text_screen, 1);
            wmove(text_screen, --cursor_y, cursor_x);
            wrefresh(text_screen);
            wscrl(line_screen, 1);
            wrefresh(line_screen);
        }
        nor_com = "";
    } else if (nor_com == "d") {
        if (line_top > 1) {
            wscrl(text_screen, -1);
            wmove(text_screen, 0, 0);
            winsstr(text_screen, text[--line_top].c_str());
            wmove(text_screen, ++cursor_y, cursor_x);
            wrefresh(text_screen);
        }
        nor_com = "";
    } else if (nor_com == "x") {
        if (text_size[now_line()] >= 1) {
            wdelch(text_screen);
            text_size[now_line()]--;
            cursor_x = min(cursor_x, text_size[now_line()] - 1);
            wmove(text_screen, cursor_y, cursor_x);
            wrefresh(text_screen);
        }
        nor_com = "";
    } else if (nor_com == "X") {
        if (cursor_x > 0) {
            wmove(text_screen, cursor_y, --cursor_x);
            wdelch(text_screen);
            text_size[now_line()]--;
            wrefresh(text_screen);
        }
        nor_com = "";
    } else if (nor_com == "O") {
        mode = INS;

        for (int i = MAX_LINE - 2; i >= now_line(); i--) {
            text_size[i + 1] = text_size[i];
        }
        text_size[now_line()] = 0;

        winsdelln(text_screen, 1);
        line_max++;
        wrefresh(text_screen);
        cursor_x = 0;
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
        nor_com = "";
    } else if (nor_com == "o") {
        mode = INS;

        for (int i = MAX_LINE - 2; i >= now_line() + 1; i--) {
            text_size[i + 1] = text_size[i];
        }
        text_size[now_line() + 1] = 0;

        wmove(text_screen, ++cursor_y, cursor_x);
        winsdelln(text_screen, 1);
        line_max++;
        wrefresh(text_screen);
        cursor_x = 0;
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
        nor_com = "";
    } else if (nor_com == "bb") {
        if (line_max > 1 && line_max > line_top) {
            wdeleteln(text_screen);
            for (int i = now_line(); i <= MAX_LINE - 2; i++) {
                text_size[i] = text_size[i + 1];
            }
            line_max--;
            wmove(text_screen, min(cursor_y, line_max - line_top), 0);
            wrefresh(text_screen);
        } else if (line_max == 1) {
            text_size[line_max] = 0;
            wdeleteln(text_screen);
            wmove(text_screen, min(cursor_y, line_max - line_top), 0);
            wrefresh(text_screen);
        }
        nor_com = "";
    } else if (nor_com == "$") {
        cursor_x = text_size[now_line()] - 1;
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
        nor_com = "";
    } else if (nor_com == "0") {
        cursor_x = 0;
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
        nor_com = "";
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
            text_size[now_line()]--;
        }
    } else if (c == KEY_BACKSPACE) {
        if (cursor_x > 0) {
            wmove(text_screen, cursor_y, --cursor_x);
            wdelch(text_screen);
            wrefresh(text_screen);
            text_size[now_line()]--;
        }
    } else if (c == KEY_ENT) {
        int tmp; //改行させる文字数
        tmp = text_size[now_line()] - cursor_x;
        for (int i = MAX_LINE - 2; i >= now_line() + 1; i--) {
            text_size[i + 1] = text_size[i];
        }
        text_size[now_line() + 1] = tmp;
        text_size[now_line()] -= tmp;

        int size = tmp + 100;
        char *str = (char *)malloc(sizeof(char) * size);
        winnstr(text_screen, str, tmp);
        for (int i = 1; i <= tmp; i++) {
            waddch(text_screen, ' ');
            wrefresh(text_screen);
        }

        wmove(text_screen, ++cursor_y, 0);
        winsdelln(text_screen, 1);
        line_max++;
        wrefresh(text_screen);
        waddstr(text_screen, str);
        wrefresh(text_screen);

        free(str);

        /*
        wmove(text_screen, ++cursor_y, cursor_x);
        winsdelln(text_screen, 1);
        line_max++;
        wrefresh(text_screen);
        cursor_x = 0;
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
        */
    } else {
        winsch(text_screen, (char)c);
        wmove(text_screen, cursor_y, ++cursor_x);
        wrefresh(text_screen);
        text_size[now_line()]++;
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
    } else if (c == KEY_DEL) {
        int y, x;
        getyx(status_screen, y, x);
        if (x > 1) {
            wmove(status_screen, y, --x);
            wdelch(status_screen);
            wrefresh(status_screen);
        } else if (x == 1) {
            mode = NOR;

            werase(status_screen);
            wrefresh(status_screen);
            wmove(text_screen, cursor_y, cursor_x);
            wrefresh(text_screen);
        }
    } else if (c == KEY_BACKSPACE) {
        int y, x;
        getyx(status_screen, y, x);
        if (x > 1) {
            wmove(status_screen, y, --x);
            wdelch(status_screen);
            wrefresh(status_screen);
        } else if (x == 1) {
            mode = NOR;

            werase(status_screen);
            wrefresh(status_screen);
            wmove(text_screen, cursor_y, cursor_x);
            wrefresh(text_screen);
        }
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
    line_output();
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
    } else if (p == 4) {
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
    werase(line_screen);
    wmove(line_screen, 0, 0);
    wrefresh(line_screen);
    int lim =
        min(line_max, line_top + window_size_y - status_window_height - 1);
    for (int i = line_top; i <= lim; i++) {
        char tmp[10];
        snprintf(tmp, 10, "%d", i);
        mvwaddstr(line_screen, i - line_top, 0, tmp);
    }
    wrefresh(line_screen);
    getyx(text_screen, cursor_y, cursor_x);
    wmove(text_screen, cursor_y, cursor_x);
    wrefresh(text_screen);
}

void mode_output(void) {
    werase(mode_screen);
    if (mode == NOR) {
        waddstr(mode_screen, "NOR");
        wrefresh(mode_screen);

        werase(status_screen);
        wmove(status_screen, 1, window_size_x - 15);
        waddstr(status_screen, nor_com.c_str());
        wmove(status_screen, 0, 0);
        wrefresh(status_screen);
    } else if (mode == INS) {
        waddstr(mode_screen, "INS");
        wrefresh(mode_screen);
    } else if (mode == VIS) {

    } else if (mode == COM) {
        waddstr(mode_screen, "COM");
        wrefresh(mode_screen);
    }
    getyx(text_screen, cursor_y, cursor_x);
    wmove(text_screen, cursor_y, cursor_x);
    wrefresh(text_screen);
}

string text_scan(void) {
    int x, y;
    int size = 1000;
    char *tmp_str = (char *)malloc(sizeof(char) * size);
    int p = 0;
    wrefresh(text_screen);
    p = winnstr(text_screen, tmp_str, size);

    if (p == 0) {
        exit(1);
    }

    string str = tmp_str; // char*型をstring型へ変換
    str.erase(remove(str.begin(), str.end(), ' '),
              str.end()); // string型に写した文字列から空白を全て削除

    free(tmp_str);

    return str;
}

bool normal_command_check(void) {
    bool flag = false;
    for (int i = 0; i <= nor_com_list.size() - 1; i++) {
        if (nor_com_list[i].size() < nor_com.size()) {
            continue;
        }
        for (int j = 0; j <= nor_com.size() - 1; j++) {
            if (nor_com[j] != nor_com_list[i][j]) {
                break;
            }
            if (j == nor_com.size() - 1) {
                flag = true;
            }
        }
        if (flag == true) {
            break;
        }
    }

    return flag;
}

int now_line(void) {
    int y, x;
    getyx(text_screen, y, x);
    return line_top + y;
}
