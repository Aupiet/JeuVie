#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#include "simulation.h"

volatile int stop_requested = 0;

// Thread worker générique qui attend des signaux pour travailler
DWORD WINAPI WorkerThread(LPVOID lpParam) {
  info_t *info = (info_t *)lpParam;

  while (1) {
    // Attendre le signal de démarrage
    WaitForSingleObject(info->start_event, INFINITE);

    // Vérifier si on doit quitter
    if (info->should_exit) {
      break;
    }

    // Exécuter la fonction de travail
    if (info->work_func) {
      info->work_func(info);
    }

    // Signaler que le travail est terminé
    SetEvent(info->done_event);
  }

  return 0;
}

// Fonction de calcul (appelée par les threads workers)
void compute_work(info_t *info) {
  next_generation(info->grid, info->idxstart, info->idxend);
}

// Fonction d'affichage (appelée par les threads workers)
void display_work(info_t *info) {
  showgrid_native(info); // Nouveau : affichage Windows natif ultra-rapide
}

int simulation(grid_t *grid) {
  int num_threads = grid->thread > 0 ? grid->thread : 1;

  // Allouer les structures pour les threads
  info_t *worker_infos = malloc(num_threads * sizeof(info_t));
  HANDLE *done_events = malloc(num_threads * sizeof(HANDLE));

  if (!worker_infos || !done_events) {
    fprintf(stderr, "Failed to allocate thread resources\n");
    return -1;
  }

  // Calculer la division de la grille
  int rows_per_thread = grid->height / num_threads;
  int remaining_rows = grid->height % num_threads;

  // Obtenir le handle de la console pour WriteConsoleOutput
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

  // Créer le pool de threads persistants
  for (int t = 0; t < num_threads; t++) {
    worker_infos[t].grid = grid;
    worker_infos[t].idxstart = t * rows_per_thread;
    worker_infos[t].idxend = worker_infos[t].idxstart + rows_per_thread;
    if (t == num_threads - 1)
      worker_infos[t].idxend += remaining_rows;

    worker_infos[t].should_exit = 0;
    worker_infos[t].work_func = NULL;
    worker_infos[t].stop_flag = NULL;

    // Pré-allouer le buffer NATIF Windows (CHAR_INFO au lieu de strings)
    worker_infos[t].buf_width = grid->width;
    worker_infos[t].buf_height =
        worker_infos[t].idxend - worker_infos[t].idxstart;
    worker_infos[t].native_buf =
        (CHAR_INFO *)malloc(worker_infos[t].buf_width *
                            worker_infos[t].buf_height * sizeof(CHAR_INFO));

    // Créer les événements
    worker_infos[t].start_event =
        CreateEvent(NULL, FALSE, FALSE, NULL); // Auto-reset
    worker_infos[t].done_event =
        CreateEvent(NULL, FALSE, FALSE, NULL); // Auto-reset
    done_events[t] = worker_infos[t].done_event;

    // Créer le thread
    worker_infos[t].thread =
        CreateThread(NULL, 0, WorkerThread, &worker_infos[t], 0, NULL);
  }

  // Pour le timing précis
  LARGE_INTEGER freq;
  QueryPerformanceFrequency(&freq);
  double frame_target = 1.0 / (double)grid->framrate;

  // Gestion du framerate d'affichage (limité à 60 Hz pour fluidité)
  const double min_display_period = 1.0 / 60.0; // Max 60 FPS pour l'affichage
  LARGE_INTEGER last_display_time;
  QueryPerformanceCounter(&last_display_time);

#ifdef STAT
  int frames_skipped = 0;
#endif

  // Boucle principale de simulation
  for (int gen = 0; gen < grid->numgen && !stop_requested; gen++) {
    LARGE_INTEGER frame_start, compute_end;
    QueryPerformanceCounter(&frame_start);

    // ==================== PHASE DE CALCUL ====================
    // Assigner la fonction de calcul et démarrer tous les threads
    for (int t = 0; t < num_threads; t++) {
      worker_infos[t].work_func = compute_work;
      SetEvent(worker_infos[t].start_event);
    }

    // Attendre que tous les threads de calcul aient terminé
    WaitForMultipleObjects(num_threads, done_events, TRUE, INFINITE);

    // Swap des buffers (cells <-> shadow)
    uint64_t *tmp = grid->cells;
    grid->cells = grid->shadow;
    grid->shadow = tmp;

    QueryPerformanceCounter(&compute_end);

    // ==================== PHASE D'AFFICHAGE (LIMIT 60Hz) - WINDOWS NATIF
    // ==================== On n'affiche que si assez de temps s'est écoulé
    // depuis le dernier affichage
    double time_since_last_display =
        (double)(compute_end.QuadPart - last_display_time.QuadPart) /
        (double)freq.QuadPart;

    if (time_since_last_display >= min_display_period ||
        gen == grid->numgen - 1) {
      last_display_time = compute_end;

      // Assigner la fonction d'affichage et démarrer tous les threads
      for (int t = 0; t < num_threads; t++) {
        worker_infos[t].work_func = display_work;
        SetEvent(worker_infos[t].start_event);
      }

      // Attendre les threads d'affichage (SANS timeout)
      WaitForMultipleObjects(num_threads, done_events, TRUE, INFINITE);

      // MÉTHODE HYBRIDE : Construire string depuis native_buf pour debug
      printf("\033[H");
      for (int t = 0; t < num_threads; t++) {
        for (int i = 0;
             i < worker_infos[t].buf_width * worker_infos[t].buf_height; i++) {
          WCHAR ch = worker_infos[t].native_buf[i].Char.UnicodeChar;
          if (ch == L'█') {
            printf("█");
          } else {
            printf(" ");
          }
          // Nouvelle ligne tous les buf_width caractères
          if ((i + 1) % worker_infos[t].buf_width == 0) {
            printf("\n");
          }
        }
      }
      fflush(stdout);
    } else {
#ifdef STAT
      frames_skipped++;
#endif
    }

    // ==================== SYNCHRONISATION TEMPS RÉEL ====================
    LARGE_INTEGER frame_end;
    QueryPerformanceCounter(&frame_end);
    double frame_time = (double)(frame_end.QuadPart - frame_start.QuadPart) /
                        (double)freq.QuadPart;

#ifdef STAT
    grid->stat_gen_time[gen] = frame_time;
#endif

    // Attendre le reste du temps si on a fini avant
    double wait_time = frame_target - frame_time;
    if (wait_time > 0) {
      DWORD ms = (DWORD)(wait_time * 1000.0);
      if (ms > 0) {
        Sleep(ms);
      }

      // Ajustement fin avec busy-wait
      LARGE_INTEGER now;
      do {
        QueryPerformanceCounter(&now);
        frame_time = (double)(now.QuadPart - frame_start.QuadPart) /
                     (double)freq.QuadPart;
      } while (frame_time < frame_target);
    }
  }

  // Terminer proprement tous les threads
  for (int t = 0; t < num_threads; t++) {
    worker_infos[t].should_exit = 1;
    SetEvent(worker_infos[t].start_event);
    WaitForSingleObject(worker_infos[t].thread, INFINITE);
    CloseHandle(worker_infos[t].thread);
    CloseHandle(worker_infos[t].start_event);
    CloseHandle(worker_infos[t].done_event);
    free(worker_infos[t].native_buf);
  }

  free(worker_infos);
  free(done_events);

#ifdef STAT
  if (frames_skipped > 0) {
    printf("\n[STAT] Frames skipped (FPS limit): %d / %d\n", frames_skipped,
           grid->numgen);
  }
#endif

  return 0;
}

BOOL WINAPI ConsoleHandler(DWORD signal) {
  if (signal == CTRL_C_EVENT) {
    stop_requested = 1;
    printf("\n\n[INFO] Interruption détectée (Ctrl+C). Arrêt propre en "
           "cours...\n");
    return TRUE; // Empêche la terminaison brutale
  }
  return FALSE;
}

int main(int argc, char *argv[]) {
  SetConsoleOutputCP(CP_UTF8);
  SetConsoleCP(CP_UTF8);
  SetConsoleCtrlHandler(ConsoleHandler, TRUE);
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
  printf("Threads: %d\n", opts.thread);
  grid_t *grid = create_grid(&opts);
  if (opts.input_file) {
    printf("Loading initial grid from file: %s\n", opts.input_file);
    if (load_grid_from_file(grid, opts.input_file) != 0) {
      fprintf(stderr, "Failed to load grid from file: %s\n", opts.input_file);
      free_grid(grid);
      return 1;
    }
  }
  set_font_size(grid->width, grid->height);
  if (!grid) {
    printf("Failed to create grid\n");
    return 1;
  }

  simulation(grid);

  // Sauvegarder la grille si demandé
  if (opts.output_file) {
    printf("Saving final grid to file: %s\n", opts.output_file);
    if (save_grid_to_file(grid, opts.output_file) != 0) {
      fprintf(stderr, "Failed to save grid to file: %s\n", opts.output_file);
    }
  }

// Afficher les statistiques (même si interrompu par Ctrl+C)
#ifdef STAT
  printf("\n==========================\n");
  printf("Statistiques de simulation\n");
  printf("==========================\n");
  // average generation time , worst case , jitter
  double total_time = 0.0;
  double worst_time = 0.0;
  uint16_t idxworst = 0;
  double jitter = 0.0;
  int actual_gens = grid->numgen; // Nombre de générations réellement exécutées

  for (int i = 0; i < actual_gens; i++) {
    if (grid->stat_gen_time[i] > worst_time) {
      worst_time = grid->stat_gen_time[i];
      idxworst = i;
    }
    total_time += grid->stat_gen_time[i];
  }

  if (actual_gens > 0) {
    double avg_time = total_time / actual_gens;
    jitter = worst_time - avg_time;
    printf("Générations exécutées: %d / %d\n", actual_gens, opts.numgen);
    printf("Temps moyen par génération: %.6f s (vitesse max: %.2f Hz)\n",
           avg_time, 1.0 / avg_time);
    printf("Pire temps: %.6f s (génération %d)\n", worst_time, idxworst);
    printf("Jitter: %.6f s\n", jitter);
  }
  printf("==========================\n\n");
#endif

  free_grid(grid);
  free(opts.input_file);
  free(opts.output_file);

  // Nettoyage de la console (même si interrompu)
  printf("\033[2J\033[H"); // Clear screen
  CONSOLE_FONT_INFOEX font = {0};
  font.cbSize = sizeof(font);
  font.dwFontSize.Y = 16; // Taille par défaut
  font.dwFontSize.X = 8;
  SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &font);

  printf("Programme terminé.\n");
  return 0;
}
