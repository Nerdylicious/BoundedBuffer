/**
 * printer.c
 *
 * PURPOSE:		This program implements the key functionality of a
 *                  multithreaded print manager.
 *
 * TO COMPILE:		gcc -Wall printer.c -o prog -lpthread
 * TO RUN:		./prog NumThreads NumPrinters
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#define BUFFER_SIZE 3

enum BOOL{
	false = 0,
	true = 1
};
typedef enum BOOL boolean;

pthread_mutex_t count_mutex;		// the 'method' mutex
pthread_cond_t empty_slot;		// the signaling condition variable
pthread_cond_t full_slot;

typedef struct PR {

	long clientID;
	char *fileName; //ptr to a dynamically alloc’d string
	int fileSize;
} PrintRequest,*PrintRequestPTR;

//globally shared buffer
PrintRequest BoundedBuffer[BUFFER_SIZE];
int length_buffer = 0;
int front = 0;
int back = 0;

/**
 * PURPOSE: Insert the request into the bounded buffer. Only one thread is allowed
 *          to insert into the buffer at any given time. A thread is only allowed
 *          to insert into the buffer if there is at least one empty slot in the buffer.
 * 
 * INPUT PARAMETERS:
 *		request:		The request to insert into the buffer.
 * 
 * OUTPUT PARAMETERS:
 *		None
 *
 */
void insertIntoBuffer(PrintRequest request){
	
	time_t current_time;
	
	pthread_mutex_lock(&count_mutex);
	while(length_buffer == BUFFER_SIZE){
		//wait on an empty slot
		pthread_cond_wait(&empty_slot, &count_mutex);
		
	}
	//put item into buffer
	length_buffer ++;
	back = (front + length_buffer - 1) % BUFFER_SIZE;
	BoundedBuffer[back] = request;

	time(&current_time);
	printf("\n\nThread <%lu> insert\nTime: %sFile: %s", pthread_self(), ctime(&current_time), request.fileName);
	fflush(stdout);
	
	//indicate there is a full slot in buffer
	pthread_cond_signal(&full_slot);
	pthread_mutex_unlock(&count_mutex);

}

/**
 * PURPOSE: Removed the request from the bounded buffer. Only one thread is allowed
 *          to remove from the buffer at any given time. A thread is only allowed
 *          to remove from the buffer if there is at least one full slot in the buffer.
 * 
 * INPUT PARAMETERS:
 *		None
 * 
 * OUTPUT PARAMETERS:
 *		request:		The request removed from the buffer.
 *
 */
PrintRequest removeFromBuffer(){

	pthread_mutex_lock(&count_mutex);
	while(length_buffer == 0){
		//wait for a full slot
		pthread_cond_wait(&full_slot, &count_mutex);
	}
	//remove item from buffer
	PrintRequest request = BoundedBuffer[front];
	length_buffer --;
	front = (front + 1) % BUFFER_SIZE;	

	//indicate that there is now an empty slot in buffer
	pthread_cond_signal(&empty_slot);
	pthread_mutex_unlock(&count_mutex);
	return request;

}

/**
 * PURPOSE: Create a new PrintRequest.
 *
 */
PrintRequest create_print_request(int i){

	PrintRequest request;
	char *pid = malloc(sizeof(char)*50);
	char *iteration = malloc(sizeof(int));
	char *file = "FILE";
	char *delimiter = "_";
	int randnum;
	
	request.clientID = pthread_self();
	
	//construct the filename
	sprintf(pid, "%lu", pthread_self());
	sprintf(iteration, "%d", i + 1);
	request.fileName = malloc(strlen(file) + strlen(pid) + strlen(iteration) + (2*strlen(delimiter) + 2) + 3);
	sprintf(request.fileName, "%s%s%s%s%s", file, delimiter, pid, delimiter, iteration);
	//printf("\nfilename: %s", request.fileName); 	
	
	randnum = rand() % 19801 + 200;	//ranges from 200 to 20000
	request.fileSize = randnum;
		
	return request;
}


/**
 * PURPOSE: Insert a new PrintRequest into the bounded buffer by a thread.
 *
 */
void *PrintClient(void *tid){

	PrintRequest request;
	int sleep_time;
	
	int i;
	for(i = 0; i < 6; i ++){
	
		request = create_print_request(i);
		insertIntoBuffer(request);

		sleep_time = rand() % 3 + 1;
		sleep(sleep_time);
	}
	printf("\n\n[**************thread <%lu> done**************]", pthread_self());
	fflush(stdout);

	pthread_exit(NULL);
}

/**
 * PURPOSE: Run an infinite loop attempting to remove entries from the bounded
 *          buffer.
 *
 */
void *PrintServer(void *tid){

	PrintRequest request;
	time_t current_time;
	int sleep_time;

	while(1){
	
		request = removeFromBuffer();

		time(&current_time);
		printf("\n\nThread <%lu> removed\nTime: %sClient ID: %ld\nFile: %s\nFile Size: %d\nStarting print job", pthread_self(), ctime(&current_time), request.clientID, request.fileName, request.fileSize);
		fflush(stdout);
		
		sleep_time = request.fileSize / 8000; //assume filesize is in number of chars
		sleep(sleep_time);	

		time(&current_time);
		printf("\n\nThread <%lu> finished\nTime: %sClient ID: %ld\nFile: %s\nFile Size: %d\nPrint job complete", pthread_self(), ctime(&current_time), request.clientID, request.fileName, request.fileSize);
		fflush(stdout);

	}

	pthread_exit(NULL);
}

int main (int argc, char * argv[]) {

	srand(time(NULL));
	int NumPrintClients;
	int NumPrinters;

	if(argc != 3){
		printf("\nusage: ./prog NumPrintClients NumPrinters\n\n");
		exit(-1);
	}
	NumPrintClients = atoi(argv[1]);
	NumPrinters = atoi(argv[2]);
	pthread_t client_threads[NumPrintClients];
	pthread_t printer_threads[NumPrinters];
	
	pthread_mutex_init(&count_mutex, NULL);
  	pthread_cond_init(&empty_slot, NULL);
  	pthread_cond_init(&full_slot, NULL);
	
	long i;
	int rc;
	for(i = 0; i < NumPrintClients; i ++){
		
		rc = pthread_create(&client_threads[i], NULL, PrintClient, (void *)i);
		if(rc){
			printf("ERROR:rc is %d\n", rc); 
			exit(-1);
		}
	}
	for(i = 0; i < NumPrinters; i ++){
		rc = pthread_create(&printer_threads[i], NULL, PrintServer, (void *)i);
		if(rc){
			printf("ERROR:rc is %d\n", rc); 
			exit(-1);
		}
	}
	
   	for(i = 0; i < NumPrintClients; i ++){
		pthread_join(client_threads[i],NULL);
 	}
 	
    	for(i = 0; i < NumPrinters; i ++){
		pthread_join(printer_threads[i],NULL);
 	}

	/* Clean up and exit after all threads have finished */
	pthread_mutex_destroy(&count_mutex);
	pthread_cond_destroy(&empty_slot);
	pthread_cond_destroy(&full_slot);


	pthread_exit(NULL);
	return 0;
}











