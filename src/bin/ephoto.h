#ifndef _EPHOTO_H_
# define _EPHOTO_H_

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include <Eet.h>
# include <Ecore.h>
# include <Ecore_Evas.h>
# include <Ecore_File.h>
# include <Efreet_Mime.h>
# include <Elementary.h>
# include <Elementary_Cursor.h>
# include <Eina.h>
# include <Edje.h>
# include <Evas.h>
# include <Eio.h>
# include <Ethumb_Client.h>
# include <limits.h>
# include <math.h>
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <dirent.h>

# ifdef HAVE_PO
#  include <locale.h>
# endif

#if HAVE_GETTEXT && ENABLE_NLS
# include <libintl.h>
# define _(string) gettext (string)
#else
# define _(string) (string)
# define ngettext(String1, String2, Var) Var == 1 ? String1 : String2
#endif

typedef struct _Ephoto_Config Ephoto_Config;
typedef struct _Ephoto Ephoto;
typedef struct _Ephoto_Entry Ephoto_Entry;
typedef struct _Ephoto_Event_Entry_Create Ephoto_Event_Entry_Create;

typedef enum _Ephoto_State Ephoto_State;
typedef enum _Ephoto_Orient Ephoto_Orient;
typedef enum _Ephoto_Sort Ephoto_Sort;

Evas_Object *ephoto_window_add(const char *path);
void ephoto_title_set(Ephoto *ephoto, const char *title);
void ephoto_thumb_size_set(Ephoto *ephoto, int size);
Evas_Object *ephoto_thumb_add(Ephoto *ephoto, Evas_Object *parent,
    const char *path);
void ephoto_thumb_path_set(Evas_Object *obj, const char *path);
void ephoto_directory_set(Ephoto *ephoto, const char *path,
    Elm_Object_Item *expanded, Eina_Bool dirs_only, Eina_Bool thumbs_only);

Eina_Bool ephoto_config_init(Ephoto *em);
void ephoto_config_save(Ephoto *em);
void ephoto_config_free(Ephoto *em);
void ephoto_config_main(Ephoto *em);

Evas_Object *ephoto_single_browser_add(Ephoto *ephoto, Evas_Object *parent);
void ephoto_single_browser_entries_set(Evas_Object *obj, Eina_List *entries);
void ephoto_single_browser_entry_set(Evas_Object *obj, Ephoto_Entry *entry);
void ephoto_single_browser_path_pending_set(Evas_Object *obj,
    const char *path);
void ephoto_single_browser_image_data_update(Evas_Object *main,
    Evas_Object *image, Eina_Bool finished, unsigned int *image_data, int w,
    int h);
void ephoto_single_browser_cancel_editing(Evas_Object *main,
    Evas_Object *image);
/* smart callbacks called: "back" - the user wants to go back to the previous
 * screen. */
Evas_Object *ephoto_slideshow_add(Ephoto *ephoto, Evas_Object *parent);
void ephoto_slideshow_entries_set(Evas_Object *obj, Eina_List *entries);
void ephoto_slideshow_entry_set(Evas_Object *obj, Ephoto_Entry *entry);

/* smart callbacks called: "back" - the user wants to go back to the previous
 * screen. */
Evas_Object *ephoto_thumb_browser_add(Ephoto *ephoto, Evas_Object *parent);
void ephoto_thumb_browser_fsel_clear(Ephoto *ephoto);
void ephoto_thumb_browser_top_dir_set(Ephoto *ephoto, const char *dir);
void ephoto_thumb_browser_insert(Ephoto *ephoto, Ephoto_Entry *entry);
void ephoto_thumb_browser_remove(Ephoto *ephoto, Ephoto_Entry *entry);

/* smart callbacks called: "selected" - an item in the thumb browser is
 * selected. The selected Ephoto_Entry is passed as event_info argument. */

void ephoto_cropper_add(Evas_Object *main, Evas_Object *parent,
    Evas_Object *image_parent, Evas_Object *image);
void ephoto_bcg_add(Evas_Object *main, Evas_Object *parent,
    Evas_Object *image);
void ephoto_hsv_add(Evas_Object *main, Evas_Object *parent,
    Evas_Object *image);
void ephoto_color_add(Evas_Object *main, Evas_Object *parent,
    Evas_Object *image);
void ephoto_filter_blur(Evas_Object *main, Evas_Object *image);
void ephoto_filter_sharpen(Evas_Object *main, Evas_Object *image);
void ephoto_filter_black_and_white(Evas_Object *main, Evas_Object *image);
void ephoto_filter_old_photo(Evas_Object *main, Evas_Object *image);
void ephoto_filter_histogram_eq(Evas_Object *main, Evas_Object *image);

enum _Ephoto_State
{
   EPHOTO_STATE_THUMB,
   EPHOTO_STATE_SINGLE,
   EPHOTO_STATE_SLIDESHOW
};

enum _Ephoto_Orient
{
   EPHOTO_ORIENT_0 = 1,
   EPHOTO_ORIENT_FLIP_HORIZ = 2,
   EPHOTO_ORIENT_180 = 3,
   EPHOTO_ORIENT_FLIP_VERT = 4,
   EPHOTO_ORIENT_FLIP_VERT_90 = 5,
   EPHOTO_ORIENT_90 = 6,
   EPHOTO_ORIENT_FLIP_HORIZ_90 = 7,
   EPHOTO_ORIENT_270 = 8
};

enum _Ephoto_Sort
{
   EPHOTO_SORT_ALPHABETICAL_ASCENDING,
   EPHOTO_SORT_ALPHABETICAL_DESCENDING,
   EPHOTO_SORT_MODTIME_ASCENDING,
   EPHOTO_SORT_MODTIME_DESCENDING
};

struct _Ephoto_Config
{
   int config_version;
   int thumb_size;
   int thumb_gen_size;
   const char *directory;
   double slideshow_timeout;
   const char *slideshow_transition;
   int window_width;
   int window_height;
   Eina_Bool fsel_hide;
   Eina_Bool tool_hide;
   const char *open;
   Eina_Bool prompts;
   Eina_Bool drop;
   Evas_Object *slide_time;
   Evas_Object *slide_trans;
   Evas_Object *hide_toolbar;
   Evas_Object *open_dir;
   Evas_Object *open_dir_custom;
   Evas_Object *show_prompts;
   Evas_Object *move_drop;
};

struct _Ephoto
{
   Evas_Object *win;
   Evas_Object *panel;
   Evas_Object *pager;

   Evas_Object *thumb_browser;
   Evas_Object *single_browser;
   Evas_Object *slideshow;
   Elm_Object_Item *tb;
   Elm_Object_Item *sb;
   Elm_Object_Item *sl;

   Eina_List *entries;
   Eina_List *selentries;
   Eina_List *searchentries;
   Eina_List *thumbs;

   Eio_Monitor *monitor;
   Eina_List *monitor_handlers;

   const char *top_directory;

   int thumb_gen_size;
   Evas_Coord bottom_bar_size;
   struct
   {
      Ecore_Timer *thumb_regen;
   } timer;
   struct
   {
      Ecore_Job *change_dir;
   } job;

   Eio_File *ls;

   Evas_Object *prefs_win;
   Ephoto_State state, prev_state;

   Ephoto_Config *config;
   Ephoto_Entry *thumb_entry;
};

struct _Ephoto_Entry
{
   const char *path;
   const char *basename;
   const char *label;
   double size;
   Ephoto *ephoto;
   Eio_Monitor *monitor;
   Eina_List *monitor_handlers;
   Elm_Object_Item *item;
   Elm_Object_Item *parent;
   Eina_List *free_listeners;
   Eina_Bool is_dir;
   Eina_Bool no_delete;
   Evas_Object *genlist;
};

struct _Ephoto_Event_Entry_Create
{
   Ephoto_Entry *entry;
};

Ephoto_Entry *ephoto_entry_new(Ephoto *ephoto, const char *path,
    const char *label, Eina_File_Type type);
void ephoto_entry_free(Ephoto *ephoto, Ephoto_Entry *entry);
void ephoto_entry_free_listener_add(Ephoto_Entry *entry,
    void (*cb) (void *data, const Ephoto_Entry *entry), const void *data);
void ephoto_entry_free_listener_del(Ephoto_Entry *entry,
    void (*cb) (void *data, const Ephoto_Entry *entry), const void *data);
void ephoto_entries_free(Ephoto *ephoto);
int ephoto_entries_cmp(const void *pa, const void *pb);

static inline Eina_Bool
_ephoto_eina_file_direct_info_image_useful(const Eina_File_Direct_Info *info)
{
   const char *type, *bname;
   int i = 0;

   const char *filters[] = {
      "png", "jpeg", "jpg", "eet", "xpm", "tiff", "gif", "svg", "webp",
      "pmaps", "bmp", "tga", "wbmp", "ico", "psd", "jp2k", "generic", "3fr",
      "ari", "arw", "bay", "crw", "cr2", "cap", "dcs", "dcr", "dng", "drf",
      "eip", "erf", "fff", "iiq", "k25", "kdc", "mdc", "mef", "mos", "mrw",
      "nef", "nrw", "obm", "orf", "pef", "ptx", "pxn", "r3d", "raf", "raw",
      "rwl", "rw2", "rwz", "sr2", "srf", "srw", "tif", "x3f"
   };

   bname = info->path + info->name_start;
   if (bname[0] == '.')
      return EINA_FALSE;
   if ((info->type != EINA_FILE_REG) && (info->type != EINA_FILE_UNKNOWN))
      return EINA_FALSE;

   type = strrchr(bname, '.');
   if (!type)
      return EINA_FALSE;
   int count = sizeof(filters) / sizeof(filters[0]);

   for (i = 0; i < count; i++)
     {
	if (!strcasecmp(type + 1, filters[i]))
	   return evas_object_image_extension_can_load_get(bname);
     }
   return EINA_FALSE;
}

static inline Eina_Bool
_ephoto_file_image_can_save(const char *ext)
{
   int i = 0;

   const char *filters[] = {
      "png", "jpeg", "jpg", "eet", "xpm", "tiff", "tif", "gif", "svg", "webp",
      "pmaps", "bmp", "wbmp", "ico", "generic"
   };

   int count = sizeof(filters) / sizeof(filters[0]);
   for (i = 0; i < count; i++)
     {
        if (!strcasecmp(ext, filters[i]))
           return EINA_TRUE;
     }
   return EINA_FALSE;
}

extern int EPHOTO_EVENT_ENTRY_CREATE;
extern int EPHOTO_EVENT_POPULATE_START;
extern int EPHOTO_EVENT_POPULATE_END;
extern int EPHOTO_EVENT_POPULATE_ERROR;

#endif
