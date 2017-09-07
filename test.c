#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/select.h>
#include <pthread.h>
#include <signal.h>
#include "message_queue.h"
 
typedef struct {
	unsigned char message_type;
	unsigned char buf[128];
} out_message_info, * p_out_message_info;

static struct message_queue my_queue;

void* thread1(void*);  
void* thread2(void*);  
  
/* 
 * when program is started, a single thread is created, called the initial thread or main thread. 
 * Additional threads are created by pthread_create. 
 * So we just need to create two thread in main(). 
 */  
int main(int argc, char** argv)  
{  
    printf("enter main\n");  
    pthread_t tid1, tid2;  
    int rc1=0, rc2=0;  
	

	/* The biggest message we'll send
	* with this queue is 512 bytes, and
	* the queue can only be 128
	* messages deep */
	message_queue_init(&my_queue, sizeof(out_message_info), 128);
	

  
    rc1 = pthread_create(&tid1, NULL, thread1, NULL);  
    if(rc1 != 0)  
        printf("pthread2 create error! \n");   
	
	rc2 = pthread_create(&tid2, NULL, thread2, NULL);  
    if(rc2 != 0)  
        printf("pthread1 create error! \n");   
	
	
	while(1);
	
	/*и╬ЁЩ╤сап*/
//	message_queue_destroy(&my_queue);
}  

/* 
 * thread1() will be execute by thread1, after pthread_create() 
 * it will set g_Flag = 1; 
 */  
void* thread1(void* arg)  
{  
    printf("enter thread1\n");  
	p_out_message_info p_write_message;
	unsigned char i = 0;
	while (1) {
		p_write_message = message_queue_message_alloc_blocking(&my_queue);
		printf("write_pthread: free_blocks=%d, allocpos=%d, freepos=%d\n", my_queue.allocator.free_blocks, my_queue.allocator.allocpos, my_queue.allocator.freepos);
		
		p_write_message->buf[0] = i++;
		p_write_message->buf[1] = i++;
		printf("write_pthread: buf[0] = %d, buf[1] = %d\n", p_write_message->buf[0], p_write_message->buf[1]); 
		
		
		/* Construct the message here */
		message_queue_write(&my_queue, p_write_message);		
		printf("write_pthread: entries=%d, writepos=%d\n\n", my_queue.queue.entries, my_queue.queue.writepos);
		usleep(100000);
	}
}  
  
/* 
 * thread2() will be execute by thread2, after pthread_create() 
 * it will set g_Flag = 2; 
 */  
void* thread2(void* arg)  
{  
    printf("enter thread2\n");  
	p_out_message_info p_read_message;
	while (1) {
		/* Blocks until a message is available */
		p_read_message = message_queue_read(&my_queue);
		printf("read_pthread: entries=%d, readpos=%d\n", my_queue.queue.entries, my_queue.queue.readpos);
		printf("read_pthread: buf[0] = %d, buf[1] = %d\n\n", p_read_message->buf[0], p_read_message->buf[1]); 
		message_queue_message_free(&my_queue, p_read_message);

		sleep(1);
	} 
} 



















