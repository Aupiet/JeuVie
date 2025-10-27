#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <getopt.h>
#include "simulation.h"




volatile int stop_requested = 0;


DWORD WINAPI SimulationThread(LPVOID lpParam) {
    grid_t *grid = (grid_t *)lpParam;
    LARGE_INTEGER freq, start, now;
    
    QueryPerformanceFrequency(&freq);

    const double frame_duration = 1.0 / grid->framrate;  // durée d'une frame en secondes

    for (int gen = 0; gen < grid->numgen && !stop_requested; gen++) {
        QueryPerformanceCounter(&start);

        
        next_generation(grid);
        showgrid(grid);
        QueryPerformanceCounter(&now);
        grid->stat_gen_time[gen] = (double)(now.QuadPart - start.QuadPart) / freq.QuadPart;
        

        // Boucle d’attente de fin de frame
        while (1) {
            QueryPerformanceCounter(&now);
            double elapsed = (double)(now.QuadPart - start.QuadPart) / freq.QuadPart;

            if (elapsed >= frame_duration)
                break;

            // Dors un petit peu pour libérer le CPU
            Sleep(1);
        }
    }

    return 0;
}

BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        stop_requested = 1;
        return TRUE;  // Empêche la terminaison brutale
    }
    return FALSE;
}


int main(int argc, char *argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    // clear screen
    printf("\033[2J\033[H");
    options_t opts;
    if (!parse_args(argc, argv, &opts)) {
        return 1;
    }
    
    printf("Width: %d\n", opts.width);
    printf("Height: %d\n", opts.height);
    printf("Generations: %d\n", opts.numgen);
    printf("Framerate: %d Hz\n", opts.framrate);
    printf("Boundary mode: %d\n", opts.boundary_mode);
    printf("Input: %s\n", opts.input_file ? opts.input_file : "(none)");
    printf("Output: %s\n", opts.output_file ? opts.output_file : "(none)");
    grid_t *grid = create_grid(&opts);
    if (opts.input_file) {
        printf("Loading initial grid from file: %s\n", opts.input_file);
        if (load_grid_from_file(grid, opts.input_file) != 0) {
            fprintf(stderr, "Failed to load grid from file: %s\n", opts.input_file);
            free_grid(grid);
            return 1;
        }
    }
    
    set_font_size( grid->width, grid->height);
    if (!grid) {
        printf("Failed to create grid\n");
        return 1;
    }    
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    HANDLE sim_thread = CreateThread(NULL, 0, SimulationThread, grid, 0, NULL);
    WaitForSingleObject(sim_thread, INFINITE);
    CloseHandle(sim_thread);

    if (opts.output_file) {
        printf("Saving final grid to file: %s\n", opts.output_file);
        if (save_grid_to_file(grid, opts.output_file) != 0) {
            fprintf(stderr, "Failed to save grid to file: %s\n", opts.output_file);
        }
    }
    printf("Statictics\n");
    // average generation time , worst case , jitter
    double total_time = 0.0;
    double worst_time = 0.0;
    uint16_t idxworst = 0;
    double jitter = 0.0;
    for (int i = 0; i < grid->numgen; i++) {
        if (grid->stat_gen_time[i] > worst_time) {
            worst_time = grid->stat_gen_time[i];
            idxworst = i;
        }
        total_time += grid->stat_gen_time[i];
    }
    double avg_time = total_time / grid->numgen;
    jitter = worst_time - avg_time;
    printf("Average generation time: %.6f seconds (maximum speed %.6f Hz)\n", avg_time , 1.0 / avg_time);
    printf("Worst generation time: %.6f seconds (generation %d)\n", worst_time, idxworst);
    printf("Jitter: %.6f seconds\n", jitter);


    free_grid(grid);
    free(opts.input_file);
    free(opts.output_file);
    printf("\033[2J\033[H");
    CONSOLE_FONT_INFOEX font = {0};
    font.cbSize = sizeof(font);
    font.dwFontSize.Y = 16;
    font.dwFontSize.X = 8;
    SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &font);
    return 0;
}
