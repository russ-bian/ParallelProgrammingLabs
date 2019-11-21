#include <stdlib.h>
#include <stdio.h>
#include <omp.h>
#include <time.h>
#include <sys/time.h>
#include <w32api/mqoai.h>
#include <limits.h>

static const long Num_To_Add = 1000000000;
static const double Scale = 10.0 / RAND_MAX;

// Code for Queue taken from: https://www.geeksforgeeks.org/queue-set-1introduction-and-array-implementation/
// A structure to represent a queue
struct Queue
{
    int front, rear, size;
    unsigned capacity;
    long* array;
};

// function to create a queue of given capacity.
// It initializes size of queue as 0
struct Queue* createQueue(unsigned capacity)
{
    struct Queue* queue = (struct Queue*) malloc(sizeof(struct Queue));
    queue->capacity = capacity;
    queue->front = queue->size = 0;
    queue->rear = capacity - 1;  // This is important, see the enqueue
    queue->array = (long*) malloc(queue->capacity * sizeof(long));
    return queue;
}

// Queue is full when size becomes equal to the capacity
int isFull(struct Queue* queue)
{  return (queue->size == queue->capacity);  }

// Queue is empty when size is 0
int isEmpty(struct Queue* queue)
{  return (queue->size == 0); }

// Function to add an item to the queue.
// It changes rear and size
void enqueue(struct Queue* queue, long item)
{
    if (isFull(queue))
        return;
    queue->rear = (queue->rear + 1)%queue->capacity;
    queue->array[queue->rear] = item;
    queue->size = queue->size + 1;
}

// Function to remove an item from queue.
// It changes front and size
long dequeue(struct Queue* queue)
{
    if (isEmpty(queue))
        return INT_MIN;
    long item = queue->array[queue->front];
    queue->front = (queue->front + 1)%queue->capacity;
    queue->size = queue->size - 1;
    return item;
}

long add_serial(const char *numbers) {
    long sum = 0;
    for (long i = 0; i < Num_To_Add; i++) {
        sum += numbers[i];
    }
    return sum;
}

long add_parallel(const char *numbers) {
    // tracks global sum
    long global_sum = 0;
    // message queue for control thread
    struct Queue* message_queue = createQueue(3);
    // determine num to add for each thread
    int local_num_to_add = Num_To_Add/omp_get_max_threads();
#pragma omp parallel num_threads(omp_get_max_threads())
    {
        // get thread number
        int thread_number = omp_get_thread_num();
        // track local sum for current thread
        long local_sum = 0;
        // Divide the numbers array into four (or however many cores you happen to have) separate parts for each thread to operate on
        for(int i = thread_number * local_num_to_add; i < (thread_number + 1) * local_num_to_add; i++)
        {
            local_sum += numbers[i];
        }
        // worker threads send message to control thread
        if(thread_number != 0) {
            enqueue(message_queue, local_sum);
        }
        // control thread
        else if(thread_number == 0)
        {
            global_sum += local_sum;
            int i = 0;
            // spin lock
            while(1)
            {
                // spin lock while queue is empty
                while(isEmpty(message_queue)) {}
                global_sum += dequeue(message_queue);
                i++;
                // break after receiving messages from all other threads
                if(i == 3)
                {
                    break;
                }
            }
        }
    }
    return global_sum;
}

int main() {
    char *numbers = malloc(sizeof(long) * Num_To_Add);

    long chunk_size = Num_To_Add / omp_get_max_threads();
#pragma omp parallel num_threads(omp_get_max_threads())
    {
        int p = omp_get_thread_num();
        unsigned int seed = (unsigned int) time(NULL) + (unsigned int) p;
        long chunk_start = p * chunk_size;
        long chunk_end = chunk_start + chunk_size;
        for (long i = chunk_start; i < chunk_end; i++) {
            numbers[i] = (char) (rand_r(&seed) * Scale);
        }
    }

    struct timeval start, end;

    printf("Timing sequential...\n");
    gettimeofday(&start, NULL);
    long sum_s = add_serial(numbers);
    gettimeofday(&end, NULL);
    printf("Took %f seconds\n\n", end.tv_sec - start.tv_sec + (double) (end.tv_usec - start.tv_usec) / 1000000);

    printf("Timing parallel...\n");
    gettimeofday(&start, NULL);
    long sum_p = add_parallel(numbers);
    gettimeofday(&end, NULL);
    printf("Took %f seconds\n\n", end.tv_sec - start.tv_sec + (double) (end.tv_usec - start.tv_usec) / 1000000);

    printf("Sum serial: %ld\nSum parallel: %ld", sum_s, sum_p);

    free(numbers);
    return 0;
}

