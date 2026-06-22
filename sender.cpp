#include <sys/shm.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "msg.h"    /* For the message struct */

/* The size of the shared memory segment */
#define SHARED_MEMORY_CHUNK_SIZE 1000

/* The ids for the shared memory segment and the message queue */
int shmid, msqid;

/* The pointer to the shared memory */
void* sharedMemPtr;

/**
 * Sets up the shared memory segment and message queue
 * @param shmid - the id of the allocated shared memory 
 * @param msqid - the id of the allocated message queue
 */
void init(int& shmid, int& msqid, void*& sharedMemPtr)
{
    /* Create keyfile.txt and generate the key */
    FILE* fp = fopen("keyfile.txt", "w");
    fprintf(fp, "Hello world");
    fclose(fp);

    key_t key = ftok("keyfile.txt", 'a');
    if(key < 0)
    {
        perror("ftok");
        exit(-1);
    }

    /* Get the id of the shared memory segment (receiver creates it, sender just attaches) */
    shmid = shmget(key, SHARED_MEMORY_CHUNK_SIZE, 0666);
    if(shmid < 0)
    {
        perror("shmget");
        exit(-1);
    }

    /* Attach to the shared memory */
    sharedMemPtr = shmat(shmid, NULL, 0);
    if(sharedMemPtr == (void*)-1)
    {
        perror("shmat");
        exit(-1);
    }

    /* Attach to the message queue */
    msqid = msgget(key, 0666);
    if(msqid < 0)
    {
        perror("msgget");
        exit(-1);
    }
}
/**
 * Performs the cleanup functions
 * @param sharedMemPtr - the pointer to the shared memory
 * @param shmid - the id of the shared memory segment
 * @param msqid - the id of the message queue
 */
void cleanUp(const int& shmid, const int& msqid, void* sharedMemPtr)
{
    /* Detach from shared memory (sender does not own it, so no destruction) */
    if(shmdt(sharedMemPtr) < 0)
    {
        perror("shmdt");
    }
}
/**
 * The main send function
 * @param fileName - the name of the file
 * @return - the number of bytes sent
 */
unsigned long sendFile(const char* fileName)
{
    /* A buffer to store message we will send to the receiver. */
    message sndMsg;

    /* A buffer to store message received from the receiver. */
    ackMessage rcvMsg;

    /* The number of bytes sent */
    unsigned long numBytesSent = 0;

    /* Open the file */
    FILE* fp = fopen(fileName, "r");

    /* Was the file open? */
    if(!fp)
    {
        perror("fopen");
        exit(-1);
    }
    /* Read the whole file */
    while(!feof(fp))
    {
        /* Read at most SHARED_MEMORY_CHUNK_SIZE from the file and
         * store them in shared memory. fread() will return how many bytes it has
         * actually read. This is important; the last chunk read may be less than
         * SHARED_MEMORY_CHUNK_SIZE.
         */
        sndMsg.size = fread(sharedMemPtr, sizeof(char), SHARED_MEMORY_CHUNK_SIZE, fp);
		if(ferror(fp))
        {
            perror("fread");
            exit(-1);
        }

        /* Count the number of bytes sent */
        numBytesSent += sndMsg.size;

        /* Send a message to the receiver telling it the data is ready to be read */
        sndMsg.mtype = SENDER_DATA_TYPE;
        if(msgsnd(msqid, &sndMsg, sizeof(message) - sizeof(long), 0) < 0)
        {
            perror("msgsnd");
            exit(-1);
        }
        /* Wait until the receiver acknowledges it has finished saving the chunk */
        if(msgrcv(msqid, &rcvMsg, sizeof(ackMessage) - sizeof(long), RECV_DONE_TYPE, 0) < 0)
        {
            perror("msgrcv");
            exit(-1);
        }
    }
    /* Tell the receiver there is nothing more to send (size = 0) */
    sndMsg.mtype = SENDER_DATA_TYPE;
    sndMsg.size = 0;
    if(msgsnd(msqid, &sndMsg, sizeof(message) - sizeof(long), 0) < 0)
    {
        perror("msgsnd");
        exit(-1);
    }

    /* Close the file */
    fclose(fp);

    return numBytesSent;
}

/**
 * Used to send the name of the file to the receiver
 * @param fileName - the name of the file to send
 */
void sendFileName(const char* fileName)
{
    /* Get the length of the file name */
    int fileNameSize = strlen(fileName);

    /* Make sure the file name does not exceed the maximum buffer size */
    if(fileNameSize > MAX_FILE_NAME_SIZE)
    {
        fprintf(stderr, "Error: file name exceeds maximum allowed size of %d characters.\n",
                MAX_FILE_NAME_SIZE);
        exit(-1);
    }

    /* Create an instance of the struct representing the file name message */
    fileNameMsg msg;

    /* Set the message type */
    msg.mtype = FILE_NAME_TRANSFER_TYPE;

    /* Set the file name in the message */
    strncpy(msg.fileName, fileName, MAX_FILE_NAME_SIZE);

    /* Send the message using msgsnd */
    if(msgsnd(msqid, &msg, sizeof(fileNameMsg) - sizeof(long), 0) < 0)
    {
        perror("msgsnd");
        exit(-1);
    }
}
int main(int argc, char** argv)
{
    /* Check the command line arguments */
    if(argc < 2)
    {
        fprintf(stderr, "USAGE: %s <FILE NAME>\n", argv[0]);
        exit(-1);
    }

    /* Connect to shared memory and the message queue */
    init(shmid, msqid, sharedMemPtr);

    /* Send the name of the file */
    sendFileName(argv[1]);

    /* Send the file */
    fprintf(stderr, "The number of bytes sent is %lu\n", sendFile(argv[1]));

    /* Cleanup */
    cleanUp(shmid, msqid, sharedMemPtr);
    return 0;
}