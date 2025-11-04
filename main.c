#include <math.h>
#include <string.h>
#include <termios.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>

#define MAZE_WIDTH  31
#define MAZE_HEIGHT 31

#define DARK_MODE 0
#define SKIP_WALK 1
#define TORCH_SIZE 3
#define FURTHEST_FINISH 1
#define MARK_STEP 1
#define SHOW_BUILD 0

#define MAX(x, y) (x > y ? x : y)
#define MIN(x, y) (x < y ? x : y)
#define RAND(min, max) (rand() % (max - min) + min)
#define PRINT(...) { printf(__VA_ARGS__); printf("\n"); }
#define PRINTLN(...) { printf(__VA_ARGS__); printf("\n"); }
#define ASSERT(x, ...) if (!(x)) { PRINT(__VA_ARGS__); exit(1); }

#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define BOLD    "\033[1m"
#define RESET   "\033[0m"

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef float    f32;
typedef double   f64;
typedef char     c8;

c8 c;

// ---

typedef enum { STEPPED, EMPTY, PLAYER, FINISH, WALL } Cell;
typedef enum { UP, RIGHT, DOWN, LEFT } Direction;

typedef struct {
  u16 x, y;
} Coord;

c8 ascii[][16] = { (MAGENTA "░" RESET), "░", (MAGENTA "P" RESET), "F", "▓" };

c8 message[64] = "";
Cell maze[100][100];
Coord player;

void scan(const char *fmt, void *arg);
u16 is_player_at(u16 x, u16 y);
void flush_read(c8* c);
void msleep(u16 time);
Cell player_over();

// ---

void print_maze() {
  printf("\033[H\033[2J");

  for (int j = 0; j < MAZE_WIDTH * 2 + 4; j++)
    printf("%s", ascii[WALL]);
  printf("\n");

  for (int i = 0; i < MAZE_HEIGHT; i++) {
    printf("%s%s", ascii[WALL], ascii[WALL]);
    for (int j = 0; j < MAZE_WIDTH; j++) {
      if (i == MAZE_HEIGHT / 2 && j - (MAZE_WIDTH / 2 - strlen(message) / 2) >= 0 && j - (MAZE_WIDTH / 2 - strlen(message) / 2) < strlen(message))
        printf("%c ", message[j - (MAZE_WIDTH / 2 - strlen(message) / 2)]);
      else if (!(maze[i][j] == STEPPED && MARK_STEP) && DARK_MODE && sqrt(pow(j - player.x, 2) + pow(i - player.y, 2)) > TORCH_SIZE)
        printf("  ");
      else
        printf("%s%s", is_player_at(j, i) ? ascii[2] : ascii[maze[i][j]], is_player_at(j, i) ? ascii[2] : ascii[maze[i][j]]);
    }
    printf("%s%s\n", ascii[WALL], ascii[WALL]);
  }

  for (int j = 0; j < MAZE_WIDTH * 2 + 4; j++)
    printf("%s", ascii[WALL]);
  printf("\n");


}

Coord maze_generator(u16 x, u16 y, u16 dis, u16 get_last) {
  static Coord last;
  static u16 last_dis;

  if (get_last) return last;
  maze[y][x] = EMPTY;
  if (dis > last_dis){last_dis = dis;
  last = (Coord) {x,y};}

  Direction possible_dirs[4];
  u16 possible_dirs_len = 0;

  if (SHOW_BUILD) {
  print_maze();
  msleep(10);
}
  
  while (1) {
    possible_dirs_len = 0;
    if (y >= 2 && maze[y - 2][x] == WALL) possible_dirs[possible_dirs_len++] = UP;
    if (x >= 2 && maze[y][x - 2] == WALL) possible_dirs[possible_dirs_len++] = LEFT;
    if (y + 2 < MAZE_HEIGHT && maze[y + 2][x] == WALL) possible_dirs[possible_dirs_len++] = DOWN;
    if (x + 2 < MAZE_WIDTH  && maze[y][x + 2] == WALL) possible_dirs[possible_dirs_len++] = RIGHT;

    if (!possible_dirs_len) return (Coord){0,0};
    Direction chosen = possible_dirs[RAND(0, possible_dirs_len)];

    maze[y + (chosen == DOWN ? 1 : chosen == UP ? -1 : 0)][x + (chosen == RIGHT ? 1 : chosen == LEFT ? -1 : 0)] = EMPTY;
    maze_generator(x + (chosen == RIGHT ? 2 : chosen == LEFT ? -2 : 0), y + (chosen == DOWN ? 2 : chosen == UP ? -2 : 0), dis + 1, 0);
  }
}

void init_maze() {
  message[0] = 0;

  for (u16 i = 0; i < MAZE_HEIGHT; i++)
    for (u16 j = 0; j < MAZE_WIDTH; j++)
      maze[i][j] = WALL;

  player = (Coord) { RAND(2,  MAZE_WIDTH - 2) / 2 * 2, RAND(1, MAZE_HEIGHT - 1) / 2 * 2 };
  maze_generator(player.x, player.y, 0, 0);
  maze[player.y][player.x] = STEPPED;

  Coord finish = maze_generator(0,0, 0, 1);

  maze[finish.y][finish.x] = FINISH;
}

u16 is_player_at(u16 x, u16 y) {
  return player.x == x && player.y == y;
}

Cell player_over() {
  return maze[player.y][player.x];
}

void compute_inputs() {
  Coord tmp = player;
  u16 hit_wall = 0;

  if (c == 'd') player.x++;
  else if (c == 'a') player.x--;
  else if (c == 's') player.y++;
  else if (c == 'w') player.y--;
  else return;

  if (player.x >= MAZE_WIDTH || player.y >= MAZE_HEIGHT || player_over() == WALL) {
    player = tmp;
    hit_wall = 1;
  }
  else if (player_over() == FINISH) {
    strcpy(message, "YOU WIN!!!");
    print_maze();
    msleep(1e3);
    init_maze();
    return;
  }

  maze[player.y][player.x] = STEPPED;

  if (!SKIP_WALK || hit_wall) return;

  u16 possible_dirs_len = 0;
  if (player.y >= 1 && maze[player.y - 1][player.x] <= EMPTY) possible_dirs_len++;
  if (player.x >= 1 && maze[player.y][player.x - 1] <= EMPTY) possible_dirs_len++;
  if (player.y + 1 < MAZE_HEIGHT && maze[player.y + 1][player.x] <= EMPTY) possible_dirs_len++;
  if (player.x + 1 < MAZE_WIDTH  && maze[player.y][player.x + 1] <= EMPTY) possible_dirs_len++;

  if (possible_dirs_len > 2) return;

  print_maze();
  msleep(25);
  compute_inputs();
}

// ---

struct termios t_old;

void restore_terminal() { 
  printf("\e[?25h");
  fflush(stdout);
  tcsetattr(0, TCSANOW, &t_old); 
}

void init_terminal() { 
  tcgetattr(0, &t_old);
  struct termios t = t_old;
  t.c_lflag &= ~(ICANON | ECHO);
  tcsetattr(0, 0, &t);
  printf("\e[?25l");
}

void flush_read(c8* c) {
  int flags = fcntl(0, F_GETFL, 0);
  fcntl(0, F_SETFL, flags | O_NONBLOCK);
  while (read(0, c, 1) > 0);
  fcntl(0, F_SETFL, flags);
  read(0, c, 1);
}

void scan(const char *fmt, void *arg) {
  int flags = fcntl(0, F_GETFL, 0);
  fcntl(0, F_SETFL, flags | O_NONBLOCK);
  while (read(0, &c, 1) > 0);
  fcntl(0, F_SETFL, flags);
  struct termios t;
  tcgetattr(0, &t);
  struct termios t_block = t;
  t_block.c_lflag |= (ICANON | ECHO);
  tcsetattr(0, TCSANOW, &t_block);
  scanf(fmt, arg);
  tcsetattr(0, TCSANOW, &t);
}

void msleep(u16 time) {
  usleep(time * 1e3);
}

// ---

int main() {
  srand(time(0));
  init_terminal();
  atexit(restore_terminal);

  init_maze();
  do { print_maze(); flush_read(&c); compute_inputs(); } while (c != 27);
}
