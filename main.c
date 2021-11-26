
#include <ncurses.h>
#include <time.h>

// TODO: Panel for stamping down interesting patterns at cursor?
// TODO: Reimplement in SDL?

#define BOARD_WIDTH 200
#define BOARD_HEIGHT 200 

#define ALIVE_CHAR ACS_CKBOARD
#define DEAD_CHAR ' ' 

typedef enum state_struct {
    ALIVE = 0,
    DEAD,
    UNDEF,
} state_t;

typedef enum game_state_enum {
    STEPPING = 0,
    EDITING,
    STATE_UNDEF
} game_state_t;

typedef struct board_struct {
    int board_width;
    int board_height;
    int viewport_x;
    int viewport_y;
    int viewport_width;
    int viewport_height;
    state_t board_a[BOARD_HEIGHT][BOARD_WIDTH];
    state_t board_b[BOARD_HEIGHT][BOARD_WIDTH];
    state_t (*front_ptr)[BOARD_HEIGHT][BOARD_WIDTH];
    state_t (*back_ptr)[BOARD_HEIGHT][BOARD_WIDTH];
    double last_update_time;
} board_t;

void init_board(board_t* board, int vx, int vy, int vw, int vh) {
    board->board_width = BOARD_WIDTH;
    board->board_height= BOARD_HEIGHT;
    board->viewport_x = vx;
    board->viewport_y = vy;
    board->viewport_width = vw;
    board->viewport_height = vh;

    for(int r = 0; r < board->board_height; r++) {
        for(int c = 0; c < board->board_width; c++) {
            board->board_a[r][c] = DEAD;
        }
    }

    board->front_ptr = &board->board_a;
    board->back_ptr = &board->board_b;
}

void render_board(WINDOW* win, board_t* board) {

    box(win, 0, 0);
    int off_x = 1;
    int off_y = 1;
    int vp_x = 0;
    int vp_y = 0;
    int board_min_x = board->viewport_x;
    int board_max_x = board->viewport_x + board->viewport_width;
    int board_min_y = board->viewport_y;
    int board_max_y = board->viewport_y + board->viewport_height;
    for(int r = board_min_y; r < board_max_y; r++) {
        vp_x = 0;
        for(int c = board_min_x; c < board_max_x; c++) {
            state_t val = (*board->front_ptr)[r][c];
            int s = (val == ALIVE) ? ALIVE_CHAR : DEAD_CHAR;

            mvwaddch(win, off_y+vp_y, off_x+vp_x, s | COLOR_PAIR(1));
            vp_x++;
        }
        vp_y++;
    }
    wrefresh(win);
}

int get_num_neighbors(board_t* board, int x, int y) {
    int minx = x == 0 ? 0 : x - 1;
    int maxx = x == (BOARD_WIDTH - 1) ? BOARD_WIDTH - 1 : x + 1;
    int miny = y == 0 ? 0 : y - 1;
    int maxy = y == (BOARD_HEIGHT- 1) ? BOARD_HEIGHT- 1 : y + 1;

    int num_neighbors = 0;
    for(int r = miny; r <= maxy; r++) {
        for(int c = minx; c <= maxx; c++) {
            if(r == y && c == x) continue;
            if( (*board->front_ptr)[r][c] == ALIVE) num_neighbors++;
        }
    }
    return num_neighbors;
}

void update_board(board_t* board) {
    clock_t begin = clock();
    for(int r = 0; r < board->board_height; r++) {
        for(int c = 0; c < board->board_width; c++) {
            int num_neighbors = get_num_neighbors(board, c, r);
            state_t cur_state = (*board->front_ptr)[r][c];
            state_t nxt_state = DEAD;
            if(cur_state == ALIVE) {
                if (num_neighbors == 2 || num_neighbors == 3) {
                    nxt_state = ALIVE;
                }
                else {
                    nxt_state = DEAD;
                }
            } 
            else if (cur_state == DEAD) {
                if(num_neighbors == 3) {
                    nxt_state = ALIVE;
                }
                else {
                    nxt_state = DEAD;
                }
            }
            (*board->back_ptr)[r][c] = nxt_state;
        }
    } 
    state_t (*tmp)[BOARD_HEIGHT][BOARD_WIDTH] = board->front_ptr;
    board->front_ptr = board->back_ptr;
    board->back_ptr = tmp;
    clock_t end = clock();
    board->last_update_time = (double)(end - begin) / CLOCKS_PER_SEC;
}

void translate_viewport(board_t* board, int dy, int dx) {
    int new_y = board->viewport_y + dy;
    int new_x = board->viewport_x + dx;

    if( (new_y >= 0) && (new_y < (BOARD_HEIGHT - board->viewport_height + 1) ) ) board->viewport_y = new_y;

    if( (new_x >= 0) && (new_x < (BOARD_WIDTH - board->viewport_width + 1) ) ) board->viewport_x = new_x;
}

void toggle_state(board_t* board, int vp_y, int vp_x) {
    int r, c;
    r = board->viewport_y + vp_y;
    c = board->viewport_x + vp_x;
    if( (*board->front_ptr)[r][c] == ALIVE) {
        (*board->front_ptr)[r][c] = DEAD;
    }
    else if( (*board->front_ptr)[r][c] == DEAD) {
        (*board->front_ptr)[r][c] = ALIVE;
    }
}

int main(int argc, char** argv) {

    board_t local_board;

    initscr();

    int maxx, maxy;
    getmaxyx(stdscr, maxy, maxx);
    int viewoffx, viewoffy, vieww, viewh;
    int headerlen = 3;
    int footerlen = 1;
    viewoffx = 2;
    viewoffy = headerlen + 1;
    vieww = maxx - 2*viewoffx;
    viewh = maxy - viewoffy - footerlen;

    init_board(&local_board, 0, 0, vieww-2, viewh-2);
    WINDOW* win = newwin(viewh, vieww, viewoffy, viewoffx);
    game_state_t cur_state = STEPPING;
    bool freerunning = false;
    double timeout = 0.5;
    int x = 1;
    int y = 1;

    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);

    clear();
    noecho();
    cbreak();

    keypad(win, TRUE);
    mvprintw(0, maxx - 16, "Size: (%3d, %3d)", maxy, maxx);
    mvprintw(0, 0, "Press F1 to exit.");
    mvprintw(1, 0, "Press <Space> to step.");
    move(2, 0); clrtoeol();
    mvprintw(2, 0, "Cur state: %s (%d, %d)", cur_state == EDITING ? "EDITING" : "STEPPING", y-1, x-1); 
    curs_set(0);
    refresh();

    render_board(win, &local_board);

    int breakout = 0;
    int step = 0;
    while(1) {
        int c = wgetch(win);
        switch(c) {
            case ' ':
            {
                if(cur_state == STEPPING) {
                    update_board(&local_board);
                    move(1, 0); clrtoeol();
                    mvprintw(1, 0, "Stepping: %d", step++);
                    refresh();
                }
                else if(cur_state == EDITING) {
                    toggle_state(&local_board, y-1, x-1);
                }
            } break;
            case 'f':
            {
                freerunning = !freerunning;
                if(freerunning) {
                    halfdelay(timeout * 10);
                } else {
                    nocbreak(); // leave half delay
                    cbreak();   // turn cbreak back on
                }
            } break;
            case '+':
            {
                if(timeout > 0.2) {
                    timeout /= 2;
                    if(freerunning) {
                        nocbreak();
                        cbreak();
                        halfdelay(timeout * 10);
                    }
                }
            } break;
            case '-':
            {
                timeout *= 2;
                if(freerunning) {
                    nocbreak();
                    cbreak();
                    halfdelay(timeout * 10);
                }
            } break;
            case ERR:
            {
                // getch should time out here if halfdelay is on
                // so act as though we stepped
                update_board(&local_board);
                move(1, 0); clrtoeol();
                mvprintw(1, 0, "Stepping: %d", step++);
                refresh();
            } break;
            case 'e':
            {
                if(cur_state == EDITING) {
                    cur_state = STEPPING;
                    curs_set(0);
                    wmove(win, y, x);
                }
                else if(cur_state == STEPPING) {
                    cur_state = EDITING;
                    curs_set(1);
                    wmove(win, y, x);
                }
            } break;
            case 'w':
            {
                translate_viewport(&local_board, -1, 0);
            } break;
            case 'a':
            {
                translate_viewport(&local_board, 0, -1);
            } break;
            case 's':
            {
                translate_viewport(&local_board, 1, 0);
            } break;
            case 'd':
            {
                translate_viewport(&local_board, 0, 1);
            } break;
            case KEY_UP:
            {
                if(y == 1) y = local_board.viewport_height;
                else y--;
                wmove(win, y, x);
            } break;
            case KEY_DOWN:
            {
                if(y == local_board.viewport_height) y = 1;
                else y++;
                wmove(win, y, x);
            } break;
            case KEY_LEFT:
            {
                if(x == 1) x = local_board.viewport_width;
                else x--;
                wmove(win, y, x);
            } break;
            case KEY_RIGHT:
            {
                if(x == local_board.viewport_width) x = 1;
                else x++;
                wmove(win, y, x);
            } break;

            case KEY_F(1):
            {
                breakout = 1; 
            } break;

        }
        if (breakout) break;
        move(2, 0); clrtoeol();
        mvprintw(2, 0, "Cur state: %s (%d, %d)", cur_state == EDITING ? "EDITING" : "STEPPING", y-1, x-1); 
        if(freerunning) {
            attron(A_BLINK);
            mvprintw(2, maxx-19, "FREERUNNING (%0.3f)", timeout);
            attroff(A_BLINK);
        }
        mvprintw(1, maxx-21, "Last Update: %0.6f", local_board.last_update_time);
        mvprintw(viewoffy-1, 0, "(%3d,%3d)", local_board.viewport_x, local_board.viewport_y);
        mvprintw(maxy-1, maxx-10, "(%3d,%3d)", local_board.viewport_x+local_board.viewport_width,
                                               local_board.viewport_y+local_board.viewport_height);
        refresh();
        render_board(win, &local_board);
        wmove(win, y, x);
    }
    move(0, 0); clrtoeol();
    mvprintw(0, 0, "Exiting!");
    getch();
    delwin(win);
    endwin();

}
