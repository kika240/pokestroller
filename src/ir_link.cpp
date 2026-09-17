#include "ir_link.h"
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cerrno>
#include <cstring>
#include <deque>
#include <string>
#include <algorithm>

struct PWLink {
    int server = -1, peer = -1, state = 0;
    uint16_t port = 0;
    std::deque<uint8_t> tx, rx;
    std::string error;
};
static int make_socket() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0 || fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
        close(fd); return -1;
    }
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes));
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
    return fd;
}
PWLink *pw_link_create() { return new PWLink; }
void pw_link_close(PWLink *p) {
    if (p->server >= 0) close(p->server);
    if (p->peer >= 0) close(p->peer);
    p->server = p->peer = -1;
    p->state = 0; p->port = 0;
    p->tx.clear(); p->rx.clear(); p->error.clear();
}
void pw_link_destroy(PWLink *p) { if (p) { pw_link_close(p); delete p; } }
static bool fail(PWLink *p, const char *error) {
    std::string message(error);
    pw_link_close(p); p->state = -1; p->error = message; return false;
}
bool pw_link_listen(PWLink *p, uint16_t port) {
    pw_link_close(p);
    p->server = make_socket();
    if (p->server < 0) return fail(p, strerror(errno));
    int yes = 1; setsockopt(p->server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in address{}; address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); address.sin_port = htons(port);
    if (bind(p->server, (sockaddr *)&address, sizeof(address)) || listen(p->server, 1))
        return fail(p, strerror(errno));
    socklen_t length = sizeof(address);
    if (getsockname(p->server, (sockaddr *)&address, &length)) return fail(p, strerror(errno));
    p->port = ntohs(address.sin_port); p->state = 1; return true;
}
bool pw_link_connect(PWLink *p, const char *host, uint16_t port) {
    pw_link_close(p);
    sockaddr_in address{}; address.sin_family = AF_INET; address.sin_port = htons(port);
    if (!strcmp(host, "localhost")) host = "127.0.0.1";
    if (!port || inet_pton(AF_INET, host, &address.sin_addr) != 1)
        return fail(p, "Adresse IPv4 ou port invalide.");
    p->peer = make_socket();
    if (p->peer < 0) return fail(p, strerror(errno));
    int result = connect(p->peer, (sockaddr *)&address, sizeof(address));
    if (result && errno != EINPROGRESS) return fail(p, strerror(errno));
    p->port = port; p->state = result ? 1 : 2; return true;
}
void pw_link_poll(PWLink *p) {
    if (p->state < 1) return;
    if (p->server >= 0 && p->peer < 0) {
        int fd = accept(p->server, nullptr, nullptr);
        if (fd < 0) { if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) fail(p,strerror(errno)); return; }
        p->peer = fd;
        fcntl(fd,F_SETFL,O_NONBLOCK); fcntl(fd,F_SETFD,FD_CLOEXEC);
        int yes = 1; setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&yes,sizeof(yes));
        setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&yes,sizeof(yes));
        p->state = 2;
    }
    if (p->state == 1) {
        pollfd fd{p->peer,POLLOUT,0};
        if (poll(&fd,1,0) <= 0) return;
        int error = 0; socklen_t length=sizeof(error);
        if (getsockopt(p->peer,SOL_SOCKET,SO_ERROR,&error,&length)) { fail(p,strerror(errno)); return; }
        if (error) { fail(p,strerror(error)); return; }
        p->state = 2;
    }
    uint8_t bytes[4096];
    while (!p->tx.empty()) {
        size_t count = std::min(p->tx.size(),sizeof(bytes));
        std::copy_n(p->tx.begin(),count,bytes);
        ssize_t sent = send(p->peer,bytes,count,0);
        if (sent < 0) {
            if (errno == EINTR) continue;
            if (errno != EAGAIN && errno != EWOULDBLOCK) fail(p,strerror(errno));
            break;
        }
        if (!sent) break;
        p->tx.erase(p->tx.begin(),p->tx.begin()+sent);
    }
    if (p->state != 2) return;
    // Bound work per poll and retained data to keep AppKit responsive.
    for (int i=0;i<16;i++) {
        ssize_t count = recv(p->peer,bytes,sizeof(bytes),0);
        if (count < 0) {
            if (errno == EINTR) continue;
            if (errno != EAGAIN && errno != EWOULDBLOCK) fail(p,strerror(errno));
            break;
        }
        if (!count) { fail(p,"Connexion fermée par le correspondant."); break; }
        if (p->rx.size()+count > 65536) { fail(p,"Trop de données IR en attente."); break; }
        p->rx.insert(p->rx.end(),bytes,bytes+count);
    }
}
bool pw_link_send(PWLink *p, const uint8_t *data, size_t count) {
    if (p->state != 2) return false;
    if (count > 65536-p->tx.size()) return fail(p,"Trop de données IR à transmettre.");
    p->tx.insert(p->tx.end(),data,data+count); return true;
}
size_t pw_link_read(PWLink *p, uint8_t *bytes, size_t capacity) {
    size_t count=std::min(capacity,p->rx.size());
    for(size_t i=0;i<count;i++){bytes[i]=p->rx.front();p->rx.pop_front();} return count;
}
int pw_link_state(const PWLink *p) { return p->state; }
uint16_t pw_link_port(const PWLink *p) { return p->port; }
const char *pw_link_error(const PWLink *p) { return p->error.c_str(); }
