#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/util.h>

#include <signal.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>
#include <assert.h>

#include "input.h"

#define MAX_CLIENTS 16
#define BUF_SIZE 256

struct clients {
  struct sockaddr_in addr;    // adres klienta
  time_t time;                // czas ostatniej aktywności
};

struct protocol {
  short unsigned int type; 
  short unsigned int len;
  char buf[BUF_SIZE];
};

struct listener_pack {
  int port;
  char *addr;
};

struct event_base *base;
struct bufferevent *bev;
pthread_t listener;
int listener_sock;
int client_timeout;
struct clients clients[MAX_CLIENTS];
struct input input;
char radio_name[BUF_SIZE];
char *str;
unsigned long count_audio;
uint16_t count_data;
unsigned long max_audio;
uint16_t max_data;
int start;
int meta;
int modeA;

void init_clients() {
  memset(&clients, 0, sizeof(clients));
}

int get_client_slot(void) {
  for(int i=0; i < MAX_CLIENTS; i++)
    if(!clients[i].time)
      return i;
  return -1;
}

/* zwraca indeks slotu, który odpowiada klientowi o adresie z parametru got */
int get_matching_slot(struct sockaddr_in *got) {
  if(got->sin_port == 0)
    return -1;
  for(int i=0; i < MAX_CLIENTS; i++)
    if((got->sin_addr.s_addr == clients[i].addr.sin_addr.s_addr) &&
       (got->sin_port == clients[i].addr.sin_port))
      return i;
  return -1;
}

/* podłączenie do grupy rozsyłania, oraz obsługa zleceń klientów */
void *serve_clients_request(void *data)
{
  /* stworzenie gniazda do nasłuchiwania grupowego */
  struct listener_pack *multi = (struct listener_pack*)data;

  listener_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if(listener_sock == -1) {
    syserr("Error preparing socket.");
  }

  struct ip_mreq ip_mreq;
  ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
  if (inet_aton(multi->addr, &ip_mreq.imr_multiaddr) == 0) {	  
    syserr("Error, inet_aton - invalid multicast address");
  }

  if (setsockopt(listener_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&ip_mreq, sizeof ip_mreq) < 0)
    syserr("setsockopt");

  struct sockaddr_in sin;
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = htonl(INADDR_ANY);
  sin.sin_port = htons(multi->port);
  if(bind(listener_sock, (struct sockaddr *)&sin, sizeof(sin)) == -1)
    syserr("bind");

  /* miejsce na adres klienta */
  struct sockaddr_in sin_client;
  socklen_t addr_size = 0;
  /* dane do przesłania */
  struct protocol pro;
  /* indeks aktualnego klienta */
  int cl;
  /* pętla do obsługi żądań */
  while(1) {
    if(recvfrom(listener_sock, &pro, sizeof(struct protocol), 0,(struct sockaddr *)&sin_client, &addr_size) < 0)
      continue;
    /* Nie przyjmuje złych komunikatów ani adresóœ zerowych */
    if(ntohl(sin_client.sin_addr.s_addr) == 0 || pro.len != 0)
      continue;
    /* zmiana na zwykły porządek bajtów */
    pro.len = ntohs(pro.len);
    pro.type = ntohs(pro.type);
    switch(pro.type) {
      case 1: // DISCOVER
        /* Sprawdzenie, czy klient jest już obsługiwany */
        cl = get_matching_slot(&sin_client);
        if(cl != -1) {
          fprintf(stderr, "Klient jest obsługiwany\n");
          break;
        }
        /* Wyszukiwanie wolnego slotu dla klienta */
        cl = get_client_slot();
        if(cl == -1) {
          fprintf(stderr, "Zbyt dużo klientów\n");
          break;
        }
        /* Wysłanie komunikatu IAM */
        pro.type = 2;
        memset(pro.buf, 0, BUF_SIZE);
        snprintf(pro.buf, BUF_SIZE, "%s", radio_name);
        pro.len = strlen(radio_name);
        /* zmiana na sieciowy porządek bajtów */
        pro.len = htons(pro.len);
        pro.type = htons(pro.type);
        sendto(listener_sock, &pro, sizeof(struct protocol), 0, (struct sockaddr *)&sin_client, addr_size);
        /* Uzupełnienie danych w slocie klienta */
        clients[cl].time = time(NULL);
        mempcpy(&clients[cl].addr, &sin_client, sizeof(struct sockaddr_in));
        break;
      case 3: // KEEPALIVE
        /* Odszukanie slotu klienta */
        cl = get_matching_slot(&sin_client);
        if(cl == -1) {
          fprintf(stderr, "Brak klienta w bazie\n");
          break;
        }
        /* Zaaktualizowanie czasu ostatniej aktywności klienta */
        clients[cl].time = time(NULL);
        break;
    }
  }
}

void a_read_cb (struct bufferevent *bev, void *arg) {
  size_t size;
beg:  
  if(start) {
    /* Wczytywanie treści radia */
    struct protocol pro;
    memset(pro.buf, 0, BUF_SIZE);
    /* ustawienie wartości pro.type oraz pro.len */
    if(count_audio < max_audio || max_audio == 0) {
      /* tresć audio */
      pro.type = 4;
      size = (BUF_SIZE < max_audio - count_audio || 
                 max_audio == 0) ? BUF_SIZE : max_audio - count_audio;
    } else {
      /* metadane */
      pro.type = 6;
      if(count_data == 0){
        bufferevent_read(bev, pro.buf, 1);
        max_data = (uint16_t)16*(uint8_t)pro.buf[0];
        memset(pro.buf, 0, BUF_SIZE);
      }
      size = (BUF_SIZE < max_data - count_data) ? BUF_SIZE : max_data - count_data;
    }
    /* wczytanie wiadomości */
    size = bufferevent_read(bev, pro.buf, size);
    pro.len = size;
    /* aktualizacja liczników */
    if(pro.type == 4) {
      count_audio += pro.len;
    } else {
      count_data += pro.len;
      if(count_data == max_data) {
        count_audio = 0;
        count_data = 0;
      }
    }
    if(!modeA) {
      /* zmiana na porządek sieciowy */
      pro.len = htons(pro.len);
      pro.type = htons(pro.type);
      /* iteracja po klientach w celu wysłania danych */
      for(int i=0;i<MAX_CLIENTS;i++) {
        if(clients[i].time != 0) {
          /* sprawdzenie czy klient jest aktywny */
          if(time(NULL) - clients[i].time < client_timeout)
            sendto(listener_sock, &pro, sizeof(struct protocol), 0,
                   (struct sockaddr *)&clients[i].addr, sizeof(clients[i].addr));
          else
            memset(&clients[i], 0, sizeof(struct clients));
        }
      }
    } else {
      if(pro.type == 4)
        write(STDOUT_FILENO, pro.buf, pro.len);
      else 
        write(STDERR_FILENO, pro.buf, pro.len);
    }
  } else {
    /* wczytywanie danych o radiu */
    str = evbuffer_readln(bufferevent_get_input(bev), NULL, EVBUFFER_EOL_CRLF_STRICT); 
    /* dane do przesłania */
    while (str != NULL) {
      /* zapisanie nazwy radia */
      if(strlen(radio_name) == 0) {
        if(strstr(str, "icy-name:") != NULL){
          snprintf(radio_name, BUF_SIZE, "%s", str+9);
        }
      }
      /* zapisanie metaint */
      if(max_audio == 0) {
        if(strstr(str, "icy-metaint:") != NULL){
          max_audio = atoi(str+12);
          if(!meta) syserr("Error, unwanted metadata.");
        }
      }
      /* sprawdzenie czy wczytano juz linijke \r\n\ */
      if(strlen(str) == 0){
        start = 1;
        free(str);
        goto beg;
      }
      free(str);
      /* próba wczytania następnej linijki */
      str = evbuffer_readln(bufferevent_get_input(bev), NULL, EVBUFFER_EOL_CRLF_STRICT); 
    }
  }
}  


void an_event_cb (struct bufferevent *bev, short what, void *arg) {
  if (what & BEV_EVENT_CONNECTED) {
    return;
  }
  if (event_base_loopbreak(base) == -1)
    syserr("event_base_loopbreak");
}

void sighandler(int sig) {
  if(str != NULL) free(str);
  event_base_loopbreak(base);
  bufferevent_free(bev);
  event_base_free(base);
  close(listener_sock);
}

/* MAIN */
int main(int argc, char *argv[])
{
/* sprawdzamy parametry */
  get_input(&input, &argc, &argv);
  modeA = (input.client_port == -1);

/* obsługa sygnału Ctrl+C */
  struct sigaction action;
  sigset_t block_mask;

  sigemptyset (&block_mask);

  action.sa_handler = sighandler;
  action.sa_mask = block_mask;
  action.sa_flags = 0;
  
  if (sigaction (SIGINT, &action, 0) == -1)
    syserr("sigaction");

if(!modeA) {
/* Wątek dla klientów */
  pthread_attr_t attr;
  struct listener_pack multi;
  multi.addr = (input.multi == -1) ? "239.10.11.12" : argv[input.multi];
  multi.port = atoi(argv[input.client_port]);
  if(pthread_attr_init(&attr) != 0)
    syserr("attr_init");
  if(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0)
    syserr("setdetach");
  if(pthread_create(&listener, &attr, serve_clients_request, &multi) != 0)
    syserr("create");

/* timeout dla klientów */
  client_timeout = (input.client_timeout == -1) ? 5 : atoi(argv[input.client_timeout]);
  if(client_timeout < 1) syserr("Error, bad timeout.");
}

/* event base */ 
  base = event_base_new();
  if(!base) syserr("Error creating base.");

/* wydarzenie komunikacji z radiem */
  max_audio = 0;
  count_audio = 0;
  max_data = 0;
  max_audio = 0;
  start = 0;

  bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
  if (!bev)
    syserr("bufferevent_socket_new");
  bufferevent_setcb(bev, a_read_cb, NULL, an_event_cb, (void *)bev);

  struct timeval timeout_read;
  timeout_read.tv_sec = (input.timeout == -1) ? 5 : atoi(argv[input.timeout]);
  if(timeout_read.tv_sec < 1) syserr("Error, bad timeout.");
  timeout_read.tv_usec = 0;
  bufferevent_set_timeouts(bev, &timeout_read, NULL);

  struct addrinfo addr_hints = {
    .ai_flags = 0,
    .ai_family = AF_INET,
    .ai_socktype = SOCK_STREAM,
    .ai_protocol = 0,
    .ai_addrlen = 0,
    .ai_addr = NULL,
    .ai_canonname = NULL,
    .ai_next = NULL
  };
  struct addrinfo *addr;

  if (getaddrinfo(argv[input.host], argv[input.port], &addr_hints, &addr))
    syserr("getaddrinfo");

  if(bufferevent_socket_connect(bev, addr->ai_addr, addr->ai_addrlen) == -1)
    syserr("bufferevent_socket_connect");
  freeaddrinfo(addr);
  if(bufferevent_enable(bev, EV_READ | EV_WRITE) == -1)
    syserr("bufferevent_enable");

/* Wysłanie żądania do serwera */
  char buf[BUF_SIZE];
  memset(buf, 0 , BUF_SIZE);
  if(input.meta == -1) {
    meta = 0;
  } else if (strstr(argv[input.meta], "yes") && strlen(argv[input.meta]) == 3) {
    meta = 1;
  } else if (strstr(argv[input.meta], "no") && strlen(argv[input.meta]) == 2) {
    meta = 0;
  } else {
    syserr("metadata");
  }

  snprintf(buf, BUF_SIZE, "GET %s HTTP/1.0\r\nHost: %s\r\nIcy-MetaData: %d\r\nConnection: close\r\n\r\n",
    argv[input.resource], argv[input.host], meta);
  bufferevent_write(bev, buf, strlen(buf));

/* Główna pętla */
  if(event_base_dispatch(base) == -1) 
    syserr("Error running dispatch loop.");

  return 0;
}
