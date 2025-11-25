#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <getopt.h>
#include <time.h>

#include "simulation.h"



volatile int stop_requested = 0;




DWORD WINAPI SimulationThread(LPVOID lpParam) {
    info_t *info = (info_t*) lpParam;
    next_generation(info->grid, info->idxstart, info->idxend);
    showgrid(info);
    return 0;
}


#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "simulation.h"

int simulation(grid_t *grid) {
    int num_threads = grid->thread > 0 ? grid->thread : 1;
    HANDLE *threads = malloc(num_threads * sizeof(HANDLE));
    info_t *infos = malloc(num_threads * sizeof(info_t));

    int rows_per_thread = grid->height / num_threads;
    int remaining_rows = grid->height % num_threads;

    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    double frame_target = 1.0 / (double)grid->framrate; // durée cible en secondes

    for (int gen = 0; gen < grid->numgen && !stop_requested; gen++) {
        LARGE_INTEGER start_qpc, end_qpc;
        QueryPerformanceCounter(&start_qpc);

        // Lancer les threads
        for (int t = 0; t < num_threads; t++) {
            infos[t].grid = grid;
            infos[t].idxstart = t * rows_per_thread;
            infos[t].idxend = infos[t].idxstart + rows_per_thread;
            if (t == num_threads - 1)
                infos[t].idxend += remaining_rows;

            threads[t] = CreateThread(NULL, 0, SimulationThread, &infos[t], 0, NULL);
        }

        WaitForMultipleObjects(num_threads, threads, TRUE, INFINITE);

        // swap buffers (une seule fois)
        uint64_t *tmp = grid->cells;
        grid->cells = grid->shadow;
        grid->shadow = tmp;

        // Affichage (fusion)
        printf("\033[H");
        for (int t = 0; t < num_threads; t++) {
            fwrite(infos[t].local_buf, 1, strlen(infos[t].local_buf), stdout);
            free(infos[t].local_buf);
            CloseHandle(threads[t]);
        }
        fflush(stdout);

        QueryPerformanceCounter(&end_qpc);

        // Calcul du temps écoulé
        double elapsed_sec = (double)(end_qpc.QuadPart - start_qpc.QuadPart) / (double)freq.QuadPart;
        #ifdef STAT
        grid->stat_gen_time[gen] = elapsed_sec;
        #endif
        // Synchronisation précise pour respecter la fréquence
        double remaining = frame_target - elapsed_sec;
        if (remaining > 0) {
            DWORD ms = (DWORD)(remaining * 1000.0);
            if (ms > 0)
                Sleep(ms);
            // Ajustement fin si nécessaire (très court délai)
            LARGE_INTEGER after_sleep;
            do {
                QueryPerformanceCounter(&after_sleep);
            } while ((double)(after_sleep.QuadPart - end_qpc.QuadPart) / freq.QuadPart < remaining);
        }
    }

    free(threads);
    free(infos);
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
    SetConsoleCtrlHandler(ConsoleHandler,TRUE);
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


    simulation(grid);
    if (opts.output_file) {
        printf("Saving final grid to file: %s\n", opts.output_file);
        if (save_grid_to_file(grid, opts.output_file) != 0) {
            fprintf(stderr, "Failed to save grid to file: %s\n", opts.output_file);
        }
    }
    #ifdef STAT
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
    #endif

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
