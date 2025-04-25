#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFFER_SIZE 1024
#define SERVER_IP "192.168.0.16"
#define SERVER_PORT 1279
#define MAX_CLIENTS 100

// 结构体用于存储IP地址和连接次数
typedef struct {
    char ip[16];  // 存储IP地址字符串
    int count;    // 连接次数
} ClientStats;

ClientStats client_stats[MAX_CLIENTS];  // 客户端统计数组
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;  // 互斥锁保护共享数据

// 查找或创建客户端统计记录
ClientStats* get_client_stats(const char* ip) {
    int empty_slot = -1;
    
    pthread_mutex_lock(&stats_mutex);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_stats[i].ip[0] == '\0' && empty_slot == -1) {
            empty_slot = i;  // 记录第一个空位
        }
        if (strcmp(client_stats[i].ip, ip) == 0) {
            pthread_mutex_unlock(&stats_mutex);
            return &client_stats[i];
        }
    }
    
    // 如果没找到且有空位，创建新记录
    if (empty_slot != -1) {
        strncpy(client_stats[empty_slot].ip, ip, sizeof(client_stats[empty_slot].ip) - 1);
        client_stats[empty_slot].ip[sizeof(client_stats[empty_slot].ip) - 1] = '\0';
        client_stats[empty_slot].count = 0;
        pthread_mutex_unlock(&stats_mutex);
        return &client_stats[empty_slot];
    }
    
    pthread_mutex_unlock(&stats_mutex);
    return NULL;  // 没有空位了
}

// 打印所有客户端统计信息
void print_all_stats() {
    pthread_mutex_lock(&stats_mutex);
    
    printf("\n=== Client Connection Statistics ===\n");
    printf("%-15s %s\n", "IP Address", "Connection Count");
    printf("--------------------------------\n");
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_stats[i].ip[0] != '\0') {
            printf("%-15s %d\n", client_stats[i].ip, client_stats[i].count);
        }
    }
    
    printf("================================\n\n");
    
    pthread_mutex_unlock(&stats_mutex);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    char buffer[BUFFER_SIZE];
    int bytes_read;

    // 初始化客户端统计数组
    memset(client_stats, 0, sizeof(client_stats));

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
    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server started on %s:%d\n", SERVER_IP, SERVER_PORT);
    printf("Waiting for connections...\n");

    // 设置定时打印统计信息
    alarm(60);  // 每分钟打印一次统计信息

    while (1) {
        // 接受客户端连接
        client_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept failed");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        
        // 更新客户端统计信息
        ClientStats* stats = get_client_stats(client_ip);
        if (stats != NULL) {
            pthread_mutex_lock(&stats_mutex);
            stats->count++;
            int current_count = stats->count;
            pthread_mutex_unlock(&stats_mutex);
            
            printf("Client connected: %s:%d (Total connections: %d)\n", 
                   client_ip, ntohs(client_addr.sin_port), current_count);
        } else {
            printf("Client connected: %s:%d (Statistics full, not tracking)\n",
                   client_ip, ntohs(client_addr.sin_port));
        }

        // 与客户端通信
        while ((bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1)) > 0) {
            buffer[bytes_read] = '\0';
            printf("Received from %s: %s", client_ip, buffer);
            
            // 回显消息给客户端
            if (write(client_fd, buffer, bytes_read) < 0) {
                perror("write failed");
                break;
            }
        }

        if (bytes_read == 0) {
            printf("Client %s disconnected\n", client_ip);
        } else if (bytes_read < 0) {
            perror("read failed");
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
