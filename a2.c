#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>

#include "a2_helper.h"

//----------------------------Named semaphores--=-----------------------------------//

sem_t* gSemaphore_N_1; //Used to syncronize threads (T2 and T3 of P4) and (T3 of P5)
sem_t* gSemaphore_N_2; //Used to syncronize threads (T2 and T3 of P4) and (T3 of P5)

//----------------------------------------------------------------------------------//


//---------------------------Unnamed semaphores-------------------------------------//

sem_t gSemaphore_1; //Used to syncronize threads T2 and T4 of P5
sem_t gSemaphore_2; //Used to syncronize threads T2 and T4 of P5

sem_t gSemaphore_3; //Used to syncronize threads of P2 (#Make sure that the number of "running" threads <= 5)
sem_t gSemaphore_4; //Used to syncronize threads of P2 (#When T2.13 arrives make sure that he waits until 4 more threads are "running" in the same time with him)
sem_t gSemaphore_5; //Used to syncronize threads of P2 (#Protect gCount variabile from race condition)
sem_t gSemaphore_6; //Used to syncronize threads of P2 (#Marks the arrival of T2.13)
sem_t gSemaphore_7; /*Used to syncronize threads of P2 (#Make sure that the number of "running" threads <= 4 until T2.13 arrives.
                                                        #When T2.13 arrives block the other threads until we have 4 more threads "running" in the same time with T2.13)*/

//----------------------------------------------------------------------------------//

int gCount = 0;

void* ThreadFunction_P2(void* argument)
{
    sem_wait(&gSemaphore_3);

    info(BEGIN, 2, *((int*)argument));

    sem_wait(&gSemaphore_5);
        gCount++;
    sem_post(&gSemaphore_5);

    int semValue;
    sem_getvalue(&gSemaphore_6, &semValue);
    if (semValue == 0 && gCount == 4)
    {
        sem_post(&gSemaphore_4);
    }

    sem_wait(&gSemaphore_7);
    sem_post(&gSemaphore_7);

    info(END,   2, *((int*)argument));

    sem_post(&gSemaphore_3);

    return NULL;
}

void* ThreadFunction_P2_T13(void* argument)
{
    info(BEGIN, 2, *((int*)argument));

    if (gCount != 4)
    {
        sem_wait(&gSemaphore_6);
        sem_wait(&gSemaphore_4);
    }

    info(END,   2, *((int*)argument));

    sem_post(&gSemaphore_7);

    sem_post(&gSemaphore_3);

    return NULL;
}

void* ThreadFunction_P4(void* argument)
{
    info(BEGIN, 4, *((int*)argument));

    info(END,   4, *((int*)argument));

    return NULL;
}

void* ThreadFunction_P4_T2(void* argument)
{
    info(BEGIN, 4, *((int*)argument));

    info(END,   4, *((int*)argument));

    sem_post(gSemaphore_N_2);

    return NULL;
}

void* ThreadFunction_P4_T3(void* argument)
{
    sem_wait(gSemaphore_N_1);

    info(BEGIN, 4, *((int*)argument));

    info(END,   4, *((int*)argument));

    return NULL;
}

void* ThreadFunction_P5(void* argument)
{
    info(BEGIN, 5, *((int*)argument));

    info(END,   5, *((int*)argument));

    return NULL;
}

void* ThreadFunction_P5_T2(void* argument)
{
    info(BEGIN, 5, *((int*)argument));

    sem_post(&gSemaphore_1);
    sem_wait(&gSemaphore_2);

    info(END,   5, *((int*)argument));

    return NULL;
}

void* ThreadFunction_P5_T3(void* argument)
{
    sem_wait(gSemaphore_N_2);

    info(BEGIN, 5, *((int*)argument));

    info(END,   5, *((int*)argument));

    sem_post(gSemaphore_N_1);

    return NULL;
}

void* ThreadFunction_P5_T4(void* argument)
{
    sem_wait(&gSemaphore_1);

    info(BEGIN, 5, *((int*)argument));
    info(END,   5, *((int*)argument));

    sem_post(&gSemaphore_2);

    return NULL;
}


int main()
{
    init(); //Tester initialization

    info(BEGIN, 1, 0); //Begin of P1

    pid_t pid_2 = fork();
    if (pid_2 == -1)
    {
        perror("Failed to create process P2");
        return -1;
    }
    else if (pid_2 == 0) //P2
    {
        info(BEGIN, 2, 0); //Begin of P2

        pid_t pid_3 = fork();
        if (pid_3 == -1)
        {
            perror("Failed to create process P3");
            return -1;
        }
        else if (pid_3 == 0) //P3
        {
            info(BEGIN, 3, 0); //Begin of P3

            pid_t pid_4 = fork();
            if (pid_4 == -1)
            {
                perror("Failed to create process P4");
                return -1;
            }
            else if(pid_4 == 0) //P4
            {
                info(BEGIN, 4, 0); //Begin of P4

                gSemaphore_N_1 = sem_open("Semaphore name 1", O_CREAT, 0644, 0);
                gSemaphore_N_2 = sem_open("Semaphore name 2", O_CREAT, 0644, 0);

                pid_t pid_8 = fork();
                if (pid_8 == -1)
                {
                    perror("Failed to create process P8");
                    return -1;
                }
                else if(pid_8 == 0) //P8
                {
                    info(BEGIN, 8, 0); //Begin of P8

                    //P8 Here...

                    info(END, 8, 0); //End of P8
                }
                else
                {
                    waitpid(pid_8, NULL, 0);

                    //P4--Threads
                    int* threadNumbers_P4 = (int*)malloc(5 * sizeof(int));

                    pthread_t threadID_P4[5];
                    for (int i = 0; i < 5; i++)
                    {
                        threadNumbers_P4[i] = i + 1;

                        if (i + 1 == 2)
                        {
                            pthread_create(&threadID_P4[i], NULL, ThreadFunction_P4_T2, (void*)&threadNumbers_P4[i]);
                        }
                        else if (i + 1 == 3)
                        {
                            pthread_create(&threadID_P4[i], NULL, ThreadFunction_P4_T3, (void*)&threadNumbers_P4[i]);
                        }
                        else
                        {
                            pthread_create(&threadID_P4[i], NULL, ThreadFunction_P4, (void*)&threadNumbers_P4[i]);
                        }
                    }
                    for (int i = 0; i < 5; i++)
                    {
                        pthread_join(threadID_P4[i], NULL);
                    }

                    free(threadNumbers_P4);

                    sem_close(gSemaphore_N_1);
                    sem_close(gSemaphore_N_2);

                    info(END, 4, 0); //End of P4
                }
            }
            else
            {
                pid_t pid_5 = fork();
                if (pid_5 == -1)
                {
                    perror("Failed to create process P5");
                    return -1;
                }
                else if (pid_5 == 0) //P5
                {
                    info(BEGIN, 5, 0); //Begin of P5

                    gSemaphore_N_1 = sem_open("Semaphore name 1", O_CREAT, 0644, 0);
                    gSemaphore_N_2 = sem_open("Semaphore name 2", O_CREAT, 0644, 0);

                    pid_t pid_6 = fork();
                    if (pid_6 == -1)
                    {
                        perror("Failed to create process P6");
                        return -1;
                    }
                    else if(pid_6 == 0) //P6
                    {
                        info(BEGIN, 6, 0); //Begin of P6

                        //P6 Here...

                        info(END, 6, 0); //End of P6
                    }
                    else
                    {
                        pid_t pid_7 = fork();
                        if (pid_7 == -1)
                        {
                            perror("Failed to create process P7");
                            return -1;
                        }
                        else if (pid_7 == 0) //P7
                        {
                            info(BEGIN, 7, 0); //Begin of P7

                            //P7 Here...

                            info(END, 7, 0); //End of P7
                        }
                        else
                        {
                            waitpid(pid_6, NULL, 0);
                            waitpid(pid_7, NULL, 0);

                            //P5--Threads
                            int* threadNumbers_P5 = (int*)malloc(5 * sizeof(int));

                            sem_init(&gSemaphore_1, 0, 0);
                            sem_init(&gSemaphore_2, 0, 0);

                            pthread_t threadID_P5[5];
                            for (int i = 0; i < 5; i++)
                            {
                                threadNumbers_P5[i] = i + 1;

                                if (i + 1 == 2)
                                {
                                    pthread_create(&threadID_P5[i], NULL, ThreadFunction_P5_T2, (void*)&threadNumbers_P5[i]);
                                }
                                else if (i + 1 == 3)
                                {
                                    pthread_create(&threadID_P5[i], NULL, ThreadFunction_P5_T3, (void*)&threadNumbers_P5[i]);
                                }
                                else if (i + 1 == 4)
                                {
                                    pthread_create(&threadID_P5[i], NULL, ThreadFunction_P5_T4, (void*)&threadNumbers_P5[i]);
                                }
                                else
                                {
                                    pthread_create(&threadID_P5[i], NULL, ThreadFunction_P5, (void*)&threadNumbers_P5[i]);
                                }
                            }
                            for (int i = 0; i < 5; i++)
                            {
                                pthread_join(threadID_P5[i], NULL);
                            }

                            free(threadNumbers_P5);

                            sem_destroy(&gSemaphore_1);
                            sem_destroy(&gSemaphore_2);

                            sem_close(gSemaphore_N_1);
                            sem_close(gSemaphore_N_2);

                            info(END, 5, 0); //End of P5
                        }
                    }
                }
                else
                {
                    waitpid(pid_4, NULL, 0);
                    waitpid(pid_5, NULL, 0);

                    info(END, 3, 0); //End of P3
                }
            }
        }
        else
        {
            waitpid(pid_3, NULL, 0);

            //P2--Threads
            int* threadNumbers_P2 = (int*)malloc(41 * sizeof(int));

            sem_init(&gSemaphore_3, 0, 4);
            sem_init(&gSemaphore_4, 0, 0);
            sem_init(&gSemaphore_5, 0, 1);
            sem_init(&gSemaphore_6, 0, 1);
            sem_init(&gSemaphore_7, 0, 0);

            pthread_t threadID_P2[41];
            for (int i = 0; i < 41; i++)
            {
                threadNumbers_P2[i] = i + 1;

                if (i + 1 == 13)
                {
                    pthread_create(&threadID_P2[i], NULL, ThreadFunction_P2_T13, (void*)&threadNumbers_P2[i]);
                }
                else
                {
                    pthread_create(&threadID_P2[i], NULL, ThreadFunction_P2, (void*)&threadNumbers_P2[i]);
                }
            }
            for (int i = 0; i < 41; i++)
            {
                pthread_join(threadID_P2[i], NULL);
            }

            free(threadNumbers_P2);

            sem_destroy(&gSemaphore_3);
            sem_destroy(&gSemaphore_4);
            sem_destroy(&gSemaphore_5);
            sem_destroy(&gSemaphore_6);
            sem_destroy(&gSemaphore_7);

            info(END, 2, 0); //End of P2
        }
    }
    else
    {
        waitpid(pid_2, NULL, 0);

        sem_unlink("Semaphore name 1");
        sem_unlink("Semaphore name 2");

        info(END, 1, 0); //End of P1
    }

    return 0;
}
