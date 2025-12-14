#include "contiki.h"
#include "net/rime/rime.h"
#include "sys/clock.h" // Necessário para clock_time()
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DESTINO 2 // SINK
#define TEMP_IDEAL 25

static int current_temp = 0;
static char command[10] = "";

/*---------------------------------------------------------------------------*/
PROCESS(example_unicast_process, "Example unicast - HEAD");
AUTOSTART_PROCESSES(&example_unicast_process);
/*---------------------------------------------------------------------------*/

// Função auxiliar para enviar a temperatura atual ou "FINISHED"
static void send_status(struct unicast_conn *c, const char *msg)
{
    linkaddr_t addr;
    char buffer[10];

    if (msg == NULL)
    {
        snprintf(buffer, sizeof(buffer), "%d", current_temp);
        printf("EDGE: Enviando Temperatura %d para SINK.\n", current_temp);
        packetbuf_copyfrom(buffer, strlen(buffer) + 1);
    }
    else
    {
        printf("EDGE: Enviando status: %s\n", msg);
        packetbuf_copyfrom(msg, strlen(msg) + 1);
    }

    addr.u8[0] = DESTINO;
    addr.u8[1] = 0;

    if (!linkaddr_cmp(&addr, &linkaddr_node_addr))
    {
        unicast_send(c, &addr);
    }
}

// Handler chamado ao receber um pacote do SINK
static void recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
    char *cmd_recv = (char *)packetbuf_dataptr();
    printf("EDGE: Recebido comando '%s' do SINK.\n", cmd_recv);

    strncpy(command, cmd_recv, sizeof(command) - 1);
    command[sizeof(command) - 1] = '\0';

    if (strcmp(command, "AQUECER") == 0 || strcmp(command, "RESFRIAR") == 0)
    {

        // Aplica o ajuste de 1 grau
        if (strcmp(command, "AQUECER") == 0)
        {
            current_temp++;
        }
        else if (strcmp(command, "RESFRIAR") == 0)
        {
            current_temp--;
        }

        if (current_temp == TEMP_IDEAL)
        {
            // Se atingiu 25º, envia FINALIZED
            send_status(c, "FINISHED");
        }
        else
        {
            // Se não, envia a nova temperatura IMEDIATAMENTE (cria o loop rápido)
            send_status(c, NULL);
        }
    }
    else if (strcmp(command, "STABLE") == 0)
    {
        printf("EDGE: Reiniciando ciclo.\n");
        current_temp = (int)((unsigned int)rand() % 51);
        send_status(c, NULL);
    }
}

static const struct unicast_callbacks unicast_callbacks = {recv_uc};
static struct unicast_conn uc;
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_unicast_process, ev, data)
{
    PROCESS_EXITHANDLER(unicast_close(&uc);)

    PROCESS_BEGIN();

    unicast_open(&uc, 146, &unicast_callbacks);

    // Inicializa o gerador de números aleatórios com uma seed mais robusta
    srand(clock_time() + linkaddr_node_addr.u8[0]);

    // 1. INÍCIO DO CICLO (PRIMEIRA LEITURA) (CORRIGIDO: cast para unsigned int)
    current_temp = (int)((unsigned int)rand() % 51);
    send_status(&uc, NULL);

    while (1)
    {

        // O processo espera apenas por eventos de pacote RIME (recv_uc) para continuar
        PROCESS_WAIT_EVENT();
    }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/