/*
This file present all abstraction needed to port LiteNES.
  (The current working implementation uses allegro library.)

To port this project, replace the following functions by your own:
1) nes_hal_init()
    Do essential initialization work, including starting a FPS HZ timer.

2) nes_set_bg_color(c)
    Set the back ground color to be the NES internal color code c.

3) nes_flush_buf(*buf)
    Flush the entire pixel buf's data to frame buffer.

4) nes_flip_display()
    Fill the screen with previously set background color, and
    display all contents in the frame buffer.

5) wait_for_frame()
    Implement it to make the following code is executed FPS times a second:
        while (1) {
            wait_for_frame();
            do_something();
        }

6) int nes_key_state(int b)
    Query button b's state (1 to be pressed, otherwise 0).
    The correspondence of b and the buttons:
      0 - Power
      1 - A
      2 - B
      3 - SELECT
      4 - START
      5 - UP
      6 - DOWN
      7 - LEFT
      8 - RIGHT
*/
#include "hal.h"
#include "fce.h"
#include "common.h"
#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>

static int fb_fd;
static unsigned char *fb_mem;
static int px_width;
static int line_width;
static int screen_width;
static struct fb_var_screeninfo var;

ALLEGRO_EVENT_QUEUE *fce_event_queue;
ALLEGRO_TIMER *fce_timer = NULL;


static int lcd_fb_display_px(int color, int x, int y)
{
    unsigned char  *pen8;
    unsigned short *pen16;

    unsigned char r,g,b;

    pen8 = (unsigned char *)(fb_mem + y*line_width + x*px_width);
    pen16 = (unsigned short *)pen8;

    r = (color>>16) & 0xff;
    g = (color>>8) & 0xff;
    b = (color>>0) & 0xff;
    *pen16 = (b>>3)<<11 | (g>>2)<<5 | (r>>3);
    //*pen16 = (r>>3)<<11 | (g>>2)<<5 | (b>>3);

    return 0;
}

static int pal2color(pal pal)
{
    int color = 0;
    color = pal.r << 16 | pal.g <<8 | pal.b;
    return color;
}

static int lcd_fb_init()
{
    fb_fd = open("/dev/fb0", O_RDWR);
    if(-1 == fb_fd){
        printf("cat't open /dev/fb0 \n");
        return -1;
    }
    if(-1 == ioctl(fb_fd, FBIOGET_VSCREENINFO, &var)){
        close(fb_fd);
        printf("cat't ioctl /dev/fb0 \n");
        return -1;
    }
    px_width = var.bits_per_pixel / 8;
    line_width = var.xres * px_width;
    screen_width = var.yres * line_width;

    fb_mem = (unsigned char *)mmap(NULL, screen_width, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if(fb_mem == (void *)-1){
        close(fb_fd);
        printf("cat't mmap /dev/fb0 \n");
        return -1;
    }
    memset(fb_mem, 0 , screen_width);
    return 0;
}

void wait_for_frame()
{
    while (1){   
        ALLEGRO_EVENT event;
        al_wait_for_event(fce_event_queue, &event);
        if (event.type == ALLEGRO_EVENT_TIMER) break;
    }
}

/* Set background color. RGB value of c is defined in fce.h */
void nes_set_bg_color(int c)
{
    int i,j;

    for(i=0; i<SCREEN_WIDTH; i++){
        for(j=0; j<SCREEN_HEIGHT; j++){
            lcd_fb_display_px(pal2color(palette[c]), i, j);
        }
    }

}

/* Flush the pixel buffer */
void nes_flush_buf(PixelBuf *buf)
{
    Pixel *p;
    int i,x,y,color;
    for (i = 0; i < buf->size; i++){
        p = &buf->buf[i];
        x = p->x;
        y = p->y;

        color = pal2color(palette[p->c]);
        lcd_fb_display_px(color, x, y);
        lcd_fb_display_px(color, x+1, y);
        lcd_fb_display_px(color, x, y+1);
        lcd_fb_display_px(color, x+1, y+1);
    }
}

/* Initialization:
   (1) start a 1/FPS Hz timer.
   (2) register fce_timer handle on each timer event */
void nes_hal_init()
{
    al_init();
    al_init_primitives_addon();
    al_install_keyboard();
    al_install_joystick();
    al_reconfigure_joysticks();

    fce_timer = al_create_timer(1.0 / FPS);
    fce_event_queue = al_create_event_queue();
    al_register_event_source(fce_event_queue, al_get_timer_event_source(fce_timer));
    al_start_timer(fce_timer);

    if(-1 == lcd_fb_init()){
        printf("lcd fb init error \n");
        return ;
    }
}

/* Update screen at FPS rate by allegro's drawing function.
   Timer ensures this function is called FPS times a second. */
void nes_flip_display()
{
}

/* Query a button's state.
   Returns 1 if button #b is pressed. */
int nes_key_state(int b)
{
    ALLEGRO_KEYBOARD_STATE state;
    al_get_keyboard_state(&state);
    switch (b) 
    {   
        case 0: // On / Off 
            return 1;
        case 1: // A
            return al_key_down(&state, ALLEGRO_KEY_K);
        case 2: // B
            return al_key_down(&state, ALLEGRO_KEY_J);
        case 3: // SELECT
            return al_key_down(&state, ALLEGRO_KEY_U);
        case 4: // START
            return al_key_down(&state, ALLEGRO_KEY_I);
        case 5: // UP
            return al_key_down(&state, ALLEGRO_KEY_W);
        case 6: // DOWN
            return al_key_down(&state, ALLEGRO_KEY_S);
        case 7: // LEFT
            return al_key_down(&state, ALLEGRO_KEY_A);
        case 8: // RIGHT
            return al_key_down(&state, ALLEGRO_KEY_D);
        default:
            return 1;
    }   
}

int nes_joy_state(int b)
{
    ALLEGRO_JOYSTICK_STATE state;
    ALLEGRO_JOYSTICK *joy;
    joy=al_get_joystick(al_get_num_joysticks()-1);
    al_get_joystick_state(joy, &state);

    switch (b) {   
        case 0: // On / Off 
            return 1;
        case 1: // A
            return !!state.button[1];
        case 2: // B
            return !!state.button[2];
        case 3: // SELECT
            return !!state.button[8];
        case 4: // START
            return !!state.button[9];
        case 5: // UP
            return state.stick[0].axis[1] == -1;
        case 6: // DOWN
            return state.stick[0].axis[1] == 1;
        case 7: // LEFT
            return state.stick[0].axis[0] == -1;
        case 8: // RIGHT
            return state.stick[0].axis[0] == 1;
        default:
            return 1;
    }   

    return 0;
}

