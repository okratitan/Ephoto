#include "ephoto.h"

#define CONFIG_VERSION 8

static int _ephoto_config_load(Ephoto *ephoto);
static Eina_Bool _ephoto_on_config_save(void *data);

static Eet_Data_Descriptor *edd = NULL;

Eina_Bool
ephoto_config_init(Ephoto *ephoto)
{
   Eet_Data_Descriptor_Class eddc;

   if (!eet_eina_stream_data_descriptor_class_set(&eddc, sizeof (eddc),
                                                  "Ephoto_Config",
                                                  sizeof(Ephoto_Config)))
     {
        ERR("Unable to create the config data descriptor!");
        return EINA_FALSE;
     }

   if (!edd) edd = eet_data_descriptor_stream_new(&eddc);

#undef T
#undef D
#define T Ephoto_Config
#define D edd
#define C_VAL(edd, type, member, dtype) \
   EET_DATA_DESCRIPTOR_ADD_BASIC(edd, type, #member, member, dtype)

   C_VAL(D, T, config_version, EET_T_INT);
   C_VAL(D, T, thumb_size, EET_T_INT);
   C_VAL(D, T, thumb_gen_size, EET_T_INT);
   C_VAL(D, T, directory, EET_T_STRING);
   C_VAL(D, T, slideshow_timeout, EET_T_DOUBLE);
   C_VAL(D, T, slideshow_transition, EET_T_STRING);
   C_VAL(D, T, editor, EET_T_STRING);
   C_VAL(D, T, window_width, EET_T_INT);
   C_VAL(D, T, window_height, EET_T_INT);
   C_VAL(D, T, thumb_browser_panel, EET_T_INT);
   C_VAL(D, T, single_browser_panel, EET_T_INT);

   switch (_ephoto_config_load(ephoto))
     {
      case 0:
         /* Start a new config */
         ephoto->config->config_version = CONFIG_VERSION;
         ephoto->config->slideshow_timeout = 4.0;
         ephoto->config->slideshow_transition = eina_stringshare_add("fade");
         ephoto->config->editor = eina_stringshare_add("gimp %s");
         ephoto->config->window_width = 900;
         ephoto->config->window_height = 600;
         ephoto->config->thumb_browser_panel = 0;
         ephoto->config->single_browser_panel = 0;
         break;
      default:
         return EINA_TRUE;
     }

   ephoto_config_save(ephoto);
   return EINA_TRUE;
}

void
ephoto_config_save(Ephoto *ephoto)
{
   _ephoto_on_config_save(ephoto);
}

void
ephoto_config_free(Ephoto *ephoto)
{
   free(ephoto->config);
   ephoto->config = NULL;
}

static int
_ephoto_config_load(Ephoto *ephoto)
{
   Eet_File *ef;
   char buf[4096], buf2[4096];

   snprintf(buf2, sizeof(buf2), "%s/.config/ephoto", getenv("HOME"));
   ecore_file_mkpath(buf2);
   snprintf(buf, sizeof(buf), "%s/ephoto.cfg", buf2);

   ef = eet_open(buf, EET_FILE_MODE_READ);
   if (!ef)
     {
        ephoto_config_free(ephoto);
        ephoto->config = calloc(1, sizeof(Ephoto_Config));
        return 0;
     }

   ephoto->config = eet_data_read(ef, edd, "config");
   eet_close(ef);

   if (!ephoto->config || ephoto->config->config_version > CONFIG_VERSION)
     {
        ephoto_config_free(ephoto);
        ephoto->config = calloc(1, sizeof(Ephoto_Config));
        return 0;
     }

   if (ephoto->config->config_version < CONFIG_VERSION)
     {
        ecore_file_unlink(buf);
        ephoto_config_free(ephoto);
        ephoto->config = calloc(1, sizeof(Ephoto_Config));
        return 0;
     }
   return 1;
}

static Eina_Bool
_ephoto_on_config_save(void *data)
{
   Ephoto *ephoto = data;
   Eet_File *ef;
   char buf[4096], buf2[4096];

   snprintf(buf, sizeof(buf), "%s/.config/ephoto/ephoto.cfg", getenv("HOME"));
   snprintf(buf2, sizeof(buf2), "%s.tmp", buf);

   ef = eet_open(buf2, EET_FILE_MODE_WRITE);
   if (!ef) goto save_end;

   eet_data_write(ef, edd, "config", ephoto->config, 1);
   if (eet_close(ef)) goto save_end;

   if (!ecore_file_mv(buf2, buf)) goto save_end;

   INF("Config saved");

save_end:
   ecore_file_unlink(buf2);

   return ECORE_CALLBACK_CANCEL;
}
