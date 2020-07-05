/*
 * Node3.c
 *
 *  Created on: Sep 10, 2017
 *      Author: user
 */

/*
(Node1) with Rime address 4.0
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
static int temperatura;
static bool ledon = false;
static bool allarme = false;
static bool red = false;
static bool blue = false;
static bool green = false;
static char* dati;
//############################################################

PROCESS(temp_process, "Temp - Node 3");
PROCESS(broadcast_process, "Broadcast process - Node 3");
PROCESS(led_process, "Led - Node 3");
PROCESS(alarm_process, "Alarm process - Node 3");
AUTOSTART_PROCESSES(&temp_process, &broadcast_process);

//############################################################
//# BROADCAST                                                #
//############################################################
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from){

  printf("Node 3 broadcast message received from %d.%d: '%s'\n", from->u8[0], from->u8[1], (char *)packetbuf_dataptr());
  dati = (char *)packetbuf_dataptr();
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
}

static void broadcast_sent(struct broadcast_conn *c, int status, int num_tx){
  printf("Node 3 broadcast message sent. Status %d. For this packet, this is transmission number %d\n", status, num_tx);
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv, broadcast_sent}; //Fare attenzione all'ordine
static struct broadcast_conn broadcast;

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
//# processo che gestisce led allarme                        #
//############################################################
PROCESS_THREAD(alarm_process, ev, data){
  static struct etimer et;
  PROCESS_BEGIN();
  if(ledon){// era in apertura
    process_post(&led_process, PROCESS_EVENT_MSG,"Alarm activate");// allarme attivato: blocco l'apertura
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
      if(ledon){// se allarme attivato durante l'apertura, rieseguo l'apertura
        process_post(&led_process, PROCESS_EVENT_MSG,"Alarm deactivate");
      }
      PROCESS_EXIT();//termino il processo
    }
  }
  PROCESS_END();
}

//############################################################
//# ogni 10 secondi prende la temperatura                    #
//############################################################
PROCESS_THREAD(temp_process, ev, data){
    static struct etimer timertemp;
    PROCESS_BEGIN();
    etimer_set(&timertemp, CLOCK_SECOND*10);
    // test
    //SENSORS_ACTIVATE(button_sensor);
    temperatura = 0;
    while(1){
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timertemp));
      // test
        //PROCESS_WAIT_EVENT();
        SENSORS_ACTIVATE(sht11_sensor);
        temperatura = (sht11_sensor.value(SHT11_SENSOR_TEMP)/10-396)/10;
        SENSORS_DEACTIVATE(sht11_sensor);

      //test 
/*        if(ev == sensors_event && data == &button_sensor)
      temperatura = (temperatura + 15)%40;
      if(etimer_expired(&timertemp)){ */
        
    ledon = true;
    etimer_reset(&timertemp);
    if(!allarme)
      process_start(&led_process ,NULL);
    
    // test
      //}
    }
    PROCESS_END();
}

//############################################################
//# gestisce i led                               #
//############################################################
PROCESS_THREAD(led_process, ev, data){
  static struct etimer timerled;
  static struct etimer tt;
  static int cont = 0;
  PROCESS_BEGIN();

  etimer_set(&timerled, CLOCK_SECOND);
  etimer_set(&tt, CLOCK_SECOND*5);
  
  while(1){
    PROCESS_WAIT_EVENT(); 
    if(etimer_expired(&timerled)){
      cont++;
          if(temperatura < 10)
            leds_toggle(LEDS_BLUE);
          else if(temperatura >=10 && temperatura < 28)
            leds_toggle(LEDS_GREEN);
          else
            leds_toggle(LEDS_RED);
          
          etimer_reset(&timerled);
      }
        if(etimer_expired(&tt)){ //ho finito
            ledon = false;
            leds_off(LEDS_ALL);
            PROCESS_EXIT();
        } 
        //allarme
      if(ev == PROCESS_EVENT_MSG){
          etimer_stop(&tt);
          if(leds_get() & LEDS_BLUE) blue = true; //salvo stato led
          if(leds_get() & LEDS_RED) red = true;
          if(leds_get() & LEDS_GREEN) green = true;
          else blue = false;
          process_post(&alarm_process, PROCESS_EVENT_MSG,NULL); // avvio allarme
          PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_MSG); // aspetto fine allarme
          if(blue) leds_on(LEDS_BLUE);
          else leds_off(LEDS_BLUE);
          if(red) leds_on(LEDS_RED);
          else leds_off(LEDS_RED);
          if(green) leds_on(LEDS_GREEN);
          else leds_off(LEDS_GREEN);
          etimer_restart(&tt);
      }
      
  }
  PROCESS_END();
}












