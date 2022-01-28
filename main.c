// Nicholas Delli Carpini

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

// ----- GLOBALS -----

unsigned int seed;
time_t start_time;

struct performer {
    u_int8_t id;
    u_int8_t type;
    int time;
};

// stage data
unsigned int stage_size = 4;
u_int8_t stage_type = 0;
int *stage_slots;

// number of performers
unsigned int num_flamenco = 15; // type = 1
unsigned int num_juggle = 8;    // type = 2
unsigned int num_soloist = 2;   // type = 3

unsigned int num_total;

// max time per performance
unsigned int time_flamenco;
unsigned int time_juggle;
unsigned int time_soloist;

struct performer *queue;

pthread_mutex_t type_lock;
pthread_mutex_t *slot_lock;

// ----- FUNCTIONS -----

// creates a single performer given its id and type
// unsigned int num - the id for the performer to have
// u_int8_t - the type of the performer
//
// returns the initialized performer
struct performer initialize_performer(unsigned int num, u_int8_t type) {
    struct performer ret_val;

    ret_val.id = num;
    ret_val.type = type;

    switch (type) {
        case 1:
            ret_val.time = rand() % time_flamenco + 1;
            break;
        case 2:
            ret_val.time = rand() % time_juggle + 1;
            break;
        case 3:
            ret_val.time = rand() % time_soloist + 1;
            break;
    }

    return ret_val;
}

// print the output and sleep for a performer thread's performance
// int slot - stage slot the performer is in
// int wait_time - the performance (sleep) time of the performer
// u_int8_t id - id of the performer
// u_int8_t type - type of the performer
//
// returns void
void perform(int slot, int wait_time, u_int8_t id, u_int8_t type) {
    stage_slots[slot] = 1;

    char *name;

    switch (type) {
        case 1:
            name = "Flamenco Dancer";
            break;
        case 2:
            name = "Juggler";
            break;
        case 3:
            name = "Soloist";
            break;
    }
    printf("%lds | %s [%d] has started on Stage %d\n    They will be performing for %ds\n"
           , (time(NULL) - start_time), name, id, slot, wait_time);

    sleep(wait_time);

    printf("%lds | %s [%d] has finished & is leaving Stage %d\n", (time(NULL) - start_time), name, id, slot);

    stage_slots[slot] = 0;
}

// thread for a single performer, waits until the type matches the performer and there's a free slot, and then perform
// void *args - (long) the position of the performer in the randomized queue
//
// return void
void *perform_thread(void *args) {
    long queue_id = (long) args;

    u_int8_t id = queue[queue_id].id;
    u_int8_t type = queue[queue_id].type;
    int time = queue[queue_id].time;

    int total_use;
    while(1) {
        total_use = 0;

        // keep checking for a free stage slot until found, then exit thread
        for (int i = 0; i < stage_size; i++) {
            // if the current slot is empty
            if (stage_slots[i] == 0) {
                // if the performer type!=3 & the stage has no set type
                if (stage_type == 0 && type != 3) {
                    // try to lock the type & current slot
                    if (pthread_mutex_trylock(&type_lock) == 0) {
                        if (pthread_mutex_trylock(&slot_lock[i]) == 0) {
                            stage_type = type;

                            perform(i, time, id, type);
                            pthread_mutex_unlock(&type_lock);
                            pthread_mutex_unlock(&slot_lock[i]);
                            pthread_exit(0);
                        }
                        // bc theres no way to check the status of a lock w/o locking it, undo the type_lock if the
                        // slot_lock was unsuccessful
                        else {
                            pthread_mutex_unlock(&type_lock);
                        }
                    }
                }
                // if the performer type matches the current stage type
                else if (stage_type == type && type != 3) {
                    // try to lock the current slot
                    if (pthread_mutex_trylock(&slot_lock[i]) == 0) {
                        perform(i, time, id, type);
                        pthread_mutex_unlock(&slot_lock[i]);
                        pthread_exit(0);
                    }
                }
            }
            else {
                total_use++;
            }
        }

        // if the stage is empty, then automatically start the performer (mainly for type=3)
        if (total_use == 0 && stage_slots[0] == 0) {
            // try to lock the type & current slot
            if (pthread_mutex_trylock(&type_lock) == 0) {
                if (pthread_mutex_trylock(&slot_lock[0]) == 0) {
                    stage_type = type;

                    perform(0, time, id, type);
                    pthread_mutex_unlock(&type_lock);
                    pthread_mutex_unlock(&slot_lock[0]);
                    pthread_exit(0);
                }
                // bc theres no way to check the status of a lock w/o locking it, undo the type_lock if the
                // slot_lock was unsuccessful
                else {
                    pthread_mutex_unlock(&type_lock);
                }
            }
        }
    }
}

// ----- MAIN -----
int main(int argc, char *argv[]) {
    // check if seed arg provided
    if (argc != 2) {
        printf("ERROR: Missing seed (either input file or number)\n");
        return 1;
    }

    // check if seed arg is file
    FILE *file = fopen(argv[1], "r");
    if (file == NULL) { // if invalid file launch arg
        seed = atoi(argv[1]);
        printf("Seed = %d [Read from input arg]\n", seed);
    }
    else {
        char fc = fgetc(file);
        char seed_char[12];
        for (int i = 0; fc != EOF; i++) {
            if (i < 11) { // bind the input num to 10 digits as max digits for uint
                seed_char[i] = fc;
            } else {
                printf("ERROR: Input value must be an unsigned 32-bit int\n");
                return 1;
            }
            fc = fgetc(file);
        }

        seed = atoi(seed_char);
        printf("Seed = %d [Read from input file]\n", seed);
    }

    srand(seed);
    printf("Stage Created: %d slots initialized\n", stage_size);
    
    num_total = num_flamenco + num_juggle + num_soloist;
    
    time_flamenco = rand() % 10 + 1;
    time_juggle = rand() % 10 + 1;
    time_soloist = rand() % 10 + 1;

    int temp_flamenco = 0;
    int temp_juggle = 0;
    int temp_soloist = 0;
    int pref_performer;
    int performer_set;

    queue = malloc(num_total * sizeof(struct performer));

    // randomize the order of the performers
    for (int i = 0; i < num_total; i++) {
        performer_set = 0;
        while (!performer_set) {
            pref_performer = rand() % 3 + 1;
            switch (pref_performer) {
                case 1:
                    if (temp_flamenco < num_flamenco) {
                        queue[i] = initialize_performer(temp_flamenco, 1);
                        performer_set = 1;
                        temp_flamenco++;
                    }
                    break;
                case 2:
                    if (temp_juggle < num_juggle) {
                        queue[i] = initialize_performer(temp_juggle, 2);
                        performer_set = 1;
                        temp_juggle++;
                    }
                    break;
                case 3:
                    if (temp_soloist < num_soloist) {
                        queue[i] = initialize_performer(temp_soloist, 3);
                        performer_set = 1;
                        temp_soloist++;
                    }
                    break;
            }
        }
    }

    // double check that all performers have been queued
    if ((temp_flamenco + temp_soloist + temp_juggle) != num_total) {
        printf("ERROR: Not all performers queued?\n");
        return 1;
    }

    printf("%d Flamenco Dancer(s) initialized\n", num_flamenco);
    printf("%d Juggler(s) initialized\n", num_juggle);
    printf("%d Soloist(s) initialized\n", num_soloist);

    pthread_t thread_id[num_total];
    stage_slots = malloc(stage_size * sizeof(int));
    slot_lock = malloc(stage_size * sizeof(pthread_mutex_t));

    // create the pthread locks
    if (pthread_mutex_init(&type_lock, NULL) != 0) {
        printf("ERROR: Failed to initialize type_lock\n");
        return 1;
    }
    for (int i = 0; i < stage_size; i++) {
        if (pthread_mutex_init(&slot_lock[i], NULL) != 0) {
            printf("ERROR: Failed to initialize slot_lock[%d]\n", i);
            return 1;
        }
    }

    // create the threads for each performer & start the timer for time offsets in perform()
    start_time = time(NULL);
    for (long i = 0; i < num_total; i++) {
        if (pthread_create(&thread_id[i], NULL, perform_thread, (void *) i) != 0) {
            printf("ERROR: Failed to initialize pthread[%ld]\n", i);
            return 1;
        }
    }

    // wait for all threads to finish
    for (int i = 0; i < num_total; i++) {
        pthread_join(thread_id[i], NULL);
    }

    pthread_mutex_destroy(&type_lock);
    for (int i = 0; i < stage_size; i++) {
        pthread_mutex_destroy(&slot_lock[i]);
    }

    free(queue);
    free(stage_slots);
    free(slot_lock);
    printf("%lds | The Spectacular is Finished\n", (time(NULL) - start_time));

    return 0;
}