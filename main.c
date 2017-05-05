/* Written by Jamy Spencer 01 Apr 2017 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h> 
#include <sys/msg.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "forkerlib.h"
#include "obj.h"
#include "timespeclib.h"


void AbortProc();
void AlarmHandler();

static struct list* user_list;
static struct timespec* sys_clock;
static user_mem_space_t* active_user_table;
static int shmid[2];
static frame_t frame_table[256];
static int msgQueues[3];

int main ( int argc, char *argv[] ){

	struct stats mystats;

	mystats.child_count = 0;
	mystats.total_spawned = 0;

	char* file_name = "test.out";
	int c, i, j, check;

    struct list* this_user;
	int max_users = 5;
	int max_time_next_rsrc_check = DEFAULT_TIME_NEXT_RSRC_CHECK;
	char* child_arg = (char*) malloc(digit_quan(DEFAULT_TIME_NEXT_RSRC_CHECK) + 1);
    char chr_buf[50];
	sprintf(child_arg, "%lu", DEFAULT_TIME_NEXT_RSRC_CHECK);
	int max_run_time = 20;
	int quantum = QUANTUM;
	int returning_child;
	struct timespec when_next_fork = zeroTimeSpec();
    struct timespec time_now;


	signal(2, AbortProc);
	signal(SIGALRM, AlarmHandler);
	user_list = NULL;

	while ( (c = getopt(argc, argv, "hl:m:q:r:t:")) != -1) {
		switch(c){
		case 'h':
			printf("-h\tHelp Menu\n");
			printf("-i\tChanges the chance that an interrupt will occur(default is 50)\n");
			printf("-l\tSet log file name(default is test.out)\n-m\tChanges the number of consumer processes(default is 18)\n");
			printf("-o\tChanges the maximum scheduling overhead in nanoseconds(default is 1000)\n");
			printf("-q\tChanges the base quantum used by processes in nanoseconds(default is 4000000)\n");
			printf("-t\tChanges the number of seconds to wait until the oss terminates all users and itself(default is 20)\n");
			return 0;
			break;
		case 'l':
			file_name = optarg;
			break;
		case 'm':
			max_users = atoi(optarg);
			if (max_users > MAX_USERS || max_users < 1){
				printf("Error: -s is out of acceptable range, set to %d\n", MAX_USERS);
				max_users = MAX_USERS;
			}
			break;
		case 'q':
			quantum = atoi(optarg);
			if (quantum < 0 || quantum > BILLION){
				printf("Error: quantum cannot be a negative number");
				exit(1);
			}
			break;
		case 'r':
			max_time_next_rsrc_check = atoi(optarg);
			if (max_time_next_rsrc_check > 10000 || max_time_next_rsrc_check < 0){
				printf("Error: Time between resource checks must be between 0 and 10000\n");
				exit(1);
			}
			else{
				child_arg = optarg;
			}
			break;
		case 't':
			max_run_time = atoi(optarg);
			if (max_run_time < 0){
				printf("Error: maximum run-time cannot be a negative number\n");
				exit(1);
			}
			break;
		case '?':
			return 1;
			break;
		}
	}
	
	alarm(max_run_time);

	//clear log file
	FILE* file_write = fopen(file_name, "w+");
	fclose(file_write);

    //Attach message queues
    MsgQueueAttach(msgQueues);
    int clock_lock = msgQueues[0];
    int page_lock = msgQueues[1];
    msg_t clock_key;
    clock_key.mtype = 1;
    int table_lock = msgQueues[1];
    msg_t table_key;
    table_key.mtype = 1;
    if ((msgsnd(clock_lock, &clock_key, sizeof(msg_t), 0)) == -1){
        perror("msgsnd, initial message");
    }
    if ((msgsnd(table_lock, &table_key, sizeof(msg_t), 0)) == -1){
        perror("msgsnd, initial message");
    }

	//init random numbers
	srand(time(0));

	//intitialize frame table to "addresses" and dirty bit vector to all zeros
	for (i = 0; i < 256; i++){
		frame_table[i].user_pid = 0;
		frame_table[i].rel_addr = 0;
		frame_table[i].flags = 0;
	}

    //make and initialize a utilization table
    int active_user_vector[MAX_USERS];
    for (int i; i < MAX_USERS; i++){
        active_user_vector[i] = 0;
    }

	//initialize user_clock and array of page_table in shared memory
	shrMemMakeAttach(shmid, &active_user_table, &sys_clock);
	sys_clock->tv_sec = 0;
	sys_clock->tv_nsec = 0;
	for (i = 0; i < MAX_USERS; i++){
		active_user_table[i].user_pid = 0;
        for (j = 0; j < 32; j++) {
            active_user_table[i].pages[j].rel_addr = 0;
        }
	}


	do{
		//advance clock
        if((msgrcv(clock_lock, &clock_key, sizeof(msg_t), 1, 0)) ==-1){
            perror("msgrcv");
        }

		*sys_clock = addLongToTimespec(rand() % 5555000 + 55000, sys_clock);
		check = cmp_timespecs(*sys_clock, when_next_fork);
        time_now = *sys_clock;

        if ((msgsnd(clock_lock, &clock_key, sizeof(msg_t), 0)) == -1){
            perror("msgsnd");
        }

		//Create new user if it is time.
		if (check >= 0 && mystats.total_spawned < 100 && time_now.tv_sec < 2 && mystats.child_count < max_users){
			if ((this_user = MakeChild(&user_list, time_now, child_arg)) == NULL){
				perror("MakeChild failed");
				AbortProc();			
			}
            for (i = 0; i < MAX_USERS; i++){
                if (active_user_vector[i] == 0){
                    active_user_vector[i] = 1;
                    active_user_table[i].user_pid = this_user->item.process_id;
                }
            }
			(mystats.total_spawned)++;
			(mystats.child_count)++;
			addLongToTimespec(rand() % MAX_SPAWN_DELAY + 1, &when_next_fork);
		}
        //logging
        for (i = 0; i < 20; i++){
            Log(file_name, chr_buf);
        }


	
		//Wait for returning users
		if ((returning_child = waitpid(-1, NULL, WNOHANG)) != 0){

			if (returning_child != -1){
				user_list = destroyNode(user_list, returning_child);
//				printf("Child %d returned/removed\n", returning_child);
				(mystats.child_count)--;
			}
	}

	}while(mystats.child_count > 0 || (sys_clock->tv_sec < 2 && mystats.total_spawned < 100));

    printf("exit conditions: child count:%d total:%d\n ", mystats.child_count, mystats.total_spawned);

    for (i = 0; i < 3; i++){
        msgctl(msgQueues[i], IPC_RMID, NULL);
    }
	shmdt(sys_clock);
	shmdt(active_user_table);
	shmctl(shmid[0], IPC_RMID, NULL);
	shmctl(shmid[1], IPC_RMID, NULL);

	return 0;
}



void AlarmHandler(){
	perror("Time ran out");
	AbortProc();
}

void AbortProc(){
    int i;
	KillUsers(user_list);
    for (i = 0; i < 3; i++){
        msgctl(msgQueues[i], IPC_RMID, NULL);
    }
	shmdt(active_user_table );
	shmdt(sys_clock);
	shmctl(shmid[1], IPC_RMID, NULL);
	shmctl(shmid[0], IPC_RMID, NULL);
	kill(0, 2);
	exit(1);
}