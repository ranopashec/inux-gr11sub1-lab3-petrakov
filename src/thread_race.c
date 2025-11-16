// Сохраните этот код в lab3/src/thread_race.c

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h> // Для Задания A (Вариант 2) [cite: 67]

// Глобальный счетчик
long g_counter = 0;
// Атомарный глобальный счетчик (для режима 'atomic')
atomic_long g_atomic_counter = 0;

// Мьютекс для синхронизации (для режима 'mutex')
pthread_mutex_t g_mutex;

// Аргументы для потока
typedef struct {
    long increments;
} thread_args_t;

// Функция потока: инкремент с мьютексом
void* thread_func_mutex(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    for (long i = 0; i < args->increments; i++) {
        pthread_mutex_lock(&g_mutex);
        g_counter++;
        pthread_mutex_unlock(&g_mutex);
    }
    return NULL;
}

// Функция потока: инкремент с атомиком
void* thread_func_atomic(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    for (long i = 0; i < args->increments; i++) {
        // Используем memory_order_relaxed, как указано в задании [cite: 67]
        atomic_fetch_add_explicit(&g_atomic_counter, 1, memory_order_relaxed);
    }
    return NULL;
}

// Функция потока: инкремент без синхронизации (для демонстрации гонки)
void* thread_func_unsync(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    for (long i = 0; i < args->increments; i++) {
        g_counter++;
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <num_threads> <num_increments> <mode: unsync|mutex|atomic>\n", argv[0]);
        return 1;
    }

    int num_threads = atoi(argv[1]);
    long total_increments = atol(argv[2]);
    char* mode = argv[3];

    if (num_threads <= 0 || total_increments <= 0) {
        fprintf(stderr, "N and M must be positive integers.\n");
        return 1;
    }

    // Распределяем инкременты по потокам
    long increments_per_thread = total_increments / num_threads;
    long remainder = total_increments % num_threads;

    pthread_t threads[num_threads];
    thread_args_t thread_args[num_threads];

    void* (*thread_func)(void*);

    // Выбираем режим
    if (strcmp(mode, "mutex") == 0) {
        thread_func = thread_func_mutex;
        pthread_mutex_init(&g_mutex, NULL);
        g_counter = 0; // Сбрасываем неатомарный счетчик
    } else if (strcmp(mode, "atomic") == 0) {
        thread_func = thread_func_atomic;
        atomic_init(&g_atomic_counter, 0); // Сбрасываем атомарный счетчик
    } else if (strcmp(mode, "unsync") == 0) {
        thread_func = thread_func_unsync;
        g_counter = 0; // Сбрасываем неатомарный счетчик
    } else {
        fprintf(stderr, "Unknown mode: %s. Use 'unsync', 'mutex', or 'atomic'.\n", mode);
        return 1;
    }

    // Замер времени
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    // Создаем потоки
    for (int i = 0; i < num_threads; i++) {
        thread_args[i].increments = increments_per_thread + (i < remainder ? 1 : 0);
        if (pthread_create(&threads[i], NULL, thread_func, &thread_args[i]) != 0) {
            perror("pthread_create");
            return 1;
        }
    }

    // Ждем завершения потоков [cite: 86]
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // Останавливаем таймер
    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed_sec = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

    // Определяем финальное значение
    long final_counter;
    if (strcmp(mode, "atomic") == 0) {
        final_counter = atomic_load(&g_atomic_counter);
    } else {
        final_counter = g_counter;
    }

    // Очистка
    if (strcmp(mode, "mutex") == 0) {
        pthread_mutex_destroy(&g_mutex); // [cite: 106]
    }

    // Вывод результатов
    printf("Mode: %s\n", mode);
    printf("Threads: %d\n", num_threads);
    printf("Expected: %ld\n", total_increments);
    printf("Actual: %ld\n", final_counter);
    printf("Time: %.6f seconds\n", elapsed_sec);

    return 0;
}
