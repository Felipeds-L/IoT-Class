#include "contiki.h"
#include "net/rime/rime.h"
#include "net/rime/collect.h"
#include "net/rime/unicast.h"
#include "dev/leds.h"
#include "net/netstack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
// Definição da temperatura alvo e tolerância
#define TEMP_TARGET 25
#define TEMP_TOLERANCE 2 
#define TEMP_MIN (TEMP_TARGET - TEMP_TOLERANCE) // 23 C
#define TEMP_MAX (TEMP_TARGET + TEMP_TOLERANCE) // 27 C

// Estruturas de Conexão
static struct collect_conn tc;
static struct unicast_conn uc;

/*---------------------------------------------------------------------------*/
// Funções Auxiliares de Comunicação
/*---------------------------------------------------------------------------*/

// Envia o comando de atuador (Unicast)
static void send_command(const linkaddr_t *receiver, const char *command) {
    packetbuf_clear();
    packetbuf_set_datalen(sprintf(packetbuf_dataptr(), "%s", command) + 1);
    unicast_send(&uc, receiver);
    printf(">> DECISÃO: Sink enviou comando '%s' para %d.%d\n", 
           command, receiver->u8[0], receiver->u8[1]);
}

// Lógica de controle de Temperatura
static void check_and_command(int temp_celsius, const linkaddr_t *from)
{
    const char *command_to_send = NULL;
    
    printf(">> DECISÃO: Lendo %d C. Target: %d-%d C.\n", 
           temp_celsius, TEMP_MIN, TEMP_MAX);
    
    // Sinaliza atuação no próprio Sink com LEDs (opcional)
    leds_off(LEDS_RED | LEDS_GREEN);

    if (temp_celsius < TEMP_MIN) {
        command_to_send = "AQUECER";
        leds_on(LEDS_GREEN); // Sinaliza Aquecimento
        
    } else if (temp_celsius > TEMP_MAX) {
        command_to_send = "RESFRIAR";
        leds_on(LEDS_RED);   // Sinaliza Resfriamento
        
    } else {
        printf(">> DECISÃO: Temperatura OK (%d C). Sem atuação.\n", temp_celsius);
        return; 
    }

    send_command(from, command_to_send);
}

/*---------------------------------------------------------------------------*/
// Callbacks de Rede
/*---------------------------------------------------------------------------*/

// Callback para o Unicast (opcional, apenas para debug de recebimento)
static void recv_unicast(struct unicast_conn *c, const linkaddr_t *from)
{
  printf("Comando Unicast recebido de %d.%d (Confirmação ou outro dado): '%s'\n",
         from->u8[0], from->u8[1], (char *)packetbuf_dataptr());
}

// Callback para o Collect (RECEBIMENTO de dados do sensor)
static void recv_collect(const linkaddr_t *originator, uint8_t seqno, uint8_t hops)
{
    if(hops > 0) {
        const char *data_str = (char *)packetbuf_dataptr();
        int temp_celsius = atoi(data_str); 

        printf("1 Recebe TEMPERATURA %d C de %d.%d (HOPS %d)\n",
               temp_celsius, originator->u8[0], originator->u8[1], hops);

        // Lógica de controle principal:
        check_and_command(temp_celsius, originator);
    }
}

/*---------------------------------------------------------------------------*/
static const struct collect_callbacks collect_callbacks = { recv_collect };
static const struct unicast_callbacks unicast_callbacks = { recv_unicast, NULL };
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
PROCESS(control_process, "Temperature Control Sink Process");
AUTOSTART_PROCESSES(&control_process);
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(control_process, ev, data)
{
    PROCESS_BEGIN();

    // 1. Abre a conexão Collect (para receber dados)
    collect_open(&tc, 130, COLLECT_ROUTER, &collect_callbacks);

    // 2. Abre a conexão Unicast (para enviar comandos)
    unicast_open(&uc, 140, &unicast_callbacks); 

    // O Sink sempre deve ser o nó 1.0
    if(linkaddr_node_addr.u8[0] == 1 &&
       linkaddr_node_addr.u8[1] == 0) {
        printf("Eu sou o Sink (1.0). Pronto para receber e comandar.\n");
        collect_set_sink(&tc, 1);
    }
    
    // O Sink apenas espera eventos de rede
    while(1) {
        PROCESS_WAIT_EVENT(); 
    }

    PROCESS_END();
}