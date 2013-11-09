#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<ctype.h>
#include<pthread.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<sys/select.h>
#include<sys/time.h>
#include<unistd.h>
#include<errno.h>
#include<arpa/inet.h>
#include<limits.h>

#define DATAGRAM_LEN    128
#define TIMEOUT         500

static void net_send(int dst, const char *data);
static void net_receive(void);

static char outdata[DATAGRAM_LEN + 1];
static char indata[DATAGRAM_LEN + 1];

static int me = 0;
static int my_socket;
static struct sockaddr_in *routers = NULL;
static int *totab;
static int *inseq;
static int *outseq;
static int largest_id = 0;
static struct sockaddr_in si_me, si_other;
static pthread_t rcv_thread, send_thread;
static pthread_mutex_t lock;
static int msg_pending = 0;
static int msg_timeout = 0;
static int errrate = 1;

static void dijkstra(int *m, int a, int dim)
{
    int *visited, *dist, *prevtab;
    int current;
    int i, mindist;
    
    totab = (int *)calloc(dim, sizeof(*totab));
    prevtab = (int *)calloc(dim, sizeof(*prevtab));
    visited = (int *)calloc(dim, sizeof(*visited));
    dist = (int *)calloc(dim, sizeof(*dist));
    for (i = 1; i < dim; i++) {
        dist[i] = INT_MAX;
        visited[i] = 0;
    }
    current = a;
    dist[current] = 0;
    ///*
    do {
        int min = -1;
        mindist = INT_MAX;
        //printf("curr = %d\n", current);
        for (i = 0; i < dim; i++) {
            int d;
            if (i == current) {
                continue;
            }
            d = m[current * dim + i];
            if (d != 0 && !visited[i]) {
                d += dist[current];
                //printf("check %d to %d = %d\n", current, i, d);
                if (d < dist[i]) {
                    dist[i] = d;
                    prevtab[i] = current;
                }
            }
            if (dist[i] < mindist && !visited[i]) {
                min = i;
                mindist = dist[i];
            }
        }
        visited[current] = 1;
        current = min;
    } while(mindist < INT_MAX);
    /*
    printf("src = %d\n", a);
    for (i = 1; i < dim; i++) {
        printf("dist(%d): %d\n", i, dist[i]);
    }
    for (i = 1; i < dim; i++) {
        printf("prev(%d): %d\n", i, prevtab[i]);
    }
    */

    for (i = 1; i < dim; i++) {
        int j;
        if (prevtab[i] == 0) {
            //printf("aaa\n");
            continue;
        }
        j = prevtab[i];
        while(j != a && m[a * dim + j] == 0) {
            j = prevtab[j];
        }
        if (j == a) {
            j = i;
        }
        totab[i] = j;
        //printf("primpasso %d %d -- %d\n", i, j, m[a * dim +j]);
    }
    /*
    for (i = 1; i < dim; i++) {
        printf("next(%d): %d\n", i, totab[i]);
    }
    */
    free(prevtab);
    free(dist);
    free(visited);
}

static void loadgraph(void)
{
    FILE *fd;
    int from, to, w;
    int *m;
    int i, j;
    int dim = largest_id + 1;
    m = calloc(dim * dim, sizeof(*m));
    fd = fopen("enlaces.config", "rt");
    if (fd == NULL) {
        return;
    }
    while(fscanf(fd, "%d %d %d", &from, &to, &w) == 3) {
        m[from * dim + to] = w;
        m[to * dim + from] = w;
    }
    fclose(fd);

    /*
    for (i = 0; i < dim; i++) {
        for (j = 0; j < dim; j++) {
            printf("<%d> ", m[i * dim + j]);
        }
        printf("\n");
    }
    */
    dijkstra(m, me, dim);
    free(m);
}

static int path_next(int n)
{
    if (n <= 0 || n > largest_id) {
        return 0;
    }
    return totab[n];
}

static int luck(int per)
{
    if ((rand() % 100) < per)
        return 1;
    return 0;
}

static void sys_exit(int n)
{
    free(totab);
    free(outseq);
    free(inseq);
    free(routers);
    routers = NULL;
    pthread_mutex_destroy(&lock);
    exit(n);
}
 
static void sys_sleep(int milli)
{
    struct timeval t;
    t.tv_sec = milli / 1000;
    t.tv_usec = (milli % 1000) * 1000;
    select(0, NULL, NULL, NULL, &t);
}

static int sys_getmilli(void)
{
    int curtime;
    struct timeval tp;
    struct timezone tzp;
    static int  secbase = 0;
    gettimeofday(&tp, &tzp);
    if (secbase == 0) {
        secbase = tp.tv_sec;
        return tp.tv_usec / 1000;
    }
    curtime = (tp.tv_sec - secbase) * 1000 + tp.tv_usec / 1000;
    return curtime;
}

static void loadconfig(void)
{
    FILE *fd;
    int id, port;
    int addr[4];

    fd = fopen("roteador.config", "rt");
    if (fd == NULL) {
        return;
    }
    while(fscanf(fd, "%d %d %d.%d.%d.%d", &id, &port, &addr[0], &addr[1], &addr[2], &addr[3]) == 6) {
        if (id > largest_id) {
            largest_id = id;
        }
    }
    routers = (struct sockaddr_in *)calloc(largest_id + 1, sizeof(*routers));
    inseq = (int *)calloc(largest_id + 1, sizeof(*inseq));
    outseq = (int *)calloc(largest_id + 1, sizeof(*outseq));
    fseek(fd, 0, SEEK_SET);
    while(fscanf(fd, "%d %d %d.%d.%d.%d", &id, &port, &addr[0], &addr[1], &addr[2], &addr[3]) == 6) {
        unsigned long ha = addr[3] + (addr[2]<<8) + (addr[1]<<16) + (addr[0]<<24);
        routers[id].sin_family = AF_INET;
        routers[id].sin_port = htons(port);
        routers[id].sin_addr.s_addr = htonl(ha);
    }
    fclose(fd);
}

static int msg_getdst(const char *msg)
{
    return 10 * (msg[0] - '0') + msg[1] - '0';
}

static void msg_setdst(char *msg, int n)
{
    char nn[16];
    sprintf(nn, "%02d", n);
    msg[0] = nn[0];
    msg[1] = nn[1];
}

static int msg_getsrc(const char *msg)
{
    return 10 * (msg[2] - '0') + msg[3] - '0';
}

static void msg_setsrc(char *msg, int n)
{
    char nn[16];
    sprintf(nn, "%02d", n);
    msg[2] = nn[0];
    msg[3] = nn[1];
}

static int msg_getcmd(const char *msg)
{
    return msg[4];
}

static void msg_setcmd(char *msg, int cmd)
{
    msg[4] = cmd;
}

static int msg_getseq(const char *msg)
{
    return msg[5] - '0';
}

static void msg_setseq(char *msg, int n)
{
    msg[5] = n + '0';
}

static const char *msg_gettext(const char *msg)
{
    return &msg[6];
}

static void msg_settext(char *msg, const char *text)
{
    strncpy(&msg[6], text, DATAGRAM_LEN - 6);
}

static void sendmessage(const char *msg)
{
    int dst;
    dst = msg_getdst(msg);
    if (dst <= 0 || dst > largest_id || dst == me) {
        printf("try send %d\n", dst);
        return;
    }
    pthread_mutex_lock(&lock);
    msg_setsrc(outdata, me);
    msg_setdst(outdata, dst);
    msg_setcmd(outdata, 'M');
    msg_setseq(outdata, outseq[dst]);
    msg_settext(outdata, &msg[3]);
    msg_pending = 1;
    pthread_mutex_unlock(&lock);
}

static void kbd_main(void)
{
    char msg[100 + 2];

    for (;;) {
        printf("$ ");
        if (fgets(msg, sizeof(msg), stdin) == NULL) {
            break;
        }
        int len = strlen(msg);
        if (len > 0) {
            if (msg[len - 1] == '\n') {
                msg[len - 1] = '\0';
            }
        }
        if (msg[0] == 'q') {
            break;
        }
        sendmessage(msg);
    }
}
 
static void *rcv_main(void *arg)
{
    //printf("oi\n");
    for (;;) {
        int src, dst;
        //sys_sleep(100);
        net_receive();      // block
        src = msg_getsrc(indata);
        dst = msg_getdst(indata);
        if (dst != me) {
            net_send(dst, indata);
        } else {
            if (msg_getcmd(indata) == 'C') {
                if (msg_getseq(indata) == outseq[src]) {
                    msg_pending = 0;
                }
                printf("ME: %s\n", msg_gettext(indata));
            } else {
                msg_setsrc(indata, me);
                msg_setdst(indata, src);
                msg_setcmd(indata, 'C');
                net_send(src, indata);
                if (msg_getseq(indata) == inseq[src]) {
                    inseq[src] ^= 1;
                    printf("%02d: %s\n", src, msg_gettext(indata));
                }
            }
        }
    }
    return NULL;
}
 
static void *send_main(void *arg)
{
    for (;;) {
        if (msg_pending) {
            int dst;
            pthread_mutex_lock(&lock);
            dst = msg_getdst(outdata);
            net_send(dst, outdata);  
            msg_timeout = sys_getmilli() + TIMEOUT;
            while (msg_pending) {
                int t = sys_getmilli();
                if (msg_timeout <= t) {
                    net_send(dst, outdata);
                    msg_timeout += TIMEOUT;
                    if (msg_timeout <= t) {
                        msg_timeout = t + TIMEOUT;
                    }
                } else {
                    sys_sleep(msg_timeout - t);
                }
            }
            outseq[dst] ^= 1;
            pthread_mutex_unlock(&lock);
        }
        sys_sleep(50);
    }
    return NULL;
}

static void cmdline(int argc, char **argv)
{
    int i;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-e") == 0) {
            errrate = atoi(argv[++i]);
            if (errrate < 1) {
                errrate = 1;
            } else if (errrate > 100) {
                errrate = 100;
            }
        } else {
            me = atoi(argv[i]);
        }
    }
}

static void net_receive(void)
{
    int reclen;
    socklen_t slen = sizeof(si_other);
    reclen = recvfrom(my_socket, indata, DATAGRAM_LEN, MSG_WAITALL,
        (struct sockaddr *)&si_other, &slen);
    indata[reclen] = '\0';
    printf("RECV <%s>\n", indata);
}

static void net_send(int dst, const char *data)
{
    dst = path_next(dst);
    if (dst < 0 || dst > largest_id) {
        printf("Error in packet\n");
        return;
    }
    printf("SEND <%s>", data);
    if (luck(errrate)) {
        printf(" LOST!!!\n");
        return;
    } else {
        printf("\n");
    }
    socklen_t slen = sizeof(si_other);
    sendto(my_socket, data, DATAGRAM_LEN, 0,
        (struct sockaddr *)&routers[dst], slen);
}

static void makesocket(void)
{
    my_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (my_socket == -1) {
        printf("erro\n");
        sys_exit(1);
    }
    si_me.sin_family = AF_INET;
    si_me.sin_port = routers[me].sin_port;
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(my_socket, (struct sockaddr *)&si_me, sizeof(si_me)) == -1) {
        printf("erro\n");
        sys_exit(1);
    }

}
 
int main(int argc, char **argv)
{
    int err;

    srand(time(NULL));
    pthread_mutex_init(&lock, NULL);
    cmdline(argc, argv);
    loadconfig();
    loadgraph();
    makesocket();
    err = pthread_create(&rcv_thread, NULL, &rcv_main, NULL);
    err = pthread_create(&send_thread, NULL, &send_main, NULL);
    printf("\nnode number = %d\n", me);
    kbd_main();
    sys_exit(0);
    return 0;
}
