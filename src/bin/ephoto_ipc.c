#include "ephoto.h"
#undef ERR
#define ERR(...) do { printf(__VA_ARGS__); putc('\n', stdout); } while (0)

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
#ifdef USE_IPC
   int port = getpid();
   /* NOTE: windows has no getppid(), so use an envvar */
   char port_str[sizeof("2147483648")] = "";
   snprintf(port_str, sizeof(port_str), "%d", port);
   setenv("EPHOTO_IPC_PORT", port_str, 1);
   _e_ipc_server = ecore_ipc_server_add
       (ECORE_IPC_LOCAL_SYSTEM, "ephoto", port, NULL);
   if (!_e_ipc_server)
     {
        ERR("Couldn't create Ephoto IPC server port=%d", port);
        return 0;
     }
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
