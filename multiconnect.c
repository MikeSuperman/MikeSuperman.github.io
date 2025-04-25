#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define BUFFER_SIZE 1024
#define SERVER_IP "192.168.0.16"
#define SERVER_PORT 1279
#define MAX_CLIENTS 100
#define MAX_THREADS 50

typedef struct {
    char ip[16];
    int count;
} ClientStats;

typedef struct {
    int client_fd;
    struct sockaddr_in client_addr;
} ClientInfo;

ClientStats client_stats[MAX_CLIENTS];
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;
int active_threads = 0;
pthread_mutex_t threads_mutex = PTHREAD_MUTEX_INITIALIZER;

// 获取或创建客户端统计记录
ClientStats* get_client_stats(const char* ip) {
    int empty_slot = -1;
    
    pthread_mutex_lock(&stats_mutex);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_stats[i].ip[0] == '\0' && empty_slot == -1) {
            empty_slot = i;
        }
        if (strcmp(client_stats[i].ip, ip) == 0) {
            pthread_mutex_unlock(&stats_mutex);
            return &client_stats[i];
        }
    }
    
    if (empty_slot != -1) {
        strncpy(client_stats[empty_slot].ip, ip, sizeof(client_stats[empty_slot].ip) - 1);
        client_stats[empty_slot].ip[sizeof(client_stats[empty_slot].ip) - 1] = '\0';
        client_stats[empty_slot].count = 0;
        pthread_mutex_unlock(&stats_mutex);
        return &client_stats[empty_slot];
    }
    
    pthread_mutex_unlock(&stats_mutex);
    return NULL;
}

// 打印所有客户端统计信息
void print_all_stats() {
    pthread_mutex_lock(&stats_mutex);
    
    printf("\n=== Client Connection Statistics ===\n");
    printf("%-15s %-20s\n", "IP Address", "Connection Count");
    printf("----------------------------------------\n");
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_stats[i].ip[0] != '\0') {
            printf("%-15s %-20d\n", client_stats[i].ip, client_stats[i].count);
        }
    }
    
    printf("========================================\n");
    printf("Active threads: %d\n", active_threads);
    printf("========================================\n\n");
    
    pthread_mutex_unlock(&stats_mutex);
}

// 处理客户端连接的线程函数
void* handle_client(void* arg) {
    ClientInfo* client_info = (ClientInfo*)arg;
    int client_fd = client_info->client_fd;
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_info->client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_info->client_addr.sin_port);
    
    // 更新活跃线程计数
    pthread_mutex_lock(&threads_mutex);
    active_threads++;
    pthread_mutex_unlock(&threads_mutex);
    
    // 更新客户端统计信息
    ClientStats* stats = get_client_stats(client_ip);
    if (stats != NULL) {
        pthread_mutex_lock(&stats_mutex);
        stats->count++;
        int current_count = stats->count;
        pthread_mutex_unlock(&stats_mutex);
        
//        printf("Client connected: %s:%d (Total connections: %d)\n", client_ip, client_port, current_count);
    } else {
        printf("Client connected: %s:%d (Statistics full, not tracking)\n",
               client_ip, client_port);
    }
    
    // 处理客户端通信
    char buffer[BUFFER_SIZE];
    int bytes_read;
    
    while (bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1)) {
        if (bytes_read < 0) {
            perror("read failed");
            break;
        } else if (bytes_read == 0) {
            printf("Client %s:%d disconnected\n", client_ip, client_port);
            break;
        }
        
        buffer[bytes_read] = '\0';
//        printf("Received from %s:%d: %s", client_ip, client_port, buffer);
        
        // 回显消息给客户端
        if (write(client_fd, buffer, bytes_read) < 0) {
            perror("write failed");
            break;
        }
    }
    
    // 关闭客户端连接
    close(client_fd);
    free(client_info);
    
    // 更新活跃线程计数
    pthread_mutex_lock(&threads_mutex);
    active_threads--;
    pthread_mutex_unlock(&threads_mutex);
    
    return NULL;
}

// 定时打印统计信息的信号处理函数
void alarm_handler(int sig) {
    print_all_stats();
    alarm(30); // 重新设置定时器
}

int main() {
    int server_fd;
    struct sockaddr_in server_addr;
    
    // 初始化客户端统计数组
    memset(client_stats, 0, sizeof(client_stats));
    
    // 设置信号处理
    signal(SIGALRM, alarm_handler);
    alarm(30); // 每分钟打印一次统计信息
    
    // 创建socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    // 设置SO_REUSEADDR选项
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    
    // 配置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(SERVER_PORT);
    
    // 绑定socket到指定IP和端口
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // 开始监听
    if (listen(server_fd, 10) < 0) { // 增加等待队列长度
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    printf("Server started on %s:%d\n", SERVER_IP, SERVER_PORT);
    printf("Waiting for connections...\n");
    print_all_stats();
    
    while (1) {
        ClientInfo* client_info = malloc(sizeof(ClientInfo));
        if (!client_info) {
            perror("malloc failed");
            continue;
        }
        
        socklen_t client_len = sizeof(client_info->client_addr);
        client_info->client_fd = accept(server_fd, 
                                      (struct sockaddr *)&client_info->client_addr, 
                                      &client_len);
        
        if (client_info->client_fd < 0) {
            perror("accept failed");
            free(client_info);
            continue;
        }
        
        // 检查活跃线程数
        pthread_mutex_lock(&threads_mutex);
        if (active_threads >= MAX_THREADS) {
            pthread_mutex_unlock(&threads_mutex);
            printf("Max threads reached. Rejecting new connection.\n");
            close(client_info->client_fd);
            free(client_info);
            continue;
        }
        pthread_mutex_unlock(&threads_mutex);
        
        // 创建线程处理客户端
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void*)client_info) != 0) {
            perror("pthread_create failed");
            close(client_info->client_fd);
            free(client_info);
            
            pthread_mutex_lock(&threads_mutex);
            active_threads--;
            pthread_mutex_unlock(&threads_mutex);
        } else {
            pthread_detach(thread_id); // 分离线程，自动回收资源
        }
    }
    
    close(server_fd);
    return 0;
}
