#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "pico/bootrom.h"
#include "inc/ssd1306.h"   // Biblioteca para o display SSD1306
#include "inc/font.h"      // Fontes para caracteres (8x8)

// ---------------------- DEFINIÇÕES DE HARDWARE ---------------------------
#define LARGURA_TELA 128
#define ALTURA_TELA 64

#define PORTA_I2C i2c1
#define SDA_I2C 14
#define SCL_I2C 15
#define OLED_ENDERECO 0x3C

#define LED_VERDE     11
#define LED_AZUL      12
#define LED_VERMELHO  13

#define BOTAO_A       5
#define BOTAO_B       6
#define BOTAO_JOYSTICK 22

#define JOYSTICK_X_ADC 26
#define JOYSTICK_Y_ADC 27
#define ADC_MAX 4095

#define BUZZER 10

static volatile uint32_t tempo_ultimo_botaoA = 0;
static volatile uint32_t tempo_ultimo_botaoB = 0;
static volatile uint32_t tempo_ultimo_js     = 0;
const uint32_t atraso_debounce_us = 200000; // 200 ms

volatile bool flag_botaoA = false;
volatile bool flag_botaoB = false;
volatile bool flag_botaoJS = false;
volatile bool edicao_cancelada = false;
static int pos_indicador_anterior = -1;

// ---------------------- VARIÁVEL GLOBAL DO DISPLAY ---------------------------
ssd1306_t display;

// ---------------------- ESTRUTURAS E TIPOS ---------------------------
typedef struct {
    int horas;
    int minutos;
} Horario;

typedef enum {
    ESTADO_BEM_VINDO,
    ESTADO_MENU_PRINCIPAL,
    ESTADO_EDITAR_HORA_ATUAL,
    ESTADO_EDITAR_ALARME,
    ESTADO_MENU_POMODORO
} EstadoAplicacao;

typedef enum {
    JOY_NENHUM,
    JOY_CIMA,
    JOY_BAIXO,
    JOY_ESQUERDA,
    JOY_DIREITA
} DirecaoJoystick;

// ---------------------- VARIÁVEIS GLOBAIS ---------------------------
EstadoAplicacao estado_atual = ESTADO_BEM_VINDO;
Horario horario_atual = {0, 0};
Horario horario_alarme = {0, 0};

int selecao_menu_principal = 0;
int selecao_pomodoro = 0;
const char* opcoes_pomodoro[] = {"25/5", "30/15", "40/20", "60/30"};

// ---------------------- FUNÇÕES DE INTERUPÇÃO ---------------------------
void trata_interrupcao_gpio(uint gpio, uint32_t eventos) {
    uint32_t agora = time_us_32();
    if (gpio == BOTAO_A && (agora - tempo_ultimo_botaoA > atraso_debounce_us)) {
         flag_botaoA = true;
         tempo_ultimo_botaoA = agora;
    }
    if (gpio == BOTAO_B && (agora - tempo_ultimo_botaoB > atraso_debounce_us)) {
         flag_botaoB = true;
         tempo_ultimo_botaoB = agora;
    }
    if (gpio == BOTAO_JOYSTICK && (agora - tempo_ultimo_js > atraso_debounce_us)) {
         flag_botaoJS = true;
         tempo_ultimo_js = agora;
    }
}

// ---------------------- FUNÇÃO DE VERIFICAÇÃO DO BOOTSEL ---------------------------
void verifica_bootsel() {
    if (flag_botaoJS) {
        flag_botaoJS = false;
        ssd1306_fill(&display, false);
        ssd1306_send_data(&display);
        reset_usb_boot(0, 0);
    }
}

// ---------------------- FUNÇÃO AUXILIAR: PREENCHE UM RETÂNGULO ---------------------------
void ssd1306_fill_rect(ssd1306_t *disp, int x, int y, int w, int h, bool color) {
    for (int i = y; i < y + h; i++) {
        for (int j = x; j < x + w; j++) {
            ssd1306_pixel(disp, j, i, color);
        }
    }
}

// Atualiza somente a área onde um texto é exibido
void atualiza_area_texto(const char* texto, int x, int y, int w, int h) {
    ssd1306_fill_rect(&display, x, y, w, h, false);
    ssd1306_draw_string(&display, texto, x, y);
    ssd1306_send_data(&display);
}

// ---------------------- CONFIGURAÇÃO DOS COMPONENTES ---------------------------
void configura_componentes() {
    gpio_init(LED_VERMELHO);
    gpio_set_dir(LED_VERMELHO, GPIO_OUT);
    gpio_init(LED_VERDE);
    gpio_set_dir(LED_VERDE, GPIO_OUT);
    gpio_init(LED_AZUL);
    gpio_set_dir(LED_AZUL, GPIO_OUT);

    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);
    
    gpio_init(BOTAO_JOYSTICK);
    gpio_set_dir(BOTAO_JOYSTICK, GPIO_IN);
    gpio_pull_up(BOTAO_JOYSTICK);

    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, trata_interrupcao_gpio);
    gpio_set_irq_enabled(BOTAO_B, GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(BOTAO_JOYSTICK, GPIO_IRQ_EDGE_FALL, true);

    adc_init();
    adc_gpio_init(JOYSTICK_X_ADC);
    adc_gpio_init(JOYSTICK_Y_ADC);

    gpio_init(BUZZER);
    gpio_set_dir(BUZZER, GPIO_OUT);
    gpio_put(BUZZER, 0);
}

void configura_pwm_no_pino(uint pino) {
    gpio_set_function(pino, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(pino);
    pwm_set_wrap(slice, 1000);
    pwm_set_chan_level(slice, pwm_gpio_to_channel(pino), 0);
    pwm_set_enabled(slice, true);
}

void define_brilho_led(uint pino, float brilho) {
    uint slice = pwm_gpio_to_slice_num(pino);
    uint canal = pwm_gpio_to_channel(pino);
    uint nivel = (uint)(brilho * 1000);
    pwm_set_chan_level(slice, canal, nivel);
    // Garante que o canal esteja habilitado
    pwm_set_enabled(slice, true);
}

void play_buzzer_tone() {
    gpio_init(BUZZER);
    gpio_set_dir(BUZZER, GPIO_OUT);
    gpio_set_function(BUZZER, GPIO_FUNC_PWM);
    
    uint slice = pwm_gpio_to_slice_num(BUZZER);
    pwm_set_wrap(slice, 1000); 
    pwm_set_chan_level(slice, pwm_gpio_to_channel(BUZZER), 1000); 
    pwm_set_enabled(slice, true);
    pwm_set_chan_level(slice, pwm_gpio_to_channel(BUZZER), 2000); 
    sleep_ms(500); // Som por 500 ms
    pwm_set_chan_level(slice, pwm_gpio_to_channel(BUZZER), 0); 
    sleep_ms(500); // Silêncio por 500 ms
}

void stop_buzzer_tone() {
    uint slice = pwm_gpio_to_slice_num(BUZZER);
    pwm_set_enabled(slice, false);
    gpio_put(BUZZER, 0);
    gpio_set_function(BUZZER, GPIO_FUNC_SIO);
}

// ---------------------- FUNÇÕES DE LEITURA DO JOYSTICK ---------------------------
DirecaoJoystick le_joystick() {
    const int limiar = 1000;
    adc_select_input(0);
    uint16_t valor_x = adc_read();
    adc_select_input(1);
    uint16_t valor_y = adc_read();

    if (valor_y > 2048 + limiar) return JOY_CIMA;
    if (valor_y < 2048 - limiar) return JOY_BAIXO;
    if (valor_x > 2048 + limiar) return JOY_DIREITA;
    if (valor_x < 2048 - limiar) return JOY_ESQUERDA;
    return JOY_NENHUM;
}

// ---------------------- FUNÇÕES DE EXIBIÇÃO NO DISPLAY ---------------------------
void desenha_moldura() {
    ssd1306_rect(&display, 0, 0, LARGURA_TELA, ALTURA_TELA, true, false);
}

void mostra_tela(const char* titulo) {
    ssd1306_fill(&display, false);
    desenha_moldura();
    ssd1306_draw_string(&display, titulo, 10, 5);
    ssd1306_send_data(&display);
}

// ---------------------- MENUS ---------------------------
void mostrar_boas_vindas() {
    mostra_tela("Bem vindo ao");
    ssd1306_draw_string(&display, "Study Buddy", 10, 20);
    ssd1306_draw_string(&display, "clique A para", 10, 40);
    ssd1306_draw_string(&display, "continuar", 10, 50);
    ssd1306_send_data(&display);
}

void desenha_menu_principal() {
    ssd1306_fill(&display, false);
    desenha_moldura();
    ssd1306_draw_string(&display, "Alarme", 22, 10);
    ssd1306_draw_string(&display, "de estudos", 22, 20);
    ssd1306_draw_string(&display, "Metodo", 22, 40);
    ssd1306_draw_string(&display, "pomodoro", 20, 50);
    ssd1306_send_data(&display);
}

void desenha_seta_menu_principal(int selecao) {
    int pos_nova = (selecao == 0) ? 10 : 40;
    if (pos_indicador_anterior != -1 && pos_indicador_anterior != pos_nova) {
         ssd1306_fill_rect(&display, 10, pos_indicador_anterior, 10, 8, false);
    }
    ssd1306_draw_string(&display, ":", 10, pos_nova);
    pos_indicador_anterior = pos_nova;
    ssd1306_send_data(&display);
}

void desenha_menu_pomodoro() {
    ssd1306_fill(&display, false);
    desenha_moldura();
    ssd1306_draw_string(&display, "Metodo pomodoro", 6, 5);
    for (int i = 0; i < 4; i++) {
         ssd1306_draw_string(&display, opcoes_pomodoro[i], 20, 20 + i * 10);
    }
    ssd1306_send_data(&display);
}

void desenha_seta_menu_pomodoro(int selecao) {
    ssd1306_fill_rect(&display, 10, 20, 10, 40, false);
    ssd1306_draw_string(&display, ":", 10, 20 + selecao * 10);
    ssd1306_send_data(&display);
}

Horario editar_horario(const char* titulo, bool ignore) {
    Horario horario = {0, 0};
    int indice_edicao = 0;
    edicao_cancelada = false;
    bool editando = true;
    int offsets[4] = {0, 8, 24, 32};
    int pos_x = (LARGURA_TELA - 40) / 2;  // largura de "HH:MM" = 40px
    int pos_y = 30;

    ssd1306_fill(&display, false);
    desenha_moldura();
    ssd1306_draw_string(&display, titulo, 10, 5);
    ssd1306_send_data(&display);

    while(editando) {
        verifica_bootsel();
        char str_horario[6];
        sprintf(str_horario, "%02d:%02d", horario.horas, horario.minutos);
        atualiza_area_texto(str_horario, pos_x, pos_y, 40, 16);
        ssd1306_fill_rect(&display, pos_x, pos_y+16, 40, 8, false);
        ssd1306_draw_string(&display, ":", pos_x + offsets[indice_edicao], pos_y+16);
        ssd1306_send_data(&display);
    
        DirecaoJoystick direcao = le_joystick();
        if (direcao == JOY_CIMA) {
            indice_edicao = (indice_edicao + 3) % 4;
            sleep_ms(200);
        } else if (direcao == JOY_BAIXO) {
            indice_edicao = (indice_edicao + 1) % 4;
            sleep_ms(200);
        } else if (direcao == JOY_ESQUERDA) { 
            if (indice_edicao == 0) {
                int dezena = horario.horas / 10;
                dezena = (dezena - 1);
                if (dezena < 0) dezena = 2;
                int unidade = horario.horas % 10;
                if (dezena == 2 && unidade > 3) unidade = 3;
                horario.horas = dezena * 10 + unidade;
            } else if (indice_edicao == 1) {
                int dezena = horario.horas / 10;
                int unidade = horario.horas % 10;
                int max = (dezena == 2) ? 3 : 9;
                unidade = (unidade - 1);
                if (unidade < 0) unidade = max;
                horario.horas = dezena * 10 + unidade;
            } else if (indice_edicao == 2) {
                int dezena = horario.minutos / 10;
                dezena = (dezena - 1);
                if (dezena < 0) dezena = 5;
                int unidade = horario.minutos % 10;
                horario.minutos = dezena * 10 + unidade;
            } else if (indice_edicao == 3) {
                int dezena = horario.minutos / 10;
                int unidade = horario.minutos % 10;
                unidade = (unidade - 1);
                if (unidade < 0) unidade = 9;
                horario.minutos = dezena * 10 + unidade;
            }
            sleep_ms(200);
        } else if (direcao == JOY_DIREITA) { 
            if (indice_edicao == 0) {
                int dezena = horario.horas / 10;
                dezena = (dezena + 1) % 3;
                int unidade = horario.horas % 10;
                if (dezena == 2 && unidade > 3) unidade = 3;
                horario.horas = dezena * 10 + unidade;
            } else if (indice_edicao == 1) {
                int dezena = horario.horas / 10;
                int unidade = horario.horas % 10;
                int max = (dezena == 2) ? 3 : 9;
                unidade = (unidade + 1) % (max + 1);
                horario.horas = dezena * 10 + unidade;
            } else if (indice_edicao == 2) {
                int dezena = horario.minutos / 10;
                dezena = (dezena + 1) % 6;
                int unidade = horario.minutos % 10;
                horario.minutos = dezena * 10 + unidade;
            } else if (indice_edicao == 3) {
                int dezena = horario.minutos / 10;
                int unidade = horario.minutos % 10;
                unidade = (unidade + 1) % 10;
                horario.minutos = dezena * 10 + unidade;
            }
            sleep_ms(200);
        }
        
        if ( flag_botaoA) { 
            flag_botaoA = false; 
            editando = false; 
        }
        if (flag_botaoB) { 
            flag_botaoB = false; 
            edicao_cancelada = true; 
            editando = false; 
        }
    }
    return horario; 
}
  
void executar_alarme(Horario atual, Horario alarme) {
    int minutos_atual = atual.horas * 60 + atual.minutos;
    int minutos_alarme = alarme.horas * 60 + alarme.minutos;
    if (minutos_alarme <= minutos_atual)
         minutos_alarme += 24 * 60;
    int diff_minutos = minutos_alarme - minutos_atual;
    int segundos_totais = diff_minutos * 60;
    int area_x = (LARGURA_TELA - 40) / 2;
    int area_y = 30, area_w = 40, area_h = 16;
    
    while(segundos_totais > 0) {
         verifica_bootsel();
         int horas_restantes = segundos_totais / 3600;
         int minutos_restantes = (segundos_totais % 3600) / 60;
         int segundos_restantes = segundos_totais % 60;
         char buffer[10];
         sprintf(buffer, "%02d:%02d:%02d", horas_restantes, minutos_restantes, segundos_restantes);
         atualiza_area_texto(buffer, area_x, area_y, area_w, area_h);
         sleep_ms(1000);
         segundos_totais--;
         if(flag_botaoB) {
             flag_botaoB = false; 
             return; 
         }
    }
    
    define_brilho_led(LED_VERMELHO, 1.0);
    mostra_tela("Alarme!");
    ssd1306_draw_string(&display, "Clique A para", 10, 30);
    ssd1306_draw_string(&display, "desligar", 10, 40);
    ssd1306_send_data(&display);
    while (!flag_botaoA){
        play_buzzer_tone();
        verifica_bootsel();
        sleep_ms(10);
    }
    flag_botaoA = false;
    stop_buzzer_tone();
    define_brilho_led(LED_VERMELHO, 0.0);
}

void executar_pomodoro(int tempo_estudo, int tempo_pausa) {
    char buffer[20];
    int area_x = (LARGURA_TELA - 40) / 2;
    int area_y = 30, area_w = 40, area_h = 16;
    
    while (1) {
         // Fase de estudos (brilho máximo para teste)
         define_brilho_led(LED_VERDE, 1.0);
         ssd1306_fill(&display, false);
         desenha_moldura();
         ssd1306_draw_string(&display, "Estudos", 10, 5);
         sprintf(buffer, "%02d:00", tempo_estudo);
         ssd1306_draw_string(&display, buffer, area_x, area_y);
         ssd1306_send_data(&display);
         
         int segundos_estudo = tempo_estudo * 60;
         while(segundos_estudo > 0) {
              verifica_bootsel();
              int min_restantes = segundos_estudo / 60;
              int seg_restantes = segundos_estudo % 60;
              sprintf(buffer, "%02d:%02d", min_restantes, seg_restantes);
              atualiza_area_texto(buffer, area_x, area_y, area_w, area_h);
              sleep_ms(1000);
              segundos_estudo--;
              if(flag_botaoB) {
                   flag_botaoB = false;
                   break;
              }
         }
         define_brilho_led(LED_VERDE, 0.0);
         if(flag_botaoB) {
              flag_botaoB = false;
              break;
         }
         
         // Fase de pausa (LED vermelho e azul em brilho máximo)
         define_brilho_led(LED_VERMELHO, 1.0);
         ssd1306_fill(&display, false);
         desenha_moldura();
         ssd1306_draw_string(&display, "Pausa", 10, 5);
         ssd1306_send_data(&display);
         while (!flag_botaoA) {
              play_buzzer_tone();
              verifica_bootsel();
         }
         flag_botaoA = false;
         define_brilho_led(LED_VERMELHO, 0.0);
         
         int segundos_pausa = tempo_pausa * 60;
         define_brilho_led(LED_AZUL, 1.0);
         while(segundos_pausa > 0) {
              verifica_bootsel();
              int min_restantes = segundos_pausa / 60;
              int seg_restantes = segundos_pausa % 60;
              sprintf(buffer, "%02d:%02d", min_restantes, seg_restantes);
              atualiza_area_texto(buffer, area_x, area_y, area_w, area_h);
              sleep_ms(1000);
              segundos_pausa--;
              if(flag_botaoB) {
                   flag_botaoB = false;
                   break;
              }
         }
         define_brilho_led(LED_AZUL, 0.0);
         if(flag_botaoB) {
              flag_botaoB = false;
              break;
         }
         
         // Preparar novo ciclo de estudos
         define_brilho_led(LED_VERMELHO, 1.0);
         ssd1306_fill(&display, false);
         desenha_moldura();
         ssd1306_draw_string(&display, "Estudos", 10, 5);
         ssd1306_send_data(&display);
         while (!flag_botaoA) {
            play_buzzer_tone();
            verifica_bootsel();
       }
         if(flag_botaoB) {
              flag_botaoB = false;
              define_brilho_led(LED_VERMELHO, 0.0);
              break;
         }
         flag_botaoA = false;
         define_brilho_led(LED_VERMELHO, 0.0);
         // Reinicia o ciclo pomodoro
    }
}

int main() {
    stdio_init_all();
    
    configura_componentes();
    configura_pwm_no_pino(LED_VERDE);
    configura_pwm_no_pino(LED_AZUL);
    configura_pwm_no_pino(LED_VERMELHO);
    
    ssd1306_init_config_clean(&display, SCL_I2C, SDA_I2C, PORTA_I2C, OLED_ENDERECO);
    ssd1306_config(&display);
    
    while (1) {
        verifica_bootsel();
        switch (estado_atual) {
            case ESTADO_BEM_VINDO:
                mostrar_boas_vindas();
                while (!flag_botaoA) { verifica_bootsel(); sleep_ms(100); }
                flag_botaoA = false;
                estado_atual = ESTADO_MENU_PRINCIPAL;
                break;
            case ESTADO_MENU_PRINCIPAL: {
                    desenha_menu_principal();
                    // Ignorando o botão B neste menu
                    while(!flag_botaoA) {
                        DirecaoJoystick direcao = le_joystick();
                        if (direcao == JOY_CIMA || direcao == JOY_ESQUERDA ||
                            direcao == JOY_BAIXO || direcao == JOY_DIREITA) {
                            selecao_menu_principal = (selecao_menu_principal + 1) % 2;
                            desenha_seta_menu_principal(selecao_menu_principal);
                            sleep_ms(200);
                        }
                        if(flag_botaoB) {
                            flag_botaoB = false;
                        }
                        verifica_bootsel();
                        sleep_ms(50);
                    }
                    flag_botaoA = false;
                    estado_atual = (selecao_menu_principal == 0) ? ESTADO_EDITAR_HORA_ATUAL : ESTADO_MENU_POMODORO;
                }
                break;
            case ESTADO_EDITAR_HORA_ATUAL:
                horario_atual = editar_horario("Hora Atual", false);
                if (edicao_cancelada) { 
                    estado_atual = ESTADO_MENU_PRINCIPAL; 
                    break; 
                }
                estado_atual = ESTADO_EDITAR_ALARME;
                break;
            case ESTADO_EDITAR_ALARME:
                horario_alarme = editar_horario("Alarme", false);
                if (edicao_cancelada) { 
                    estado_atual = ESTADO_MENU_PRINCIPAL; 
                    break; 
                }
                executar_alarme(horario_atual, horario_alarme);
                estado_atual = ESTADO_MENU_PRINCIPAL;
                break;
            case ESTADO_MENU_POMODORO: {
                    desenha_menu_pomodoro();
                    while(!flag_botaoA && !flag_botaoB) {
                        DirecaoJoystick direcao = le_joystick();
                        if (direcao == JOY_CIMA || direcao == JOY_ESQUERDA) {
                            selecao_pomodoro = (selecao_pomodoro + 3) % 4;
                            desenha_seta_menu_pomodoro(selecao_pomodoro);
                            sleep_ms(200);
                        } else if (direcao == JOY_BAIXO || direcao == JOY_DIREITA) {
                            selecao_pomodoro = (selecao_pomodoro + 1) % 4;
                            desenha_seta_menu_pomodoro(selecao_pomodoro);
                            sleep_ms(200);
                        }
                        verifica_bootsel();
                        sleep_ms(50);
                    }
                    flag_botaoA = false;
                    {
                        int tempo_estudo = 25, tempo_pausa = 5;
                        switch (selecao_pomodoro) {
                            case 0: tempo_estudo = 25; tempo_pausa = 5; break;
                            case 1: tempo_estudo = 30; tempo_pausa = 15; break;
                            case 2: tempo_estudo = 40; tempo_pausa = 20; break;
                            case 3: tempo_estudo = 60; tempo_pausa = 30; break;
                        }
                        executar_pomodoro(tempo_estudo, tempo_pausa);
                    }
                    estado_atual = ESTADO_MENU_PRINCIPAL;
                }
                break;
            default:
                estado_atual = ESTADO_MENU_PRINCIPAL;
                break;
        }
        sleep_ms(50);
    }
    return 0;
}
