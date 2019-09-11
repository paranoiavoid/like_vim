#include <curses.h>


enum MODE { //現在のモードの状態
    NOR,    //ノーマルモード
    INS,    //挿入モード
    VIS,    //ビジュアルモード
    COM,    //コマンドラインモード
};

char input(void) {
    return getch();
}

int output(char c) {
    int flag=0;
    addch(c);
    if(c=='\n'){
        flag=1;
    }

    return flag;    
}

int main(void) {
    initscr();

    erase();

    int x,y;
    x=0;
    y=0;
    while(1) {
        move(y,x);
        if(output(input())==1) {
            break;
        }
        y++;
    }

    timeout(-1);
    y++;
    addstr("END");
    getch();

    endwin();
    return 0;
}
