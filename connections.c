/*
 * chatterbox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
#ifndef CONNECTIONS_H_
#define CONNECTIONS_H_

#define MAX_RETRIES     10
#define MAX_SLEEPING     3
#if !defined(UNIX_PATH_MAX)
#define UNIX_PATH_MAX  64
#endif

#include <message.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>

/**
 * @file  connection.h
 * @brief Contiene le funzioni che implementano il protocollo 
 *        tra i clients ed il server
 */

/**
 * @function openConnection
 * @brief Apre una connessione AF_UNIX verso il server 
 *
 * @param path Path del socket AF_UNIX 
 * @param ntimes numero massimo di tentativi di retry
 * @param secs tempo di attesa tra due retry consecutive
 *
 * @return il descrittore associato alla connessione in caso di successo
 *         -1 in caso di errore
 */
int openConnection(char* path, unsigned int ntimes, unsigned int secs){

	int fd, retval;

	struct sockaddr_un sa;
	sa.sun_family=AF_UNIX;
	strncpy(sa.sun_path,path, sizeof(sa.sun_path));

	if( (fd= socket(AF_UNIX, SOCK_STREAM, 0))==-1){
        	perror("Stabilendo connessione");
       		return -1;
  	}
	 for(int i=0; i<ntimes; i++){
		/*Tento la connessione*/
		if( (retval=connect(fd,(struct sockaddr*)&sa, sizeof(struct sockaddr_un))) == -1 ) {
				printf("sleeping...\n");
			sleep(secs);
		}
		else break;
	}
	
	if(retval==-1) return -1; /*Connessione fallita*/
	return fd;
}

// -------- server side ----- 
/**
 * @function readHeader
 * @brief Legge l'header del messaggio
 *
 * @param fd     descrittore della connessione
 * @param hdr    puntatore all'header del messaggio da ricevere
 *
 * @return <=0 se c'e' stato un errore 
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa) 
 */
int readHeader(long connfd, message_hdr_t *hdr){
	/*NT: Implement with readPointer*/
	memset(hdr,0,sizeof(message_hdr_t));
	int retval=read(connfd, hdr, sizeof(message_hdr_t));
	if(retval<0){
		perror("reading Header");
	}
	return retval;
}

/**
 * @function readData
 * @brief Legge il body del messaggio
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al body del messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa) 
 */
int readData(long fd, message_data_t *data){
	/*Leggo il campo receiver*/
	memset(data,0,sizeof(message_data_t));
	/*int retval=read(fd, data->hdr.receiver, MAX_NAME_LENGTH*sizeof(char));
	if(retval==0) return retval;
	else if(retval<0){
		perror("readData: reading receiver");
		return retval;
	}*/
	int retval= read(fd, &(data->hdr), sizeof(message_data_hdr_t));
	if(retval<0){
		perror("Reading header of data");
		return retval;
	}
	if(data->hdr.len==0){
		data->buf=NULL;
		return retval;
	}
	data->buf= (char*) malloc(data->hdr.len*sizeof(char));
	memset(data->buf,0,data->hdr.len*sizeof(char));
	int toRead=data->hdr.len;
	char* tmpBuffer=data->buf;
	while(toRead >0 ){
		retval= read(fd,tmpBuffer, toRead);
		if(retval<0){
			perror("Reading buffer");
			return retval;
		}
		toRead=toRead-retval;
		tmpBuffer=tmpBuffer+retval;
	}
	/*Leggo la lunghezza del buffer e il buffer stesso*/
	/*retval=read(fd, &data->hdr.len, sizeof(unsigned int));
	if(retval==0) return retval;
	else if(retval<0){
		perror("readData: reading buffer length");
		return retval;
	}*/

	/*Alloco spazio per il buffer*/
	/*data->buf=malloc(data->hdr.len*sizeof(char));

	int byteRimanenti = data->hdr.len;
    	char* writePointer = data->buf;
    	while(byteRimanenti > 0){
        	retval = read(fd, writePointer, byteRimanenti);
       		if(retval<0){
            		perror("readData: reading buffer");
           		return retval;
       		}
        
        	byteRimanenti = byteRimanenti - retval;
        	writePointer = writePointer + retval;
        	assert(writePointer <= data->buf + data->hdr.len);
    	}*/
	return retval;
}

/**
 * @function readMsg
 * @brief Legge l'intero messaggio
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al messaggio
 *
 * @return <=0 se c'e' stato un errore
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa) 
 */
int readMsg(long fd, message_t *msg){
	memset(msg,0,sizeof(message_t));
	int retval=readHeader(fd, &(msg->hdr));
	retval=readData(fd,&(msg->data));
	return retval;
}

/* da completare da parte dello studente con altri metodi di interfaccia */

/**
 * @function sendData
 * @brief Invia il body del messaggio al server
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 */
int sendData(long fd, message_data_t *msg){
	int retval=write(fd, &(msg->hdr), sizeof(message_data_hdr_t));
	if(retval<0){
		perror("Sending data hdr");
	}
	int iterations=2;
	if(msg->hdr.len > 0){
		int toSend=msg->hdr.len;
		char* tmpBuffer=msg->buf;
		while(toSend >0 && iterations>0){
			retval=write(fd,tmpBuffer,toSend);
			if(retval<0){
				perror("Writing buffer");
			}
			toSend=toSend-retval;
			tmpBuffer=tmpBuffer+retval;
			iterations--;
		}
	}
	return retval;
}

// ------- client side ------
/**
 * @function sendRequest
 * @brief Invia un messaggio di richiesta al server 
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return <=0 se c'e' stato un errore
 */
int sendRequest(long fd, message_t *msg){ //funzione con cui il client manda una richiesta al server
	//printf("inviare l'op %d a %s\n", msg->hdr.op, msg->data.hdr.receiver);
	/*Scrivo l'header del messaggio*/
	/*int retval = write(fd, &(msg->hdr), sizeof(message_hdr_t));
   	if(retval<=0) return retval;*/
	int retval=write(fd, &(msg->hdr), sizeof(message_hdr_t));
	if(retval<0){
		//perror("Sending header");
		return retval;
	}
	/*Controllo la presenza della parte dati*/
	if( (msg->data.buf == NULL || msg->data.hdr.len == 0 )  && msg->hdr.op!=CREATEGROUP_OP && msg->hdr.op!=ADDGROUP_OP && msg->hdr.op!=DELGROUP_OP) return retval;
	/*Invio il campo receiver*/
	/*retval = write(fd, &(msg->data.hdr.receiver), MAX_NAME_LENGTH*sizeof(char));
   	if(retval<=0) return retval;*/
	retval=sendData(fd, &(msg->data));
	/*Invio la lunghezza del buffer*/
	/*retval=write(fd,&(msg->data.hdr.len), sizeof(unsigned int));
	if(retval<=0) return retval;*/

	/*Invio il buffer*/
    	/*int byteRimanenti = msg->data.hdr.len;
    	char* writePointer = msg->data.buf;
    	while(byteRimanenti > 0){
        	retval = write(fd, writePointer, byteRimanenti);
        	if(retval<0){
            		perror("writing buf");
            		return retval;
       		 }
        	byteRimanenti = byteRimanenti - retval;
        	writePointer = writePointer + retval;
       		assert(writePointer <= msg->data.buf + msg->data.hdr.len);
  	  }*/

	return retval;
}




/* da completare da parte dello studente con eventuali altri metodi di interfaccia */


#endif /* CONNECTIONS_H_ */
