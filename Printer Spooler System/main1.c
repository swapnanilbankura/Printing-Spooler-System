#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

//Function Declerations
void PushToQueue(int fileSize);
void DisplayQueue();
int GetUserInput(char *inputType );
int GetRandomFileSize();
void* PrintFiles(void* inputs);
void* EnqueuePrintRequest(void* inputs);
void QueueManager(int printCount);

// Variables for queues : https://www.journaldev.com/36220/queue-in-c
// For our project we need a simple queue logic so I didnot implement linked list 
# define SIZE 100000
int inp_arr[SIZE];
int Rear = - 1, Front = - 1;

// Semaphore variables
sem_t sendToPrinter;
sem_t bufferAvailable;
pthread_mutex_t mutexPrinter;

// Other Variables
int jobCounter = 1, minFileSize = 50, maxFileSize = 512;
int countPrintJobs , maxBufferSize, randomFileSize,totalFileSize;
int bufferSize, cancelledJobCounter = 0, printedCounter = 0, enqueuedCounter = 0;

// struct, which is used to pass parameters to pthreads
struct args {
    int id;
    int fileSize;
};

void main() 
{
	printf("Printer Spooler ( MultiThread Semaphore version )\n");
	printf("Bedirhan Bardakci - CSE331 Spring 2021 Assignment 2-a\n");
	
	// For fixed size testings
	countPrintJobs = 10;
	bufferSize = 1000;

	// Uncomment two below lines if you want interactive inputs
	//countPrintJobs = getUserInput("number of printing jobs");
	//bufferSize = getUserInput("buffer size (Kb) of printer");
	
	maxBufferSize = bufferSize;
	
	printf("\nTotal Number of Printing Jobs To Simulate is %d\n",countPrintJobs);
	printf("\nFree Buffer is %d Kb\n\n",bufferSize);
	
	QueueManager(countPrintJobs);
}

void QueueManager(int printCount) 
{
	// to randomize file sizes
	srand(time(NULL));
	int i = 0, lastCheckedJob = 0;
	for (i = 0; i < printCount; i++)
	{
		PushToQueue(GetRandomFileSize());
	}
	
	DisplayQueue();
	
	// initialize semaphores
	sem_init(&bufferAvailable, 0, 1);
	sem_init(&sendToPrinter, 0, 1);
	
	// to guarantee printing one by one with using pthread_mutex_lock & pthread_mutex_unlock
	pthread_mutex_init(&mutexPrinter, NULL);
	
	// identify 2 threads , 1 for enqueue operations, 1 for print operations
	pthread_t enqueueThread[printCount],printThreads[printCount];

	// 0.05 = every 5Kb , if you want much more accurate tracking change it to 0.01
	struct timespec ts = {0, 0.05*1000000000L };
	
	struct timespec ts2 = { 0, 0.2 * 1000000000L};	// check evry 0.2 seconds
	
	while(jobCounter <= printCount)
	{	
		// for much more accurate Buffer tracking I use nanosleep
		nanosleep (&ts, NULL);
		
		if (Front == - 1 || Front > Rear)
    	{
        	printf("\nQueue Empty \n");
        	return ;
    	}
	    else
	    {
	    	int tempFileSize = inp_arr[Front];
	    	
	    	if(lastCheckedJob != jobCounter)
	    	{
	    		printf("\nJob %d arrived with size %d Kb", jobCounter, tempFileSize);
	    		lastCheckedJob = jobCounter;
			}
	        	
	    	if (tempFileSize > maxBufferSize)
			{
				printf("\nJob %d cancelled and will not be queued, because it is bigger than printer max buffer size %d Kb\n", jobCounter, maxBufferSize);
				lastCheckedJob = jobCounter;
				
				tempFileSize = inp_arr[Front];
				
				//dequeue because will never be enqueued
				Front = Front + 1;
				
				cancelledJobCounter++;
				jobCounter++;	
			}
			else
			{
				if(bufferSize >= tempFileSize)
				{
		        	struct args *myParameters = (struct args*)malloc(sizeof(struct args));
				
					myParameters->id = jobCounter;
					myParameters->fileSize = tempFileSize;
				
					//dequeue because enqueued soon
					Front = Front + 1;
					
					enqueuedCounter++;
					// creating pthreads
					pthread_create(&enqueueThread[enqueuedCounter],NULL,EnqueuePrintRequest,(void*)myParameters); 
					sem_post(&bufferAvailable);
	    			pthread_create(&printThreads[enqueuedCounter],NULL,PrintFiles,(void*)myParameters); 
					
					pthread_join(enqueueThread[enqueuedCounter],NULL);
					
					tempFileSize = inp_arr[Front];
					
					jobCounter++;						
				}
				else
				{
					nanosleep(&ts2, NULL);
					printf("\nJob %d - File size : %d Kb is waiting for available buffer, current empty buffer :%d",jobCounter, tempFileSize, bufferSize);	
				}
			}					
	    }
	}
	
	// to guarantee all printings will be finished
	if(cancelledJobCounter < printCount)
	{
	    for(i= 1; i <= enqueuedCounter; i++)
    	{
    		pthread_join(printThreads[i],NULL);
    	}	
	}

	printf("\nTotal Printed Pages :  %d", printCount - cancelledJobCounter);	
}

void* PrintFiles(void* inputs) // Print Thread
{
	sem_wait(&sendToPrinter);
	
	if (cancelledJobCounter < countPrintJobs)
	{
	    pthread_mutex_lock(&mutexPrinter);
    	int fileSize = ((struct args*)inputs)->fileSize;
    	int threadId = ((struct args*)inputs)->id;
    	
    	//Print Time Calculation - 1Kb in 0.01 actual seconds.
    	double printTime = fileSize * 0.01;
      	
        int intpart = (int)printTime;
        double decpart = printTime - intpart;
    
        // 1 seconds = 1000000000 nanoseconds
        // nanosleep ref : 
    	// https://linuxquestions.org/questions/programming-9/can-someone-give-me-the-example-of-the-simplest-usage-of-nanosleep-in-c-4175429688/
    	struct timespec ts = {0, 0.01*1000000000L };
    		
    	printf("\nJob %d starts printing for %.2lf seconds\n",threadId, printTime);
    
    	while(fileSize > 0)
    	{
    		// Release buffer for each 1Kb
    		nanosleep (&ts, NULL);
    		bufferSize = bufferSize + 1;
    		fileSize = fileSize - 1;
    	}
    
        printf("\nJob %d completed printing, Available Free Buffer : %d Kb\n",threadId, bufferSize);
        printedCounter++;
        pthread_mutex_unlock(&mutexPrinter);
	}
}

void* EnqueuePrintRequest(void* inputs) // Enqueue Thread
{
	sem_wait(&bufferAvailable);

	int fileSize = ((struct args*)inputs)->fileSize;
    int threadId = ((struct args*)inputs)->id;
    
    if(bufferSize >= fileSize)
    {	
    	bufferSize = bufferSize - fileSize;
    	
    	printf("\n\nJob %d enqueued now with size %d Kb, thread id %lu, buffer left : %d Kb\n",threadId, fileSize, pthread_self() , bufferSize);  	

		sem_post(&sendToPrinter);		
	}    
}

int GetUserInput(char *inputType )
{
	// pass inputType as parameter to reduce duplicate code for checking inputs
	int temp;
	do
	{
    	printf("\nPlease enter the %s : ",inputType );
    	
    	if(scanf("%d", &temp))
		{
			return temp;
        }
    	else
		{
        	fflush(stdin); 
        	printf("Invalid input! Please enter value as integer\n");
        }
	}while(1);
}

// getting random numbers in a range
int GetRandomFileSize() 
{
	return (rand() % (maxFileSize - minFileSize + 1)) + minFileSize;
}

void PushToQueue(int fileSize)
{
    if (Rear == SIZE - 1)
    {
    	printf("Overflow \n");
	}
    else
    {
        if (Front == - 1)
       	{	   
		   Front = 0;
		}
        Rear = Rear + 1;
        inp_arr[Rear] = fileSize;
    }
} 
  
void DisplayQueue()
{
    if (Front == - 1)
    {
    	printf("Empty Queue \n");
	}
    else
    {
        int i = 0;
        for (i = Front; i <= Rear; i++)
        {
        	printf("%d Kb , ", inp_arr[i]);
		}
        printf("\n");
    }
} 