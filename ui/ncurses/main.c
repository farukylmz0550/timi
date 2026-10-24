#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ncurses.h>
#include "msp/core.h"
#include "msp/backend.h"

int main(int argc, char **argv) {
    const char *src_path = "/run/media/farukylmz/C450-14F5";
    msp_source_t *src = NULL;
    if (msp_source_detect(src_path, &src) != 0) return 1;
    msp_opts_t opts = {0};
    msp_plan_t *plan = NULL;
    if (msp_plan_build(src, &opts, &plan) != 0) return 1;
    initscr(); cbreak(); noecho(); curs_set(0);
    mvprintw(0,0,"microsd-mp TUI (min)");
    mvprintw(1,0,"%s - %d:%02d", src->name?src->name:"", (int)(plan->total_sec/60),(int)((long)plan->total_sec%60));
    for (size_t i=0;i<plan->item_count && i<20;++i) {
        mvprintw(3+(int)i,0,"%02zu %5d:%02d %s", i+1,(int)(plan->items[i].duration_sec/60),(int)((long)plan->items[i].duration_sec%60), plan->items[i].title?plan->items[i].title:"");
    }
    mvprintw(25,0,"q:çık");
    refresh(); getch();
    endwin();
    msp_plan_free(plan); msp_source_free(src);
    return 0;
}
