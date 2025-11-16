// Сохраните этот код в lab3/src/prodcons.c

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h> // 
#include <unistd.h>
#include <stdatomic.h>
#include <time.h>

// Структура кольцевого буфера
typedef struct {
    int* buffer;
    int capacity;
    int head;
    int tail;
    pthread_mutex_t mutex; // Мьютекс для защиты индексов head/tail 
    sem_t empty_slots;     // Семафор: количество свободных слотов 
    sem_t full_slots;      // Семафор: количество занятых слотов 
} bounded_buffer_t;

// Глобальные переменные для статистики и завершения
long g_total_items;
int g_num_producers;
atomic_long g_consumed_count = 0;
atomic_long g_produced_count = 0;
atomic_int g_producers_done = 0; // Флаг завершения продюсеров

bounded_buffer_t g_buffer;

// Аргументы для потоков
typedef struct {
    int id;
    long items_to_process;
} thread_args_t;

// Функция производителя
void* producer_func(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;

    for (long i = 0; i < args->items_to_process; i++) {
        int item = (args->id * args->items_to_process) + i; // Уникальный элемент

        sem_wait(&g_buffer.empty_slots); // Ждем свободного слота

        // Критическая секция для доступа к буферу
        pthread_mutex_lock(&g_buffer.mutex);
        g_buffer.buffer[g_buffer.head] = item;
        g_buffer.head = (g_buffer.head + 1) % g_buffer.capacity;
        atomic_fetch_add(&g_produced_count, 1);
        pthread_mutex_unlock(&g_buffer.mutex);

        sem_post(&g_buffer.full_slots); // Сигналим о появлении элемента
    }
    
    // Продюсер закончил
    atomic_fetch_add(&g_producers_done, 1);
    return NULL;
}

// Функция потребителя
void* consumer_func(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;

    while (1) {
        // Проверяем условие выхода: все продюсеры закончили И буфер пуст
        if (atomic_load(&g_producers_done) == g_num_producers &&
            atomic_load(&g_produced_count) == atomic_load(&g_consumed_count)) {
            break;
        }

        // Ждем появления элемента
        sem_wait(&g_buffer.full_slots);

        // После пробуждения снова проверяем условие выхода
        // Это нужно, чтобы "поймать" сигнал последнего продюсера
        if (atomic_load(&g_producers_done) == g_num_producers &&
            atomic_load(&g_produced_count) == atomic_load(&g_consumed_count)) {
            
            sem_post(&g_buffer.full_slots); // Важно: вернуть семафор для другого потребителя
            break;
        }

        // Критическая секция для доступа к буферу
        pthread_mutex_lock(&g_buffer.mutex);
        int item = g_buffer.buffer[g_buffer.tail];
        g_buffer.tail = (g_buffer.tail + 1) % g_buffer.capacity;
        atomic_fetch_add(&g_consumed_count, 1);
        pthread_mutex_unlock(&g_buffer.mutex);

        sem_post(&g_buffer.empty_slots); // Сигналим о появлении свободного слота
        
        // (Опционально) Имитация обработки элемента
          usleep(50000); 
    }

    return NULL;
}

// Функция для "пробуждения" потребителей, которые могут спать на sem_wait
void wakeup_consumers(int num_consumers) {
    // Когда все продюсеры закончили,
    // мы должны "пнуть" всех потребителей, которые могут спать на sem_wait(&g_buffer.full_slots)
    // чтобы они могли проверить условие выхода из цикла.
    for (int i = 0; i < num_consumers; i++) {
        sem_post(&g_buffer.full_slots);
    }
}


int main(int argc, char* argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <P (producers)> <C (consumers)> <N (items)> <B (buffer_size)>\n", argv[0]);
        return 1;
    }

    g_num_producers = atoi(argv[1]);
    int num_consumers = atoi(argv[2]);
    g_total_items = atol(argv[3]);
    int buffer_size = atoi(argv[4]);

    if (g_num_producers <= 0 || num_consumers <= 0 || g_total_items <= 0 || buffer_size <= 0) {
        fprintf(stderr, "All arguments must be positive integers.\n");
        return 1;
    }

    // Инициализация буфера
    g_buffer.buffer = (int*)malloc(buffer_size * sizeof(int));
    g_buffer.capacity = buffer_size;
    g_buffer.head = 0;
    g_buffer.tail = 0;
    
    // Инициализация примитивов синхронизации
    pthread_mutex_init(&g_buffer.mutex, NULL);
    // 'empty_slots' инициализируется размером буфера 
    sem_init(&g_buffer.empty_slots, 0, buffer_size); 
    // 'full_slots' инициализируется нулем 
    sem_init(&g_buffer.full_slots, 0, 0); 

    pthread_t producers[g_num_producers];
    pthread_t consumers[num_consumers];
    thread_args_t producer_args[g_num_producers];
    thread_args_t consumer_args[num_consumers];

    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Распределяем элементы по продюсерам
    long items_per_producer = g_total_items / g_num_producers;
    long remainder = g_total_items % g_num_producers;

    // Запускаем продюсеров
    for (int i = 0; i < g_num_producers; i++) {
        producer_args[i].id = i;
        producer_args[i].items_to_process = items_per_producer + (i < remainder ? 1 : 0);
        pthread_create(&producers[i], NULL, producer_func, &producer_args[i]);
    }

    // Запускаем потребителей
    for (int i = 0; i < num_consumers; i++) {
        consumer_args[i].id = i;
        pthread_create(&consumers[i], NULL, consumer_func, &consumer_args[i]);
    }

    // Ждем завершения продюсеров
    for (int i = 0; i < g_num_producers; i++) {
        pthread_join(producers[i], NULL);
    }
    
    // Все продюсеры завершились.
    // Теперь нужно гарантировать, что потребители выйдут из цикла,
    // если они спят на пустом семафоре.
    wakeup_consumers(num_consumers);

    // Ждем завершения потребителей
    for (int i = 0; i < num_consumers; i++) {
        pthread_join(consumers[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed_sec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    // Очистка ресурсов
    sem_destroy(&g_buffer.empty_slots); // [cite: 106]
    sem_destroy(&g_buffer.full_slots);
    pthread_mutex_destroy(&g_buffer.mutex);
    free(g_buffer.buffer);

    // Проверка корректности
    printf("--- Results ---\n");
    printf("Producers: %d, Consumers: %d, Buffer: %d\n", g_num_producers, num_consumers, buffer_size);
    printf("Total items: %ld\n", g_total_items);
    printf("Produced: %ld\n", atomic_load(&g_produced_count));
    printf("Consumed: %ld\n", atomic_load(&g_consumed_count));
    printf("Time: %.6f seconds\n", elapsed_sec);

    if (atomic_load(&g_produced_count) == g_total_items && 
        atomic_load(&g_consumed_count) == g_total_items) {
        printf("SUCCESS: All items produced and consumed.\n");
    } else {
        printf("FAILURE: Item count mismatch!\n");
    }

    return 0;
}
