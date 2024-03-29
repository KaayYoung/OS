#include <types.h>
#include <lib.h>
#include <synch.h>
#include <test.h>
#include <thread.h>

#include "bar.h"
#include "bar_driver.h"



/*
 * **********************************************************************
 * YOU ARE FREE TO CHANGE THIS FILE BELOW THIS POINT AS YOU SEE FIT
 *
 */

/* Declare any globals you need here (e.g. locks, etc...) */

//
struct semaphore *customerId[NCUSTOMERS];

//semaphore to block the customer from ordering while waiting for their drinks
struct semaphore *customer[NCUSTOMERS];

//semaphore to block access to bartender once the buffer is empty 
struct semaphore *bartender;

//semaphore to count the number of free slot in the buffer
struct semaphore *orderSem;

// buffer to hold all the orders 
struct barorder orderBuffer[NCUSTOMERS];

//mutual exclusive lock to control access the buffer
struct lock *mutex;

//locks for bottles that are being used
struct lock *bottleLock[NBOTTLES];

//start and end index of the buffer
unsigned int start, end;

//sorting algorithm to sort the bottles
void sort(unsigned int *bottles);

/*
 * **********************************************************************
 * FUNCTIONS EXECUTED BY CUSTOMER THREADS
 * **********************************************************************
 */

/*
 * order_drink()
 *
 * Takes one argument referring to the order to be filled. The
 * function makes the order available to staff threads and then blocks
 * until a bartender has filled the glass with the appropriate drinks.
 */

void order_drink(struct barorder *order)
{       
        //reduce the number of free slot in the buffer
        P(orderSem);

        //enter critical region and acquire lock
        lock_acquire(mutex);
        
        end ++;
        end = end % NCUSTOMERS;
        //record its owner

        // if (customer[end]->sem_count != 0) {
        //      kprintf("end, :%d    sem :%d\n", end, customer[end]->sem_count);
            
        //      P(customerId[end]);
        //      exit();
        // }
        order->customerID = end;

        //add order to buffer
        orderBuffer[end] = *order;
        //release lock after exit critical region
        lock_release(mutex);

        //notifies bartender of new order
        V(bartender);
        
        // block customer from ordering until drink is served
        P(customer[order->customerID]);        
        // kprintf("customer_block: %d, sem:%d\n ", order->customerID, customer[order->customerID]->sem_count);

}

/*
 * **********************************************************************
 * FUNCTIONS EXECUTED BY BARTENDER THREADS
 * **********************************************************************
 */

/**sort()
 * 
 * This function sort the bottles using bubble sort so that locks can be acquired in order. This prevents deadlock
*/
void sort(unsigned int *bottles){
        int i, j, temp;
        bool swap;

        for(i = 0; i < DRINK_COMPLEXITY-1; i++){
                swap = false;
                for(j = 0; j < DRINK_COMPLEXITY-i-1; j++){
                        if(bottles[j] > bottles[j+1]){
                                temp = bottles[j];
                                bottles[j] = bottles[j+1];
                                bottles[j+1] = temp;
                                swap = true;
                        }
                }
                if(swap == false) break;
        } 
}


/*
 * take_order()
 *
 * This function waits for a new order to be submitted by
 * customers. When submitted, it returns a pointer to the order.
 *
 */

struct barorder *take_order(void)
{
        struct barorder *ret; 

        //decrease the number of free slot in the buffer
        P(bartender);

        //enter critical region and acquire lock
        lock_acquire(mutex);
        //take order from buffer
        ret = &orderBuffer[start];
        start ++;
        start = start % NCUSTOMERS;
        //release lock after exit critical region
        lock_release(mutex);

        //increase the number of free spots availabe in the order buffer
        V(orderSem);
        return ret;
}


/*
 * fill_order()
 *
 * This function takes an order provided by take_order and fills the
 * order using the mix() function to mix the drink.
 *
 * NOTE: IT NEEDS TO ENSURE THAT MIX HAS EXCLUSIVE ACCESS TO THE
 * REQUIRED BOTTLES (AND, IDEALLY, ONLY THE BOTTLES) IT NEEDS TO USE TO
 * FILL THE ORDER.
 */

void fill_order(struct barorder *order)
{       
        int i, b; 
        unsigned int *bottles;
        
        bottles = order->requested_bottles;

        sort(bottles);       
        
        for(i = 0; i < DRINK_COMPLEXITY; i++){
                b = bottles[i]-1;
                // if bottles[i] == 0, we don't have to lock the bottle

                if(bottles[i] == 0){
                		kprintf("bottle[%d]:  %d\n",i, bottles[i]);    
                        continue;
                
                //check if thread is holding the lock
                }else if(!lock_do_i_hold(bottleLock[b])){
                        //acquire lock
                        lock_acquire(bottleLock[b]);
                }
               
                
        }
        
        /* the call to mix the drinks*/
        mix(order);

        
        for(i = (DRINK_COMPLEXITY-1); i >= 0; i--){
                b = bottles[i]-1;
                //if bottles[i] == 0, we don't have unlock the bottle
                if(bottles[i] == 0){
                        continue;
                
                //check if thread is holding the lock
                }else if(lock_do_i_hold(bottleLock[b])){

                        //release the lock
                        lock_release(bottleLock[b]);
                }
               
        }

}


/*
 * serve_order()
 *
 * Takes a filled order and makes it available to (unblocks) the
 * waiting customer.
 */

void serve_order(struct barorder *order)
{       
        //unblock customer so that customer can order another cup of drinks
        V(customer[order->customerID]);
        //kprintf("customer: %d, sem:%d\n ", order->customerID, customer[order->customerID]->sem_count);
        //V(customerId[order->customerID]);
        //V(customerId[order->customerID]);
        
        
}

/*
 * **********************************************************************
 * INITIALISATION AND CLEANUP FUNCTIONS
 * **********************************************************************
 */


/*
 * bar_open()
 *
 * Perform any initialisation you need prior to opening the bar to
 * bartenders and customers. Typically, allocation and initialisation of
 * synch primitive and variable.
 */

void bar_open(void)
{       
        int i, index;
        char sem_name[5];
        char sem2_name[5];
        char lock_name[5]; 
        end = NCUSTOMERS -1; 
        start = 0;
   		


        // create a cv

        // create a semaphore array and load them to buff
        for(i = 0; i < NCUSTOMERS; i++){
                snprintf(sem2_name, 5 , "c%d", i);
                customerId[i] = sem_create(sem2_name, 0);
                if(customerId[i] == NULL){
                        panic("semaphore for each customerId %d create fail", i);
                }
        }

        //create semaphore for each customer
        for(i = 0; i < NCUSTOMERS; i++){
                snprintf(sem_name, 5 , "c%d", i);
                customer[i] = sem_create(sem_name, 0);
                if(customer[i] == NULL){
                        panic("semaphore for customer %d create fail", i);
                }
        }

        bartender = sem_create("bartender", 0);
        if(bartender == NULL){
                panic("semaphore for bartender create fail");
        }

        
        orderSem = sem_create("orderSem", NCUSTOMERS);
        if(orderSem == NULL){
                panic("semaphore for customer create fail");
        }
        
        mutex = lock_create("mutex");
        if(mutex == NULL){
                panic("mutual exclusion lock create fail");
        }

        //create lock for each bottle
        for(i = 0; i < NBOTTLES; i++){
                index = i+1;
                snprintf(lock_name, 5, "b%d", index);
                bottleLock[i] = lock_create(lock_name);
                if (bottleLock == NULL){
                        panic("lock for bottle %d create fail", index);
                }
        }

}

/*
 * bar_close()
 *
 * Perform any cleanup after the bar has closed and everybody
 * has gone home.
 */

void bar_close(void)
{       
        int i;


        //
        for(i = 0; i< NCUSTOMERS; i++){
                sem_destroy(customerId[i]);
        }

        for(i = 0; i< NCUSTOMERS; i++){
                sem_destroy(customer[i]);
        }

        for(i = 0; i < NBOTTLES; i++){
                lock_destroy(bottleLock[i]);
        }

        
        sem_destroy(orderSem);
        sem_destroy(bartender);
        lock_destroy(mutex);
}

