/*
 * membox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
/**
 * @file tools.c
 * @brief File contenente funzioni di utilità per il server chatty
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <tools.h>

pthread_mutex_t queueMtx=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queueEmpty=PTHREAD_COND_INITIALIZER;

struct fdRdy* fdHead=NULL;
struct fdRdy* fdTail=NULL;

void termHandler(){
	terminate=1;
	pthread_cond_broadcast(&queueEmpty);
}

void usr1Handler(){
}

/**
 * @function insQueue
 * @brief Inserisce il fd passato come parametro in fondo alla coda da cui prelevano i worker
 *
 * @param fd     descrittore della connessione da inserire nella coda
 *
 */
void insQueue(int fd){
	struct fdRdy* el=malloc(sizeof(struct fdRdy));
	el->fd=fd;
	
	pthread_mutex_lock(&queueMtx);
	if(fdHead==NULL){
		el->fdNext=NULL;
		el->fdPrec=NULL;
		fdHead=el;
		fdTail=el;
	}
	else{
		el->fdPrec=fdTail;
		el->fdNext=NULL;
		fdTail=el;
	}	
	pthread_cond_signal(&queueEmpty);
	pthread_mutex_unlock(&queueMtx);
}

/**
 * @function extractFd
 * @brief Estrae il primo elemento dalla coda delle richieste da gestire
 *
 * @return <=0 se c'e' stato un errore 
 *         (se <0 errno deve essere settato, se == 0 connessione chiusa) 
 */
int extractFd(){

	pthread_mutex_lock(&queueMtx);
	while(fdHead==NULL && terminate==0 ) /*La coda è al momento vuota*/
		pthread_cond_wait(&queueEmpty,&queueMtx);
	
	if(terminate==1){ //Caso in cui sono stato risvegliato da un segnale
		pthread_mutex_unlock(&queueMtx);
		pthread_exit(NULL);
	}
	struct fdRdy* tmp_head=fdHead;
	int fd=tmp_head->fd;
	fdHead=fdHead->fdNext;
	pthread_mutex_unlock(&queueMtx);
	free(tmp_head);
	return fd;
}

/**
 * @function addHistory
 * @brief Inserisce il messaggio passato come parametro nella history del receiver
 *
 * @param msg il messaggio da memorizzare
 * @param rec puntatore all'oggetto corrispondente al receiver nella tabella hash
 *
 * @return 
 */
int addHistory(message_t* msg, userAcc_t* rec){
	if(rec->addIndex==max_hist_msgs-1){
		//Caso in cui è stato raggiunto il numero massimo di messaggi
		free(rec->hist[rec->addIndex].data.buf);
		rec->addIndex=0;
	}
	if(rec->nMsgs<max_hist_msgs){
		 rec->nMsgs++;
		 rec->hist=realloc(rec->hist, rec->nMsgs*sizeof(message_t));
	}
	rec->hist[rec->addIndex]=*msg;
	rec->addIndex++;
	return 0;
}


