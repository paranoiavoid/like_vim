/*
カーソルが一時的に２箇所に現れるバグがある
bbやx,Xなどで文字を削除コピーしたときにペーストできる様に配列に保存する
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

enum COPY_MODE { //コピーの種類分け
    NO,
    LINE,
    BLOCK,
    V_NOR,
    V_LINE,
    V_BLOCK,
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
int line_top = 1;                  //画面の一番上の行番号
int line_max = 1;                  //行が存在する最大の行番号
vector<string> text(MAX_LINE, ""); //テキストを保存しておく二次元文字配列
vector<int> text_size(MAX_LINE, 0); //各行のテキストの文字数を管理
string nor_com; //ノーマルモードのコマンドを格納
vector<string> text_copy(MAX_LINE,
                         ""); //コピーされた文字列を保存す二次元文字配列
int copy_line = 0;     //テキストがコピーされている行数
COPY_MODE cpmode = NO; //今のコピーされたテキストのモード

//ノーマルモードのコマンドをリスト化
vector<string> nor_com_list = {
    "i", "a", "I", "A",  "h", "j", "k",  "l", ":",  /* "u", "d",*/ "x",
    "X", "O", "o", "dd", "$", "0", "gg", "G", "zt", "H",
    "M", "L", "p", "P",  "yy"};

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
void text_save(void);       //テキスト情報を保存する
void text_output(void);     //画面移動後のテキストを表示する
void text_copy_reset(void); //コピーされた文字列をリセットする
void text_copy_func(
    int xf, int yf, int xl, int yl,
    COPY_MODE mode); //テキストをコピーする関数
                     // xはカーソルの位置,yは絶対的な行番号で指定
void text_paste_func(
    int xf, int yf, int y_line,
    COPY_MODE mode); //テキストをペーストする関数
                     // xはカーソルの位置,yは絶対的な行番号で指定

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
        if (now_line() < line_max) {
            if (cursor_y < (window_size_y - status_window_height - 1) - 1) {
                if (text_size[now_line() + 1] <= cursor_x) {
                    cursor_x = max(text_size[now_line() + 1] - 1, 0);
                }
                wmove(text_screen, ++cursor_y, cursor_x);
                wrefresh(text_screen);
            } else {
                if (text_size[now_line() + 1] <= cursor_x) {
                    cursor_x = max(text_size[now_line() + 1] - 1, 0);
                }
                text_save();
                line_top++;
                text_output();
                wmove(text_screen, cursor_y, cursor_x);
                wrefresh(text_screen);
            }
        }
        nor_com = "";
    } else if (nor_com == "k") {
        if (now_line() > 1) {
            if (cursor_y >= 1) {
                if (text_size[now_line() - 1] <= cursor_x) {
                    cursor_x = max(text_size[now_line() - 1] - 1, 0);
                }
                cursor_y = max(cursor_y - 1, 0);
                wmove(text_screen, cursor_y, cursor_x);
                wrefresh(text_screen);
            } else {
                if (text_size[now_line() - 1] <= cursor_x) {
                    cursor_x = max(text_size[now_line() - 1] - 1, 0);
                }
                text_save();
                line_top--;
                text_output();
                wmove(text_screen, cursor_y, cursor_x);
                wrefresh(text_screen);
            }
        }
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
    }
    /*
    else if (nor_com == "u") {
        if (line_top < line_max) {
            text_save();
            line_top++;
            text_output();
            wmove(text_screen, --cursor_y, cursor_x);
            wrefresh(text_screen);
        }
        nor_com = "";
    } else if (nor_com == "d") {
             if (line_top > 1) {
            text_save();
            line_top--;
            text_output();
            wmove(text_screen, ++cursor_y, cursor_x);
            wrefresh(text_screen);
        }
        nor_com = "";
    }
    */
    else if (nor_com == "x") {
        if (text_size[now_line()] >= 1) {
            text_copy_func(cursor_x, now_line(), cursor_x, now_line(), BLOCK);
            wdelch(text_screen);
            text_size[now_line()]--;
            cursor_x = min(cursor_x, text_size[now_line()] - 1);
            wmove(text_screen, cursor_y, cursor_x);
            wrefresh(text_screen);
        }
        nor_com = "";
    } else if (nor_com == "X") {
        if (cursor_x > 0) {
            text_copy_func(cursor_x - 1, now_line(), cursor_x - 1, now_line(),
                           BLOCK);
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
    } else if (nor_com == "dd") {
        if (line_max > 1 && line_max > line_top) {
            text_save();
            text_copy_func(0, now_line(), text_size[now_line()] - 1, now_line(),
                           LINE);
            wdeleteln(text_screen);
            for (int i = now_line(); i <= MAX_LINE - 2; i++) {
                text_size[i] = text_size[i + 1];
            }
            line_max--;
            wmove(text_screen, min(cursor_y, line_max - line_top), 0);
            wrefresh(text_screen);
        } else if (line_max == 1) {
            text_save();
            text_copy_func(0, now_line(), text_size[now_line()] - 1, now_line(),
                           LINE);
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
    } else if (nor_com == "gg") {
        text_save();
        line_top = 1;
        text_output();
        cursor_y = 0;
        cursor_x = 0;
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
        nor_com = "";
    } else if (nor_com == "G") {
        text_save();
        if (line_max < line_top + (window_size_y - status_window_height -
                                   1)) { //最終行が画面内にある場合
            cursor_y = line_max - line_top;
            cursor_x = 0;
        } else { //最終行が画面内にない場合
            line_top =
                line_max - (window_size_y - status_window_height - 1) + 1;
            text_output();
            cursor_y = (window_size_y - status_window_height - 1) - 1;
            cursor_x = 0;
        }
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
        nor_com = "";
    } else if (nor_com == "zt") {
        text_save();
        line_top = now_line();
        text_output();
        cursor_y = 0;
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
        nor_com = "";
    } else if (nor_com == "H") {
        cursor_y = 0;
        cursor_x = 0;
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
        nor_com = "";
    } else if (nor_com == "M") {
        cursor_y = min((window_size_y - status_window_height - 1) - 1,
                       line_max - line_top) /
                   2;
        cursor_x = 0;
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
        nor_com = "";
    } else if (nor_com == "L") {
        cursor_y = min((window_size_y - status_window_height - 1) - 1,
                       line_max - line_top);
        cursor_x = 0;
        wmove(text_screen, cursor_y, cursor_x);
        wrefresh(text_screen);
        nor_com = "";
    } else if (nor_com == "p") {
        text_save();
        if (cpmode == LINE) {
            text_paste_func(0, now_line() + 1, copy_line, cpmode);
            cursor_x = 0;
            wmove(text_screen, ++cursor_y, cursor_x);
        } else if (cpmode == BLOCK) {
            text_paste_func(cursor_x + 1, now_line(), 0, cpmode);
            cursor_x += text_copy[1].size();
            wmove(text_screen, cursor_y, cursor_x);
        }
        wrefresh(text_screen);
        nor_com = "";
    } else if (nor_com == "P") {
        text_save();
        if (cpmode == LINE) {
            text_paste_func(0, now_line(), copy_line, cpmode);
            cursor_x = 0;
            wmove(text_screen, cursor_y, cursor_x);
        } else if (cpmode == BLOCK) {
            text_paste_func(cursor_x, now_line(), 0, cpmode);
            cursor_x += text_copy[1].size() - 1;
            wmove(text_screen, cursor_y, cursor_x);
        }
        wrefresh(text_screen);
        nor_com = "";
    } else if (nor_com == "yy") {
        text_save();
        text_copy_func(0, now_line(), text_size[now_line()] - 1, now_line(),
                       LINE);
        wrefresh(text_screen);
        nor_com = "";
    } else if (nor_com[0] >= '1' && nor_com[0] <= '9') {

        for (int i = 0; i < nor_com.size(); i++) {
            if (nor_com[i] >= '0' && nor_com[i] <= '9') {
                continue;
            } else {
                int num = stoi(nor_com.substr(0, i));
                // string tmp = nor_com.substr(1, 1);
                string tmp = nor_com.substr(i);
                if (tmp == "j") {
                    if (now_line() < line_max) {
                        if (now_line() + num > line_max) {
                            num = line_max - now_line();
                        }
                        if (cursor_y + (num - 1) <
                            (window_size_y - status_window_height - 1) - 1) {
                            if (text_size[now_line() + num] <= cursor_x) {
                                cursor_x =
                                    max(text_size[now_line() + num] - 1, 0);
                            }
                            cursor_y += num;
                            wmove(text_screen, cursor_y, cursor_x);
                            wrefresh(text_screen);
                        } else {
                            if (text_size[now_line() + num] <= cursor_x) {
                                cursor_x =
                                    max(text_size[now_line() + num] - 1, 0);
                            }
                            text_save();
                            line_top +=
                                num -
                                ((window_size_y - status_window_height - 1) -
                                 (cursor_y + 1));
                            text_output();
                            cursor_y =
                                (window_size_y - status_window_height - 1) - 1;
                            wmove(text_screen, cursor_y, cursor_x);
                            wrefresh(text_screen);
                        }
                    }
                } else if (tmp == "k") {
                    if (now_line() > 1) {
                        if (now_line() - num <= 0) {
                            num = now_line() - 1;
                        }
                        if (cursor_y - num >= 0) {
                            if (text_size[now_line() - num] <= cursor_x) {
                                cursor_x =
                                    max(text_size[now_line() - num] - 1, 0);
                            }
                            cursor_y = max(cursor_y - num, 0);
                            wmove(text_screen, cursor_y, cursor_x);
                            wrefresh(text_screen);
                        } else {
                            if (text_size[now_line() - num] <= cursor_x) {
                                cursor_x =
                                    max(text_size[now_line() - num] - 1, 0);
                            }
                            text_save();
                            line_top -= num - (cursor_y);
                            text_output();
                            cursor_y = 0;
                            wmove(text_screen, cursor_y, cursor_x);
                            wrefresh(text_screen);
                        }
                    }
                } else if (tmp == "h") {
                    cursor_x = max(cursor_x - num, 0);
                    wmove(text_screen, cursor_y, cursor_x);
                    wrefresh(text_screen);
                } else if (tmp == "l") {
                    cursor_x = min(text_size[now_line()] - 1, cursor_x + num);
                    wmove(text_screen, cursor_y, cursor_x);
                    wrefresh(text_screen);
                } else if (tmp == "x") {
                    if (text_size[now_line()] >= 1) {
                        num = min(num, text_size[now_line()] - cursor_x);
                        text_copy_func(cursor_x, now_line(),
                                       cursor_x + (num - 1), now_line(), BLOCK);
                        for (int i = 1; i <= num; i++) {
                            wdelch(text_screen);
                        }
                        text_size[now_line()] -= num;
                        cursor_x = min(cursor_x, text_size[now_line()] - 1);
                        wmove(text_screen, cursor_y, cursor_x);
                        wrefresh(text_screen);
                    }
                } else if (tmp == "X") {
                    if (cursor_x > 0) {
                        num = min(num, cursor_x);
                        text_copy_func(cursor_x - 1 - (num - 1), now_line(),
                                       cursor_x - 1, now_line(), BLOCK);
                        for (int i = 1; i <= num; i++) {
                            wmove(text_screen, cursor_y, --cursor_x);
                            wdelch(text_screen);
                        }
                        text_size[now_line()] -= num;
                        wrefresh(text_screen);
                    }
                } else if (tmp == "G") {
                    if (line_top <= num &&
                        line_top + (window_size_y - status_window_height - 1) -
                                1 >=
                            num) {
                        cursor_y = num - line_top;
                        cursor_x = 0;
                        wmove(text_screen, cursor_y, cursor_x);
                        wrefresh(text_screen);
                    } else if (num < line_top) {
                        cursor_y = 0;
                        cursor_x = 0;
                        text_save();
                        line_top = num;
                        text_output();
                        wmove(text_screen, cursor_y, cursor_x);
                        wrefresh(text_screen);
                    } else { //下側の画面外に移動したい行がある
                        cursor_y = (window_size_y - status_window_height - 1);
                        cursor_x = 0;
                        text_save();
                        line_top = num + 1 -
                                   (window_size_y - status_window_height - 1);
                        text_output();
                        wmove(text_screen, cursor_y, cursor_x);
                        wrefresh(text_screen);
                    }
                } else if (tmp == "d") {
                    break;
                } else if (tmp == "dd") {
                    num--; //コピーしたい行数-1の値にする
                    if (now_line() + num >= line_max) {
                        num = line_max - now_line();
                    }
                    text_save();

                    text_copy_func(0, now_line(),
                                   window_size_x - line_window_width,
                                   now_line() + num, LINE);
                    // wdeleteln(text_screen);
                    for (int i = now_line(); i <= MAX_LINE - (num + 1) - 1;
                         i++) {
                        text[i] = text[i + (num + 1)];
                    }
                    for (int i = now_line(); i <= MAX_LINE - (num + 1) - 1;
                         i++) {
                        text_size[i] = text_size[i + (num + 1)];
                    }
                    line_max -= (num + 1);
                    if (line_max <= 0) {
                        line_max = 1;
                    }
                    if (cursor_y == 0 && line_top > line_max) {
                        line_top = max(line_top - 1, 1);
                    }
                    text_output();
                    wmove(text_screen, min(cursor_y, line_max - line_top), 0);
                    wrefresh(text_screen);
                } else if (tmp == "y") {
                    break;
                } else if (tmp == "yy") {

                    num--; //コピーしたい行数-1の値にする
                    if (now_line() + num >= line_max) {
                        num = line_max - now_line();
                    }
                    text_save();

                    text_copy_func(0, now_line(),
                                   window_size_x - line_window_width,
                                   now_line() + num, LINE);
                    // wdeleteln(text_screen);
                    text_output();
                    wmove(text_screen, min(cursor_y, line_max - line_top), 0);
                    wrefresh(text_screen);
                }

                nor_com = "";
                break;
            }
        }
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

        if (cursor_y >= window_size_y - status_window_height - 2) {
            text_save();
            line_top++;
            line_max++;
            text_output();
            wrefresh(text_screen);
            wmove(text_screen, cursor_y, 0);
            wrefresh(text_screen);
            waddstr(text_screen, str);
            wrefresh(text_screen);

        } else {
            wmove(text_screen, ++cursor_y, 0);
            winsdelln(text_screen, 1);
            line_max++;
            wrefresh(text_screen);
            waddstr(text_screen, str);
            wrefresh(text_screen);
        }

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
    text_save();
    text_output();
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
                return true;
            }
        }
    }

    /*この段階で0のみの入力が弾かれているはず*/
    flag = true;
    for (int i = 0; i < nor_com.size(); i++) {
        if (nor_com[i] >= '0' && nor_com[i] <= '9') {
            continue;
        } else {
            string tmp = nor_com.substr(i);
            if (tmp == "j") {
            } else if (tmp == "k") {
            } else if (tmp == "h") {
            } else if (tmp == "l") {
            } else if (tmp == "x") {
            } else if (tmp == "X") {
            } else if (tmp == "G") {
            } else if (tmp == "d") {
                break;
            } else if (tmp == "dd") {
            } else if (tmp == "y") {
                break;
            } else if (tmp == "yy") {
            } else {
                flag = false;
            }

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

void text_save(void) {
    for (int i = line_top; i <= line_max; i++) {
        if (i - line_top >= window_size_y - status_window_height - 1) {
            break;
        }
        wmove(text_screen, i - line_top, 0);
        text[i] = text_scan();
        wrefresh(text_screen);
    }
    wmove(text_screen, cursor_y, cursor_x);
    wrefresh(text_screen);
}

void text_output(void) {
    werase(text_screen);

    for (int i = line_top; i <= line_max; i++) {
        if (i - line_top >= window_size_y - status_window_height - 1) {
            break;
        }
        wmove(text_screen, i - line_top, 0);
        waddstr(text_screen, text[i].c_str());
        wrefresh(text_screen);
    }

    wmove(text_screen, cursor_y, cursor_x);
    wrefresh(text_screen);
}

void text_copy_reset(void) {
    for (int i = 0; i < MAX_LINE; i++) {
        text_copy[i] = "";
    }
}

void text_copy_func(int xf, int yf, int xl, int yl, COPY_MODE mode) {

    text_copy_reset();

    wrefresh(text_screen);
    copy_line = (yl - yf) + 1;
    cpmode = mode;

    if (mode == LINE) { //行コピー

        for (int y = yf; y <= yl; y++) {
            text_copy[y - yf + 1] =
                text[y].substr(xf, min(xl - xf + 1, text_size[y] - xf + 1));
        }
    } else if (mode == BLOCK) {
        for (int y = yf; y <= yl; y++) {
            text_copy[y - yf + 1] =
                text[y].substr(xf, min(xl - xf + 1, text_size[y] - xf + 1));
        }
    }
}

void text_paste_func(int xf, int yf, int y_line, COPY_MODE mode) {
    wrefresh(text_screen);

    if (mode == LINE) {
        for (int y = MAX_LINE - 1; y >= yf + y_line; y--) {
            text[y] = text[y - y_line];
            text_size[y] = text_size[y - y_line];
        }
        for (int y = 1; y <= y_line; y++) {
            text[yf + y - 1] = text_copy[y];
            text_size[yf + y - 1] = text_copy[y].size();
        }
        line_max += y_line;
    } else if (mode == BLOCK) {

        int x_column = text_copy[1].size();
        for (int x = text_size[yf] - 1; x >= xf; x--) {
            text[yf][x + x_column] = text[yf][x];
        }
        for (int x = 0; x < x_column; x++) {
            text[yf][xf + x] = text_copy[1][x];
        }
        text_size[yf] += x_column;
    }
    text_output();
}
