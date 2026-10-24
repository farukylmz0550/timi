#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include "msp/core.h"
#include "msp/backend.h"

static void usage(const char *prog) {
    printf("Kullanım:\n");
    printf("  %s [seçenekler]\n\n", prog);
    printf("Seçenekler:\n");
    printf("  -s, --source <YOL>   Kaynak (boşsa otomatik algıla)\n");
    printf("      --list           45 dk bloğunu listele ve çık\n");
    printf("  -e, --export <HEDEF> Bloğu hedefe yaz (GUI yok)\n");
    printf("      --shuffle        Blok seçimini karıştır\n");
    printf("  -p, --play           Blok'u çal (miniaudio)\n");
    printf("  -h, --help           Yardımı göster\n");
}

static int has_data_yaml(const char *path) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s/data.yaml", path);
    return access(buf, R_OK) == 0;
}

static int auto_detect(char *out, size_t len) {
    const char *user = getenv("USER");
    if (user) {
        char media[512];
        snprintf(media, sizeof(media), "/run/media/%s", user);
        DIR *d = opendir(media);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d))) {
                if (e->d_name[0] == '.') continue;
                char cand[1024];
                snprintf(cand, sizeof(cand), "%s/%s", media, e->d_name);
                struct stat st;
                if (stat(cand, &st) == 0 && S_ISDIR(st.st_mode)) {
                    if (has_data_yaml(cand)) {
                        strncpy(out, cand, len-1); out[len-1]=0;
                        closedir(d); return 0;
                    }
                }
            }
            closedir(d);
        }
    }
    const char *home = getenv("HOME");
    if (home) {
        char music[512];
        snprintf(music, sizeof(music), "%s/Music", home);
        struct stat st;
        if (stat(music, &st) == 0 && S_ISDIR(st.st_mode)) {
            strncpy(out, music, len-1); out[len-1]=0;
            return 0;
        }
    }
    return -1;
}

int main(int argc, char **argv) {
    const char *src_path = NULL;
    const char *exp_path = NULL;
    int list_only = 0;
    int do_play = 0;
    msp_opts_t opts = {0};

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
        else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--source")) { if (i+1<argc) src_path = argv[++i]; }
        else if (!strcmp(argv[i], "--list")) { list_only = 1; }
        else if (!strcmp(argv[i], "-e") || !strcmp(argv[i], "--export")) { if (i+1<argc) exp_path = argv[++i]; }
        else if (!strcmp(argv[i], "--shuffle")) { opts.shuffle = true; }
        else if (!strcmp(argv[i], "--fewer-shuffle")) { opts.fewer_shuffle = true; opts.shuffle = true; }
        else if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--play")) { do_play = 1; }
        else { fprintf(stderr, "Bilinmeyen: %s\n", argv[i]); usage(argv[0]); return 2; }
    }

    char auto_buf[1024];
    if (!src_path) {
        if (auto_detect(auto_buf, sizeof(auto_buf)) != 0) {
            fprintf(stderr, "Kaynak bulunamadı (-s ile belirtin)\n");
            return 1;
        }
        src_path = auto_buf;
    }

    msp_source_t *src = NULL;
    if (msp_source_detect(src_path, &src) != 0) {
        fprintf(stderr, "Kaynak algılanamadı\n");
        return 1;
    }

    msp_plan_t *plan = NULL;
    if (msp_plan_build(src, &opts, &plan) != 0) {
        fprintf(stderr, "Plan oluşturulamadı\n");
        return 1;
    }

    if (list_only) {
        printf("Kaynak : %s\n", src->name ? src->name : src_path);
        printf("Toplam : %d:%02d\n", (int)(plan->total_sec/60), (int)((long)plan->total_sec%60));
        for (size_t i = 0; i < plan->item_count && i < 30; ++i) {
            printf("  %02zu. %5d:%02d  %s\n", i+1, (int)(plan->items[i].duration_sec/60), (int)((long)plan->items[i].duration_sec%60), plan->items[i].title ? plan->items[i].title : "");
        }
        if (plan->item_count > 30) printf("  ... (%zu parça)\n", plan->item_count);
        msp_plan_free(plan);
        msp_source_free(src);
        return 0;
    }

    if (exp_path) {
        char *outp = NULL;
        if (msp_export_bundle(plan, src, exp_path, &outp) != 0) {
            fprintf(stderr, "Dışa aktarma başarısız\n");
            return 1;
        }
        printf("Yazıldı: %s\n", outp ? outp : exp_path);
        free(outp);
        msp_plan_free(plan);
        msp_source_free(src);
        return 0;
    }

    if (do_play) {
        msp_player_t *pl = NULL;
        if (msp_player_create(plan, &opts, &pl) != 0) {
            fprintf(stderr, "Oynatıcı başlatılamadı\n");
            return 1;
        }
        if (msp_player_play(pl) != 0) {
            fprintf(stderr, "Oynatma başlatılamadı\n");
            msp_player_free(pl);
            msp_plan_free(plan);
            msp_source_free(src);
            return 1;
        }
        printf("Çalıyor (Ctrl+C ile durdur)\n");
        while (1) sleep(1);
        msp_player_free(pl);
        msp_plan_free(plan);
        msp_source_free(src);
        return 0;
    }

    printf("CLI modu (GUI devre dışı)\n");
    printf("Kaynak: %s\n", src->name ? src->name : src_path);
    printf("Toplam: %d:%02d | Parça: %zu\n", (int)(plan->total_sec/60), (int)((long)plan->total_sec%60), plan->item_count);
    printf("--list / --export / --play kullanın\n");
    msp_plan_free(plan);
    msp_source_free(src);
    return 0;
}
