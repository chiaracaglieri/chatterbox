/*
 * membox Progetto del corso di LSO 2017
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Prencipe, Torquati
 * 
 */
/**
 * @file threads.h
 * @brief Libreria relativa alle funzioni dispatcher e worker eseguite dai thread del server
 */


void* dispatcher(void* fd);

void* worker(void* threadno);
