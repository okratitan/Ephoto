#include "ephoto.h"
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#ifdef _WIN32
# include <winsock2.h>
#endif

#define SHSH(n, v) ((((v) << (n)) & 0xffffffff) | ((v) >> (32 - (n))))

typedef struct _E_Thumb E_Thumb;

struct _E_Thumb
{
   int   objid;
   int   w, h;
   char *file;
   char *key;
};

/* local subsystem functions */
static int       _e_ipc_init(void);
static Eina_Bool _e_ipc_cb_server_add(void *data,
                                      int type,
                                      void *event);
static Eina_Bool _e_ipc_cb_server_del(void *data,
                                      int type,
                                      void *event);
static Eina_Bool _e_ipc_cb_server_data(void *data,
                                       int type,
                                       void *event);
static Eina_Bool _e_cb_timer(void *data);
static void      _e_thumb_generate(E_Thumb *eth);
static char     *_e_thumb_file_id(char *file,
                                  char *key);

/* local subsystem globals */
static Ecore_Ipc_Server *_e_ipc_server = NULL;
static Eina_List *_thumblist = NULL;
static Ecore_Timer *_timer = NULL;
static char _thumbdir[4096] = "";

/* externally accessible functions */
int
main(int argc,
     char **argv)
{
   int i;

   for (i = 1; i < argc; i++)
     {
        if ((!strcmp(argv[i], "-h")) ||
            (!strcmp(argv[i], "-help")) ||
            (!strcmp(argv[i], "--help")))
          {
             printf(
               "This is an internal tool for Ephoto.\n"
               "do not use it.\n"
               );
             exit(0);
          }
        else if (!strncmp(argv[i], "--nice=", 7))
          {
#ifdef HAVE_NICE
             const char *val;

             val = argv[i] + 7;
             if (*val)
               {
                  if (nice(atoi(val)) < 0) perror("nice");
               }
#endif
          }
     }

   ecore_app_no_system_modules();
   ecore_init();
   ecore_app_args_set(argc, (const char **)argv);
   eet_init();
   evas_init();
   ecore_evas_init();
   edje_init();
   ecore_file_init();
   ecore_ipc_init();

   snprintf(_thumbdir, PATH_MAX, "%s/ephoto/thumbnails", efreet_cache_home_get());
   ecore_file_mkpath(_thumbdir);

   if (_e_ipc_init()) ecore_main_loop_begin();

   if (_e_ipc_server)
     {
        ecore_ipc_server_del(_e_ipc_server);
        _e_ipc_server = NULL;
     }

   ecore_ipc_shutdown();
   ecore_file_shutdown();
   ecore_evas_shutdown();
   edje_shutdown();
   evas_shutdown();
   eet_shutdown();
   ecore_shutdown();

   return 0;
}

/* local subsystem functions */
static int
_e_ipc_init(void)
{
   const char *port_str = getenv("EPHOTO_IPC_PORT");
   int port;
   if ((!port_str) || ((port = atoi(port_str)) == 0))
     {
        printf("Error: could not query Ephoto IPC port=%s\n", port_str);
        return 0;
     }
   _e_ipc_server = ecore_ipc_server_connect
     (ECORE_IPC_LOCAL_SYSTEM, "ephoto", port, NULL);
   if (!_e_ipc_server)
     {
        printf("Error: could not connect to Ephoto IPC port=%d\n", port);
        return 0;
     }

   ecore_event_handler_add(ECORE_IPC_EVENT_SERVER_ADD, _e_ipc_cb_server_add, NULL);
   ecore_event_handler_add(ECORE_IPC_EVENT_SERVER_DEL, _e_ipc_cb_server_del, NULL);
   ecore_event_handler_add(ECORE_IPC_EVENT_SERVER_DATA, _e_ipc_cb_server_data, NULL);

   return 1;
}

static Eina_Bool
_e_ipc_cb_server_add(void *data EINA_UNUSED,
                     int type   EINA_UNUSED,
                     void *event)
{
   Ecore_Ipc_Event_Server_Add *e;

   e = event;
   ecore_ipc_server_send(e->server,
                         EPHOTO_IPC_DOMAIN_THUMB,
                         1 /*hello*/,
                         0, 0, 0, NULL, 0); /* send hello */
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_ipc_cb_server_del(void *data  EINA_UNUSED,
                     int type    EINA_UNUSED,
                     void *event EINA_UNUSED)
{
   /* quit now */
   ecore_main_loop_quit();
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_ipc_cb_server_data(void *data EINA_UNUSED,
                      int type   EINA_UNUSED,
                      void *event)
{
   Ecore_Ipc_Event_Server_Data *e;
   E_Thumb *eth;
   Eina_List *l;
   char *file = NULL;
   char *key = NULL;

   e = event;
   if (e->major != EPHOTO_IPC_DOMAIN_THUMB) return ECORE_CALLBACK_PASS_ON;
   switch (e->minor)
     {
      case 1:
        if (e->data)
          {
             /* begin thumb */
             /* don't check stuff. since this connects TO E it is connecting */
             /* TO a trusted process that WILL send this message properly */
             /* formatted. if the thumbnailer dies anyway - it's not a big loss */
             /* but it is a sign of a bug in e formatting messages maybe */
             file = e->data;
             key = file + strlen(file) + 1;
             if (!key[0]) key = NULL;
             eth = calloc(1, sizeof(E_Thumb));
             if (eth)
               {
                  eth->objid = e->ref;
                  eth->w = e->ref_to;
                  eth->h = e->response;
                  eth->file = strdup(file);
                  if (key) eth->key = strdup(key);
                  _thumblist = eina_list_append(_thumblist, eth);
                  if (!_timer) _timer = ecore_timer_loop_add(0.001, _e_cb_timer, NULL);
               }
          }
        break;

      case 2:
        /* end thumb */
        EINA_LIST_FOREACH(_thumblist, l, eth)
          {
             if (eth->objid == e->ref)
               {
                  _thumblist = eina_list_remove_list(_thumblist, l);
                  free(eth->file);
                  free(eth->key);
                  free(eth);
                  break;
               }
          }
        break;

      case 3:
        /* quit now */
        ecore_main_loop_quit();
        break;

      default:
        break;
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_e_cb_timer(void *data EINA_UNUSED)
{
   E_Thumb *eth;
   /*
      Eina_List *del_list = NULL, *l;
    */

   /* take thumb at head of list */
   if (_thumblist)
     {
        eth = eina_list_data_get(_thumblist);
        _thumblist = eina_list_remove_list(_thumblist, _thumblist);
        _e_thumb_generate(eth);
        free(eth->file);
        free(eth->key);
        free(eth);

        if (_thumblist) _timer = ecore_timer_loop_add(0.01, _e_cb_timer, NULL);
        else _timer = NULL;
     }
   else
     _timer = NULL;
   return ECORE_CALLBACK_CANCEL;
}

typedef struct _Color Color;

struct _Color
{
   Color        *closest;
   int           closest_dist;
   int           use;
   unsigned char r, g, b;
};

static void
_e_thumb_generate(E_Thumb *eth)
{
   char buf[4096], dbuf[4096], *id, *td, *ext = NULL;
   Evas *evas = NULL, *evas_im = NULL;
   Ecore_Evas *ee = NULL, *ee_im = NULL;
   Evas_Object *im = NULL, *edje = NULL;
   Eet_File *ef = NULL;
   int iw, ih, alpha, ww, hh;
   const unsigned int *data = NULL;
   time_t mtime_orig, mtime_thumb;

   id = _e_thumb_file_id(eth->file, eth->key);
   if (!id) return;

   td = strdup(id);
   if (!td)
     {
        free(id);
        return;
     }
   td[2] = 0;

   snprintf(dbuf, sizeof(dbuf), "%s/%s", _thumbdir, td);
   snprintf(buf, sizeof(buf), "%s/%s/%s-%ix%i.thm",
            _thumbdir, td, id + 2, eth->w, eth->h);
   free(id);
   free(td);

   mtime_orig = ecore_file_mod_time(eth->file);
   mtime_thumb = ecore_file_mod_time(buf);
   while (mtime_thumb <= mtime_orig)
     {
        unsigned int *data1;
        Eina_Bool sortkey;
        Evas_Object *im2, *bg;

        im = NULL;
        im2 = NULL;
        bg = NULL;

        ecore_file_mkdir(dbuf);

        edje_file_cache_set(0);
        edje_collection_cache_set(0);
        ee = ecore_evas_buffer_new(1, 1);
        evas = ecore_evas_get(ee);
        evas_image_cache_set(evas, 0);
        evas_font_cache_set(evas, 0);
        ww = 0;
        hh = 0;
        alpha = 1;
        ext = strrchr(eth->file, '.');

        sortkey = EINA_FALSE;

        if ((ext) && (eth->key) &&
            ((!strcasecmp(ext, ".edj")) ||
             (!strcasecmp(ext, ".eap"))))
          {
             ww = eth->w;
             hh = eth->h;
             im = ecore_evas_object_image_new(ee);
             ee_im = evas_object_data_get(im, "Ecore_Evas");
             evas_im = ecore_evas_get(ee_im);
             evas_image_cache_set(evas_im, 0);
             evas_font_cache_set(evas_im, 0);
             evas_object_image_size_set(im, ww * 4, hh * 4);
             evas_object_image_fill_set(im, 0, 0, ww, hh);
             edje = edje_object_add(evas_im);
             if ((eth->key) &&
                 ((!strcmp(eth->key, "e/desktop/background")) ||
                  (!strcmp(eth->key, "e/init/splash"))))
               alpha = 0;
             if (edje_object_file_set(edje, eth->file, eth->key))
               {
                  evas_object_move(edje, 0, 0);
                  evas_object_resize(edje, ww * 4, hh * 4);
                  evas_object_show(edje);
               }
             evas_object_move(im, 0, 0);
             evas_object_resize(im, ww, hh);
             sortkey = EINA_TRUE;
          }
        else if ((ext) &&
                 ((!strcasecmp(ext, ".ttf")) ||
                  (!strcasecmp(ext, ".pcf")) ||
                  (!strcasecmp(ext, ".bdf")) ||
                  (!strcasecmp(ext, ".ttx")) ||
                  (!strcasecmp(ext, ".pfa")) ||
                  (!strcasecmp(ext, ".pfb")) ||
                  (!strcasecmp(ext, ".afm")) ||
                  (!strcasecmp(ext, ".sfd")) ||
                  (!strcasecmp(ext, ".snf")) ||
                  (!strcasecmp(ext, ".otf")) ||
                  (!strcasecmp(ext, ".psf")) ||
                  (!strcasecmp(ext, ".ttc")) ||
                  (!strcasecmp(ext, ".ttx")) ||
                  (!strcasecmp(ext, ".gsf")) ||
                  (!strcasecmp(ext, ".spd"))
                 ))
          {
             Evas_Coord tx = 0, ty = 0, tw = 0, th = 0;
             ww = eth->w;
             hh = eth->h;
             alpha = 0;

             bg = evas_object_rectangle_add(evas);
             evas_object_color_set(bg, 96, 96, 96, 255);
             evas_object_move(bg, 0, 0);
             evas_object_resize(bg, ww, hh);
             evas_object_show(bg);

             im = evas_object_text_add(evas);
             evas_object_text_font_set(im, eth->file, hh / 4);
             evas_object_color_set(im, 192, 192, 192, 255);
             evas_object_text_ellipsis_set(im, 0.0);
             evas_object_text_text_set(im, "ABCabc");
             evas_object_geometry_get(im, NULL, NULL, &tw, &th);
             if (tw > ww) tw = ww;
             tx = 0 + ((ww - tw) / 2);
             ty = 0 + (((hh / 2) - th) / 2);
             evas_object_move(im, tx, ty);
             evas_object_resize(im, tw, th);
             evas_object_show(im);

             im2 = evas_object_text_add(evas);
             evas_object_text_font_set(im2, eth->file, hh / 4);
             evas_object_color_set(im2, 255, 255, 255, 255);
             evas_object_text_ellipsis_set(im2, 0.0);
             evas_object_text_text_set(im2, "123!@?");
             evas_object_geometry_get(im2, NULL, NULL, &tw, &th);
             if (tw > ww) tw = ww;
             tx = 0 + ((ww - tw) / 2);
             ty = (hh / 2) + (((hh / 2) - th) / 2);
             evas_object_move(im2, tx, ty);
             evas_object_resize(im2, tw, th);
             evas_object_show(im2);
          }
        else if (evas_object_image_extension_can_load_get(ext))
          {
             im = evas_object_image_add(evas);
             evas_object_image_load_orientation_set(im, EINA_TRUE);
             evas_object_image_load_size_set(im, eth->w, eth->h);
             evas_object_image_file_set(im, eth->file, NULL);
             iw = 0; ih = 0;
             evas_object_image_size_get(im, &iw, &ih);
             alpha = evas_object_image_alpha_get(im);
             if ((iw > 0) && (ih > 0))
               {
                  ww = eth->w;
                  hh = (eth->w * ih) / iw;
                  if (hh > eth->h)
                    {
                       hh = eth->h;
                       ww = (eth->h * iw) / ih;
                    }
                  evas_object_image_fill_set(im, 0, 0, ww, hh);
               }
             evas_object_move(im, 0, 0);
             evas_object_resize(im, ww, hh);
             sortkey = EINA_TRUE;
          }
        else
          goto end;

        ecore_evas_alpha_set(ee, alpha);
        ecore_evas_resize(ee, ww, hh);
        evas_object_show(im);
        if (ww <= 0) goto end;
        data = ecore_evas_buffer_pixels_get(ee);
        if (!data) goto end;
        ef = eet_open(buf, EET_FILE_MODE_WRITE);
        if (!ef) goto end;
        eet_write(ef, "/thumbnail/orig_file",
                  eth->file, strlen(eth->file), 1);
        if (eth->key)
          eet_write(ef, "/thumbnail/orig_key",
                    eth->key, strlen(eth->key), 1);
        eet_data_image_write(ef, "/thumbnail/data",
                             (void *)data, ww, hh, alpha,
                             0, 91, 1);
        if (sortkey)
          {
             ww = 4; hh = 4;
             evas_object_image_fill_set(im, 0, 0, ww, hh);
             evas_object_resize(im, ww, hh);
             ecore_evas_resize(ee, ww, hh);
             data = ecore_evas_buffer_pixels_get(ee);
             if (!data) goto end;

             data1 = malloc(ww * hh * sizeof(unsigned int));
             memcpy(data1, data, ww * hh * sizeof(unsigned int));
             ww = 2; hh = 2;
             evas_object_image_fill_set(im, 0, 0, ww, hh);
             evas_object_resize(im, ww, hh);
             ecore_evas_resize(ee, ww, hh);
             data = ecore_evas_buffer_pixels_get(ee);
             if (data)
               {
                  unsigned int *data2;

                  data2 = malloc(ww * hh * sizeof(unsigned int));
                  memcpy(data2, data, ww * hh * sizeof(unsigned int));
                  ww = 1; hh = 1;
                  evas_object_image_fill_set(im, 0, 0, ww, hh);
                  evas_object_resize(im, ww, hh);
                  ecore_evas_resize(ee, ww, hh);
                  data = ecore_evas_buffer_pixels_get(ee);
                  if (data)
                    {
                       unsigned int *data3;
                       unsigned char id2[(21 * 4) + 1];
                       int n, i;
                       int hi, si, vi;
                       float h, s, v;
                       const int pat2[4] =
                         {
                            0, 3, 1, 2
                         };
                       const int pat1[16] =
                         {
                            5, 10, 6, 9,
                            0, 15, 3, 12,
                            1, 14, 7, 8,
                            4, 11, 2, 13
                         };

                       /* ww = hh = 1 here */
                       data3 = malloc(sizeof(unsigned int));
                       memcpy(data3, data, sizeof(unsigned int));
                       // sort_id
                       n = 0;
#define A(v) (((v) >> 24) & 0xff)
#define R(v) (((v) >> 16) & 0xff)
#define G(v) (((v) >> 8) & 0xff)
#define B(v) (((v)) & 0xff)
#define HSV(p)                                         \
  evas_color_rgb_to_hsv(R(p), G(p), B(p), &h, &s, &v); \
  hi = 20 * (h / 360.0);                               \
  si = 20 * s;                                         \
  vi = 20 * v;                                         \
  if (si < 2) hi = 25;
#define SAVEHSV(h, s, v) \
  id2[n++] = 'a' + h;    \
  id2[n++] = 'a' + v;    \
  id2[n++] = 'a' + s;
#define SAVEX(x) \
  id2[n++] = 'a' + x;
#if 0
                       HSV(data3[0]);
                       SAVEHSV(hi, si, vi);
                       for (i = 0; i < 4; i++)
                         {
                            HSV(data2[pat2[i]]);
                            SAVEHSV(hi, si, vi);
                         }
                       for (i = 0; i < 16; i++)
                         {
                            HSV(data1[pat1[i]]);
                            SAVEHSV(hi, si, vi);
                         }
#else
                       HSV(data3[0]);
                       SAVEX(hi);
                       for (i = 0; i < 4; i++)
                         {
                            HSV(data2[pat2[i]]);
                            SAVEX(hi);
                         }
                       for (i = 0; i < 16; i++)
                         {
                            HSV(data1[pat1[i]]);
                            SAVEX(hi);
                         }
                       HSV(data3[0]);
                       SAVEX(vi);
                       for (i = 0; i < 4; i++)
                         {
                            HSV(data2[pat2[i]]);
                            SAVEX(vi);
                         }
                       for (i = 0; i < 16; i++)
                         {
                            HSV(data1[pat1[i]]);
                            SAVEX(vi);
                         }
                       HSV(data3[0]);
                       SAVEX(si);
                       for (i = 0; i < 4; i++)
                         {
                            HSV(data2[pat2[i]]);
                            SAVEX(si);
                         }
                       for (i = 0; i < 16; i++)
                         {
                            HSV(data1[pat1[i]]);
                            SAVEX(si);
                         }
#endif
                       id2[n++] = 0;
                       eet_write(ef, "/thumbnail/sort_id", id2, n, 1);
                       free(data3);
                    }
                  free(data2);
               }
             free(data1);
          }
end:
        if (ef) eet_close(ef);

        /* will free all */
        if (edje) evas_object_del(edje);
        if (ee_im) ecore_evas_free(ee_im);
        else if (im) evas_object_del(im);
        if (im2) evas_object_del(im2);
        if (bg) evas_object_del(bg);
        ecore_evas_free(ee);
        eet_clearcache();
        break;
     }
   /* send back path to thumb */
   ecore_ipc_server_send(_e_ipc_server, EPHOTO_IPC_DOMAIN_THUMB, 2, eth->objid, 0, 0, buf, strlen(buf) + 1);
}

static int
e_sha1_sum(unsigned char *data, int size, unsigned char *dst)
{
   unsigned int digest[5], word[80], wa, wb, wc, wd, we, t;
   unsigned char buf[64], *d;
   int idx, left, i;
   const unsigned int magic[4] =
   {
      0x5a827999,
      0x6ed9eba1,
      0x8f1bbcdc,
      0xca62c1d6
   };

   idx = 0;
   digest[0] = 0x67452301;
   digest[1] = 0xefcdab89;
   digest[2] = 0x98badcfe;
   digest[3] = 0x10325476;
   digest[4] = 0xc3d2e1f0;

   memset(buf, 0, sizeof(buf));
   for (left = size, d = data; left > 0; left--, d++)
     {
        if ((idx == 0) && (left < 64))
          {
             memset(buf, 0, 60);
             buf[60] = (size >> 24) & 0xff;
             buf[61] = (size >> 16) & 0xff;
             buf[62] = (size >> 8) & 0xff;
             buf[63] = (size) & 0xff;
          }
        buf[idx] = *d;
        idx++;;
        if ((idx == 64) || (left == 1))
          {
             if ((left == 1) && (idx < 64)) buf[idx] = 0x80;
             for (i = 0; i < 16; i++)
               {
                  word[i] = (unsigned int)buf[(i * 4)    ] << 24;
                  word[i] |= (unsigned int)buf[(i * 4) + 1] << 16;
                  word[i] |= (unsigned int)buf[(i * 4) + 2] << 8;
                  word[i] |= (unsigned int)buf[(i * 4) + 3];
               }
             for (i = 16; i < 80; i++)
               word[i] = SHSH(1,
                              word[i - 3 ] ^ word[i - 8 ] ^
                              word[i - 14] ^ word[i - 16]);
             wa = digest[0];
             wb = digest[1];
             wc = digest[2];
             wd = digest[3];
             we = digest[4];
             for (i = 0; i < 80; i++)
               {
                  if (i < 20)
                    t = SHSH(5, wa) + ((wb & wc) | ((~wb) & wd)) +
                      we + word[i] + magic[0];
                  else if (i < 40)
                    t = SHSH(5, wa) + (wb ^ wc ^ wd) +
                      we + word[i] + magic[1];
                  else if (i < 60)
                    t = SHSH(5, wa) + ((wb & wc) | (wb & wd) | (wc & wd)) +
                      we + word[i] + magic[2];
                  else if (i < 80)
                    t = SHSH(5, wa) + (wb ^ wc ^ wd) +
                      we + word[i] + magic[3];
                  we = wd;
                  wd = wc;
                  wc = SHSH(30, wb);
                  wb = wa;
                  wa = t;
               }
             digest[0] += wa;
             digest[1] += wb;
             digest[2] += wc;
             digest[3] += wd;
             digest[4] += we;
             idx = 0;
          }
     }

   t = htonl(digest[0]); digest[0] = t;
   t = htonl(digest[1]); digest[1] = t;
   t = htonl(digest[2]); digest[2] = t;
   t = htonl(digest[3]); digest[3] = t;
   t = htonl(digest[4]); digest[4] = t;

   memcpy(dst, digest, 5 * 4);
   return 1;
}

static char *
_e_thumb_file_id(char *file,
                 char *key)
{
   char s[64];
   const char *chmap = "0123456789abcdef";
   unsigned char *buf, id[20];
   int i, len, lenf;

   len = 0;
   lenf = strlen(file);
   len += lenf;
   len++;
   if (key)
     {
        key += strlen(key);
        len++;
     }
   buf = alloca(len);

   strcpy((char *)buf, file);
   if (key) strcpy((char *)(buf + lenf + 1), key);

   e_sha1_sum(buf, len, id);

   for (i = 0; i < 20; i++)
     {
        s[(i * 2) + 0] = chmap[(id[i] >> 4) & 0xf];
        s[(i * 2) + 1] = chmap[(id[i]) & 0xf];
     }
   s[(i * 2)] = 0;
   return strdup(s);
}

