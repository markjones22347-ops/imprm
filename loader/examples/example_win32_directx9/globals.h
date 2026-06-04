#pragma once

/*icons:
A - target (person)
B - target variant 2
C - eye
D - half filled( or half empty :) ) circle
E - circle
F - flag
G - heart
H - home
I - linux
J - magic stick
K - 'play'
L - exit
M - exit variant 2
N - wifi
O - half filled shield
P - log out
Q - loading
R - water drop
S - umbrella
T - caution
U - settings
*/

#define cheat_icon_symbol "G"
#define cheat_name "half-life"
#define cheat_domain ".dev"

/*floats*/
extern float accent_colour[4];

/*ints*/
int old_tab = 0;
static int tab = 0;

/*chars*/
static char username[64];
static char password[64];
static char key[64];
static char hwid[128];
static char auth_error[256];

/*options*/
extern float content_animation;
extern float dpi_scale;

static bool update_on_f5 = true;
static bool remember_me = false;

static int game = 0;
const char* games_list[4] = { "Counter-Strike: GO", "Dying Light", "Apex Legends", "Warface" };
