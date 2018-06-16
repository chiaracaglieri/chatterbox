/*
 * membox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
/**
 * @file tools.h
 * @brief Header di funzioni di utilità per il sever chatty implementate in tools.c e variabili condivise
 */
#include <sys/select.h>
#include <stats.h>
#include <icl_hash.h>
#include <message.h>

/*Parametri di configurazione del server*/

char unix_path[100];
int max_conn;
int threads_pool;
int max_msg_size;
int max_file_size;
int max_hist_msgs;
char dir_name[100];
char stat_file_name[100];

/*Variabili della select*/

int fdMax;
fd_set set;
fd_set rdset;

/*Coda delle connessioni/richieste*/

struct fdRdy{
    int fd;
    struct fdRdy* fdNext;
    struct fdRdy* fdPrec;
};

void insQueue(int fd);
int extractFd();

typedef struct userAccount{
	char* nickname;
	message_t* hist;
	int nMsgs; //numero di messaggi correntemente nella history
	int addIndex;
}userAcc_t;

/*Tabella hash degli utenti registrati*/
icl_hash_t* usersHash;
int addHistory(message_t* msg, userAcc_t* rec);

/*Array dei file descriptor e nickname degli utenti connessi*/



typedef struct online{
	int fd;
	char* nickname;
	
}online_t;

online_t* fdArray;

/*Handler dei segnali*/
int terminate; //trigger per la terminazione dei thread

void termHandler();

void usr1Handler();

/*Gestione delle statistiche*/

struct statistics chattyStats;


