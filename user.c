/* Written by Jamy Spencer 01 Apr 2017 */
#include <semaphore.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>
#include "obj.h"
#include "timespeclib.h"

#define MAX_TIME_NEXT_TERM_CHECK 2500
#define IS_LOGGING 1

int is_terminating ();
static struct timespec* user_clock;
static user_mem_space_t* user_page_table;
int msgQueues[3];

int main ( int argc, char *argv[] ){

	int max_time_next_rsrc_op;
	int i, r;
	pid_t my_pid= getpid();
	int is_doing = TRUE;
	char chr_buf[30];
    user_mem_space_t my_page_table;

	//handle args or shutdown with error msg
	if (argc < 2){
		perror("Error: User process received too few arguments");
   //     printf("%s\n", argv[0]);
        //printf("%s\n", argv[1]);
       // printf("%s\n", argv[2]);
        return 1;
	}else{
		max_time_next_rsrc_op = atoi(argv[1]);
    }

    //Attach message queues
    MsgQueueAttach(msgQueues);
    int clock_lock = msgQueues[0];
    int page_lock = msgQueues[1];
    msg_t clock_key;
    clock_key.mtype = 1;
    int table_lock = msgQueues[1];
    msg_t table_key;
    table_key.mtype = 1;

    srand(my_pid);
    struct timespec next_terminate_check = randTime(0, MAX_TIME_NEXT_TERM_CHECK);
	struct timespec next_resource_change = randTime(0, max_time_next_rsrc_op);

	//get shared memory
	int shmid[2];

	if (shrMemMakeAttach(shmid, &user_page_table, &user_clock) == -1){
        perror("Failed to attach to shared memory");
        return 1;
    }

    for (i = 0; i < MAX_USERS; i++){
        if (user_page_table[i].user_pid == my_pid){
            my_page_table = user_page_table[i];
        }
    }

    //initialize page table
    if((msgrcv(table_lock, &table_key, sizeof(msg_t), 1, 0)) ==-1){
        perror("msgrcv");
    }
    for (int i = 0; i < 32; i++){
        my_page_table.pages[i].rel_addr = i;
    }
    printf("time after attach:%02lu:%09lu\n", user_clock->tv_sec, user_clock->tv_nsec);

    if ((msgsnd(table_lock, &table_key, sizeof(msg_t), 0)) == -1){
        perror("msgsnd");
    }


	while (is_doing){
		//check if waiting on resources
        if((msgrcv(clock_lock, &clock_key, sizeof(msg_t), 1, 0)) ==-1){
            perror("msgrcv");
        }
        printf("time on iterate:%02lu:%09lu\n", user_clock->tv_sec, user_clock->tv_nsec);
        if ((msgsnd(clock_lock, &clock_key, sizeof(msg_t), 0)) == -1){
            perror("msgsnd");
        }

		if (TRUE) {
//               printf("clock-before:%02lu%09lu vs change:%02lu%09lu\n", user_clock->tv_sec, user_clock->tv_sec, next_resource_change.tv_sec, next_resource_change.tv_nsec);

			plusEqualsTimeSpecs(&next_resource_change, randTime(0, MAX_TIME_NEXT_TERM_CHECK));

		}

		if (is_terminating()) {
			is_doing = FALSE;
		}
		else{
			plusEqualsTimeSpecs(&next_terminate_check, randTime(0, MAX_TIME_NEXT_TERM_CHECK));
		}


	//End Critical Section---------------------------------------------------			
	}
	//set all resource requests to 0

	shmdt(user_clock);
	return 0;
}

int is_terminating (){
	int cmpr = rand() % 100 + 1;
	if (cmpr > CHANCE_OF_TERMINATION){
		return TRUE;
	}
	return FALSE;
}

void Log(char* str){
	FILE* file_write = fopen("userLog.out", "a");
	fprintf(file_write,"%s", str);
	fclose(file_write);
	return;
}
