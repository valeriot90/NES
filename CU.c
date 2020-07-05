/*
 * CU.c
 *
 *  Created on: Sep 10, 2017
 *      Author: user
 */

/*
Central Unit (CU) node with Rime address 3.0
*/
// strncmp: confronta n byte, ritorna 0 se uguali
#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "contiki.h"
#include "contiki-net.h"
#include "net/rime/rime.h"
#include "sys/etimer.h"
#include "dev/leds.h"
#include "dev/button-sensor.h"

#define MAX_RETRANSMISSIONS 5
//############################################################

PROCESS(button_process, "Main process - Node CU");
PROCESS(runicast_process, "Runicast process - Node CU");

AUTOSTART_PROCESSES(&button_process, &runicast_process);

//############################################################
//# BROADCAST 												 #
//############################################################
static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from){

  printf("CU broadcast message received from %d.%d: '%s'\n", from->u8[0], from->u8[1], (char *)packetbuf_dataptr());
}
//u8: array of bytes (Rime address -Rime protocol-)
//scrive il payload del pacchetto nel buffer

static void broadcast_sent(struct broadcast_conn *c, int status, int num_tx){

  printf("CU broadcast message sent. Status %d. For this packet, this is transmission number %d\n", status, num_tx);
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv, broadcast_sent}; //Be careful to the order
static struct broadcast_conn broadcast;

//############################################################
//# RUNICAST												 #
//############################################################
static void recv_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno) // strcmp RITORNA 0 SE LE STRINGHE SONO UGUALI!
{
  	printf("CU runicast message received from %d.%d, seqno %d\n", from->u8[0], from->u8[1], seqno);
 	if(from->u8[0]==1){

	  	if(strcmp((char *)packetbuf_dataptr(),"o") && strcmp((char *)packetbuf_dataptr(),"f")){//ricevo una temperatura
	  	
		  	printf("Temperature: %s C\n", (char*)packetbuf_dataptr());
		}
		if(!strncmp((char*)packetbuf_dataptr(), "o",1))
		  	printf("Lights of garden on\n");
		if(!strncmp((char*)packetbuf_dataptr(), "f",1))
		  	printf("Lights of garden off\n");
 	}
 	if(from->u8[0]==2)//ricevo luminosita'
		printf("External light: %s lux\n",(char *)packetbuf_dataptr());

}

static void sent_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("CU runicast message sent to %d.%d, retransmissions %d\n", to->u8[0], to->u8[1], retransmissions);
}


static void timedout_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions)
{
  printf("CU runicast message timed out when sending to %d.%d, retransmissions %d\n", to->u8[0], to->u8[1], retransmissions);
}

//per skippare la posizione perche' non voglio usare dei parametri: definisco un receive vuoto, vedi sopra
static const struct runicast_callbacks runicast_calls = {recv_runicast, sent_runicast, timedout_runicast};
static struct runicast_conn runicast;

//############################################################
//# ATTENDE MESSAGGIO RUNICAST 						 		 #
//############################################################
PROCESS_THREAD(runicast_process, ev, data)
{
  PROCESS_EXITHANDLER(runicast_close(&runicast));

  PROCESS_BEGIN();

  runicast_open(&runicast, 144, &runicast_calls);
  PROCESS_WAIT_EVENT_UNTIL(0);
  PROCESS_END();
}

//############################################################
//# CONTROLLA QUANTE VOLTE È STATO PREMUTO IL PULSANTE 		 #
//############################################################

PROCESS_THREAD(button_process, ev, data){

	static struct etimer timer;
	static int comando = 0; // comando da inviare
	static bool allarme = false; // SE IN STATO DI ALLARME E' 1
	static bool gatel = false;
	/*
	1 -> ALARM
	2 -> CANCELLO BLOCCATO/SBLOCCATO
	3 -> CANCELLO/PORTA
	4 -> TEMPERATURA
	5 -> LUCE ESTERNA
	*/

	PROCESS_EXITHANDLER(broadcast_close(&broadcast));
	PROCESS_BEGIN();
	broadcast_open(&broadcast,129,&broadcast_call);
	SENSORS_ACTIVATE(button_sensor);

	etimer_set(&timer, CLOCK_SECOND*4);
	printf("Command 1: Activate/Deactivate the alarm signal\n"
		"Command 2: Lock/Unlock the gate\n"
		"Command 3: Open (and automatically close) both the door and the gate in order to let a guest enter\n"
		"Command 4: Obtain the average of the last 5 temperature values measured by Node1\n"
		"Command 5: Obtain the external light value measured by Node2\n");
	while(1){
		

		PROCESS_WAIT_EVENT(); // aspetta evento
		if(ev == sensors_event && data == &button_sensor){
			printf("Tasto premuto\n");
			comando = (comando + 1)%6;
			etimer_restart(&timer); // riparte se l'evento era il tasto

		}
		else if(etimer_expired(&timer)){ // scaduti i 4 secondi
			if(comando == 0){
				printf("Nessun comando selezionato\n");
			}
			printf("4 seconds after last button press\n"
				"Selezionato comando %d\n", comando);

			switch(comando){
				linkaddr_t recv;
				printf("Controllo comando\n");
				case 1:
					comando = 0;
					packetbuf_copyfrom("1", 2); //copia il comando nel buffer: ATTENZIONE AL FINE STRINGA!
					broadcast_send(&broadcast);
					allarme = ! allarme; // cambio lo stato dell'allarme
					if(allarme)
						printf("Alarm on\n");
					else
						printf("Alarm off\n");
					etimer_reset(&timer);
					break;
				case 2:
					comando = 0;
					if(!allarme){
						packetbuf_copyfrom("2", 2);
						recv.u8[0] = 2;
						recv.u8[1] = 0;
						printf("%u.%u: CU sending runicast to address %u.%u\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], recv.u8[0], recv.u8[1]);
  						runicast_send(&runicast, &recv, MAX_RETRANSMISSIONS);
						gatel = !gatel;
						if(gatel)
							printf("Gate Locked\n");
						else
							printf("Gate Unlocked\n");
						etimer_reset(&timer);
					}
					break;
				case 3:
					comando = 0;
					if(!allarme){
						packetbuf_copyfrom("3", 2);
						broadcast_send(&broadcast);
						printf("Door & Gate open (and closed)\n");
						etimer_reset(&timer);
					}
					break;
				case 4:
					comando = 0;
					if(!allarme){
						packetbuf_copyfrom("4", 2);
						recv.u8[0] = 1;
						recv.u8[1] = 0;
						printf("%u.%u: CU sending runicast to address %u.%u\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], recv.u8[0], recv.u8[1]);
  						runicast_send(&runicast, &recv, MAX_RETRANSMISSIONS);
						etimer_reset(&timer);
					}
					break;
				case 5:
					comando = 0; 
					if(!allarme){
						packetbuf_copyfrom("5", 2);
						recv.u8[0] = 2;
						recv.u8[1] = 0;
						printf("%u.%u: CU sending runicast to address %u.%u\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], recv.u8[0], recv.u8[1]);
  						runicast_send(&runicast, &recv, MAX_RETRANSMISSIONS);
						etimer_reset(&timer);
					}
			}
		}
	}
	printf("Fine processo\n");
    PROCESS_END();
}
