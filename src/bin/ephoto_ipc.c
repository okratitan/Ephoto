#include "ephoto.h"
#undef ERR
#define ERR(...)        do { printf(__VA_ARGS__); putc('\n', stdout); } while(0)

char *e_ipc_socket = NULL;

#ifdef USE_IPC
/* local subsystem functions */
static Eina_Bool _e_ipc_cb_client_del(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);
static Eina_Bool _e_ipc_cb_client_data(void *data EINA_UNUSED, int type EINA_UNUSED, void *event);

/* local subsystem globals */
static Ecore_Ipc_Server *_e_ipc_server = NULL;
#endif

/* externally accessible functions */
int
e_ipc_init(void)
{
   char buf[4096], buf3[4096];
   const char *tmp, *user, *base = NULL;
   int pid, trynum = 0, id1 = 0;

   tmp = eina_environment_tmp_get();
   if (ecore_file_is_dir(tmp) && ecore_file_can_write(tmp))
     base = tmp;
   else
     ERR("Temp dir could not be accessed");

#ifdef _WIN32
   user = getenv("USERNAME");
#else
   user = getenv("USER");
#endif

   setenv("EPHOTO_IPC_SOCKET", "", 1);

   pid = (int)getpid();
   for (trynum = 0; trynum <= 4096; trynum++)
     {
        snprintf(buf, sizeof(buf), "%s/e-%s@%x",
                 base, user, id1);
        if (!mkdir(buf, S_IRWXU))
          {
#ifdef USE_IPC
             snprintf(buf3, sizeof(buf3), "%s/%i",
                      buf, pid);
             _e_ipc_server = ecore_ipc_server_add
                (ECORE_IPC_LOCAL_SYSTEM, buf3, 0, NULL);
             if (_e_ipc_server)
#endif
               {
                  e_ipc_socket = strdup(ecore_file_file_get(buf));
                  break;
               }
          }
        id1 = rand();
     }
#ifdef USE_IPC
   if (!_e_ipc_server)
     {
        ERR("Gave up after 4096 sockets in '%s'. All failed", base);
        return 0;
     }
   setenv("EPHOTO_IPC_SOCKET", "", 1);
   setenv("EPHOTO_IPC_SOCKET", buf3, 1);
   ecore_event_handler_add(ECORE_IPC_EVENT_CLIENT_DEL,
                           _e_ipc_cb_client_del, NULL);
   ecore_event_handler_add(ECORE_IPC_EVENT_CLIENT_DATA,
                           _e_ipc_cb_client_data, NULL);
#endif
   return 1;
}

int
e_ipc_shutdown(void)
{
#ifdef USE_IPC
   if (_e_ipc_server)
     {
        ecore_ipc_server_del(_e_ipc_server);
        _e_ipc_server = NULL;
     }
#endif
   free(e_ipc_socket);
   return 1;
}

#ifdef USE_IPC
/* local subsystem globals */
static Eina_Bool
_e_ipc_cb_client_del(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Ipc_Event_Client_Del *e;

   e = event;
   if (ecore_ipc_client_server_get(e->client) != _e_ipc_server)
     return ECORE_CALLBACK_PASS_ON;
   /* delete client sruct */
   e_thumb_client_del(e);
   ecore_ipc_client_del(e->client);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_ipc_cb_client_data(void *data EINA_UNUSED, int type EINA_UNUSED, void *event)
{
   Ecore_Ipc_Event_Client_Data *e;

   e = event;
   if (ecore_ipc_client_server_get(e->client) != _e_ipc_server)
     return ECORE_CALLBACK_PASS_ON;
   switch (e->major)
     {
      case EPHOTO_IPC_DOMAIN_THUMB:
        e_thumb_client_data(e);
        break;

      default:
        break;
     }
   return ECORE_CALLBACK_PASS_ON;
}

#endif
