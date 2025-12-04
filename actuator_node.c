#include "contiki.h"
#include "lib/random.h"
#include "powertrace.h"

// Inclusões de Rede Rime
#include "net/rime/rime.h"
#include "net/rime/collect.h"
#include "net/rime/unicast.h"
#include "net/netstack.h"

// Inclusões de Periféricos e Sensores
#include "dev/leds.h"       
#include "dev/sht11/sht11-sensor.h" // CORRIGIDO: Inclui o cabeçalho correto do SHT11 para simulação/hardware

// Inclusões da Biblioteca Padrão
#include <stdio.h>          
#include <stdlib.h>         
#include <string.h>         

/*---------------------------------------------------------------------------*/
// Declarações de Processo
/*---------------------------------------------------------------------------*/
PROCESS(control_process, "Temperature Sensor/Actuator Process");
AUTOSTART_PROCESSES(&control_process);

/*---------------------------------------------------------------------------*/
// Variáveis e Estruturas Globais
/*---------------------------------------------------------------------------*/
static struct collect_conn tc;
static struct unicast_conn uc;
static struct etimer re_send_timer; 
static process_event_t event_actuator_done; 
static volatile int actuation_just_occurred = 0;

// Variável estática para simular a temperatura atual do ambiente (x10 para precisão)
static int current_temp_x10 = 250; 

/*---------------------------------------------------------------------------*/
// Funções Auxiliares
/*---------------------------------------------------------------------------*/

// Função de Leitura de Temperatura (SIMULADA E RANDÔMICA)
static int get_temp(void)
{
    // A variação máxima do drift (ex: 1.0 C)
    int drift_max = 20; 
    int drift = 0;
    
    // Se o atuador não acabou de atuar, aplica o drift normal
    if (actuation_just_occurred == 0) {
        // Aplica o drift randômico (simulação de variação natural)
        drift = (random_rand() % drift_max) - (drift_max / 2); // Variação entre -1.0 C e +0.9 C
    } else {
        // ZERA o drift após uma atuação, mas desativa o flag para permitir drift na próxima leitura
        printf(">> Drift ZERADO: Ambiente estável após atuação.\n");
        actuation_just_occurred = 0; // Desativa o flag imediatamente após a primeira leitura
    }
    
    current_temp_x10 += drift;
    
    // ... (restante do código de limite de temperatura permanece o mesmo)
    if (current_temp_x10 < 150) current_temp_x10 = 150;
    if (current_temp_x10 > 350) current_temp_x10 = 350;

    return current_temp_x10 / 10;
}

// Simulação do Atuador (Altera a variável de estado)
static void simulate_actuator(const char *command) {
    if (strcmp(command, "RESFRIAR") == 0) {
        printf("### ATUADOR: LIGANDO AR / RESFRIANDO o ambiente... ###\n");
        // Força a temperatura para o centro da zona alvo (25.0 C)
        current_temp_x10 = 250; 
        leds_on(LEDS_RED);
    } else if (strcmp(command, "AQUECER") == 0) {
        printf("### ATUADOR: LIGANDO AQUECEDOR / AQUECENDO o ambiente... ###\n");
        // Força a temperatura para o centro da zona alvo (25.0 C)
        current_temp_x10 = 250;
        leds_on(LEDS_GREEN);
    } else {
        printf("Comando de atuador desconhecido: %s\n", command);
    }
    
    // Feedback visual breve
    leds_off(LEDS_RED | LEDS_GREEN);
    printf(">> Temp simulada FORÇADA para: %d C\n", current_temp_x10 / 10);
    
    // ATIVA O SINALIZADOR (FLAG)
    actuation_just_occurred = 1;
}

// Envio de pacote (refatorado para ser chamado facilmente)
static void send_sensor_reading(void) {
    static linkaddr_t oldparent;
    const linkaddr_t *parent;
    
    int temp_value = get_temp(); // Chama a função randômica

    printf("Enviando leitura de temperatura (%d C)\n", temp_value);
    packetbuf_clear();
    packetbuf_set_datalen(sprintf(packetbuf_dataptr(),
                                  "%d", temp_value) + 1);
    collect_send(&tc, 15);

    // Lógica de monitoramento do parent (mantida)
    parent = collect_parent(&tc);
    if(!linkaddr_cmp(parent, &oldparent)) {
        if(!linkaddr_cmp(&oldparent, &linkaddr_null)) {
            printf("#L %d 0\n", oldparent.u8[0]);
        }
        if(!linkaddr_cmp(parent, &linkaddr_null)) {
            printf("#L %d 1\n", parent->u8[0]);
        }
        linkaddr_copy(&oldparent, parent);
    }
}

/*---------------------------------------------------------------------------*/
// Callbacks de Rede
/*---------------------------------------------------------------------------*/

// Callback para o Unicast (RECEBE comando do Sink)
static void recv_command(struct unicast_conn *c, const linkaddr_t *from)
{
    const char *command = (char *)packetbuf_dataptr();

    printf("Nó remoto recebeu comando '%s' do Sink %d.%d\n",
           command, from->u8[0], from->u8[1]);

    simulate_actuator(command);
    
    // Configura o timer para 5s (simulação do tempo de reação do ambiente)
    etimer_set(&re_send_timer, CLOCK_SECOND * 5); 
    
    // Sinaliza o processo principal para lidar com o evento 're-send'
    process_post(&control_process, event_actuator_done, NULL); 
}

// Callback para o Collect (apenas para encaminhar se for um roteador)
static void recv_collect(const linkaddr_t *originator, uint8_t seqno, uint8_t hops)
{
     if(hops > 0) {
        printf("2 Roteando pacote de %d.%d (HOPS %d)\n",
               originator->u8[0], originator->u8[1], hops);
     }
}

/*---------------------------------------------------------------------------*/
static const struct collect_callbacks collect_callbacks = { recv_collect };
static const struct unicast_callbacks unicast_callbacks = { recv_command, NULL };
/*---------------------------------------------------------------------------*/


PROCESS_THREAD(control_process, ev, data)
{
    static struct etimer periodic;
    static struct etimer et;

    PROCESS_BEGIN();

    powertrace_start(CLOCK_SECOND * 10);

    event_actuator_done = process_alloc_event();
    random_init(linkaddr_node_addr.u8[0]); // Inicializa a seed randômica
    
    // Simula a ativação do sensor (necessário para compilar com SHT11.h)
    SENSORS_ACTIVATE(sht11_sensor); 
    // Inicializa a temperatura simulada para um valor randômico inicial
    current_temp_x10 += random_rand() % 50 - 25; // Varia entre 2.5 C
    printf(">> Temp inicial simulada: %d C\n", current_temp_x10 / 10);

    collect_open(&tc, 130, COLLECT_ROUTER, &collect_callbacks);
    unicast_open(&uc, 140, &unicast_callbacks); 
    
    // Lógica para evitar que o nó remoto use o endereço 1.0
    if(linkaddr_node_addr.u8[0] == 1 && linkaddr_node_addr.u8[1] == 0) {
        printf("I am sink - ERROR: Este código é para nó remoto. Execute 'sink_node.c'.\n");
        collect_set_sink(&tc, 1);
        while(1) { PROCESS_WAIT_EVENT(); }
    } 
    
    // Nó Remoto (Sensor/Atuador) - Loop Principal
    else {
        printf("I am remote node - Sensor/Atuador\n");
        
        etimer_set(&et, 120 * CLOCK_SECOND);
        PROCESS_WAIT_UNTIL(etimer_expired(&et)); // Espera a rede estabilizar
    
        while(1) {
            etimer_set(&periodic, CLOCK_SECOND * 30);
            etimer_set(&et, random_rand() % (CLOCK_SECOND * 30));
    
            // Espera o timer aleatório expirar OU o evento de reenvio pós-atuação
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et) || ev == event_actuator_done);
    
            // 1. Tratamento do Reenvio (após atuação)
            if(ev == event_actuator_done) {
                if(etimer_expired(&re_send_timer)) {
                   printf("RE-ENVIO: Atuação concluída, enviando nova leitura.\n");
                   send_sensor_reading();
                   etimer_set(&periodic, CLOCK_SECOND * 30); // Reinicia o ciclo periódico
                }
            }
            
            // 2. Envio Periódico Padrão
            else if(etimer_expired(&et)) {
                 send_sensor_reading();
            }
    
            PROCESS_WAIT_UNTIL(etimer_expired(&periodic));
        }
    }

    PROCESS_END();
}