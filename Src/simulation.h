#ifndef SIMULATION_H
#define SIMULATION_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>
#include <getopt.h>
#include "interaction.h"


int get_cell_state(grid_t *grid, int x, int y);
int num_neighbors(grid_t *grid, int x, int y );
int get_next_state(grid_t *grid, int x, int y);
void next_generation( grid_t *grid, int debut, int fin);
DWORD WINAPI SimulationThread(LPVOID lpParam);
grid_t* create_grid(options_t *opts);
void free_grid(grid_t *grid);
int load_grid_from_file(grid_t *grid, const char *filename);
int save_grid_to_file(grid_t *grid, const char *filename);


#endif
// -------------------- includes --------------------
