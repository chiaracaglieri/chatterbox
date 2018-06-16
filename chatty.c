/*
 * membox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
/**
 * @file chatty.c
 * @brief File principale del server chatterbox
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>

/* inserire gli altri include che servono */
#include <errno.h>
#include <tools.h>
#include <icl_hash.c>
#include <sys/un.h>
#include <sys/socket.h>
#include <threads.h>
#include <stats.h>
#include <connections.h>

/* struttura che memorizza le statistiche del server, struct statistics 
 * e' definita in stats.h.
 *
 */
struct statistics  chattyStats = { 0,0,0,0,0,0,0 };

int configParse(char* filePath){

    /*Apertura del file*/
    FILE* ifp= fopen(filePath,"r");
    if(ifp==NULL){
        perror("File di configurazione");
        return -1;
    }

    char* tmpBuffer=malloc(100*sizeof(char));
    int flag=0;

     /*Loop per leggere i parametri presenti nel file*/
    while(fscanf(ifp, "%s", tmpBuffer)==1){
        switch(flag){
            case 1:
                if(strcmp(tmpBuffer, "=")!=0){
       
                    strncpy(unix_path,tmpBuffer,strlen(tmpBuffer));
                    unix_path[strlen(tmpBuffer)]='\0';
                    flag=0;
                }break;
            case 2:
                if(strcmp(tmpBuffer, "=")!=0){
                    max_conn=atoi(tmpBuffer);
                    flag=0;
                }break;
            case 3:
                if(strcmp(tmpBuffer, "=")!=0){
                    threads_pool=atoi(tmpBuffer);
                    flag=0;
                }break;
            case 4:
                if(strcmp(tmpBuffer, "=")!=0){
                    max_msg_size=atoi(tmpBuffer);
                    flag=0;
                }break;
            case 5:
                if(strcmp(tmpBuffer, "=")!=0){
                    max_file_size=atoi(tmpBuffer);
                    flag=0;
                }break;
            case 6:
                if(strcmp(tmpBuffer, "=")!=0){
                    max_hist_msgs=atoi(tmpBuffer);
                    flag=0;
                }break;
            case 7:
                if(strcmp(tmpBuffer, "=")!=0){
                  
                    strncpy(dir_name,tmpBuffer,strlen(tmpBuffer));
		    dir_name[strlen(tmpBuffer)]='\0';
                   flag=0;
                }
		break;
            case 8:
                if(strcmp(tmpBuffer, "=")!=0){
                 
                    strncpy(stat_file_name,tmpBuffer,strlen(tmpBuffer));
		    stat_file_name[strlen(tmpBuffer)]='\0';
                    flag=0;
                }break;
        }

        if(strcmp(tmpBuffer, "UnixPath")==0) flag=1;
        if(strcmp(tmpBuffer, "MaxConnections")==0)flag=2;
        if(strcmp(tmpBuffer, "ThreadsInPool")==0)flag=3;
        if(strcmp(tmpBuffer, "MaxMsgSize")==0)flag=4;
        if(strcmp(tmpBuffer, "MaxFileSize")==0)flag=5;
        if(strcmp(tmpBuffer, "MaxHistMsgs")==0)flag=6;
        if(strcmp(tmpBuffer, "DirName")==0)flag=7;
        if(strcmp(tmpBuffer, "StatFileName")==0)flag=8;
        
    }
    free(tmpBuffer);
    fclose(ifp);
    return 0;
}

static void usage(const char *progname) {
    fprintf(stderr, "Il server va lanciato con il seguente comando:\n");
    fprintf(stderr, "  %s -f conffile\n", progname);
}

int main(int argc, char *argv[]) {

    /*Controllo il numero degli argomenti*/
    if(argc<3){
        usage(argv[0]);
        return -1;
    }

    /*Eseguo il parsing del file di configurazione*/
    if( configParse(argv[2])==-1){
	usage(argv[0]);
	return -1;
    }

	 /*Ignoro SIGPIPE per evitare di essere terminato da una scrittura su un socket chiuso*/
	struct sigaction s;
	memset(&s,0,sizeof(s));    
	s.sa_handler=SIG_IGN;
	if ( (sigaction(SIGPIPE,&s,NULL) ) == -1 ) {   
		perror("sigaction: SIGPIPE");
		return -1;
    	} 

	 /*Installo i gestori dei segnali*/
	terminate=0;

	struct sigaction termAction; // terminazione immediata: SIGINT, SIGTERM, SIGQUIT
	memset(&termAction,0,sizeof(termAction));   
	termAction.sa_handler = termHandler;
 	if ( (sigaction(SIGINT,&termAction,NULL) ) == -1 ) {   
		perror("sigaction: termAction");
		return -1;
   	 } 
	if ( (sigaction(SIGTERM,&termAction,NULL) ) == -1 ) {   
		perror("sigaction: termAction");
		return -1;
   	 } 
	if ( (sigaction(SIGQUIT,&termAction,NULL) ) == -1 ) {   
		perror("sigaction: termAction");
		return -1;
   	 } 

	struct sigaction statsAction; // scrittura delle statistiche: SIGUSR1
	memset(&statsAction,0,sizeof(statsAction));   
	statsAction.sa_handler = usr1Handler;
 	if ( (sigaction(SIGUSR1,&statsAction,NULL) ) == -1 ) {   
		perror("sigaction:statsAction");
		return -1;
   	 } 

	/*Inizializzo le strutture dati necessarie*/
	
	usersHash=icl_hash_create(24,&hash_pjw, NULL); //tabella contenente gli utenti registrati

	fdArray=malloc(max_conn*sizeof(online_t));

	for(int i=0; i<max_conn; i++){	//Elemento=-1 denota una posizione libera nell'array
		fdArray[i].fd=-1;
		fdArray[i].nickname=malloc(MAX_NAME_LENGTH*sizeof(char));
	}

	/*Inizializzo la connessione*/

	int fd;
    	struct sockaddr_un sa;
	unlink(unix_path);
    	sa.sun_family=AF_UNIX;
    	strncpy(sa.sun_path,(char*) unix_path, sizeof(sa.sun_path));
        if( (fd=socket(AF_UNIX,SOCK_STREAM,0))<0){
        	perror("Socket");
       		return -1;
    	}

   	if ( bind(fd,(struct sockaddr *)&sa, sizeof(sa))<0){
        	perror("bind");
		return -1;
   	 }

   	listen(fd,SOMAXCONN); 

	/*Creazione dei thread dispatcher e workers*/
	
	pthread_t tidArray[threads_pool+1];
    
  	if( pthread_create(&tidArray[0], NULL, dispatcher, (void*) &fd)<0){
        	perror("Creazione thread dispatcher");
        	return -1;
   	 }
    
   	 for(int i=1; i<threads_pool+1; i++){
       		 if( (pthread_create(&tidArray[i], NULL, worker, (void*) (size_t) i))<0){
          		        perror("Creazione thread workers");
				return -1;
       		 }
  	  }

	/*Faccio la join dei thread creati*/
	for(int i=0; i<threads_pool+1; i++){
		pthread_join(tidArray[i],NULL);
	}

	/*Libero la memoria*/
	free(usersHash->buckets);
	free(usersHash);

   	 return 0;
}
