# Компилятор (clang или gcc)
CC = clang

# Флаги CFLAGS из задания: -Wall -Wextra -O2 -pthread
CFLAGS = -Wall -Wextra -O2 -pthread
# LDFLAGS также нужен для -pthread
LDFLAGS = -pthread

# Исполняемые файлы (в этой же папке lab3/)
TARGETS = thread_race prodcons

# Папка с исходниками
SRC_DIR = src

# Полные пути к исходникам
SRCS_RACE = $(SRC_DIR)/thread_race.c
SRCS_PRODCONS = $(SRC_DIR)/prodcons.c

# Цель 'all' (по умолчанию): собрать обе программы
all: $(TARGETS)

# Правило сборки thread_race
thread_race: $(SRCS_RACE)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Правило сборки prodcons
prodcons: $(SRCS_PRODCONS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Цель 'clean' для очистки
clean:
	rm -f $(TARGETS) *.o

# --- Примеры целей 'run' для Варианта 2 ---

# Запуск thread_race с mutex
run_race_mutex: thread_race
	./thread_race 4 1000000 mutex

# Запуск thread_race с atomic
run_race_atomic: thread_race
	./thread_race 4 1000000 atomic

# Запуск prodcons
run_prodcons: prodcons
	./prodcons 2 2 100000 6
