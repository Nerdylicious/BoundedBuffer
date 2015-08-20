/**
 * messageQueue.c
 *
 * PURPOSE:		This program implements the key functionality of a
 *                  multithreaded print manager, using Posix message queues.
 *
 * TO COMPILE:		gcc -Wall messageQueue.c -o prog -lpthread -lrt
 * TO RUN:		./prog NumThreads NumPrinters
 *
 * NOTE: Since mq_send and mq_receive only support sending messages of type const char*
 *       I needed to do something like a "serialization/deserialization". I converted
 *       the PrintRequest I wanted to send into a type char* and when I received the char* 
 *       it was reconstructed back into a PrintRequest.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <mqueue.h>

#define BUFFER_SIZE 3
#define MESSAGE_SIZE 1024
#define ARG_SIZE 20

enum BOOL{
	false = 0,
	true = 1
};
typedef enum BOOL boolean;

typedef struct PR {

	long clientID;
	char *fileName; //ptr to a dynamically alloc’d string
	int fileSize;
} PrintRequest,*PrintRequestPTR;

//globally shared buffer
//PrintRequest BoundedBuffer[BUFFER_SIZE];
//our bounded buffer is now the message queue
//int length_buffer = 0;
//int front = 0;
//int back = 0;

struct mq_attr attr;
mqd_t mq;
unsigned priority;
int status;

/**
 * PURPOSE: Tokenize a string that has information of a PrintRequest
 *
 */
int tokenize_input(char *args[], char input[]){

	int count = 0;
	int arg_count = 0;

	if(input != NULL){
		args[0] = strtok(input," ");

		while (args[count] != NULL) {
	
			count++;
			args[count] = strtok(NULL, " ");
			arg_count++;
		} 

		count++;
		args[count] = '\0';
	}
	
	return arg_count;
}

/**
 * PURPOSE: Insert the request into the message queue. Only one thread is allowed
 *          to insert into the buffer at any given time. A thread is only allowed
 *          to insert into the buffer if there is at least one empty slot in the buffer,
 *          if not mq_send blocks.
 * 
 * INPUT PARAMETERS:
 *		request:		The request to insert into the buffer.
 * 
 * OUTPUT PARAMETERS:
 *		None
 *
 */
void insertIntoBuffer(PrintRequest request, int i){
	
	time_t current_time;
	char buffer[MESSAGE_SIZE];
	//serialize
	sprintf(buffer, "%ld %s %d", request.clientID, request.fileName, request.fileSize);

	if(mq_send(mq, buffer, MESSAGE_SIZE, 2) == -1){
	//if(mq_send(mq, (const char*)&request, MESSAGE_SIZE, 2) == -1){
		perror("\nError in mq_send");
		exit(1);
	}
	else{
	
		time(&current_time);
		printf("\n\nThread <%lu> insert\nTime: %sFile: %s", pthread_self(), ctime(&current_time), request.fileName);

		fflush(stdout);
	}

}

/**
 * PURPOSE: Removed the request from the bounded buffer. Only one thread is allowed
 *          to remove from the buffer at any given time. A thread is only allowed
 *          to remove from the buffer if there is at least one full slot in the buffer,
 *          if not mq_receive blocks.
 * 
 * INPUT PARAMETERS:
 *		None
 * 
 * OUTPUT PARAMETERS:
 *		request:		The request removed from the buffer.
 *
 */
PrintRequest removeFromBuffer(char *args[]){
		
	PrintRequest request;
	char buffer[MESSAGE_SIZE];

	//if(mq_receive(mq, (char*)&request, MESSAGE_SIZE, &priority) == -1){
	if(mq_receive(mq, buffer, MESSAGE_SIZE, &priority) == -1){

		perror("\nError in mq_receive");
		exit(1);
	}
	else{
		//deserialize
		tokenize_input(args, buffer);	
		request.clientID = atol(args[0]);
		request.fileName = args[1];
		request.fileSize = atoi(args[2]);
	}
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
		insertIntoBuffer(request, i);
		
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
	char *args[ARG_SIZE];

	while(1){
		
		request = removeFromBuffer(args);

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

	int NumPrintClients;
	int NumPrinters;
	srand(time(NULL));
	
	attr.mq_maxmsg = 3;
	attr.mq_msgsize = MESSAGE_SIZE;
	attr.mq_flags = 0;

	if (-1 == (mq = mq_open("/PGmq", O_RDWR | O_CREAT, 0664, &attr))) {
  		perror("mq_open croaked");
     }
     
     if(mq_getattr(mq, &attr) == -1){
		perror("mq_getattr");
	}
	printf("\nMax number messages of queue: %ld", attr.mq_maxmsg);
	
	if(argc != 3){
		printf("\nusage: ./prog NumPrintClients NumPrinters\n\n");
		exit(-1);
	}
	NumPrintClients = atoi(argv[1]);
	NumPrinters = atoi(argv[2]);
	pthread_t client_threads[NumPrintClients];
	pthread_t printer_threads[NumPrinters];

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
  	mq_close(mq);
	mq_unlink("/PGmq");
	pthread_exit(NULL);
	return 0;
	
	
}














