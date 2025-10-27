#include "simulation.h"
#include <stdio.h>

int get_cell_state(grid_t *grid, int x, int y) {
    if (x < 0 || y < 0 || x >= grid->width || y >= grid->height)
        return 0;

    size_t bit_index = (size_t)y * grid->width + x;
    size_t uint64_index = bit_index / BITS_PER_UINT64;
    size_t bit_offset = bit_index % BITS_PER_UINT64;

    return (grid->cells[uint64_index] >> bit_offset) & 1;
}

int set_cell_state(uint64_t *grid, int x, int y, int state, int width, int height) {
    if (x < 0 || y < 0 || x >= width || y >= height)
        return -1;

    size_t bit_index = (size_t)y * width + x;
    size_t uint64_index = bit_index / BITS_PER_UINT64;
    size_t bit_offset = bit_index % BITS_PER_UINT64;

    if (state)
        grid[uint64_index] |= ((uint64_t)1 << bit_offset);
    else
        grid[uint64_index] &= ~((uint64_t)1 << bit_offset);
    return 0;
}
int num_neighbors(grid_t *grid, int x, int y ) {
    int neighbor_count = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;

            int nx = x + dx;
            int ny = y + dy;

            if (grid->boundary_mode == 1) {
                // Mode bord dur : on ignore les voisins hors grille
                if (nx < 0 || nx >= grid->width || ny < 0 || ny >= grid->height) {
                    continue;
                }
            } 
            else if (grid->boundary_mode == 2) {
                // Mode torique : on replie les coordonnées
                if (nx < 0) nx += grid->width;
                if (nx >= grid->width) nx -= grid->width;
                if (ny < 0) ny += grid->height;
                if (ny >= grid->height) ny -= grid->height;
            } 
            else if (grid->boundary_mode == 3) {
                // Mode miroir : on reflète les coordonnées
                if (nx < 0) nx = 0;
                if (nx >= grid->width) nx = grid->width - 1;
                // Miroir sur Y
                if (ny < 0) ny = 0;
                if (ny >= grid->height) ny = grid->height - 1;

            } 
            else {
                // Mode "tout autour vivant"
                if (nx < 0 || nx >= grid->width || ny < 0 || ny >= grid->height) {
                    neighbor_count++;
                    continue;
                }
            }

            neighbor_count += get_cell_state(grid, nx, ny);
        }
    }
    return neighbor_count;
}
int get_next_state(grid_t *grid, int x, int y) {
    // Aplly rules of the Game of Life here
    int current_state = get_cell_state(grid, x, y);
    int num_neighbors_alive = num_neighbors(grid, x, y);
    if (current_state) {
        if (num_neighbors_alive < 2 || num_neighbors_alive > 3)
            return 0; // Cell dies
        else
            return 1; // Cell lives
    } else {
        if (num_neighbors_alive == 3)
            return 1; // Cell becomes alive
        else
            return 0; // Cell remains dead
    }
    return 0;
}
void next_generation(grid_t *grid) {
    // Compute next generation and update grid->cells and grid->shadow accordingly
    for (int y = 0; y < grid->height; y++) {
        for (int x = 0; x < grid->width; x++) {
            int next_state = get_next_state(grid, x, y);
            set_cell_state(grid->shadow, x, y, next_state , grid->width, grid->height);
        }
    }
    // Swap cells and shadow
    uint64_t *temp = grid->cells;
    grid->cells = grid->shadow;
    grid->shadow = temp;
    return;
}


grid_t* create_grid( options_t *opts) {
    int width = opts->width;
    int height = opts->height;
    int boundary_mode = opts->boundary_mode;
    int numgen = opts->numgen;
    int framrate = opts->framrate;
    size_t total_cells = (size_t)width * (size_t)height;
    size_t total_bits = total_cells;
    size_t total_uint64s = (total_bits + BITS_PER_UINT64 - 1) / BITS_PER_UINT64;
    size_t total_memory_kb = (total_uint64s * sizeof(uint64_t) * 2) / 1024;

    if (total_memory_kb > MAX_MEMORY_KB) {
        return NULL;
    }
    printf("Allocating grid: %d x %d cells, approx %zu KB\n", width, height, total_memory_kb);

    grid_t *grid = (grid_t *)malloc(sizeof(grid_t));
    if (!grid) return NULL;

    grid->cells = (uint64_t *)calloc(total_uint64s, sizeof(uint64_t));
    grid->shadow = (uint64_t *)calloc(total_uint64s, sizeof(uint64_t));
    grid->width = width;
    grid->height = height;
    grid->boundary_mode = boundary_mode;
    grid->numgen = numgen;
    grid->framrate = framrate;
    grid->stat_gen_time = (double *)calloc((size_t)numgen, sizeof(double));
    if (!grid->cells || !grid->shadow) {
        free(grid->cells);
        free(grid->shadow);
        free(grid);
        return NULL;
    }

    grid->uint64_per_row = (width + BITS_PER_UINT64 - 1) / BITS_PER_UINT64;
    printf("uint64_per_row: %d\n", grid->uint64_per_row);
    grid->total_uint64s = total_uint64s;

    return grid;
}

void free_grid(grid_t *grid) {
    if (grid) {
        free(grid->cells);
        free(grid->shadow);
        free(grid);
    }
}


int load_grid_from_file(grid_t *grid, const char *filename) {
    char fullpath[256];  
    sprintf(fullpath, "./Save/%s", filename);
    FILE *file = fopen(fullpath, "r");
    if (!file) {
        return -1;
    }
    int x = 0, y = 0;
    int ch;
    while ((ch = fgetc(file)) != EOF) {
        if (ch == '\n') {
            y++;
            x = 0;
            continue;
        }
        if (ch == '*') {
            set_cell_state(grid->shadow, x, y, 1, grid->width, grid->height);
            x++;
        } else if (ch == '.') {
            set_cell_state(grid->shadow, x, y, 0, grid->width, grid->height);
            x++;
        }
        // Ignore other characters
    }
    fclose(file);
    // intervert shadow and cells
    uint64_t *temp = grid->cells;
    grid->cells = grid->shadow;
    grid->shadow = temp;
    return 0;
}

int save_grid_to_file(grid_t *grid, const char *filename) {
    char fullpath[256];  
    sprintf(fullpath, "./Save/%s", filename);
    FILE *file = fopen(fullpath, "w");
    if (!file) {
        return -1;
    }
    for (int y = 0; y < grid->height; y++) {
        for (int x = 0; x < grid->width; x++) {
            int state = get_cell_state(grid, x, y);
            fputc(state ? '*' : '.', file);
        }
        fputc('\n', file);
    }
    fclose(file);
    return 0;
}
