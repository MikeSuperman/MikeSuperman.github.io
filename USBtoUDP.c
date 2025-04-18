#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define UDP_SERVER_IP "192.168.0.15"
#define UDP_SERVER_PORT 1289

// 获取当前时间字符串（精确到毫秒）
void get_current_time_ms(char *time_str, size_t max_len) {
    struct timeval tv;
    struct tm *tm_info;

    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);

    strftime(time_str, max_len, "%Y-%m-%d %H:%M:%S", tm_info);
    sprintf(time_str + strlen(time_str), ".%03ld", tv.tv_usec / 1000);
}

// 创建UDP套接字
int create_udp_socket() {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("UDP socket creation failed");
        return -1;
    }
    return sockfd;
}

int main(int argc, char **argv) {
    if(argc != 2) {
        printf("Usage: %s <serial_port>\n", argv[0]);
        printf("Example: %s /dev/ttyUSB0\n", argv[0]);
        return -1;
    }

    // 初始化UDP套接字
    int udp_sock = create_udp_socket();
    if (udp_sock < 0) {
        return -1;
    }

    // 设置目标UDP地址
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(UDP_SERVER_PORT);
    if (inet_pton(AF_INET, UDP_SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        close(udp_sock);
        return -1;
    }

    const char *serial_port = argv[1];
    int serial_fd = open(serial_port, O_RDWR | O_NOCTTY);
    
    if(serial_fd == -1) {
        perror("Error opening serial port");
        close(udp_sock);
        return -1;
    }

    // 配置串口参数
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    
    if(tcgetattr(serial_fd, &tty) != 0) {
        perror("Error getting terminal attributes");
        close(serial_fd);
        close(udp_sock);
        return -1;
    }

    cfsetospeed(&tty, B9600);
    cfsetispeed(&tty, B9600);
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_oflag &= ~OPOST;
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 10;

    if(tcsetattr(serial_fd, TCSANOW, &tty) != 0) {
        perror("Error setting terminal attributes");
        close(serial_fd);
        close(udp_sock);
        return -1;
    }

    printf("Listening on %s... Press Ctrl+C to exit\n", serial_port);
    printf("UDP target: %s:%d\n", UDP_SERVER_IP, UDP_SERVER_PORT);

    char buffer[1024];
    char time_str[32];
    ssize_t n;
    
    while(1) {
        n = read(serial_fd, buffer, sizeof(buffer) - 1);
        
        if(n > 0) {
            buffer[n] = '\0';
            get_current_time_ms(time_str, sizeof(time_str));
            
            // 本地显示
            printf("[%s] Received: %s", time_str, buffer);
            printf("\r\n");
            fflush(stdout);
            
            // 通过UDP发送
            char udp_buffer[2060];
            snprintf(udp_buffer, sizeof(udp_buffer), "[%s] %s", time_str, buffer);
            
            ssize_t sent = sendto(udp_sock, udp_buffer, strlen(udp_buffer), 0,
                                 (struct sockaddr*)&serv_addr, sizeof(serv_addr));
            if (sent < 0) {
                perror("UDP send failed");
            }
        } else if(n < 0) {
            perror("Error reading from serial port");
            break;
        }
    }

    close(serial_fd);
    close(udp_sock);
    return 0;
}
