#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

static const int ROW_PINS[8]  = {23, 22, 21, 19, 18,  5, 17, 16};
static const int COLR_PINS[8] = {32, 33, 25, 26, 27, 14, 12, 13};
#define COL_GRN 2

#define BTN_LEFT  36
#define BTN_ROT   4
#define BTN_RIGHT 34

static uint8_t fb_red[8];
static uint8_t fb_green[8];
static uint8_t board[8];
static int  cur_piece;
static int  cur_rot;
static int  cur_x;
static int  cur_y;
static int  game_over;

#define FALL_INTERVAL_US  800000

static const uint8_t PIECES[7][4][4] = {
    {{0x00,0xF0,0x00,0x00},{0x20,0x20,0x20,0x20},{0x00,0xF0,0x00,0x00},{0x20,0x20,0x20,0x20}},
    {{0x60,0x60,0x00,0x00},{0x60,0x60,0x00,0x00},{0x60,0x60,0x00,0x00},{0x60,0x60,0x00,0x00}},
    {{0x40,0xE0,0x00,0x00},{0x40,0x60,0x40,0x00},{0x00,0xE0,0x40,0x00},{0x40,0xC0,0x40,0x00}},
    {{0x60,0xC0,0x00,0x00},{0x40,0x60,0x20,0x00},{0x60,0xC0,0x00,0x00},{0x40,0x60,0x20,0x00}},
    {{0xC0,0x60,0x00,0x00},{0x20,0x60,0x40,0x00},{0xC0,0x60,0x00,0x00},{0x20,0x60,0x40,0x00}},
    {{0x80,0xE0,0x00,0x00},{0x60,0x40,0x40,0x00},{0x00,0xE0,0x20,0x00},{0x40,0x40,0xC0,0x00}},
    {{0x20,0xE0,0x00,0x00},{0x40,0x40,0x60,0x00},{0x00,0xE0,0x80,0x00},{0xC0,0x40,0x40,0x00}}
};

static uint32_t rng_state = 1;
static uint32_t rand_next(void){ rng_state=rng_state*1664525u+1013904223u; return rng_state; }
static int rand_range(int max){ return (int)(rand_next()%(uint32_t)max); }

static void gpio_init_all(void){
    gpio_config_t cfg;
    cfg.intr_type=GPIO_INTR_DISABLE; cfg.mode=GPIO_MODE_OUTPUT;
    cfg.pull_down_en=GPIO_PULLDOWN_DISABLE; cfg.pull_up_en=GPIO_PULLUP_DISABLE;
    for(int i=0;i<8;i++){ cfg.pin_bit_mask=(1ULL<<ROW_PINS[i]); gpio_config(&cfg); gpio_set_level(ROW_PINS[i],1); }
    for(int i=0;i<8;i++){ cfg.pin_bit_mask=(1ULL<<COLR_PINS[i]); gpio_config(&cfg); gpio_set_level(COLR_PINS[i],1); }
    cfg.pin_bit_mask=(1ULL<<COL_GRN); gpio_config(&cfg); gpio_set_level(COL_GRN,1);
    gpio_config_t btn; btn.intr_type=GPIO_INTR_DISABLE; btn.mode=GPIO_MODE_INPUT;
    btn.pull_up_en=GPIO_PULLUP_ENABLE; btn.pull_down_en=GPIO_PULLDOWN_DISABLE;
    btn.pin_bit_mask=(1ULL<<BTN_LEFT);  gpio_config(&btn);
    btn.pin_bit_mask=(1ULL<<BTN_RIGHT); gpio_config(&btn);
    btn.pin_bit_mask=(1ULL<<BTN_ROT); gpio_config(&btn);
}

static void fb_clear(void){ memset(fb_red,0,8); memset(fb_green,0,8); }

static void fb_set_red(int col,int row){
    if(col<0||col>7||row<0||row>7) return;
    fb_red[row]|=(uint8_t)(1u<<(7-col));
}

static void mostrar_frame(void){
    for(int fila=0;fila<8;fila++){
        for(int i=0;i<8;i++){ gpio_set_level(ROW_PINS[i],1); gpio_set_level(COLR_PINS[i],1); }
        gpio_set_level(COL_GRN,1);
        for(int col=0;col<8;col++){
            int bit_r=(fb_red[fila]>>(7-col))&1;
            gpio_set_level(COLR_PINS[col], bit_r?0:1);
        }
        if(fb_green[fila]) gpio_set_level(COL_GRN,0);
        gpio_set_level(ROW_PINS[fila],0);
        esp_rom_delay_us(2000);
    }
}

typedef enum { BTN_NONE, BTN_SHORT} BtnEvent;

static BtnEvent leer_boton_izq(void){
    static int estado=1; static int64_t t_bajo=0;
    int cur=gpio_get_level(BTN_LEFT);
    if(estado==1&&cur==0){ t_bajo=esp_timer_get_time(); estado=0; }
    else if(estado==0&&cur==1){ estado=1; if(esp_timer_get_time()-t_bajo>50000) return BTN_SHORT; }
    return BTN_NONE;
}

static BtnEvent leer_boton_der(void){
    static int estado=1; static int64_t t_bajo=0;
    int cur=gpio_get_level(BTN_RIGHT);
    if(estado==1&&cur==0){ t_bajo=esp_timer_get_time(); estado=0; }
    else if(estado==0&&cur==1){ estado=1; if(esp_timer_get_time()-t_bajo>50000) return BTN_SHORT; }
    return BTN_NONE;
}

static BtnEvent leer_boton_rot(void){
    static int estado=1; static int64_t t_bajo=0;
    int cur=gpio_get_level(BTN_ROT);
    if(estado==1&&cur==0){ t_bajo=esp_timer_get_time(); estado=0; }
    else if(estado==0&&cur==1){ estado=1; if(esp_timer_get_time()-t_bajo>50000) return BTN_SHORT; }
    return BTN_NONE;
}

static int check_collision(int piece,int rot,int x,int y){
    for(int r=0;r<4;r++){
        uint8_t rb=PIECES[piece][rot][r];
        for(int c=0;c<4;c++){
            if(rb&(0x80>>c)){
                int px=x+c, py=y+r;
                if(px<0||px>=8||py>=8) return 1;
                if(py>=0&&(board[py]&(0x80>>px))) return 1;
            }
        }
    }
    return 0;
}

static int try_move(int x,int y,int rot){
    if(!check_collision(cur_piece,rot,x,y)){ cur_x=x; cur_y=y; cur_rot=rot; return 1; }
    return 0;
}

static void rotar(void){
    int nr=(cur_rot+1)%4;
    if     (!check_collision(cur_piece,nr,cur_x,  cur_y)){ cur_rot=nr; }
    else if(!check_collision(cur_piece,nr,cur_x-1,cur_y)){ cur_x--; cur_rot=nr; }
    else if(!check_collision(cur_piece,nr,cur_x+1,cur_y)){ cur_x++; cur_rot=nr; }
}

static void place_piece(void){
    for(int r=0;r<4;r++){
        uint8_t rb=PIECES[cur_piece][cur_rot][r];
        for(int c=0;c<4;c++){
            if(rb&(0x80>>c)){
                int px=cur_x+c, py=cur_y+r;
                if(px>=0&&px<8&&py>=0&&py<8) board[py]|=(0x80>>px);
            }
        }
    }
}

static void clear_lines(void){
    int full[8]={0}, count=0;
    for(int r=0;r<8;r++) if(board[r]==0xFF){ full[r]=1; count++; }
    if(count==0) return;

    for(int b=0;b<3;b++){
        for(int r=0;r<8;r++) if(full[r]){ fb_red[r]=0x00; fb_green[r]=0xFF; }
        for(int f=0;f<18;f++) mostrar_frame();
        for(int r=0;r<8;r++) if(full[r]) fb_green[r]=0x00;
        for(int f=0;f<9;f++)  mostrar_frame();
    }

    for(int r=7;r>=0;r--) if(full[r]){
        for(int rr=r;rr>0;rr--) board[rr]=board[rr-1];
        board[0]=0;
    }
}

static void spawn_piece(void){
    cur_piece=rand_range(7); cur_rot=0; cur_x=2; cur_y=0;
    if(check_collision(cur_piece,cur_rot,cur_x,cur_y)) game_over=1;
}

static void iniciar_juego(void){
    rng_state=(uint32_t)esp_timer_get_time();
    memset(board,0,8); game_over=0; spawn_piece();
}

static void construir_framebuffer(void){
    fb_clear();
    for(int r=0;r<8;r++) fb_red[r]=board[r];
    for(int r=0;r<4;r++){
        uint8_t rb=PIECES[cur_piece][cur_rot][r];
        for(int c=0;c<4;c++) if(rb&(0x80>>c)) fb_set_red(cur_x+c,cur_y+r);
    }
}

static void animacion_muerte(void){
    for(int p=0;p<5;p++){
        memset(fb_red,0xFF,8); memset(fb_green,0x00,8);
        for(int f=0;f<18;f++) mostrar_frame();
        fb_clear();
        for(int f=0;f<18;f++) mostrar_frame();
    }
}

void app_main(void){
    gpio_init_all();
    iniciar_juego();
    int64_t ultimo_fall=esp_timer_get_time();

    while(1){
        if(game_over){ animacion_muerte(); iniciar_juego(); ultimo_fall=esp_timer_get_time(); continue; }

        BtnEvent ev_izq=leer_boton_izq();
        BtnEvent ev_der=leer_boton_der();
        BtnEvent ev_rot=leer_boton_rot();

        if(ev_izq==BTN_SHORT) try_move(cur_x-1,cur_y,cur_rot);
        if(ev_der==BTN_SHORT) try_move(cur_x+1,cur_y,cur_rot);
        if(ev_rot==BTN_SHORT) rotar();

        int64_t ahora=esp_timer_get_time();
        if(ahora-ultimo_fall>=FALL_INTERVAL_US){
            ultimo_fall=ahora;
            if(!try_move(cur_x,cur_y+1,cur_rot)){ place_piece(); clear_lines(); spawn_piece(); }
        }

        construir_framebuffer();
        mostrar_frame();
    }
}