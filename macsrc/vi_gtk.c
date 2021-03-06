/*
Copyright (C) 1992-1994 Id Software, Inc.
Copyright (C) 2000 Steven Fuller <relnev@atdot.org>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <ctype.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "gtkimagedb.h"

#include "wolfdef.h"

extern jmp_buf ResetJmp;
 
static Byte *gfxbuf;

static GtkWidget *win;
static GtkWidget *main_vbox;
static GtkWidget *menubar;
static GtkItemFactory *item_factory;
static GtkAccelGroup *accel_group;

static GtkWidget *event_box;
static GdkVisual *visual;
static GdkImage *image[2];
static GtkImageDB *image_area;
static gint current;

static GdkColormap *cmap;
static GdkColor game_colors[256], default_colors[256];

static int image_focus = 1;

void Quit();

void RestoreColors()
{
	int i;

	for (i = 0; i < cmap->size; i++)
		gdk_color_change(cmap, &default_colors[i]);
}

void UpdateColors()
{
	int i;

	for (i = 0; i < cmap->size; i++)
		gdk_color_change(cmap, &game_colors[i]);
}

gint delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	g_print("delete_event\n");
	return FALSE;
}

void destroy(GtkWidget *widget, gpointer data)
{
	Quit(NULL);
}

static void menu_file_newgame(GtkWidget *w, gpointer data)
{
	longjmp(ResetJmp, EX_NEWGAME);
}

static void menu_file_savegame(GtkWidget *w, gpointer data)
{
}

static void menu_file_loadgame(GtkWidget *w, gpointer data)
{
}

static void menu_file_quit(GtkWidget *w, gpointer data)
{
	destroy(w, data);
}

static gboolean image_focus_in(GtkWidget *widget, GdkEventButton *event, gpointer data)
{	
	UpdateColors();
	image_focus = 1;
	
	return FALSE;
}

static gboolean image_focus_out(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	RestoreColors();
	image_focus = 0;
	
	return FALSE;
}

static int KeyPressed;

void keyboard_handler(int key, int press);

static gboolean key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	/* g_print("press\n"); */
	keyboard_handler(event->keyval, 1);
	
	return FALSE;
}

static gboolean key_release(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	/* g_print("release\n"); */
	keyboard_handler(event->keyval, 0);
	KeyPressed = 1;
	
	return FALSE;
}

static void draw(GtkWidget *widget, GdkRectangle *area, gpointer user_data)
{
	/* g_print("draw\n"); */
}

static void draw_default(GtkWidget *widget, gpointer user_data)
{
	/* g_print("draw default\n"); */
}

static GtkItemFactoryEntry menu_items[] = {
{ "/_File",		NULL,		NULL,			0,	"<Branch>"	},
{ "/File/New Game",	NULL,		menu_file_newgame,	0,	NULL		},
{ "/File/_",		NULL,		NULL,			0,	"<Separator>"	},
{ "/File/Save Game",	NULL,		menu_file_savegame,	0,	NULL		},
{ "/File/Load Game",	NULL,		menu_file_loadgame,	0,	NULL		},
{ "/File/_",		NULL,		NULL,			0,	"<Separator>"	},
{ "/File/Quit",		"<control>Q",	menu_file_quit,		0,	NULL		}
};

int main(int argc, char *argv[])
{
	int i;
	gboolean success[256];
	GdkColormap *cmapo;
		
	if (argc != 2) {
		fprintf(stderr, "usage: %s <mac wolf3d resource fork>\n", argv[0]);
		exit(EXIT_FAILURE);
	} 
	
	if (InitResources(argv[1])) {
		fprintf(stderr, "could not load %s\n", argv[1]);
		exit(EXIT_FAILURE);
	}
	
	gtk_init(&argc, &argv);


/* Get our visual and colormap, and create the main window with them */
	visual = gdk_visual_get_best_with_both(8, GDK_VISUAL_PSEUDO_COLOR);
	if (visual == NULL) {
		fprintf(stderr, "Unable to get a 8 bpp visual\n");
		exit(EXIT_FAILURE);
	}
	
	cmap = gdk_colormap_new(visual, TRUE);
	
	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	cmapo = gtk_widget_get_colormap(GTK_WIDGET(win));
		
	for (i = 0; i < 256; i++) {
		game_colors[i].pixel = i; 
		default_colors[i].pixel = i;
	}
	
	for (i = 0; i < cmapo->size; i++)
		default_colors[i] = cmapo->colors[i];
		
	gdk_colormap_alloc_colors(cmap, default_colors, cmapo->size, TRUE, TRUE, success);
		
	gtk_widget_set_visual(GTK_WIDGET(win), visual);	
	gtk_widget_set_colormap(GTK_WIDGET(win), cmap);	


/* Disable user resizing */
	gtk_window_set_policy(GTK_WINDOW(win), FALSE, FALSE, TRUE);
	
	gtk_window_set_title(GTK_WINDOW(win), "Wolfenstein 3D");


/* Setup Main Window event signals */
	gtk_signal_connect(GTK_OBJECT(win), "delete_event", GTK_SIGNAL_FUNC(delete_event), NULL);
	gtk_signal_connect(GTK_OBJECT(win), "destroy", GTK_SIGNAL_FUNC(destroy), NULL);
	
	gtk_signal_connect(GTK_OBJECT(win), "key_press_event", GTK_SIGNAL_FUNC(key_press), NULL);
	gtk_signal_connect(GTK_OBJECT(win), "key_release_event", GTK_SIGNAL_FUNC(key_release), NULL);
	
	gtk_widget_set_events(GTK_WIDGET(win), GDK_EXPOSURE_MASK | GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);


/* Create vbox for menu and image */	
	main_vbox = gtk_vbox_new(FALSE, 1);
	
	gtk_container_border_width(GTK_CONTAINER(main_vbox), 1);
	gtk_container_add(GTK_CONTAINER(win), main_vbox);
	gtk_widget_show(main_vbox);


/* Create Menu */	
	accel_group = gtk_accel_group_new();
	item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>", accel_group);
	gtk_item_factory_create_items(item_factory, sizeof(menu_items) / sizeof(menu_items[0]), menu_items, NULL);
	gtk_accel_group_attach(accel_group, GTK_OBJECT(win));
	
	menubar = gtk_item_factory_get_widget(item_factory, "<main>");
		
	gtk_box_pack_start(GTK_BOX(main_vbox), menubar, FALSE, TRUE, 0);
	
	gtk_widget_show(menubar);


/* Create (temp) image and image area */	
	image[0] = gdk_image_new(GDK_IMAGE_FASTEST, visual, 320, 200);
	image[1] = gdk_image_new(GDK_IMAGE_FASTEST, visual, 320, 200);
	image_area = (GtkImageDB *)gtk_imagedb_new(image[0], image[1], NULL);
	
	gtk_signal_connect(GTK_OBJECT(image_area), "draw", GTK_SIGNAL_FUNC(draw), NULL);
	gtk_signal_connect(GTK_OBJECT(image_area), "draw_default", GTK_SIGNAL_FUNC(draw_default), NULL);


/* Create event box for image area */	
	event_box = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(main_vbox), event_box); 	

	gtk_widget_show(event_box);

	gtk_container_add(GTK_CONTAINER(event_box), GTK_WIDGET(image_area));
	gtk_widget_show(GTK_WIDGET(image_area));


/* Setup focus changing for colormaps */	
	gtk_widget_set_events(GTK_WIDGET(event_box), GDK_BUTTON_PRESS_MASK);
	gtk_signal_connect(GTK_OBJECT(event_box), "button_press_event", GTK_SIGNAL_FUNC(image_focus_in), NULL);
	gtk_signal_connect(GTK_OBJECT(menubar), "button_press_event", GTK_SIGNAL_FUNC(image_focus_out), NULL);

	
/* Show Main Window */
	gtk_widget_show(win);	


/* Game-related initialization */	
	InitData();
	
	GameViewSize = 3;
	NewGameWindow(GameViewSize); 

	ClearTheScreen(BLACK);
	BlastScreen();


/* Run the game */
	return WolfMain(argc, argv);
}

void Quit(char *str)
{	
	FreeResources();

#ifdef SLOWDRAW		
	if (gfxbuf)
		free(gfxbuf);
#endif
	
	if (image[0])
		gdk_image_destroy(image[0]);
	if (image[1])
		gdk_image_destroy(image[1]);
		
	if (str && *str) {
		fprintf(stderr, "%s\n", str);
		exit(EXIT_FAILURE);
	}
	
	exit(EXIT_SUCCESS);
}

void SetPalette(Byte *pal)
{
	int i;
		
	for (i = 0; i < 256; i++) {
		game_colors[i].red = pal[i*3+0] << 8;
		game_colors[i].green = pal[i*3+1] << 8;
		game_colors[i].blue = pal[i*3+2] << 8;
		if (image_focus) gdk_color_change(cmap, &game_colors[i]);
	}
}

int VidWidth, VidHeight, ViewHeight;
#define w VidWidth
#define h VidHeight
#define v ViewHeight	

void BlastScreen()
{
#ifdef SLOWDRAW

	memcpy(image->mem, gfxbuf, w * h);
	
	gtk_widget_draw(GTK_WIDGET(image_area), NULL); 	
#else
	return;
	
	current = gtk_imagedb_swap(image_area);
	gtk_imagedb_draw(image_area, NULL);
	
	gfxbuf = image[current]->mem;
	VideoPointer = gfxbuf;
#endif		
}

void BlastScreen2(Rect *BlastRect)
{
#ifdef SLOWDRAW
	GdkRectangle r;

	r.x = BlastRect->left;
	r.y = BlastRect->top;
	r.width = BlastRect->right - BlastRect->left;
	r.height = BlastRect->bottom - BlastRect->top;

#if 1
	memcpy(image->mem, gfxbuf, w * h);
#else
	/*
	int x;
	char *ptrs, *ptrd;
	
	ptrs = gfxbuf + (w * r.y) + r.x;
	ptrd = image->mem;
	ptrd += (image->bpl * r.y) + r.x * image->bpp;
	for (x = 0; x < r.height; x++) {
		memcpy(ptrd, ptrs, r.width * image->bpp);
		ptrs += w;
		ptrd += image->bpl;
	}
	*/
#endif
	
	gtk_widget_draw(GTK_WIDGET(image_area), &r);
#else
	GdkRectangle r;

	r.x = BlastRect->left;
	r.y = BlastRect->top;
	r.width = BlastRect->right - BlastRect->left;
	r.height = BlastRect->bottom - BlastRect->top;

	current = gtk_imagedb_swap(image_area);
	gtk_imagedb_draw(image_area, &r);
	
	gfxbuf = image[current]->mem;
	VideoPointer = gfxbuf;
#endif		
}

Word VidXs[] = {320,512,640,640};       /* Screen sizes to play with */
Word VidYs[] = {200,384,400,480};
Word VidVs[] = {160,320,320,400};
Word VidPics[] = {rFaceShapes,rFace512,rFace640,rFace640};
Word VidSize = -1;

Word NewGameWindow(Word NewVidSize)
{
	LongWord *LongPtr;
	Byte *DestPtr;
	int i;
	
	printf("Called: %d\n", NewVidSize);
	
	if (NewVidSize == VidSize)
		return VidSize;
	
	printf("Setting Size: %d (from %d)\n", NewVidSize, VidSize);
		
	if (NewVidSize < 4) {
		w = VidXs[NewVidSize];
		h = VidYs[NewVidSize];
		v = VidVs[NewVidSize];
	} else {
		fprintf(stderr, "Invalid Vid size: %d\n", NewVidSize);
		exit(EXIT_FAILURE);
	}
	
	if (image[0])
		gdk_image_destroy(image[0]);
	if (image[1])
		gdk_image_destroy(image[1]);

	image[0] = gdk_image_new(GDK_IMAGE_FASTEST, visual, w, h);
	image[1] = gdk_image_new(GDK_IMAGE_FASTEST, visual, w, h);
	gtk_imagedb_set(image_area, image[0], image[1], NULL);
	
/* Main window will autoshrink */
	gtk_widget_set_usize(GTK_WIDGET(image_area), w, h);
	
#ifdef SLOWDRAW	
	if (gfxbuf)
		free(gfxbuf);
	gfxbuf = malloc(w * h);
#else
	current = 0;
	
	gfxbuf = (Byte *)image[current]->mem;
#endif	
	VideoPointer = gfxbuf;
	VideoWidth = w;
	
	InitYTable();
	SetAPalette(rBlackPal);
	ClearTheScreen(BLACK);
	BlastScreen();
	
	LongPtr = (LongWord *) LoadAResource(VidPics[NewVidSize]);

	if (GameShapes)
		FreeSomeMem(GameShapes);
		
	GameShapes = (Byte **)AllocSomeMem(lMSB(LongPtr[0]));
	DLZSS((Byte *)GameShapes, (Byte *)&LongPtr[1], lMSB(LongPtr[0]));
	ReleaseAResource(VidPics[NewVidSize]);
	
	LongPtr = (LongWord *)GameShapes;
	DestPtr = (Byte *)GameShapes;
	for (i = 0; i < ((NewVidSize == 1) ? 57 : 47); i++) 
		GameShapes[i] = DestPtr + lMSB(LongPtr[i]);
	
	VidSize = NewVidSize;
	
	return VidSize;
}

/* Keyboard Hack */
static int RSJ;

static int keys[128];

void FlushKeys()
{
	joystick1 = 0;
	memset(keys, 0, sizeof(keys));
}

struct {
	char code[13];
} CheatCodes[] = {
{ "XUSCNIELPPA" }, /* "XUSCNIELPPA" */
{ "IDDQD" }, /* "IDDQD" */
{ "BURGER" }, /* "BURGER" */
{ "WOWZERS" }, /* "WOWZERS" */
{ "LEDOUX" }, /* "LEDOUX" */
{ "SEGER" }, /* "SEGER" */
{ "MCCALL" }, /* "MCCALL" */
{ "APPLEIIGS" } /* "APPLEIIGS" */
};
const int CheatCount = sizeof(CheatCodes) / sizeof(CheatCodes[0]);
int CheatIndex;

#define SC_CURSORUPLEFT 	1
#define SC_CURSORUP	  	2
#define SC_CURSORUPRIGHT	3
#define SC_CURSORRIGHT		4
#define SC_CURSORDOWNRIGHT	5
#define SC_CURSORDOWN		6
#define SC_CURSORDOWNLEFT	7
#define SC_CURSORLEFT		8
#define SC_CURSORBLOCKLEFT	9
#define SC_CURSORBLOCKRIGHT	10
#define SC_CURSORBLOCKUP	11
#define SC_CURSORBLOCKDOWN	12
#define SC_KEYPADENTER		13
#define SC_ENTER		14
#define SC_SPACE		15
#define SC_LEFTALT		16
#define SC_RIGHTALT		17
#define SC_LEFTCONTROL		18
#define SC_RIGHTCONTROL		19
#define SC_LEFTSHIFT		20
#define SC_RIGHTSHIFT		21
#define SC_B			22

void UpdateKeys(int key, int press)
{

	switch(key) {
		case GDK_KP_Home:
			keys[SC_CURSORUPLEFT] = press;
			break;
		case GDK_KP_Up:
			keys[SC_CURSORUP] = press;
			break;
		case GDK_KP_Page_Up:
			keys[SC_CURSORUPRIGHT] = press;
			break;
		case GDK_KP_Right:
			keys[SC_CURSORRIGHT] = press;
			break;
		case GDK_KP_Page_Down:
			keys[SC_CURSORDOWNRIGHT] = press;
			break;
		case GDK_KP_Down:
			keys[SC_CURSORDOWN] = press;
			break;
		case GDK_KP_End:
			keys[SC_CURSORDOWNLEFT] = press;
			break;
		case GDK_KP_Left:
			keys[SC_CURSORLEFT] = press;
			break;
			
		case GDK_Up:
			keys[SC_CURSORBLOCKUP] = press;
			break;
		case GDK_Down:
			keys[SC_CURSORBLOCKDOWN] = press;
			break;
		case GDK_Left:
			keys[SC_CURSORBLOCKLEFT] = press;
			break;
		case GDK_Right:
			keys[SC_CURSORBLOCKRIGHT] = press;
			break;
			
		case GDK_KP_Enter:
			keys[SC_KEYPADENTER] = press;
			break;
		case GDK_Return:
			keys[SC_ENTER] = press;
			break;
		case GDK_space:
			keys[SC_SPACE] = press;
			break;
		
		case GDK_Alt_L:
			keys[SC_LEFTALT] = press;
			break;
		case GDK_Alt_R:
			keys[SC_RIGHTALT] = press;
			break;
		
		case GDK_Control_L:
			keys[SC_LEFTCONTROL] = press;
			break;
		case GDK_Control_R:
			keys[SC_RIGHTCONTROL] = press;
			break;
		
		case GDK_Shift_L:
			keys[SC_LEFTSHIFT] = press;
			break;
		case GDK_Shift_R:
			keys[SC_RIGHTSHIFT] = press;
			break;
				
		case GDK_b:
			keys[SC_B] = press;
			break;
	}
}

void keyboard_handler(int keycode, int press)
{
	int i;
	
	UpdateKeys(keycode, press);
	
	if (RSJ) {		
		if (press == 0) {
			for (i = 0; i < CheatCount; i++) {
				char *key = gdk_keyval_name(keycode);
				if (key == NULL)
					break;
				if (strlen(key) != 1)
					break;
				if (CheatCodes[i].code[CheatIndex] == toupper(key[0])) {
					CheatIndex++;
					if (CheatCodes[i].code[CheatIndex] == 0) {
						PlaySound(SND_BONUS);
						switch (i) {
						case 0:
						case 4:
							GiveKey(0);
							GiveKey(1);
							gamestate.godmode = TRUE;
							break;
						case 1:
							gamestate.godmode^=TRUE;
							break;
						case 2:
							gamestate.machinegun = TRUE;
							gamestate.chaingun = TRUE;
							gamestate.flamethrower = TRUE;
							gamestate.missile = TRUE;
							GiveAmmo(gamestate.maxammo);
							GiveGas(99);
							GiveMissile(99);
							break;
						case 3:
							gamestate.maxammo = 999;
							GiveAmmo(999);
							break;
						case 5:
							GiveKey(0);
							GiveKey(1);
							break;
						case 6:
							playstate=EX_WARPED;
							nextmap = gamestate.mapon+1;
							if (MapListPtr->MaxMap<=nextmap) 
								nextmap = 0;
							break;
						case 7:
							ShowPush ^= TRUE;
							break;
						}
						CheatIndex = 0;
					}
					break;
				} 
			}	
			if (i == CheatCount) 
				CheatIndex = 0;
		}
				
		joystick1 = 0;
		
		if (press == 0) {
			switch(keycode) {
			case GDK_1:
				gamestate.pendingweapon = WP_KNIFE;
				break;
			case GDK_2:
				if (gamestate.ammo) {
					gamestate.pendingweapon = WP_PISTOL;
				}	
				break;
			case GDK_3:
				if (gamestate.ammo && gamestate.machinegun) {
					gamestate.pendingweapon = WP_MACHINEGUN;
				}
				break;
			case GDK_4:
				if (gamestate.ammo && gamestate.chaingun) {
					gamestate.pendingweapon = WP_CHAINGUN;
				}
				break;
			case GDK_5:
				if (gamestate.gas && gamestate.flamethrower) {
					gamestate.pendingweapon = WP_FLAMETHROWER;
				}
				break;
			case GDK_6:
				if (gamestate.missiles && gamestate.missile) {
					gamestate.pendingweapon = WP_MISSILE;
				}
				break;
			case GDK_period:
			case GDK_slash:
				joystick1 = JOYPAD_START;
				break;
 			}
		}
		
		if (keys[SC_CURSORUPLEFT])
			joystick1 |= (JOYPAD_UP|JOYPAD_LFT);
		if (keys[SC_CURSORUP])
			joystick1 |= JOYPAD_UP;
		if (keys[SC_CURSORUPRIGHT])
			joystick1 |= (JOYPAD_UP|JOYPAD_RGT);
		if (keys[SC_CURSORRIGHT])
			joystick1 |= JOYPAD_RGT;
		if (keys[SC_CURSORDOWNRIGHT])
			joystick1 |= (JOYPAD_DN|JOYPAD_RGT);
		if (keys[SC_CURSORDOWN])
			joystick1 |= JOYPAD_DN;
		if (keys[SC_CURSORDOWNLEFT])
			joystick1 |= (JOYPAD_DN|JOYPAD_LFT);
		if (keys[SC_CURSORLEFT])
			joystick1 |= JOYPAD_LFT;	
		
		if (keys[SC_CURSORBLOCKLEFT]) 
			joystick1 |= JOYPAD_LFT;
		if (keys[SC_CURSORBLOCKRIGHT])
			joystick1 |= JOYPAD_RGT;
		if (keys[SC_CURSORBLOCKUP])
			joystick1 |= JOYPAD_UP;
		if (keys[SC_CURSORBLOCKDOWN])
			joystick1 |= JOYPAD_DN;
		
		if (keys[SC_KEYPADENTER])
			joystick1 |= JOYPAD_A;	
		if (keys[SC_ENTER])
			joystick1 |= JOYPAD_A;
		if (keys[SC_SPACE])
			joystick1 |= JOYPAD_A;
		
		if (keys[SC_LEFTALT]) 
			joystick1 |= JOYPAD_TR;
		if (keys[SC_RIGHTALT])
			joystick1 |= JOYPAD_TR;
			
		if (keys[SC_LEFTCONTROL])
			joystick1 |= JOYPAD_B;
		if (keys[SC_RIGHTCONTROL])
			joystick1 |= JOYPAD_B;
		
		if (keys[SC_LEFTSHIFT])
			joystick1 |= (JOYPAD_X|JOYPAD_Y);
		if (keys[SC_RIGHTSHIFT])
			joystick1 |= (JOYPAD_X|JOYPAD_Y);
	}
	
	if ((joystick1 & (JOYPAD_LFT|JOYPAD_RGT)) == (JOYPAD_LFT|JOYPAD_RGT))
		joystick1 &= ~(JOYPAD_LFT|JOYPAD_RGT);
	if ((joystick1 & (JOYPAD_UP|JOYPAD_DN)) == (JOYPAD_UP|JOYPAD_DN))
		joystick1 &= ~(JOYPAD_UP|JOYPAD_DN);
		
	if (joystick1 & JOYPAD_TR) {
		if (joystick1 & JOYPAD_LFT) {
			joystick1 = (joystick1 & ~(JOYPAD_TR|JOYPAD_LFT)) | JOYPAD_TL;
		} else if (joystick1 & JOYPAD_RGT) {
			joystick1 = joystick1 & ~JOYPAD_RGT;
		} else {
			joystick1 &= ~JOYPAD_TR;
		}
	}
							
}

int HandleEvents()
{
	KeyPressed = 0;
	
	while (gtk_events_pending())
		gtk_main_iteration();
		
	return KeyPressed;
}

void ReadSystemJoystick()
{
	RSJ = 1;
	HandleEvents();
}

int DoEvents()
{
	RSJ = 0;
	if (HandleEvents()) {
		if (keys[SC_B]) { /* Special */
			return 'B';
		}
		return 1;
	}
	return 0;
}

Word ChooseGameDiff()
{
/* 0 = easy, 1 = normal, 2 = hard, 3 = death incarnate */
	difficulty = 1;
	SetAPalette(rGamePal);

	return 1;
}
