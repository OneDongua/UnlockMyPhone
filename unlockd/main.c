#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#define PORT 8765
#define DISCOVERY_PORT 8766

// A 和 B 共同保存的 Secret。
#define SECRET "tP0lL6mR8pG0uQ4pZ6sK5kS4rE9eS8cC"

// Token 长度
#define TOKEN_LENGTH 8

/**
 * 根据时间窗口生成 Token。
 *
 * Token = SHA256(SECRET + minute)
 *
 * minute = Unix timestamp / 60
 *
 * 返回 0 表示成功，-1 表示失败。
 */
static int generate_token(
        time_t timestamp,
        char *token,
        size_t token_size) {
    if (token_size < TOKEN_LENGTH + 1)
        return -1;

    long long minute = (long long) timestamp / 60;

    char input[256];

    snprintf(
            input,
            sizeof(input),
            "%s%lld",
            SECRET,
            minute);

    char command[512];

    snprintf(
            command,
            sizeof(command),
            "printf '%%s' '%s' | /system/bin/sha256sum",
            input);

    FILE *fp = popen(command, "r");

    if (fp == NULL)
        return -1;

    char hash[128] = {0};

    if (fgets(hash, sizeof(hash), fp) == NULL) {
        pclose(fp);
        return -1;
    }

    pclose(fp);

    /*
     * sha256sum 输出类似：
     *
     * 1234567890abcdef...  -
     *
     * 取前 8 个字符。
     */
    for (int i = 0; i < TOKEN_LENGTH; i++) {
        if (!isxdigit((unsigned char) hash[i]))
            return -1;

        token[i] = hash[i];
    }

    token[TOKEN_LENGTH] = '\0';

    return 0;
}

/**
 * 检查 Token。
 *
 * 接受：
 *
 * 当前时间窗口
 * 前一个时间窗口
 * 后一个时间窗口
 */
static int verify_token(const char *received) {
    time_t now = time(NULL);

    char token[TOKEN_LENGTH + 1];

    for (int offset = -1; offset <= 1; offset++) {
        time_t timestamp = now + offset * 60;

        if (generate_token(
                timestamp,
                token,
                sizeof(token)) != 0) {
            continue;
        }

        if (strcmp(received, token) == 0) {
            return 1;
        }
    }

    return 0;
}

/**
 * 执行解锁。
 */
static void unlock_device(void) {
    system("/system/bin/input keyevent 224");

    usleep(300000);

    system("/system/bin/input keyevent 82");

    usleep(300000);

    system("/system/bin/input text 6666");
}

/**
 * UDP 设备发现线程。
 *
 * B：
 *
 * UDP -> 8766
 * "DISCOVER_UNLOCKD"
 *
 * A：
 *
 * UDP -> B
 * "UNLOCKD"
 */
static void *discovery_thread(void *arg) {
    (void) arg;

    int udp_fd = socket(
            AF_INET,
            SOCK_DGRAM,
            0);

    if (udp_fd < 0) {
        perror("discovery socket");
        return NULL;
    }

    int reuse = 1;

    if (setsockopt(
            udp_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse,
            sizeof(reuse)) < 0) {
        perror("discovery setsockopt");
    }

    struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons(DISCOVERY_PORT),
            .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    if (bind(
            udp_fd,
            (struct sockaddr *) &addr,
            sizeof(addr)) < 0) {
        perror("discovery bind");
        close(udp_fd);
        return NULL;
    }

    printf(
            "discovery listening on UDP port %d\n",
            DISCOVERY_PORT);

    fflush(stdout);

    while (1) {
        char buffer[128] = {0};

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        ssize_t len = recvfrom(
                udp_fd,
                buffer,
                sizeof(buffer) - 1,
                0,
                (struct sockaddr *) &client_addr,
                &client_len);

        if (len < 0) {
            perror("discovery recvfrom");
            continue;
        }

        buffer[len] = '\0';

        if (strcmp(buffer, "DISCOVER_UNLOCKD") == 0) {
            char client_ip[INET_ADDRSTRLEN] = {0};

            inet_ntop(
                    AF_INET,
                    &client_addr.sin_addr,
                    client_ip,
                    sizeof(client_ip));

            printf(
                    "discovery request from %s\n",
                    client_ip);

            fflush(stdout);

            const char *response = "UNLOCKD";

            ssize_t sent = sendto(
                    udp_fd,
                    response,
                    strlen(response),
                    0,
                    (struct sockaddr *) &client_addr,
                    client_len);

            if (sent < 0) {
                perror("discovery sendto");
            }
        }
    }

    close(udp_fd);

    return NULL;
}

int main(void) {
    /*
     * 创建 UDP 发现线程。
     */
    pthread_t discovery_tid;

    if (pthread_create(
            &discovery_tid,
            NULL,
            discovery_thread,
            NULL) != 0) {
        perror("pthread_create");
        return 1;
    }

    /*
     * TCP Server
     */
    int server_fd = socket(
            AF_INET,
            SOCK_STREAM,
            0);

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int reuse = 1;

    setsockopt(
            server_fd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse,
            sizeof(reuse));

    struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons(PORT),
            .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    if (bind(
            server_fd,
            (struct sockaddr *) &addr,
            sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf(
            "unlockd listening on TCP port %d\n",
            PORT);

    fflush(stdout);

    /*
     * TCP 主循环。
     */
    while (1) {
        int client_fd = accept(
                server_fd,
                NULL,
                NULL);

        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        char buffer[128] = {0};

        ssize_t len = read(
                client_fd,
                buffer,
                sizeof(buffer) - 1);

        if (len > 0) {
            buffer[len] = '\0';

            /*
             * 期望：
             *
             * unlock 12345678
             */

            char command[32];
            char token[64];

            command[0] = '\0';
            token[0] = '\0';

            if (sscanf(
                    buffer,
                    "%31s %63s",
                    command,
                    token) == 2) {
                if (strcmp(command, "unlock") == 0) {
                    printf(
                            "unlock request, token=%s\n",
                            token);

                    fflush(stdout);

                    if (verify_token(token)) {
                        printf(
                                "token verified\n");

                        fflush(stdout);

                        unlock_device();

                        write(
                                client_fd,
                                "OK\n",
                                3);
                    } else {
                        printf(
                                "invalid token\n");

                        fflush(stdout);

                        write(
                                client_fd,
                                "AUTH_ERROR\n",
                                11);
                    }
                } else {
                    write(
                            client_fd,
                            "ERROR\n",
                            6);
                }
            } else {
                write(
                        client_fd,
                        "ERROR\n",
                        6);
            }
        }

        close(client_fd);
    }

    close(server_fd);

    return 0;
}