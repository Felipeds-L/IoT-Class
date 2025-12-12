#include "contiki.h"
#include "net/rime/rime.h"
#include "lib/random.h"
#include <stdio.h>

#define DESTINO 2

/*---------------------------------------------------------------------------*/
PROCESS(example_unicast_process, "Example unicast");
AUTOSTART_PROCESSES(&example_unicast_process);
/*---------------------------------------------------------------------------*/

static void
recv_uc(struct unicast_conn *c, const linkaddr_t *from)
{
  printf("%d recebe '%s' de %d\n",
        DESTINO,
        (char *)packetbuf_dataptr(),
        from->u8[0]);
}

static const struct unicast_callbacks unicast_callbacks = {recv_uc};
static struct unicast_conn uc;

/* Temperatura atual simulada */
static int temperatura_atual = 20;   // valor inicial médio

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_unicast_process, ev, data)
{
  PROCESS_EXITHANDLER(unicast_close(&uc);)
  PROCESS_BEGIN();

  unicast_open(&uc, 146, &unicast_callbacks);

  while(1) {
    static struct etimer et;
    linkaddr_t addr;

    // Envia a cada 10s
    etimer_set(&et, CLOCK_SECOND * 10);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    // --- VARIAÇÃO SUAVIZADA ---
    // gera -1, 0 ou +1
    int delta = (random_rand() % 3) - 1;

    temperatura_atual += delta;

    // mantém dentro do intervalo desejado
    if(temperatura_atual < 10) temperatura_atual = 10;
    if(temperatura_atual > 30) temperatura_atual = 30;

    char msg[32];
    snprintf(msg, sizeof(msg), "Temp: %dC", temperatura_atual);

    printf("Enviando: %s\n", msg);

    packetbuf_copyfrom(msg, strlen(msg));

    addr.u8[0] = DESTINO;
    addr.u8[1] = 0;

    if(!linkaddr_cmp(&addr, &linkaddr_node_addr)) {
      unicast_send(&uc, &addr);
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
