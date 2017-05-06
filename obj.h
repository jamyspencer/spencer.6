/* Written by Jamy Spencer 01 Apr 2017 */
#ifndef OBJ_H
#define OBJ_H

#define MAX_USERS 18
#define DEFAULT_TIME_NEXT_RSRC_CHECK 1000
#define MAX_SPAWN_DELAY 200000
#define BILLION 1000000000
#define CHANCE_OF_TERMINATION 10
#define QUANTUM 4000000
#define FALSE 0
#define TRUE !FALSE
#define USE_BIT 2
#define REQUEST_BIT 0
#define DIRTY_BIT 1
#define SET_SNGL_BIT(var,pos)     ( var |= (1 << (pos%32)) )
#define CLEAR_SNGL_BIT(var,pos)   ( var &= ~(1 << (pos%32)) )
#define CHECK_SNGL_BIT(var,pos) ( var >> (pos%32) & 1)



typedef struct user_mem_space{
    pid_t user_pid;
    unsigned short int flags;
    unsigned short int pages[32];
} user_mem_space_t;

typedef struct queue_msg{
	long int mtype;
	char mtext[1];
} msg_t;

typedef struct frame{
    pid_t user_pid;
    int rel_addr;
    unsigned short int flags;
}frame_t;

int shrMemMakeAttach(int* shmid, user_mem_space_t** table, struct timespec** clock);
void MsgQueueAttach(int* queues);
long pwr(long n, long p);
int digit_quan(long num);

#endif
