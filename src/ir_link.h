#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct PWLink PWLink;
// Raw byte stream, compatible with PocketWalker TCP IR. Local IPv4 transport.
PWLink *pw_link_create(void);
void pw_link_destroy(PWLink *);
bool pw_link_listen(PWLink *, uint16_t port);
bool pw_link_connect(PWLink *, const char *ipv4, uint16_t port);
void pw_link_close(PWLink *);
void pw_link_poll(PWLink *);
bool pw_link_send(PWLink *, const uint8_t *, size_t);
size_t pw_link_read(PWLink *, uint8_t *, size_t);
// 0 off, 1 waiting/connecting, 2 connected, -1 failed.
int pw_link_state(const PWLink *);
uint16_t pw_link_port(const PWLink *);
const char *pw_link_error(const PWLink *);
#ifdef __cplusplus
}
#endif
