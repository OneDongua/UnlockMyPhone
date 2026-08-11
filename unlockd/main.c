#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <stdint.h>

#define PORT 8765
#define DISCOVERY_PORT 8766

// Must match PinCrypto.java. This key protects the PIN in transit/storage.
#define SECRET "tP0lL6mR8pG0uQ4pZ6sK5kS4rE9eS8cC"

static const uint8_t sbox[256] = {
0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16};

static uint8_t xtime(uint8_t x) { return (uint8_t)((x << 1) ^ ((x >> 7) * 0x1b)); }
static void aes256_key_expand(const uint8_t *key, uint8_t *rk) {
    static const uint8_t rcon[] = {1,2,4,8,16,32,64,128,27,54,108,216,171,77,154};
    memcpy(rk, key, 32);
    int bytes = 32, round = 0;
    while (bytes < 240) {
        uint8_t t[4]; memcpy(t, rk + bytes - 4, 4);
        if (bytes % 32 == 0) { uint8_t x=t[0]; t[0]=sbox[t[1]]^rcon[round++]; t[1]=sbox[t[2]]; t[2]=sbox[t[3]]; t[3]=sbox[x]; }
        else if (bytes % 32 == 16) { t[0]=sbox[t[0]]; t[1]=sbox[t[1]]; t[2]=sbox[t[2]]; t[3]=sbox[t[3]]; }
        for (int i=0;i<4;i++) { rk[bytes] = rk[bytes-32] ^ t[i]; bytes++; }
    }
}
static void aes256_block(const uint8_t *in, uint8_t *out, const uint8_t *rk) {
    uint8_t s[16]; memcpy(s,in,16); for(int i=0;i<16;i++) s[i]^=rk[i];
    for(int round=1;round<=14;round++) {
        for(int i=0;i<16;i++) s[i]=sbox[s[i]];
        uint8_t t[16];
        for(int c=0;c<4;c++){int j=4*c; t[j]=s[j];t[j+1]=s[(j+5)%16];t[j+2]=s[(j+10)%16];t[j+3]=s[(j+15)%16];}
        if(round<14) for(int c=0;c<4;c++){int j=4*c;uint8_t a=t[j],b=t[j+1],d=t[j+2],e=t[j+3];s[j]=xtime(a)^xtime(b)^b^d^e;s[j+1]=a^xtime(b)^xtime(d)^d^e;s[j+2]=a^b^xtime(d)^xtime(e)^e;s[j+3]=xtime(a)^a^b^d^xtime(e);} else memcpy(s,t,16);
        for(int i=0;i<16;i++) s[i]^=rk[round*16+i];
    } memcpy(out,s,16);
}
static int b64val(char c) { if(c>='A'&&c<='Z')return c-'A'; if(c>='a'&&c<='z')return c-'a'+26; if(c>='0'&&c<='9')return c-'0'+52; if(c=='+')return 62; if(c=='/')return 63; return -1; }
static int base64_decode(const char *in, uint8_t *out, size_t cap) { size_t n=0; int val=0,bits=-8; for(;*in;in++){ if(*in=='=')break; int x=b64val(*in); if(x<0) return -1; val=(val<<6)|x; bits+=6; if(bits>=0){if(n>=cap)return -1;out[n++]=(uint8_t)((val>>bits)&255);bits-=8;}} return (int)n; }
static int decrypt_pin(const char *encoded, char *pin, size_t pin_size) {
    uint8_t payload[128], rk[240], stream[16]; int length=base64_decode(encoded,payload,sizeof(payload));
    if(length<17 || length-16 >= (int)pin_size || length-16 > 16) return -1;
    aes256_key_expand((const uint8_t *)SECRET,rk); uint8_t counter[16]; memcpy(counter,payload,16);
    for(int i=16;i<length;i++){ if((i-16)%16==0){aes256_block(counter,stream,rk); for(int j=15;j>=0;j--)if(++counter[j])break;} pin[i-16]=(char)(payload[i]^stream[(i-16)%16]); }
    pin[length-16]='\0'; for(int i=0;i<length-16;i++)if(!isdigit((unsigned char)pin[i]))return -1; return 0;
}

/**
 * 查询当前是否显示锁屏界面。
 *
 * 返回：
 *   1  当前为锁屏状态
 *   0  当前未锁屏
 *  -1  无法读取或识别系统状态
 */
static int is_screen_locked(void)
{
    FILE *fp = popen(
        "dumpsys window | grep -E 'mKeyguardShowing|mDreamingLockscreen'",
        "r");

    if (fp == NULL)
        return -1;

    char buffer[512];

    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        if (strstr(buffer, "mKeyguardShowing=true") != NULL ||
            strstr(buffer, "mDreamingLockscreen=true") != NULL)
        {
            pclose(fp);
            return 1;
        }
    }

    pclose(fp);
    return 0;
}

/**
 * 执行解锁。
 */
static void unlock_device(const char *pin) {
    system("/system/bin/input keyevent 224");

    usleep(300000);

    system("/system/bin/input keyevent 82");

    usleep(300000);

    char command[64];
    snprintf(command, sizeof(command), "/system/bin/input text '%s'", pin);
    system(command);
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
             * unlock <base64(iv + AES-256-CTR ciphertext)>
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
                            printf("unlock request received\n");

                    fflush(stdout);

                    char pin[17];
                    if (decrypt_pin(token, pin, sizeof(pin)) == 0) {
                        printf("PIN decrypted\n");

                        fflush(stdout);

                        int lock_state = is_screen_locked();

                        if (lock_state == 1) {
                            printf("device is locked, unlocking\n");
                            fflush(stdout);

                            unlock_device(pin);

                            write(
                                    client_fd,
                                    "OK\n",
                                    sizeof("OK\n") - 1);
                        } else if (lock_state == 0) {
                            printf("device is already unlocked\n");
                            fflush(stdout);

                            write(
                                    client_fd,
                                    "ALREADY_UNLOCKED\n",
                                    sizeof("ALREADY_UNLOCKED\n") - 1);
                        } else {
                            printf("unable to determine lock state\n");
                            fflush(stdout);

                            write(
                                    client_fd,
                                    "LOCK_STATE_UNKNOWN\n",
                                    sizeof("LOCK_STATE_UNKNOWN\n") - 1);
                        }
                    } else {
                        printf(
                                "invalid encrypted PIN\n");

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
