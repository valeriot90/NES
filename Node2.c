/*
 * Node2.c
 *
 *  Created on: Sep 10, 2017
 *      Author: user
 */

/*
NODE2: ~GATE~
(Node2) with Rime address 2.0
cancello chiuso: led VERDE OFF, ROSSO ON
cancello aperto: led VERDE ON, ROSSO OFF

APERTURA CANCELLO: per 16 secondi, ogni 2 secondi LED BLU

Valore luce esterna
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
static int secondi = 0;
static bool red = false;
static bool blue = false;
static bool green = false;
static bool allarme = false;
static bool opening = false; // se il cancello e' in apertura
static bool gatel = false; // indica se il cancello e' chiuso
static char* dati;

//############################################################

PROCESS(runicast_process, "Runicast process - Node 2");
PROCESS(broadcast_process, "Broadcast process - Node 2");
PROCESS(alarm_process, "Alarm process - Node 2");
PROCESS(opening_process, "Opening process - Node 2");
PROCESS(light_process, "Light process - Node 2");

AUTOSTART_PROCESSES(&runicast_process, &broadcast_process);

//############################################################
//# BROADCAST                                                #
//############################################################
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from){
  printf("Node 2 broadcast message received from %d.%d: '%s'\n", from->u8[0], from->u8[1], (char *)packetbuf_dataptr());
  dati = (char *)packetbuf_dataptr();
  printf("Nodo 2 broad: %s\n", dati);
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
    if(allarme)// se attivo
      process_start(&alarm_process,NULL);
    else// se non attivo
      process_exit(&alarm_process);//voglio killare alarm_process
  }
  if((!strncmp(dati, "3",1))){
    if(!opening && !gatel){//lo eseguo se non è già in esecuzione
      opening = true;//segnalo che il comando 3 è in esecuzione
      process_start(&opening_process,NULL);
    }
  }
}

static void broadcast_sent(struct broadcast_conn *c, int status, int num_tx){
  printf("Node 2 broadcast message sent. Status %d. For this packet, this is transmission number %d\n", status, num_tx);
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv, broadcast_sent}; //Be careful to the order
static struct broadcast_conn broadcast;
//############################################################
//# RUNICAST                           #
//############################################################
static void recv_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno)
{
  printf("Node 2 runicast message received from %d.%d, seqno %d\n", from->u8[0], from->u8[1], seqno);
  dati = (char *)packetbuf_dataptr();
  printf("Nodo 2 unic: %s\n", dati);
  //if((!strncmp(dati, "2",1))){
  if(!strncmp((char *)packetbuf_dataptr(), "2", 1)){
    gatel =(!gatel);//switch dello stato del cancello
    if(gatel){ //chiudo
      printf("Node 2: chiudo\n");
      leds_on(LEDS_RED);
      leds_off(LEDS_GREEN);

    }else{ //apro
      printf("Node 2: apro\n");
      leds_on(LEDS_GREEN);
      leds_off(LEDS_RED);
    }
  }
  //if((!strncmp(dati, "5",1))){
  if(!strncmp((char *)packetbuf_dataptr(), "5", 1)){
    process_start(&light_process,NULL);
  }
}

static void sent_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("Node 2 runicast message sent to %d.%d, retransmissions %d\n", to->u8[0], to->u8[1], retransmissions);
}

static void timedout_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("Node 2 runicast message timed out when sending to %d.%d, retransmissions %d\n", to->u8[0], to->u8[1], retransmissions);
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
        if(secondi < 16 && (secondi%2 == 0)) leds_toggle(LEDS_BLUE);
        if(secondi >= 16) leds_off(LEDS_BLUE);
        if(secondi > 30){ //ho finito
            secondi = 0;
            opening = false; //non mi sto aprendo piu'
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
//# processo che gestisce led allarme                  #
//############################################################
PROCESS_THREAD(alarm_process, ev, data){
  static struct etimer et;
  PROCESS_BEGIN();
  if(opening){// era in apertura
    process_post(&opening_process, PROCESS_EVENT_MSG,"Alarm activate");// allarme attivato: blocco l'apertura
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_MSG);// aspetto che opening salva lo stato del led
  }
  leds_on(LEDS_ALL);
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
//# processo luce esterna                                #
//############################################################
PROCESS_THREAD(light_process, ev, data){
  PROCESS_BEGIN();
  while(runicast_is_transmitting(&runicast));
  linkaddr_t recv; //indirizzo della CU
  SENSORS_ACTIVATE(light_sensor);
  int value=light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC)*10/7;
  SENSORS_DEACTIVATE(light_sensor);
  char buffer[4];
  itoa(value,buffer,10);
  packetbuf_copyfrom(buffer, 4);
  recv.u8[0] = 3;
  recv.u8[1] = 0;
  runicast_send(&runicast, &recv, MAX_RETRANSMISSIONS);
  printf("%u.%u: Node 2 sending runicast to address %u.%u\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], recv.u8[0], recv.u8[1]);
  PROCESS_END();
}




