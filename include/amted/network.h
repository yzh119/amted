/*!
 * \file amted/network.h
 * \brief Network related structures and functions for AMTED server/client.
 */
#ifndef AMTED_NETWORK_H_
#define AMTED_NETWORK_H_


#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#ifdef USE_EPOLL
#include <sys/epoll.h>
#endif

#endif  // AMTED_NETWORK_H_
