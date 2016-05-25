#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>

#include "metadata_exporter.h"
#include "metadata_input_munin.h"
#include "backend_event_loop.h"

/* polling interval in seconds */
#define MUNIN_POLLING_INTERVAL 5  

/* TODO
- handle socket disconnect/reconnect 
*/
ssize_t md_munin_readLine(int fd, void *buffer, size_t n)
{
    ssize_t numRead;                   
    size_t totRead;                  
    char *buf;
    char ch;

    if (n <= 0 || buffer == NULL) {
        errno = EINVAL;
        return -1;
    }
    buf = buffer;     

    totRead = 0;
    for (;;) {
        numRead = read(fd, &ch, 1);

        if (numRead == -1) {
            if (errno == EINTR)    
                continue;
            else
                return -1;         

        } else if (numRead == 0) {      
            if (totRead == 0)          
                return 0;
            else                     
                break;

        } else {                       
            if (totRead < n - 1) {  
                totRead++;
                *buf++ = ch;
            }

            if (ch == '\n')
                break;
        }
    }

    *buf = '\0';
    return totRead;
}

uint8_t md_munin_reconnect (struct md_input_munin *mim, const char *address, const char *port) {
    int n = -1;
    char buffer[256] = "\n";
    struct hostent* server;
    struct sockaddr_in serv_addr;

    if ((mim->munin_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      fprintf(stderr, "Error opening socket.\n");
      return RETVAL_FAILURE;
    }
    if ((server = gethostbyname(address)) == NULL) {
      fprintf(stderr, "No such host: %s\n", address);
      return RETVAL_FAILURE;
    }
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(atoi(port));
  
    if (connect(mim->munin_socket_fd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
      fprintf(stderr, "Could not connect to munin.\n");
      return RETVAL_FAILURE;
    } 
    
    if ((n = md_munin_readLine(mim->munin_socket_fd, buffer, 255))<0) {
      fprintf(stderr, "Could not read munin welcome string.\n");
      close(mim->munin_socket_fd);
      return RETVAL_FAILURE;
    }
    printf("%s", buffer);
    
    return RETVAL_SUCCESS;
}

int dtos_variable(struct json_object *jso, struct printbuf *pb, int level, int flags) {
  double val = json_object_get_double(jso);
  double ival;
  double fraction = modf(val, &ival);
  if (fraction > 0) {
    sprintbuf(pb, "%.6lf", val);
  } else {
    sprintbuf(pb, "%.0lf", ival);
  }
  return 0;
}

void md_munin_json_add_key_value(char* kv, json_object* blob) {
  char *running = kv;
  // a munin string is in the form uptime.value 1.23
  char *key     = strsep(&running, ".");
  if (running == NULL) {
    fprintf(stderr, "Munin return line did not match format key.value value\n");
    return;
  }
  char *svalue = strsep(&running, " "); //ignore .value
  if (running == NULL || (strncmp(svalue, "value", 5)!=0) ) {
    fprintf(stderr, "Munin return line did not match format key.value value\n");
    return;
  }
  char *value   = strsep(&running, "\n");
  
  struct json_object *obj_add = NULL;
  char *pEnd;
  double dvalue = strtod(value, &pEnd);
  if (*pEnd == 0) {
    if ((obj_add = json_object_new_double(dvalue))==NULL) {
      return;
    }
    json_object_set_serializer(obj_add, dtos_variable, NULL, NULL);
  } else {
    if ((obj_add = json_object_new_string(value))==NULL) {
      return;
    }
  } 
  json_object_object_add(blob, key, obj_add);
}

static void md_input_munin_handle_event(void *ptr, int32_t fd, uint32_t events)
{
    struct md_input_munin *mim = ptr;
    int i=0,n;
    uint64_t exp;

    if ((n=read(fd, &exp, sizeof(uint64_t))) < sizeof(uint64_t)) {
        fprintf(stderr, "Munin timer failed.\n");
        return;
    }
     
    setsockopt( mim->munin_socket_fd, IPPROTO_TCP, TCP_NODELAY, (void *)&i, sizeof(i));

    struct json_object *blob = NULL;
    struct timeval tv;

    if (!(blob = json_object_new_object()))
        return;

    for (i=0; i < mim->module_count; i++) {
      char cmd[256];
      char buffer[256];
      snprintf(cmd, 255, "fetch %s\n", mim->modules[i]);

      if ((n=write(mim->munin_socket_fd, cmd, strlen(cmd)))<0) {
          fprintf(stderr, "Writing to munin failed.\n");
          return;
      }

      struct json_object *obj_mod = NULL;
      if (!(obj_mod = json_object_new_object()))
        break;
      json_object_object_add(blob, mim->modules[i], obj_mod);

      while (1) {
        if ((n=md_munin_readLine(mim->munin_socket_fd, buffer, 255))<0) {
            fprintf(stderr, "Reading from munin failed.\n");
            return;
        }
        if ((buffer[0] == '#') || (buffer[0] == '.')) 
          break;
        md_munin_json_add_key_value(buffer, obj_mod);
      }
    }
       
    struct md_munin_event munin_event;
    memset(&munin_event, 0, sizeof(struct md_munin_event));
    gettimeofday(&tv, NULL);

    munin_event.md_type   = META_TYPE_MUNIN;
    munin_event.tstamp    = tv.tv_sec;
    munin_event.sequence  = mde_inc_seq(mim->parent);
    munin_event.json_blob = blob;

    //fprintf(stderr, "JSON dump: %s\n", json_object_to_json_string(blob));
    mde_publish_event_obj(mim->parent, (struct md_event *) &munin_event);
    json_object_put(blob);
}

static uint8_t md_munin_config(struct md_input_munin *mim,
                              const char *address,
                              const char *port,
                              const char *modules)
{
    int timer;
    struct itimerspec delay;

    if (md_munin_reconnect(mim, address, port) == RETVAL_SUCCESS) {
 
      char *running;
      if ((running = strdup(modules)) == NULL) {
          fprintf(stderr, "Memory allocation failed.");
          return RETVAL_FAILURE;
      }

      char **tokens = NULL;
      char * token  = strsep(&running, ",");
      int n_commas  = 0,
          n_tokens  = 0;

      while(token) {
        if ((tokens = realloc(tokens, sizeof(char*)* ++n_commas))==NULL) {
          fprintf(stderr, "Memory allocation failed.");
          return RETVAL_FAILURE;
        }
        tokens[n_commas-1] = token;
        token = strsep(&running, ",");
        n_tokens++;
      }

      mim->module_count = n_tokens;
      mim->modules      = tokens;

      if ((timer = timerfd_create(CLOCK_REALTIME, 0)) < 0) {
        fprintf(stderr, "Could not create munin polling timer.");
        return RETVAL_FAILURE;
      }
      
      delay.it_value.tv_sec     = MUNIN_POLLING_INTERVAL;
      delay.it_value.tv_nsec    = 0;
      delay.it_interval.tv_sec  = MUNIN_POLLING_INTERVAL;
      delay.it_interval.tv_nsec = 0;
      if (timerfd_settime(timer, TFD_TIMER_ABSTIME, &delay, NULL) < 0) {
        fprintf(stderr, "Could not initialize munin polling timer.");
        return RETVAL_FAILURE;
      }

      if(!(mim->event_handle = backend_create_epoll_handle(
                               mim, timer, md_input_munin_handle_event)))
        return RETVAL_FAILURE;

      backend_event_loop_update(
          mim->parent->event_loop, EPOLLIN, EPOLL_CTL_ADD, timer, mim->event_handle);

      return RETVAL_SUCCESS;
    } else {
      return RETVAL_FAILURE;
    }
}

void md_input_munin_destroy(void *ptr) 
{
    struct md_input_munin *mim = ptr;
    if ((mim != NULL) && (mim->module_count > 0)) { 
      free(mim->modules);
      mim->module_count = 0;
    }
}

static uint8_t md_input_munin_init(void *ptr, json_object* config)
{
    struct md_input_munin *mim = ptr;
    const char *address = NULL, *port = NULL;
    const char *modules = "memory,cpu";

    json_object* subconfig;
    if (json_object_object_get_ex(config, "munin", &subconfig)) {
        json_object_object_foreach(subconfig, key, val) {
            if (!strcmp(key, "address"))
                address = json_object_get_string(val);
            else if (!strcmp(key, "port"))
                port = json_object_get_string(val);
            else if (!strcmp(key, "modules"))
                modules = json_object_get_string(val);
        }
    }

    if (address == NULL || port == NULL) {
        fprintf(stderr, "Missing required Munin argument\n");
        return RETVAL_FAILURE;
    }

    return md_munin_config(mim, address, port, modules);
}

void md_munin_usage()
{
    fprintf(stderr, "\"munin\": {\t\tMunin input\n");
    // TODO: --munin-binary (no need to run systemd, nginx or inetd)
    fprintf(stderr, "  \"address\":\t\tmunin address\n");
    fprintf(stderr, "  \"port\":\t\tmunin port\n");
    // TODO: change comma separated modules into a JSON array
    fprintf(stderr, "  \"modules\":\t\tcomma separated, modules to export\n");
    fprintf(stderr, "},\n");
}

void md_munin_setup(struct md_exporter *mde, struct md_input_munin *mim)
{
    mim->parent  = mde;
    mim->init    = md_input_munin_init;
    mim->destroy = md_input_munin_destroy;
}
