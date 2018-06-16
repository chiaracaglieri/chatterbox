/*
 * membox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
/**
 * @file threads.c
 * @brief File contenente il codice dei thread dispatcher e worker 
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
#include <sys/select.h>
#include <sys/socket.h>
#include <message.h>
#include <connections.h>
#include <config.h>

pthread_mutex_t setMtx=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t statsMtx=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t fdarrayMtx=PTHREAD_MUTEX_INITIALIZER;

void* dispatcher(void* fd_p){
	
	int fd=*((int*) fd_p);
	/*Setto fdMax al fd del server*/
	fdMax= fd; 

	int i, fdClient;
	pthread_mutex_lock(&setMtx);
	
	/*Inizializzo i set*/
	FD_ZERO(&set);
	FD_ZERO(&rdset);

	/*Aggiungo il fd relativo alla socket al set*/
	FD_SET(fd,&set);
	pthread_mutex_unlock(&setMtx);

	/*Inizializzo il timer per evitare che la select si blocchi indefinitamente*/
	struct timeval tv;
    	tv.tv_sec = 1/100;
    	tv.tv_usec = 0;

	/*Entro nel loop per ascoltare le richieste in arrivo*/
	while(terminate==0){
		pthread_mutex_lock(&setMtx);
		/*Re-inizializzo il set di lavoro*/
		rdset=set;
		pthread_mutex_unlock(&setMtx);

		if( select(fdMax+1,&rdset,NULL,NULL,&tv)==-1){
			perror("select");
			pthread_exit(NULL);
		}
		else{
			for( i=0; i<=fdMax; i++){
				if(FD_ISSET(i,&rdset)){
					/*Ho trovato un fd pronto*/
					if(i==fd){
						/*Nuova connessione da gestire*/
						fdClient=accept(fd,NULL,0);
						/*Controllo che non venga superato max_conn*/
						if(chattyStats.nonline==max_conn){
							message_t tooManyConn;
							setHeader(&tooManyConn.hdr,OP_FAIL,"Server");
							sendRequest(fdClient,&tooManyConn);
							close(fdClient);
							break;
						}
						else{
							pthread_mutex_lock(&setMtx);
							/*Inserisco il fd del client nel set*/
							FD_SET(fdClient,&set);
							if(fdClient>fdMax) fdMax=fdClient;
							pthread_mutex_unlock(&setMtx);

							/*Aggiorno le statistiche*/
							pthread_mutex_lock(&statsMtx);
							chattyStats.nonline++;
							pthread_mutex_unlock(&statsMtx);
						}
					}
					else{
						/*Nuova richiesta da connessione precedente*/
						pthread_mutex_lock(&setMtx);
						/*Rimuovo il fd dal set in attesa che la richiesta venga gestita*/
						FD_CLR(i,&set);
						pthread_mutex_unlock(&setMtx);
						/*Inserisco il fd del client nella coda dei Worker*/
						insQueue(i);
					}
				}
			}
		}
	}
	pthread_exit(0);

}

void* worker(void* threadno){
	int fd;
	/*Servo le richieste fino a che non arriva un segnale di terminazione*/
	while(terminate==0){

		/*Estraggo un fd dalla coda dei client da servire*/
		fd=extractFd();
		
		/*Controllo che la connessione non sia stata chiusa*/
		message_t req;
		if(readHeader(fd,&req.hdr)==0){
			//Connessione chiusa dal client
			/*Resetto la entry corrispondente al client in fdArray*/
			for(int j=0; j<max_conn; j++){
				pthread_mutex_lock(&fdarrayMtx);
				if(fdArray[j].fd==fd){
					fdArray[j].fd=-1;
					pthread_mutex_unlock(&fdarrayMtx);
					/*Aggiorno la relativa statistica*/
					pthread_mutex_lock(&statsMtx);
					chattyStats.nonline--;
					pthread_mutex_unlock(&statsMtx);
					break;
				}
				pthread_mutex_unlock(&fdarrayMtx);
			}
			close(fd);
		}
		else{
			printf("Worker %d: %s richiede l'op %d\n", (int) (size_t) threadno, req.hdr.sender, req.hdr.op);
		
			/*Preparo il messaggio di ok*/
			message_t mOk;
			setHeader(&mOk.hdr,OP_OK,"Server");

			switch(req.hdr.op){
				case(REGISTER_OP):{
					/*Controllo la lunghezza del nickname*/
					if( strlen(req.hdr.sender)>MAX_NAME_LENGTH){
						/*Invio il messaggio di errore*/
						message_t nickTooLong;
						setHeader(&nickTooLong.hdr,OP_FAIL,"Server");
						sendRequest(fd,&nickTooLong);
						break;
					}

					/*Controllo che il nickname non sia già presente*/
					if( icl_hash_find(usersHash, req.hdr.sender)!=NULL){
						/*Invio il messaggio di errore*/
						message_t nickAlready;
						setHeader(&nickAlready.hdr,OP_NICK_ALREADY,"Server");
						sendRequest(fd,&nickAlready);
						break;
					}
				
					/*Creo la entry della tabella hash e memorizzo il nickname*/
					userAcc_t* element=malloc(sizeof(userAcc_t));
					element->nickname=malloc(MAX_NAME_LENGTH+1*sizeof(char));
					strcpy(element->nickname,req.hdr.sender);
					element->hist=NULL;
					element->addIndex=0;
					element->nMsgs=0;
					icl_hash_insert(usersHash,req.hdr.sender, element);

					free(element->nickname);
					free(element);

					/*Cerco una posizione vuota nell'array dei fd e vi inserisco i dati del client*/
					for(int i=0; i<max_conn; i++){
						pthread_mutex_lock(&fdarrayMtx);
						if(fdArray[i].fd==-1){
							/*Ho trovato una posizione libera*/
							fdArray[i].fd=fd;
							strncpy(fdArray[i].nickname,req.hdr.sender,strlen(req.hdr.sender)+1);
							pthread_mutex_unlock(&fdarrayMtx);
							break;
						}
						pthread_mutex_unlock(&fdarrayMtx);
					}
				
					/*Aggiorno la relativa statistica*/
					pthread_mutex_lock(&statsMtx);
					chattyStats.nusers++;
					pthread_mutex_unlock(&statsMtx);
				}
				case(CONNECT_OP):{
					if(req.hdr.op==CONNECT_OP){
						/*Cerco il nickname del client nell'array dei fd o, altrimenti, lo aggiungo nuovamente*/
						for(int i=0; i<max_conn; i++){
							pthread_mutex_lock(&fdarrayMtx);
							if( strcmp(fdArray[i].nickname,req.hdr.sender)==0){
								/*Ho trovatoil nickname*/
								fdArray[i].fd=fd;
								pthread_mutex_unlock(&fdarrayMtx);
								break;
							}
							pthread_mutex_unlock(&fdarrayMtx);
						}
						/*Non ho trovato il nickname, quindi lo aggiungo nuovamente*/
						for(int i=0; i<max_conn; i++){
							pthread_mutex_lock(&fdarrayMtx);
							if(fdArray[i].fd==-1){
								/*Ho trovato una posizione libera*/
								fdArray[i].fd=fd;
								strncpy(fdArray[i].nickname,req.hdr.sender,strlen(req.hdr.sender)+1);
								pthread_mutex_unlock(&fdarrayMtx);
								break;
							}
							pthread_mutex_unlock(&fdarrayMtx);
						}
					}
				}
				case(USRLIST_OP):{
					/*Controllo che il client sia registrato*/
					if( icl_hash_find(usersHash,req.hdr.sender)==NULL){
						/*Invio il messaggio di errore*/
						message_t nickUnknown;
						setHeader(&nickUnknown.hdr,OP_NICK_UNKNOWN,"Server");
						sendRequest(fd,&nickUnknown);
						break;
					}
					/*Recupero la lista degli utenti connessi*/
					int offset=0;
					int length=chattyStats.nonline*(MAX_NAME_LENGTH+1);

					char *list_online=malloc(length*sizeof(char));

					pthread_mutex_lock(&fdarrayMtx);
					for(int i=0; i<max_conn; i++){
						if( fdArray[i].fd!=-1){
		                   			strncpy(list_online+offset,fdArray[i].nickname, MAX_NAME_LENGTH+1);
		                   			offset=offset+MAX_NAME_LENGTH+1;
						}
					}
					pthread_mutex_unlock(&fdarrayMtx);

					/*Invio il messaggio di ok e la lista utenti*/
					message_t onlineList;
					setHeader(&onlineList.hdr,OP_OK,"Server");
					setData(&onlineList.data,req.hdr.sender,(char*)list_online,length);
					sendRequest(fd,&onlineList);

					free(list_online);
				}break;
				case(POSTTXT_OP):{
	
					/*Leggo la parte dati del messaggio*/
					readData(fd,&req.data);

					/*Controllo che il client destinatario sia registrato*/
					userAcc_t* rec;
					if(  (rec=icl_hash_find(usersHash,req.data.hdr.receiver))==NULL){
						/*Invio il messaggio di errore*/
						message_t nickUnknown;
						setHeader(&nickUnknown.hdr,OP_NICK_UNKNOWN,"Server");
						sendRequest(fd,&nickUnknown);
						break;
					}
					
					/*Controllo che la lunghezza del messaggio non ecceda i limiti*/
					if( strlen(req.data.buf)>max_msg_size){
						/*Invio il messaggio di errore*/
						message_t msgTooLong;
						setHeader(&msgTooLong.hdr,OP_MSG_TOOLONG,"Server");
						sendRequest(fd,&msgTooLong);
						break;
					}

					/*Aggiorno preventivamente la statistica*/
					pthread_mutex_lock(&statsMtx);
					chattyStats.nnotdelivered++;
					pthread_mutex_unlock(&statsMtx);

					/*Salvo il messaggio nella history corrispondente al receiver e mando l'ok*/
					addHistory(&req, rec);
					sendRequest(fd,&mOk);
					/*Se il receiver è connesso, eseguo l'invio immediato*/
					for(int i=0; i<max_conn; i++){
						pthread_mutex_lock(&fdarrayMtx);
						if( strcmp(fdArray[i].nickname,req.data.hdr.receiver)==0 && fdArray[i].fd!=-1){
							/*Ho trovato il nickname e il client è online*/
							pthread_mutex_unlock(&fdarrayMtx);
							/*Eseguo l'invio*/
							sendRequest(fdArray[i].fd, &req);
							/*Aggiorno le relative statistiche*/
							pthread_mutex_lock(&statsMtx);
							chattyStats.nnotdelivered--;
							chattyStats.ndelivered++;
							pthread_mutex_unlock(&statsMtx);
							break;
						}
						pthread_mutex_unlock(&fdarrayMtx);
					}

				};
				case(POSTTXTALL_OP):{
				};
				case(POSTFILE_OP):{
				};
				case(GETFILE_OP):{
				};
				case(GETPREVMSGS_OP):{
					/*Ricerco il client nella tabella hash*/
					userAcc_t* client;
					client=icl_hash_find(usersHash,req.hdr.sender);

					/*Invio l'ok e il numero di messaggi presenti nella history*/
					message_t numMsgs;
					memset(&numMsgs,0,sizeof(message_t));	

					setHeader(&numMsgs.hdr,OP_OK,"Server");
					
					setData(&numMsgs.data,req.hdr.sender,(char*) &client->nMsgs,sizeof(client->nMsgs));
					//sendRequest(fd,&numMsgs);  
					
					/*Scorro la history e invio i messaggi presenti*/
					/*for(int i=0; i<client->nMsgs-1; i++){			
						sendRequest(fd,&client->hist[i]);*/
						/*Aggiorno la statistica*/
						/*pthread_mutex_lock(&statsMtx);
						chattyStats.nnotdelivered--;
						chattyStats.ndelivered++;
						pthread_mutex_unlock(&statsMtx);
					}*/

				};
				case(UNREGISTER_OP):{
				};
				case(DISCONNECT_OP):{
				};
				case(CREATEGROUP_OP):{
				};
				case(ADDGROUP_OP):{
				};
				case(DELGROUP_OP):{
				};
			}
			pthread_mutex_lock(&setMtx);
			FD_SET(fd,&set);
			if(fd>fdMax) fdMax=fd;
			pthread_mutex_unlock(&setMtx);
		}
	}
	pthread_exit(0);
}
