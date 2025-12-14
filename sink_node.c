#include "contiki.h"
#include "net/rime/rime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEMP_MIN 23
#define TEMP_MAX 27
#define TEMP_IDEAL 25
#define DESTINO 2

/*---------------------------------------------------------------------------*/
PROCESS(example_unicast_process, "Example unicast - SINK");
AUTOSTART_PROCESSES(&example_unicast_process);
/*---------------------------------------------------------------------------*/
static void send_command(struct unicast_conn *c, const linkaddr_t *addr, char *cmd)
{
    packetbuf_copyfrom(cmd, strlen(cmd) + 1);
    printf("SINK: Enviando comando '%s' para %d\n", cmd, addr->u8[0]);
    unicast_send(c, addr);
}

static void
recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
    char *pacote = (char *)packetbuf_dataptr();
    int temp = 0;
    char *comando_resposta;

    // 1. Verifica se o pacote é o status "FINISHED"
    if (strcmp(pacote, "FINISHED") == 0)
    {
        printf("SINK: Recebido 'FINISHED'. TEMPERATURA ESTABILIZADA (ideal: %d). ✅\n", TEMP_IDEAL);
        // Envia um status de volta para o EDGE para que ele inicie a espera de 1 minuto
        send_command(c, from, "RESTART");
        return;
    }

    // 2. Se não é "FINISHED", assume-se que é a temperatura.
    temp = atoi(pacote);
    printf("SINK: Recebida Temperatura: %d de %d\n", temp, from->u8[0]);

    // 3. Lógica de Controle
    if (temp < TEMP_MIN)
    {
        comando_resposta = "AQUECER";
    }
    else if (temp > TEMP_MAX)
    {
        comando_resposta = "RESFRIAR";
    }
    else
    {
        // Se a temperatura está dentro da faixa aceitável (23 <= temp <= 27)
        comando_resposta = "STABLE";
    }

    // 4. Envia o comando de volta
    send_command(c, from, comando_resposta);
}

static const struct unicast_callbacks unicast_callbacks = {recv_uc};
static struct unicast_conn uc;
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_unicast_process, ev, data)
{
    PROCESS_EXITHANDLER(unicast_close(&uc);)

    PROCESS_BEGIN();

    unicast_open(&uc, 146, &unicast_callbacks);

    printf("SINK: Controlador de Temperatura (Faixa aceita: %d-%d) aguardando leituras...\n", TEMP_MIN, TEMP_MAX);

    // O SINK é reativo, ele apenas espera por eventos RIME.
    while (1)
    {
        PROCESS_WAIT_EVENT();
    }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/