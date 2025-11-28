#include "interaction.h"

void set_console_fullscreen() {
  HWND hwnd = GetConsoleWindow();
  if (hwnd != NULL) {
    ShowWindow(hwnd, SW_MAXIMIZE);
  }
}

void set_font_size(int width, int height) {
  CONSOLE_FONT_INFOEX font = {0};
  font.cbSize = sizeof(font);
  if (height <= 50 && width <= 100) {
    font.dwFontSize.Y = 10;
    font.dwFontSize.X = 10;
  } else if (height <= 100 && width <= 300) {
    font.dwFontSize.Y = 6;
    font.dwFontSize.X = 6;
  } else if (height <= 200 && width <= 500) {
    font.dwFontSize.Y = 2;
    font.dwFontSize.X = 2;
  } else {
    font.dwFontSize.Y = 1;
    font.dwFontSize.X = 1;
  }
  font.FontFamily = FF_DONTCARE;
  font.FontWeight = FW_NORMAL;
  // wcscpy(font.FaceName, L"Consolas");
  SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &font);
  // setup window size in full screen
  set_console_fullscreen();
}

void print_help(const char *progname) {
  printf("Usage: %s [OPTIONS]\n", progname);
  printf("\nOptions:\n");
  printf("  --width <N>          Set grid width (default: 100)\n");
  printf("  --height <N>         Set grid height (default: 100)\n");
  printf("  --gens <N>           Number of generations (default: 1000)\n");
  printf("  --target-hz <N>      Target framerate in Hz (default: 10)\n");
  printf("  --boundary <mode>    Boundary mode: edge, torus, mirror, alive "
         "(default: edge)\n");
  printf("  --in <file>          Input file for initial grid\n");
  printf("  --out <file>         Output file for final state\n");
  printf("  --thread <N>         Number of thread\n");
  printf("  --help               Show this help message\n");
  printf("\nExample:\n");
  printf("  %s --width 320 --height 240 --gens 500 --boundary torus \\\n",
         progname);
  printf("     --in glider.txt --out result.txt --target-hz 60\n");
}

int parse_args(int argc, char **argv, options_t *opts) {
  // valeurs par défaut
  opts->width = 100;
  opts->height = 100;
  opts->numgen = 1000;
  opts->framrate = 10;
  opts->boundary_mode = 1;
  opts->input_file = NULL;
  opts->output_file = NULL;
  opts->thread = 1;

  const struct option longopts[] = {{"width", required_argument, 0, 'w'},
                                    {"height", required_argument, 0, 'h'},
                                    {"gens", required_argument, 0, 'g'},
                                    {"target-hz", required_argument, 0, 'f'},
                                    {"boundary", required_argument, 0, 'b'},
                                    {"in", required_argument, 0, 'i'},
                                    {"out", required_argument, 0, 'o'},
                                    {"thread", required_argument, 0, 't'},
                                    {"help", no_argument, 0, '?'},
                                    {0, 0, 0, 0}};

  int c;
  while ((c = getopt_long(argc, argv, "w:h:g:f:b:i:o:?", longopts, NULL)) !=
         -1) {
    switch (c) {
    case 'w':
      opts->width = atoi(optarg);
      break;
    case 'h':
      opts->height = atoi(optarg);
      break;
    case 'g':
      opts->numgen = atoi(optarg);
      break;
    case 'f':
      opts->framrate = atoi(optarg);
      break;
    case 'b':
      if (strcmp(optarg, "edge") == 0)
        opts->boundary_mode = 1;
      else if (strcmp(optarg, "torus") == 0)
        opts->boundary_mode = 2;
      else if (strcmp(optarg, "mirror") == 0)
        opts->boundary_mode = 3;
      else if (strcmp(optarg, "alive") == 0)
        opts->boundary_mode = 4;
      else {
        fprintf(stderr, "Invalid boundary mode: %s\n", optarg);
        return 0;
      }
      break;
    case 'i':
      opts->input_file = strdup(optarg);
      break;
    case 'o':
      opts->output_file = strdup(optarg);
      break;
    case 't':
      opts->thread = atoi(optarg);
      break;
    case '?':
      print_help(argv[0]);
      exit(0);
    default:
      fprintf(stderr, "Try '%s --help' for usage.\n", argv[0]);
      return 0;
    }
  }

  return 1;
}

// Ancienne fonction showgrid (DEPRECATED - conservée pour compatibilité mais
// non utilisée) La nouvelle version showgrid_native est utilisée dans main.c

// Nouvelle fonction : affichage natif Windows ultra-rapide
void showgrid_native(info_t *info) {
  grid_t *grid = info->grid;

  // Remplir le buffer CHAR_INFO directement (pas de conversion string UTF-8)
  int idx = 0;
  for (int y = info->idxstart; y < info->idxend; y++) {
    // Vérifier le flag d'arrêt si présent
    if (info->stop_flag && *(info->stop_flag)) {
      return; // Interrompu proprement
    }

    for (int x = 0; x < grid->width; x++) {
      size_t bit_index = (size_t)y * grid->width + x;
      size_t uint64_index = bit_index / BITS_PER_UINT64;
      size_t bit_offset = bit_index % BITS_PER_UINT64;
      int state = (grid->cells[uint64_index] >> bit_offset) & 1;

      CHAR_INFO *cell = &info->native_buf[idx++];

      if (state) {
        // Cellule vivante : bloc plein blanc
        cell->Char.UnicodeChar = L'█';
        cell->Attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE |
                           FOREGROUND_INTENSITY;
      } else {
        // Cellule morte : espace noir
        cell->Char.UnicodeChar = L' ';
        cell->Attributes = 0;
      }
    }
  }
}
