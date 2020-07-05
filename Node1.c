/*
 * Node1.c
 *
 *  Created on: Sep 10, 2017
 *      Author: user
 */

/*
leds_get() & LEDS_RED
The red LED is on if the result of this statement is a non-zero number; off otherwise.

LED OFF return 0
*/


/*
NODE1 ~DOOR~
(Node1) with Rime address 1.0

TEMPERATURA: Media delle ultime 5 misurazioni (presa ogni 10 secondi)

ALLARME: ogni 2 secondi tutti i led si accendono e spengono,
alla disattivazione tornano allo stato precedente

APERTURA CANCELLO: dopo 14 secondi, 16 secondi LED BLU ogni 2 sec
*/

#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "contiki.h"
#include "contiki-net.h"
#include "net/rime/rime.h"
#include "sys/etimer.h"
#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "dev/light-sensor.h"
#include "dev/sht11/sht11-sensor.h" //Sensor temperature

#define MAX_RETRANSMISSIONS 5

//############################################################
static int temperatura[5];
static int avgtemp = 0;
static int secondi = 0;
static bool red = false;
static bool blue = false;
static bool green = false;
static bool allarme = false;
static bool opening = false; // se true indica l'apertura della porta e del cancello
static char* dati;
static bool lightgarden = false;

//############################################################

PROCESS(runicast_process, "Runicast process - Node 1");
PROCESS(broadcast_process, "Broadcast process - Node 1");
PROCESS(alarm_process, "Alarm process - Node 1");
PROCESS(opening_process, "Opening process - Node 1");
PROCESS(gardenlight_process, "Garden's lights process - Node 1");
PROCESS(temperature_process, "Temperature process - Node 1");
PROCESS(sendt_process, "Invio temp - Node 1");

AUTOSTART_PROCESSES(&runicast_process, &broadcast_process, &gardenlight_process, &temperature_process);

//############################################################
//# BROADCAST                                                #
//############################################################
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from){

  printf("Node 1 broadcast message received from %d.%d: '%s'\n", from->u8[0], from->u8[1], (char *)packetbuf_dataptr());
  dati = (char *)packetbuf_dataptr();
  printf("Nodo 1 broad: %s\n", dati);
  if((!strncmp(dati, "1",1))){
    if(allarme==false){
    // salvo stato led
    if(leds_get() & LEDS_RED) red = true;
      else red = false;
    if(leds_get() & LEDS_GREEN) green = true;
      else green = false;
    if(leds_get() & LEDS_BLUE) blue = true;
    else blue = false;
    }
    allarme=(!allarme);// cambia stato allarme
    if(allarme){// se attivo
      process_start(&alarm_process,NULL);
    }
    else// se non attivo
      process_exit(&alarm_process);
  }
  if((!strncmp(dati, "3",1))){
    if(!opening){// se non in apertura, si apre
      opening=true;
      process_start(&opening_process,NULL);
    }
  }
}

static void broadcast_sent(struct broadcast_conn *c, int status, int num_tx){
  printf("Node 1 broadcast message sent. Status %d. For this packet, this is transmission number %d\n", status, num_tx);
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv, broadcast_sent}; //Fare attenzione all'ordine
static struct broadcast_conn broadcast;
//############################################################
//# RUNICAST                                                 #
//############################################################
static void recv_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
  printf("Node 1 runicast message received from %d.%d, seqno %d\n", from->u8[0], from->u8[1], seqno);
  if(!strncmp((char *)packetbuf_dataptr(), "4", 1))
    process_start(&sendt_process, NULL);
}

static void sent_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("Node 1 runicast message sent to %d.%d, retransmissions %d\n", to->u8[0], to->u8[1], retransmissions);

}

static void timedout_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("Node 1 runicast message timed out when sending to %d.%d, retransmissions %d\n", to->u8[0], to->u8[1], retransmissions);
}

static const struct runicast_callbacks runicast_calls = {recv_runicast, sent_runicast, timedout_runicast};
static struct runicast_conn runicast;

//############################################################
//# ATTENDE MESSAGGIO BROADCAST                              #
//############################################################
PROCESS_THREAD(broadcast_process, ev, data){
  PROCESS_EXITHANDLER(broadcast_close(&broadcast));
  PROCESS_BEGIN();
  broadcast_open(&broadcast, 129, &broadcast_call);
  PROCESS_WAIT_EVENT_UNTIL(0);
  PROCESS_END();
}

//############################################################
//# ATTENDE MESSAGGIO UNICAST                                #
//############################################################
PROCESS_THREAD(runicast_process, ev, data){
  PROCESS_EXITHANDLER(runicast_close(&runicast));
  PROCESS_BEGIN();
  runicast_open(&runicast, 144, &runicast_calls);
  PROCESS_WAIT_EVENT_UNTIL(0);
  PROCESS_END();
}

//############################################################
//# processo che gestisce led allarme                        #
//############################################################
PROCESS_THREAD(alarm_process, ev, data){
  static struct etimer et;
  PROCESS_BEGIN();
  if(opening){// era in apertura
    process_post(&opening_process, PROCESS_EVENT_MSG,"Alarm activate");// allarme attivato: blocco l'apertura
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_MSG);// aspetto che opening salva lo stato del led
  }
  leds_on(LEDS_ALL);//accendo tutti i led
  while(1){
    etimer_set(&et,CLOCK_SECOND*2);
    PROCESS_WAIT_EVENT();
    if(etimer_expired(&et))
      leds_toggle(LEDS_ALL);
    if(ev == PROCESS_EVENT_EXIT){ // processo termina se l'allarme termina
      if(red) leds_on(LEDS_RED);
        else leds_off(LEDS_RED);
      if(green) leds_on(LEDS_GREEN);
        else leds_off(LEDS_GREEN);
      if(blue) leds_on(LEDS_BLUE);
      else leds_off(LEDS_BLUE);
      SENSORS_ACTIVATE(button_sensor);
      if(opening){// se allarme attivato durante l'apertura, rieseguo l'apertura
        process_post(&opening_process, PROCESS_EVENT_MSG,"Alarm deactivate");
      }
      PROCESS_EXIT();//termino il processo
    }
  }
  PROCESS_END();
}

//############################################################
//# LED DI APERTURA PORTA E CANCELLO                         #
//############################################################
PROCESS_THREAD(opening_process, ev, data){
    static struct etimer time;
    PROCESS_BEGIN();
    etimer_set(&time, CLOCK_SECOND);
    while(1){
      PROCESS_WAIT_EVENT();
      //timer scaduto
      if(etimer_expired(&time)){
        secondi += 1; // led blu aspetta per 14 secondi
        if(secondi > 14 && secondi < 30 && (secondi%2 == 0)) leds_toggle(LEDS_BLUE);
        if(secondi > 30){ //ho finito
            secondi = 0;
            opening = false; //non mi sto aprendo piu'
            leds_off(LEDS_BLUE);
            PROCESS_EXIT();
        }
      }
      //allarme
      if(ev == PROCESS_EVENT_MSG){
        etimer_stop(&time);
        if(leds_get() & LEDS_BLUE) blue = true; //salvo stato led
        else blue = false;
        process_post(&alarm_process, PROCESS_EVENT_MSG,NULL); // avvio allarme
        PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_MSG); // aspetto fine allarme
        if(blue) leds_on(LEDS_BLUE);
        else leds_off(LEDS_BLUE);
        etimer_restart(&time);
      }
      else etimer_reset(&time);
    }
    PROCESS_END();
}

//############################################################
//# ogni 10 secondi prende la temperatura                    #
//############################################################
// sempre attivo, prende la temperatura, quando riceve il comando invia il valore medio alla CU
PROCESS_THREAD(temperature_process, ev, data){

    static struct etimer timertemp;
    static int i;
    int tot =0;
    int j = 0;
    PROCESS_BEGIN();
    etimer_set(&timertemp, CLOCK_SECOND*10);

    while(1){

        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timertemp));
        SENSORS_ACTIVATE(sht11_sensor);
        temperatura[i] = (sht11_sensor.value(SHT11_SENSOR_TEMP)/10-396)/10;
        SENSORS_DEACTIVATE(sht11_sensor);
        printf("Nodo 1 Temperatura rilevata %d\n", (sht11_sensor.value(SHT11_SENSOR_TEMP)/10-396)/10);
        i = (i + 1) %5;

        for(j=0; j<5; j++)
          tot +=temperatura[j];
        avgtemp = tot/5;
        etimer_reset(&timertemp);
    }

    PROCESS_END();
}

//############################################################
//# processo che invia la temperatura                        #
//############################################################
PROCESS_THREAD(sendt_process, ev, data){
  linkaddr_t recv;

  PROCESS_BEGIN();
  while(runicast_is_transmitting(&runicast)); //aspetto
  char buffer[4];
  itoa(avgtemp, buffer, 10); // int to string
  packetbuf_copyfrom(buffer, 4);
  recv.u8[0] = 3;
  recv.u8[1] = 0;
  printf("%u.%u: Node 1 sending runicast to address %u.%u\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], recv.u8[0], recv.u8[1]);
  runicast_send(&runicast, &recv, MAX_RETRANSMISSIONS);
  PROCESS_END();
}

//############################################################
//# processo che gestisce la luce del giardino               #
//############################################################
// sempre attivo in attesa della pressione del pulsante
PROCESS_THREAD(gardenlight_process, ev, data){

    PROCESS_BEGIN();
    SENSORS_ACTIVATE(button_sensor);

    while(1){
        PROCESS_WAIT_EVENT();
        linkaddr_t recv;
        if(ev == sensors_event && data == &button_sensor){
            while(runicast_is_transmitting(&runicast));

            if(!lightgarden){
              leds_on(LEDS_GREEN);
              leds_off(LEDS_RED);
              packetbuf_copyfrom("o", 2);
              lightgarden = true;
              printf("Nodo 1 luci giardino on");
            }
            else{
              leds_off(LEDS_GREEN);
              leds_on(LEDS_RED);
              packetbuf_copyfrom("f", 2);
              lightgarden = false;
              printf("Nodo 1 luci giardino off");
            }

        recv.u8[0] = 3;
        recv.u8[1] = 0;
        printf("%u.%u: Node 1 sending runicast to address %u.%u\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], recv.u8[0], recv.u8[1]);
        runicast_send(&runicast, &recv, MAX_RETRANSMISSIONS);//inviato alla CU lo stato delle luci
        }
    }

    PROCESS_END();
}


