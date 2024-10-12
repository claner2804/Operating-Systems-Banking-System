#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <stdbool.h>

#define SHM_NAME "/bank_accounts"
#define ACCOUNTS 12
#define CREDIT_AMOUNT 1000
#define INITIAL_BALANCE 1000
#define MUTEX_NAME "/bank_mutex"
#define ALARM_INTERVAL 3

int *accounts;
int *credit_sums;
bool running = true;
pthread_mutex_t *mutex;

void signal_handler(int signum) {
    // Signal handler to stop the simulation gracefully
    printf("\n[Signal Handler] Signal %d empfangen. Beende die Simulation...\n", signum);
    running = false;
    pthread_mutex_lock(mutex);
    pthread_mutex_unlock(mutex);
}

void clientAccessAccount(int account_index, int amount) {
    // Lock the mutex to ensure only one client can access the accounts at a time
    pthread_mutex_lock(mutex);
    accounts[account_index] += amount; // Modify the account balance
    printf("[Client] Konto %d geändert. Neuer Kontostand: %d\n", account_index, accounts[account_index]);
    // Unlock the mutex after modification
    pthread_mutex_unlock(mutex);
}

void serverReadAccounts() {
    // Lock the mutex to safely read and modify accounts
    pthread_mutex_lock(mutex);
    for (int i = 0; i < ACCOUNTS; i++) {
        // Print the current balance of each account
        printf("[Server] Kontostand von Konto %d: %d\n", i, accounts[i]);
        // If the account is negative, grant a credit to bring it back up
        if (accounts[i] < 0) {
            accounts[i] += CREDIT_AMOUNT;
            credit_sums[i] += CREDIT_AMOUNT;
            printf("[Server] Kredit von %d an Konto %d gewährt. Neuer Kontostand: %d\n", CREDIT_AMOUNT, i, accounts[i]);
        }
    }
    // Unlock the mutex after reading and modifying accounts
    pthread_mutex_unlock(mutex);
}

void alarm_handler(int signum) {
    // Alarm handler to periodically read accounts
    serverReadAccounts();
    alarm(ALARM_INTERVAL); // Reset the alarm
}

int main() {
    // Register signal handlers for SIGINT and SIGALRM
    signal(SIGINT, signal_handler);
    signal(SIGALRM, alarm_handler);

    // Create shared memory for accounts
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Fehler bei shm_open");
        exit(EXIT_FAILURE);
    }

    // Set the size of shared memory
    if (ftruncate(shm_fd, ACCOUNTS * sizeof(int) * 2) == -1) {
        perror("Fehler bei ftruncate");
        exit(EXIT_FAILURE);
    }

    // Map the shared memory to the process's address space
    void *shm_ptr = mmap(0, ACCOUNTS * sizeof(int) * 2, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        perror("Fehler bei mmap");
        exit(EXIT_FAILURE);
    }

    // Initialize pointers to accounts and credit sums within shared memory
    accounts = (int *)shm_ptr;
    credit_sums = (int *)(shm_ptr + ACCOUNTS * sizeof(int));

    // Initialize the accounts and credit sums
    for (int i = 0; i < ACCOUNTS; i++) {
        accounts[i] = INITIAL_BALANCE;
        credit_sums[i] = 0;
    }

    // Set up shared mutex attributes
    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED);

    // Create shared memory for the mutex
    int mutex_fd = shm_open(MUTEX_NAME, O_CREAT | O_RDWR, 0666);
    if (mutex_fd == -1) {
        perror("Fehler bei shm_open für Mutex");
        exit(EXIT_FAILURE);
    }

    // Set the size of shared memory for the mutex
    if (ftruncate(mutex_fd, sizeof(pthread_mutex_t)) == -1) {
        perror("Fehler bei ftruncate für Mutex");
        exit(EXIT_FAILURE);
    }

    // Map the mutex to the process's address space
    mutex = mmap(NULL, sizeof(pthread_mutex_t), PROT_READ | PROT_WRITE, MAP_SHARED, mutex_fd, 0);
    if (mutex == MAP_FAILED) {
        perror("Fehler bei mmap für Mutex");
        exit(EXIT_FAILURE);
    }

    // Initialize the mutex
    pthread_mutex_init(mutex, &mutex_attr);

    // Set the alarm to trigger periodically
    alarm(ALARM_INTERVAL);

    // Create child processes to simulate clients accessing accounts
    for (int i = 0; i < 4; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("Fehler bei fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            srand(time(NULL) ^ (getpid() << 16)); // Seed the random number generator for each child process
            while (running) {
                int account_index = rand() % ACCOUNTS; // Choose a random account
                int amount = (rand() % 200) - 100; // Generate a random amount between -100 and 100
                clientAccessAccount(account_index, amount); // Access the account
                usleep(500000); // Wait for 0.5 seconds before the next access
            }
            exit(EXIT_SUCCESS); // Exit the child process
        }
    }

    // Main process waits for termination signal
    while (running) {
        pause(); // Wait for signals
    }

    // Terminate all child processes
    printf("[Main] Beende alle Kindprozesse.\n");
    kill(0, SIGTERM);
    while (wait(NULL) > 0);

    // Clean up resources
    pthread_mutex_destroy(mutex);
    munmap(accounts, ACCOUNTS * sizeof(int) * 2);
    shm_unlink(SHM_NAME);
    shm_unlink(MUTEX_NAME);

    printf("[Main] Programm beendet.\n");
    return 0;
}
