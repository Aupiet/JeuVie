#ifndef INTERACTION_H
#define INTERACTION_H
    #include <stdio.h>
    #include <stdlib.h>
    #include <stdint.h>
    #include <windows.h>
    #include <getopt.h>
    
    #define MAX_MEMORY_KB 64
    #define BITS_PER_UINT64 64
    typedef struct {
        uint64_t *cells;
        uint64_t *shadow;
        uint8_t height;
        uint8_t width;
        uint8_t boundary_mode;
        uint16_t numgen;
        uint16_t framrate;
        int uint64_per_row;
        size_t total_uint64s;
        uint16_t thread; 
        #ifdef STAT
            double *stat_gen_time;
        #endif
    } grid_t;

    typedef struct {
        uint16_t width;
        uint16_t height;
        uint16_t numgen;
        uint16_t framrate;
        uint16_t boundary_mode; // 1: edge, 2: torus, 3: mirror, 4: alive
        char *input_file;
        char *output_file;
        uint16_t thread;
    } options_t;

    typedef struct {
        uint16_t idxstart;
        uint16_t idxend;
        grid_t *grid;
        char *local_buf;
        size_t local_size;
        #ifdef STAT
            int gennum;
        #endif
    } info_t;

    void print_help(const char *progname);
    int parse_args(int argc, char **argv, options_t *opts);
    void set_font_size(int width, int height);
    void set_console_fullscreen();
    void showgrid(info_t *info);
#endif