#include "ephoto.h"

#define ZOOM_MAX            512
#define ZOOM_MIN            128
#define ZOOM_STEP           32

#define TODO_ITEM_MIN_BATCH 5

#define FILESEP             "file://"
#define FILESEP_LEN         sizeof(FILESEP) - 1

#define DRAG_TIMEOUT        0.3
#define ANIM_TIME           0.2

static Eina_Bool _5s_cancel = EINA_FALSE;
static Ecore_Timer *_5s_timeout = NULL;

typedef struct _Ephoto_Thumb_Browser Ephoto_Thumb_Browser;

struct _Ephoto_Thumb_Browser
{
   Ephoto *ephoto;
   Evas_Object *main;
   Evas_Object *table;
   Evas_Object *grid;
   Evas_Object *nolabel;
   Evas_Object *infolabel;
   Evas_Object *bar;
   Evas_Object *fsel;
   Evas_Object *min;
   Evas_Object *max;
   Evas_Object *leftbox;
   Evas_Object *direntry;
   Evas_Object *dir_loading;
   Evas_Object *ficon;
   Elm_Object_Item *dir_current;
   Ephoto_Sort sort;
   Eio_File *ls;
   Eina_List *cut_items;
   Eina_List *copy_items;
   Eina_List *handlers;
   Eina_List *idler_pos;
   Eina_List *todo_items;
   Ecore_Idler *idler;
   Ecore_Job *change_dir_job;
   Ecore_Timer *click_timer;
   int thumbs_only;
   int dirs_only;
   int totimages;
   int file_errors;
   int dragging;
   double totsize;
   struct
   {
      Ecore_Animator *todo_items;
      int count;
      int processed;
   } animator;
   Eina_Bool main_deleted:1;
};

static Elm_Gengrid_Item_Class _ephoto_thumb_file_class;
static Elm_Genlist_Item_Class _ephoto_dir_class;

static void _ephoto_dir_hide_folders(void *data, Evas_Object *obj,
    void *event_info);
static void _ephoto_dir_show_folders(void *data, Evas_Object *obj,
    void *event_info);

static void
_todo_items_free(Ephoto_Thumb_Browser *tb)
{
   eina_list_free(tb->todo_items);
   tb->todo_items = NULL;
}

static void
_on_list_expand_req(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto_Thumb_Browser *tb = data;
   Elm_Object_Item *it = event_info;

   ecore_job_del(tb->change_dir_job);
   tb->change_dir_job = NULL;
   ecore_timer_del(tb->click_timer);
   tb->click_timer = NULL;
   elm_genlist_item_expanded_set(it, EINA_TRUE);
}

static void
_on_list_contract_req(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto_Thumb_Browser *tb = data;
   Elm_Object_Item *it = event_info;

   ecore_job_del(tb->change_dir_job);
   tb->change_dir_job = NULL;
   ecore_timer_del(tb->click_timer);
   tb->click_timer = NULL;
   elm_genlist_item_expanded_set(it, EINA_FALSE);
}

static void
_on_list_expanded(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Ephoto_Thumb_Browser *tb = data;
   Elm_Object_Item *it = event_info;
   const char *path = elm_object_item_data_get(it);
   
   tb->dirs_only = 0;
   if (strlen(path) == strlen(tb->ephoto->config->directory))
     {
	if (!strcmp(path, tb->ephoto->config->directory))
	  tb->dirs_only = 1;
        else
          tb->dirs_only = 0;
     }
   tb->thumbs_only = 0;
   ephoto_directory_set(tb->ephoto, path, it, tb->dirs_only, tb->thumbs_only);
   ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);
}

static void
_on_list_contracted(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Ephoto_Thumb_Browser *tb = data;
   Elm_Object_Item *it = event_info;
   const char *path = elm_object_item_data_get(it);

   elm_genlist_item_subitems_clear(it); 
   if (strlen(path) == strlen(tb->ephoto->config->directory))
     {
	if (!strcmp(path, tb->ephoto->config->directory))
          {
             return;
          }
     }
   tb->thumbs_only = 1;
   tb->dirs_only = 0;
   ephoto_directory_set(tb->ephoto, path, NULL,
       tb->dirs_only, tb->thumbs_only);
   ephoto_title_set(tb->ephoto,
       tb->ephoto->config->directory);
}

static void
_dir_job(void *data)
{
   Elm_Object_Item *it = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(it, "thumb_browser");
   const char *path = elm_object_item_data_get(it);

   tb->change_dir_job = NULL;
   tb->thumbs_only = 1;
   tb->dirs_only = 0;
   ephoto_directory_set(tb->ephoto, path, NULL,
       tb->dirs_only, tb->thumbs_only);
   ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);
}

static void
_wait_job(void *data)
{
   Elm_Object_Item *it = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(it, "thumb_browser");

   if (tb->change_dir_job)
     ecore_job_del(tb->change_dir_job);
   tb->change_dir_job = ecore_job_add(_dir_job, it);
}

static void
_on_list_selected(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto_Thumb_Browser *tb = data;
   Elm_Object_Item *it = event_info;

   evas_object_data_set(it, "thumb_browser", tb);
   if (!tb->dragging)
     {
        tb->dir_current = it;

        ecore_job_add(_wait_job, it);        
     }
}

static char *
_ephoto_dir_item_text_get(void *data, Evas_Object *obj EINA_UNUSED,
    const char *part EINA_UNUSED)
{
   return strdup(basename(data));
}

static char *
_ephoto_thumb_item_text_get(void *data, Evas_Object *obj EINA_UNUSED,
    const char *part EINA_UNUSED)
{
   Ephoto_Entry *e = data;

   return strdup(e->label);
}

static Evas_Object *
_ephoto_dir_item_icon_get(void *data EINA_UNUSED, Evas_Object *obj,
    const char *part)
{
   if (!strcmp(part, "elm.swallow.end"))
      return NULL;
   Evas_Object *ic = elm_icon_add(obj);

   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   elm_icon_standard_set(ic, "folder");
   return ic;
}

static Evas_Object *
_ephoto_thumb_file_icon_get(void *data, Evas_Object *obj,
    const char *part)
{
   Ephoto_Entry *e = data;
   Evas_Object *thumb = NULL;

   if (strcmp(part, "elm.swallow.icon"))
     return NULL;

   if (e)
     thumb = ephoto_thumb_add(e->ephoto, obj, e->path);
   return thumb;
}

static void
_ephoto_dir_item_del(void *data, Evas_Object *obj EINA_UNUSED)
{
   eina_stringshare_del(data);
}

static void
_ephoto_thumb_item_del(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED)
{
   /* The entry is already freed when changing directories. */
}

static int
_entry_cmp(const void *pa, const void *pb)
{
   const Elm_Object_Item *ia = pa;
   const Elm_Object_Item *ib = pb;
   const char *a, *b;

   a = elm_object_item_data_get(ia);
   b = elm_object_item_data_get(ib);

   return strcasecmp(a, b);
}

static int
_entry_cmp_grid_alpha_asc(const void *pa, const void *pb)
{
   const Ephoto_Entry *a, *b;

   a = elm_object_item_data_get(pa);
   b = elm_object_item_data_get(pb);

   return strcasecmp(a->basename, b->basename);
}

static int
_entry_cmp_grid_alpha_desc(const void *pa, const void *pb)
{
   const Ephoto_Entry *a, *b;
   int i;

   a = elm_object_item_data_get(pa);
   b = elm_object_item_data_get(pb);
   i = strcasecmp(a->basename, b->basename);
   if (i < 0)
     i = 1;
   else if (i > 0)
     i = -1;

   return i;
}

static int
_entry_cmp_grid_mod_asc(const void *pa, const void *pb)
{
   const Ephoto_Entry *a, *b;
   long long moda, modb;

   a = elm_object_item_data_get(pa);
   b = elm_object_item_data_get(pb);

   moda = ecore_file_mod_time(a->path);
   modb = ecore_file_mod_time(b->path);

   if (moda < modb)
     return -1;
   else if (moda > modb)
     return 1;
   else
     return strcasecmp(a->basename, b->basename);
}

static int
_entry_cmp_grid_mod_desc(const void *pa, const void *pb)
{
   const Ephoto_Entry *a, *b;
   long long moda, modb;

   a = elm_object_item_data_get(pa);
   b = elm_object_item_data_get(pb);

   moda = ecore_file_mod_time(a->path);
   modb = ecore_file_mod_time(b->path);

   if (moda < modb)
     return 1;
   else if (moda > modb)
     return -1;
   else
     {
        int i;

        i = strcasecmp(a->basename, b->basename);
        if (i < 0)
          i = 1;
        else if (i > 0)
          i = -1;
        return i;
     }
}

static void
_sort_alpha_asc(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   tb->sort = EPHOTO_SORT_ALPHABETICAL_ASCENDING;
   tb->thumbs_only = 1;
   tb->dirs_only = 0;
   ephoto_directory_set(tb->ephoto, tb->ephoto->config->directory,
       NULL, tb->dirs_only, tb->thumbs_only);
}

static void
_sort_alpha_desc(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   tb->sort = EPHOTO_SORT_ALPHABETICAL_DESCENDING;
   tb->thumbs_only = 1;
   tb->dirs_only = 0;
   ephoto_directory_set(tb->ephoto, tb->ephoto->config->directory,
       NULL, tb->dirs_only, tb->thumbs_only);
}

static void
_sort_mod_asc(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   tb->sort = EPHOTO_SORT_MODTIME_ASCENDING;
   tb->thumbs_only = 1;
   tb->dirs_only = 0;
   ephoto_directory_set(tb->ephoto, tb->ephoto->config->directory,
       NULL, tb->dirs_only, tb->thumbs_only);
}

static void
_sort_mod_desc(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   tb->sort = EPHOTO_SORT_MODTIME_DESCENDING;
   tb->thumbs_only = 1;
   tb->dirs_only = 0;
   ephoto_directory_set(tb->ephoto, tb->ephoto->config->directory,
       NULL, tb->dirs_only, tb->thumbs_only);
}

static Eina_Bool
_check_for_subdirs(Ephoto_Entry *entry)
{
   Eina_Iterator *ls = eina_file_direct_ls(entry->path);
   Eina_File_Direct_Info *info;

   if (!ls)
     return EINA_FALSE;
   EINA_ITERATOR_FOREACH(ls, info)
     {
        if (info->type == EINA_FILE_DIR)
          {
             eina_iterator_free(ls);
             return EINA_TRUE;
          }
     }
   eina_iterator_free(ls);
   return EINA_FALSE;
}

static Eina_Bool
_todo_items_process(void *data)
{
   Ephoto_Thumb_Browser *tb = data;
   Ephoto_Entry *entry;
   int i = 0;

   if ((!tb->ls) && (tb->animator.processed == tb->animator.count))
     {
        if (tb->animator.count == 0)
          return EINA_TRUE;
	if (tb->dir_loading)
	  {
	     evas_object_del(tb->dir_loading);
             evas_object_freeze_events_set(tb->main, EINA_FALSE);
             elm_object_focus_set(tb->main, EINA_TRUE);
	  }
	tb->animator.todo_items = NULL;
	return EINA_FALSE;
     }
   if ((tb->ls) && (eina_list_count(tb->todo_items) < TODO_ITEM_MIN_BATCH))
      return EINA_TRUE;

   tb->animator.todo_items = NULL;

   EINA_LIST_FREE(tb->todo_items, entry)
   {
      i++;
      if (i > TODO_ITEM_MIN_BATCH)
	 return EINA_TRUE;
      if (entry->is_dir)
	{
	   const Elm_Genlist_Item_Class *ic;
	   const char *path;

	   ic = &_ephoto_dir_class;
	   path = eina_stringshare_add(entry->path);
           if (_check_for_subdirs(entry))
             entry->item =
                 elm_genlist_item_sorted_insert(tb->fsel, ic, path,
                 entry->parent, ELM_GENLIST_ITEM_TREE, _entry_cmp, NULL, NULL);
           else
             entry->item =
                 elm_genlist_item_sorted_insert(tb->fsel, ic, path,
                 entry->parent, ELM_GENLIST_ITEM_NONE, _entry_cmp, NULL, NULL);
	   if (!entry->item)
	     {
		ephoto_entry_free(tb->ephoto, entry);
	     }
        }
      else
	{
	   const Elm_Gengrid_Item_Class *ic;

	   ic = &_ephoto_thumb_file_class;

           if (tb->sort == EPHOTO_SORT_ALPHABETICAL_ASCENDING)
	     entry->item =
	         elm_gengrid_item_sorted_insert(tb->grid, ic, entry,
	         _entry_cmp_grid_alpha_asc, NULL, NULL);
           else if (tb->sort == EPHOTO_SORT_ALPHABETICAL_DESCENDING)
             entry->item =
                 elm_gengrid_item_sorted_insert(tb->grid, ic, entry,
                 _entry_cmp_grid_alpha_desc, NULL, NULL);
           else if (tb->sort == EPHOTO_SORT_MODTIME_ASCENDING)
             entry->item =
                 elm_gengrid_item_sorted_insert(tb->grid, ic, entry,
                 _entry_cmp_grid_mod_asc, NULL, NULL);
           else if (tb->sort == EPHOTO_SORT_MODTIME_DESCENDING)
             entry->item =
                 elm_gengrid_item_sorted_insert(tb->grid, ic, entry,
                 _entry_cmp_grid_mod_desc, NULL, NULL);
	   if (entry->item)
	     {
		elm_object_item_data_set(entry->item, entry);
             }
           else
	     {
		ephoto_entry_free(tb->ephoto, entry);
	     }
	}
      tb->animator.processed++;
   }

   return EINA_TRUE;
}

static void
_ephoto_dir_go_home(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   elm_genlist_clear(tb->fsel);
   tb->thumbs_only = 0;
   tb->dirs_only = 0;

   ephoto_directory_set(tb->ephoto, getenv("HOME"), NULL,
       tb->dirs_only, tb->thumbs_only);
   ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);
}

static void
_ephoto_dir_go_up(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   if (strcmp(tb->ephoto->config->directory, "/"))
     {
	char path[PATH_MAX];

	snprintf(path, PATH_MAX, "%s", tb->ephoto->config->directory);
	elm_genlist_clear(tb->fsel);
	tb->thumbs_only = 0;
        tb->dirs_only = 0;

	ephoto_directory_set(tb->ephoto, dirname(path), NULL,
            tb->dirs_only, tb->thumbs_only);
	ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);
     }
}

static void
_ephoto_dir_go_trash(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   char path[PATH_MAX];

   snprintf(path, PATH_MAX, "%s/.config/ephoto/trash", getenv("HOME"));
   if (!ecore_file_exists(path))
      ecore_file_mkpath(path);
   elm_genlist_clear(tb->fsel);
   tb->thumbs_only = 0;
   tb->dirs_only = 0;

   ephoto_directory_set(tb->ephoto, path, NULL,
       tb->dirs_only, tb->thumbs_only);
   ephoto_title_set(tb->ephoto, _("Trash"));
}

static void
_ephoto_direntry_go(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   const char *dir;
   Ephoto_Thumb_Browser *tb = data;

   dir = elm_object_text_get(tb->direntry);
   if (ecore_file_is_dir(dir))
     {
	elm_genlist_clear(tb->fsel);
        tb->thumbs_only = 0;
        tb->dirs_only = 0;

	ephoto_directory_set(tb->ephoto, dir, NULL,
            tb->dirs_only, tb->thumbs_only);
	ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);
     }
}

static void
_ephoto_thumb_activated(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto_Thumb_Browser *tb = data;
   Eina_List *selected, *s;
   Elm_Object_Item *item;
   Elm_Object_Item *it = event_info;
   Ephoto_Entry *e = elm_object_item_data_get(it);

   elm_gengrid_item_selected_set(it, EINA_TRUE);
   selected =
       eina_list_clone(elm_gengrid_selected_items_get(tb->grid));
   if (eina_list_count(selected) <= 1)
     tb->ephoto->selentries = NULL;
   else
     {
        EINA_LIST_FOREACH(selected, s, item)
          {
             tb->ephoto->selentries = eina_list_append(tb->ephoto->selentries,
                 elm_object_item_data_get(item));
          }
     }
   evas_object_smart_callback_call(tb->main, "view", e);
}

static void
_zoom_set(Ephoto_Thumb_Browser *tb, int zoom)
{
   double scale = elm_config_scale_get();

   if (zoom > ZOOM_MAX)
      zoom = ZOOM_MAX;
   else if (zoom < ZOOM_MIN)
      zoom = ZOOM_MIN;

   ephoto_thumb_size_set(tb->ephoto, zoom);
   elm_gengrid_item_size_set(tb->grid, zoom * scale, zoom * scale);
   if (zoom >= ZOOM_MAX)
      elm_object_disabled_set(tb->max, EINA_TRUE);
   if (zoom > ZOOM_MIN)
      elm_object_disabled_set(tb->min, EINA_FALSE);
   if (zoom <= ZOOM_MIN)
      elm_object_disabled_set(tb->min, EINA_TRUE);
   if (zoom < ZOOM_MAX)
      elm_object_disabled_set(tb->max, EINA_FALSE);
}

static void
_zoom_in(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   int zoom = tb->ephoto->config->thumb_size + ZOOM_STEP;

   _zoom_set(tb, zoom);
}

static void
_zoom_out(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   int zoom = tb->ephoto->config->thumb_size - ZOOM_STEP;

   _zoom_set(tb, zoom);
}

static void
_slideshow(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Eina_List *selected, *s;
   Elm_Object_Item *item;
   Elm_Object_Item *it = elm_gengrid_selected_item_get(tb->grid);
   Ephoto_Entry *entry;

   if (it)
      entry = elm_object_item_data_get(it);
   else
      entry = eina_list_nth(tb->ephoto->entries, 0);

   if (!entry)
      return;
   selected =
       eina_list_clone(elm_gengrid_selected_items_get(tb->grid));
   if (eina_list_count(selected) <= 1)
     tb->ephoto->selentries = NULL;
   else
     {
        EINA_LIST_FOREACH(selected, s, item)
          {
             tb->ephoto->selentries = eina_list_append(tb->ephoto->selentries,
                 elm_object_item_data_get(item));
          }
     }
   evas_object_smart_callback_call(tb->main, "slideshow", entry);
}

static void
_general_settings(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(popup, "thumb_browser");

   ephoto_config_general(tb->ephoto);
}

static void
_slideshow_settings(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(popup, "thumb_browser");

   ephoto_config_slideshow(tb->ephoto);
}

static void
_about_settings(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(popup, "thumb_browser");

   ephoto_config_about(tb->ephoto);
}

static void
_close_settings(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(popup, "thumb_browser");

   evas_object_del(popup);
   evas_object_freeze_events_set(tb->main, EINA_FALSE);
   elm_object_focus_set(tb->main, EINA_TRUE);
}

static void
_settings(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Object *popup, *list, *button, *ic;

   evas_object_freeze_events_set(tb->main, EINA_TRUE);

   popup = elm_popup_add(tb->ephoto->win);
   elm_popup_scrollable_set(popup, EINA_TRUE);
   elm_object_part_text_set(popup, "title,text", _("Settings Panel"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   list = elm_list_add(popup);
   evas_object_size_hint_weight_set(list, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(list, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_list_mode_set(list, ELM_LIST_EXPAND);

   ic = elm_icon_add(list);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "preferences-system");
   evas_object_show(ic);
   elm_list_item_append(list, _("General Settings"), ic, NULL,
       _general_settings, popup);

   ic = elm_icon_add(list);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "media-playback-start");
   evas_object_show(ic);
   elm_list_item_append(list, _("Slideshow Settings"), ic, NULL,
       _slideshow_settings, popup);

   ic = elm_icon_add(list);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "help-about");
   evas_object_show(ic);
   elm_list_item_append(list, _("About Ephoto"), ic, NULL, _about_settings,
       popup);

   ic = elm_icon_add(popup);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "window-close");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Close"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _close_settings, popup);
   elm_object_part_content_set(popup, "button1", button);
   evas_object_show(button);

   elm_list_go(list);
   evas_object_show(list);

   evas_object_data_set(popup, "thumb_browser", tb);
   elm_object_content_set(popup, list);
   evas_object_show(popup);
}

static void
_ephoto_dir_show_folders(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   evas_object_show(tb->leftbox);
   elm_table_pack(tb->table, tb->leftbox, 0, 0, 1, 1);
   
   elm_table_unpack(tb->table, tb->nolabel);
   elm_table_pack(tb->table, tb->nolabel, 1, 0, 4, 1);
    
   elm_table_unpack(tb->table, tb->grid);
   elm_table_pack(tb->table, tb->grid, 1, 0, 4, 1);

   elm_object_item_del(tb->ficon);
   tb->ficon = elm_toolbar_item_prepend(tb->bar, "system-file-manager", _("Folders"),
       _ephoto_dir_hide_folders, tb);

   tb->ephoto->config->fsel_hide = 0;
}

static void
_ephoto_dir_hide_folders(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   evas_object_hide(tb->leftbox);
   elm_table_unpack(tb->table, tb->leftbox);
   
   elm_table_unpack(tb->table, tb->nolabel);
   elm_table_pack(tb->table, tb->nolabel, 0, 0, 5, 1);
   
   elm_table_unpack(tb->table, tb->grid);
   elm_table_pack(tb->table, tb->grid, 0, 0, 5, 1);

   elm_object_item_del(tb->ficon);
   tb->ficon = elm_toolbar_item_prepend(tb->bar, "system-file-manager", _("Folders"),
       _ephoto_dir_show_folders, tb);

   elm_object_focus_set(tb->main, EINA_TRUE);
   tb->ephoto->config->fsel_hide = 1;
}

static void
_complete_ok(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(popup, "thumb_browser");

   evas_object_del(popup);
   evas_object_freeze_events_set(tb->main, EINA_FALSE);
   elm_object_focus_set(tb->main, EINA_TRUE);
}

static void
_complete(Ephoto_Thumb_Browser *tb, const char *title, const char *text)
{
   Evas_Object *popup, *box, *label, *ic, *button;

   evas_object_freeze_events_set(tb->main, EINA_TRUE);

   popup = elm_popup_add(tb->ephoto->win);
   elm_object_part_text_set(popup, "title,text", title);
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);

   label = elm_label_add(box);
   elm_object_text_set(label, text);
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, label);
   evas_object_show(label);

   ic = elm_icon_add(popup);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "window-close");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Ok"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _complete_ok, popup);
   elm_object_part_content_set(popup, "button1", button);
   evas_object_show(button);

   evas_object_data_set(popup, "thumb_browser", tb);
   elm_object_part_content_set(popup, "default", box);
   evas_object_show(popup);
}

static Evas_Object *
_processing(Ephoto_Thumb_Browser *tb, const char *title, const char *text)
{
   Evas_Object *popup, *box, *label, *pb;

   evas_object_freeze_events_set(tb->main, EINA_TRUE);

   popup = elm_popup_add(tb->ephoto->win);
   elm_object_part_text_set(popup, "title,text", title);
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);

   label = elm_label_add(box);
   elm_object_text_set(label, text);
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, label);
   evas_object_show(label);

   pb = elm_progressbar_add(box);
   evas_object_size_hint_weight_set(pb, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(pb, EVAS_HINT_FILL, 0.5);
   elm_object_style_set(pb, "wheel");
   elm_progressbar_pulse_set(pb, EINA_TRUE);
   elm_box_pack_end(box, pb);
   evas_object_show(pb);
   elm_progressbar_pulse(pb, EINA_TRUE);

   evas_object_data_set(popup, "thumb_browser", tb);
   elm_object_part_content_set(popup, "default", box);

   return popup;
}

static Eina_Bool
_move_idler_cb(void *data)
{
   Evas_Object *popup = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(popup, "thumb_browser");
   const char *destination = evas_object_data_get(popup, "destination");
   const char *file;
   int i;

   if (!tb->idler_pos)
      tb->idler_pos = eina_list_nth(tb->idler_pos, 0);
   if (!tb->idler_pos)
     {
	ecore_idler_del(tb->idler);
	tb->idler = NULL;
	eina_list_free(tb->idler_pos);
	tb->idler_pos = NULL;
	if (tb->file_errors > 0)
	  {
	     char msg[PATH_MAX];

	     snprintf(msg, PATH_MAX, "%s %d %s.",
		 _("There was an error moving"), tb->file_errors,
		 ngettext("file", "files", tb->file_errors));
	     _complete(tb, _("Error"), msg);
	  }
	tb->file_errors = 0;
	tb->thumbs_only = 1;
	
        ephoto_directory_set(tb->ephoto, tb->ephoto->config->directory, NULL,
            tb->dirs_only, tb->thumbs_only);
	ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);

	evas_object_del(popup);
        evas_object_freeze_events_set(tb->main, EINA_FALSE);
        elm_object_focus_set(tb->main, EINA_TRUE);

	return EINA_FALSE;
     }

   for (i = 0; i < 5; i++)
     {
	file = eina_list_data_get(tb->idler_pos);
	if (!file)
	   break;
	if (ecore_file_exists(file) && ecore_file_is_dir(destination) &&
	    !strncmp("image/", efreet_mime_type_get(file), 6))
	  {
	     char dest[PATH_MAX], fp[PATH_MAX], extra[PATH_MAX];
	     int ret;

	     snprintf(fp, PATH_MAX, "%s", file);
	     snprintf(dest, PATH_MAX, "%s/%s", destination, basename(fp));
	     if (ecore_file_exists(dest))
	       {
		  snprintf(extra, PATH_MAX, "%s/CopyOf%s", destination,
		      basename(fp));
		  if (ecore_file_exists(extra))
		    {
		       int count;

		       for (count = 2; ecore_file_exists(extra); count++)
			 {
			    memset(extra, 0, sizeof(extra));
			    snprintf(extra, PATH_MAX, "%s/Copy%dOf%s",
				destination, count, basename(fp));
			 }
		    }
		  ret = ecore_file_mv(file, extra);
	       }
             else
	       ret = ecore_file_mv(file, dest);
	     if (!ret)
               tb->file_errors++;
	  }
	tb->idler_pos = eina_list_next(tb->idler_pos);
     }
   return EINA_TRUE;
}

static void
_move_files(Ephoto_Thumb_Browser *tb, Eina_List *files,
    const char *destination)
{
   Evas_Object *popup = _processing(tb, _("Moving Files"),
       _("Please wait while your files are moved."));

   evas_object_data_set(popup, "thumb_browser", tb);
   evas_object_data_set(popup, "destination", destination);
   evas_object_show(popup);

   tb->idler_pos = eina_list_clone(files);
   eina_list_free(files);
   if (!tb->idler)
      tb->idler = ecore_idler_add(_move_idler_cb, popup);
}

static Eina_Bool
_copy_idler_cb(void *data)
{
   Evas_Object *popup = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(popup, "thumb_browser");
   const char *destination = evas_object_data_get(popup, "destination");
   const char *file;
   int i;

   if (!tb->idler_pos)
      tb->idler_pos = eina_list_nth(tb->idler_pos, 0);
   if (!tb->idler_pos)
     {
	ecore_idler_del(tb->idler);
	tb->idler = NULL;
	eina_list_free(tb->idler_pos);
	tb->idler_pos = NULL;
	if (tb->file_errors > 0)
	  {
	     char msg[PATH_MAX];

	     snprintf(msg, PATH_MAX, "%s %d %s.",
		 _("There was an error copying"), tb->file_errors,
		 ngettext("file", "files", tb->file_errors));
	     _complete(tb, _("Error"), msg);
	  }
	tb->file_errors = 0;
	tb->thumbs_only = 1;
        if (strlen(destination) == strlen(tb->ephoto->config->directory))
          {
             if (strcmp(destination, tb->ephoto->config->directory))
               {
                  evas_object_del(popup);
                  evas_object_freeze_events_set(tb->main, EINA_FALSE);
                  elm_object_focus_set(tb->main, EINA_TRUE);

                  return EINA_FALSE;
               }
          }
        ephoto_directory_set(tb->ephoto, tb->ephoto->config->directory, NULL,
            tb->dirs_only, tb->thumbs_only);
	ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);

	evas_object_del(popup);
        evas_object_freeze_events_set(tb->main, EINA_FALSE);
        elm_object_focus_set(tb->main, EINA_TRUE);

	return EINA_FALSE;
     }
   for (i = 0; i < 5; i++)
     {
	file = eina_list_data_get(tb->idler_pos);
	if (!file)
	   break;
	if (ecore_file_exists(file) && ecore_file_is_dir(destination) &&
	    !strncmp("image/", efreet_mime_type_get(file), 6))
	  {
	     char dest[PATH_MAX], fp[PATH_MAX], extra[PATH_MAX];
	     int ret;

	     snprintf(fp, PATH_MAX, "%s", file);
	     snprintf(dest, PATH_MAX, "%s/%s", destination, basename(fp));
	     if (ecore_file_exists(dest))
	       {
		  snprintf(extra, PATH_MAX, "%s/CopyOf%s", destination,
		      basename(fp));
		  if (ecore_file_exists(extra))
		    {
		       int count;

		       for (count = 2; ecore_file_exists(extra); count++)
			 {
			    memset(extra, 0, PATH_MAX);
			    snprintf(extra, PATH_MAX, "%s/Copy%dOf%s",
				destination, count, basename(fp));
			 }
		    }
		  ret = ecore_file_cp(file, extra);
	       }
             else
               ret = ecore_file_cp(file, dest);
	     if (!ret)
		tb->file_errors++;
	  }
	tb->idler_pos = eina_list_next(tb->idler_pos);
     }
   return EINA_TRUE;
}

static void
_copy_files(Ephoto_Thumb_Browser *tb, Eina_List *files,
    const char *destination)
{
   Evas_Object *popup = _processing(tb, _("Copying Files"),
       _("Please wait while your files are copied."));

   evas_object_data_set(popup, "thumb_browser", tb);
   evas_object_data_set(popup, "destination", destination);
   evas_object_show(popup);

   tb->idler_pos = eina_list_clone(files);
   eina_list_free(files);
   if (!tb->idler)
      tb->idler = ecore_idler_add(_copy_idler_cb, popup);
}

static void
_new_dir_confirm(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Evas_Object *popup = data;
   Evas_Object *entry = evas_object_data_get(popup, "entry");
   Ephoto_Thumb_Browser *tb = evas_object_data_get(popup, "thumb_browser");
   Elm_Object_Item *item = elm_genlist_selected_item_get(tb->fsel);
   const char *file = evas_object_data_get(popup, "file");
   const char *text = elm_object_text_get(entry);
   char new_file_name[PATH_MAX];
   int ret;

   snprintf(new_file_name, PATH_MAX, "%s/%s", file, text);
   ret = ecore_file_mkdir(new_file_name);
   if (!ret)
     {
        _complete(tb, _("Error"), _("There was an error creating this directory."));
     }
   else
     {
        tb->dirs_only = 1;
        tb->thumbs_only = 0;
        if (item)
          {
             elm_genlist_item_subitems_clear(item);
             ephoto_directory_set(tb->ephoto, file, item,
                 tb->dirs_only, tb->thumbs_only);
          }
        else
          {
             elm_genlist_clear(tb->fsel);
             ephoto_directory_set(tb->ephoto, file, NULL,
                 tb->dirs_only, tb->thumbs_only);
          }
        ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);
     }
   evas_object_del(popup);
   evas_object_freeze_events_set(tb->main, EINA_FALSE);
   elm_object_focus_set(tb->main, EINA_TRUE);
}

static void
_new_dir_cancel(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(popup, "thumb_browser");

   evas_object_del(popup);
   evas_object_freeze_events_set(tb->main, EINA_FALSE);
   elm_object_focus_set(tb->main, EINA_TRUE);
}

static void
_new_dir(Ephoto_Thumb_Browser *tb, const char *file)
{
   Evas_Object *popup, *box, *entry, *button, *ic;

   evas_object_freeze_events_set(tb->main, EINA_TRUE);

   popup = elm_popup_add(tb->ephoto->win);
   elm_object_part_text_set(popup, "title,text", _("New Directory"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);
   evas_object_data_set(popup, "thumb_browser", tb);
   evas_object_data_set(popup, "file", file);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);

   entry = elm_entry_add(box);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_TRUE);
   elm_object_text_set(entry, _("New Directory"));
   elm_entry_select_all(entry);
   elm_scroller_policy_set(entry, ELM_SCROLLER_POLICY_OFF,
       ELM_SCROLLER_POLICY_OFF);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, entry);
   evas_object_show(entry);
   evas_object_data_set(popup, "entry", entry);

   ic = elm_icon_add(popup);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "document-save");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Save"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _new_dir_confirm, popup);
   elm_object_part_content_set(popup, "button1", button);
   evas_object_show(button);

   ic = elm_icon_add(popup);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "window-close");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Cancel"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _new_dir_cancel, popup);
   elm_object_part_content_set(popup, "button2", button);
   evas_object_show(button);

   elm_object_part_content_set(popup, "default", box);
   evas_object_show(popup);
}

static void
_rename_confirm(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Evas_Object *popup = data;
   Evas_Object *entry = evas_object_data_get(popup, "entry");
   Ephoto_Thumb_Browser *tb = evas_object_data_get(popup, "thumb_browser");
   const char *file = evas_object_data_get(popup, "file");
   const char *text = elm_object_text_get(entry);
   char *escaped = ecore_file_escape_name(text);
   char new_file_name[PATH_MAX], dir[PATH_MAX];
   int ret;

   if (!escaped)
     {
	evas_object_del(popup);
        evas_object_freeze_events_set(tb->main, EINA_FALSE);
        elm_object_focus_set(tb->main, EINA_TRUE);
	return;
     }
   snprintf(dir, PATH_MAX, "%s", file);
   if (ecore_file_is_dir(file))
     snprintf(new_file_name, PATH_MAX, "%s/%s", dirname(dir), text);
   else
     snprintf(new_file_name, PATH_MAX, "%s/%s.%s", dirname(dir), escaped,
         strrchr(dir, '.'));
   ret = ecore_file_mv(file, new_file_name);
   if (!ret)
     {
        if (ecore_file_is_dir(new_file_name))
          _complete(tb, _("Error"), _("There was an error renaming this directory."));
        else
          _complete(tb, _("Error"), _("There was an error renaming this file."));
     }
   else
     {
        if (ecore_file_is_dir(new_file_name))
          {
             Elm_Object_Item *item = elm_genlist_selected_item_get(tb->fsel);
             Elm_Object_Item *parent = elm_genlist_item_parent_get(item);
             tb->dirs_only = 1;
             tb->thumbs_only = 0;
             if (parent)
               {
                  elm_genlist_item_subitems_clear(parent);
                  ephoto_directory_set(tb->ephoto, tb->ephoto->config->directory,
                      parent, tb->dirs_only, tb->thumbs_only);
               }
             else
               {
                  elm_genlist_clear(tb->fsel);
                  ephoto_directory_set(tb->ephoto, tb->ephoto->config->directory,
                      NULL, tb->dirs_only, tb->thumbs_only);
               }
          }
        else
          {
	     tb->thumbs_only = 1;
             tb->dirs_only = 0;
	     ephoto_directory_set(tb->ephoto, tb->ephoto->config->directory, NULL,
                 tb->dirs_only, tb->thumbs_only);
          }
	ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);
     }
   evas_object_del(popup);
   evas_object_freeze_events_set(tb->main, EINA_FALSE);
   elm_object_focus_set(tb->main, EINA_TRUE);
   free(escaped);
}

static void
_rename_cancel(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(popup, "thumb_browser");

   evas_object_del(popup);
   evas_object_freeze_events_set(tb->main, EINA_FALSE);
   elm_object_focus_set(tb->main, EINA_TRUE);
}

static void
_rename_file(Ephoto_Thumb_Browser *tb, const char *file)
{
   Evas_Object *popup, *box, *entry, *button, *ic;
   char buf[PATH_MAX], *bn, *string;

   evas_object_freeze_events_set(tb->main, EINA_TRUE);

   popup = elm_popup_add(tb->ephoto->win);
   if (ecore_file_is_dir(file))
     elm_object_part_text_set(popup, "title, text", _("Rename Directory"));
   else
     elm_object_part_text_set(popup, "title,text", _("Rename File"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);
   evas_object_data_set(popup, "thumb_browser", tb);
   evas_object_data_set(popup, "file", file);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);

   snprintf(buf, PATH_MAX, "%s", file);
   bn = basename(buf);
   string = ecore_file_strip_ext(bn);

   entry = elm_entry_add(box);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_TRUE);
   elm_object_text_set(entry, string);
   elm_entry_select_all(entry);
   elm_scroller_policy_set(entry, ELM_SCROLLER_POLICY_OFF,
       ELM_SCROLLER_POLICY_OFF);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, entry);
   evas_object_show(entry);
   evas_object_data_set(popup, "entry", entry);

   free(string);

   ic = elm_icon_add(popup);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "document-save");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Rename"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _rename_confirm, popup);
   elm_object_part_content_set(popup, "button1", button);
   evas_object_show(button);

   ic = elm_icon_add(popup);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "window-close");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Cancel"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _rename_cancel, popup);
   elm_object_part_content_set(popup, "button2", button);
   evas_object_show(button);

   elm_object_part_content_set(popup, "default", box);
   evas_object_show(popup);
}

static Eina_Bool
_delete_idler_cb(void *data)
{
   Evas_Object *popup = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(popup, "thumb_browser");
   const char *file;
   char destination[PATH_MAX];
   int i;

   snprintf(destination, PATH_MAX, "%s/.config/ephoto/trash", getenv("HOME"));

   if (!ecore_file_exists(destination))
      ecore_file_mkpath(destination);

   if (!tb->idler_pos)
      tb->idler_pos = eina_list_nth(tb->idler_pos, 0);
   if (!tb->idler_pos)
     {
	ecore_idler_del(tb->idler);
	tb->idler = NULL;
	eina_list_free(tb->idler_pos);
	tb->idler_pos = NULL;
	if (tb->file_errors > 0)
	  {
	     char msg[PATH_MAX];

	     snprintf(msg, PATH_MAX, "%s %d %s.",
		 _("There was an error deleting"), tb->file_errors,
		 ngettext("file", "files", tb->file_errors));
	     _complete(tb, _("Error"), msg);
	  }
	tb->file_errors = 0;
	tb->thumbs_only = 1;
	ephoto_directory_set(tb->ephoto, tb->ephoto->config->directory, NULL,
            tb->dirs_only, tb->thumbs_only);
	ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);

	evas_object_del(popup);
        evas_object_freeze_events_set(tb->main, EINA_FALSE);
        elm_object_focus_set(tb->main, EINA_TRUE);

	return EINA_FALSE;
     }
   for (i = 0; i < 5; i++)
     {
	file = eina_list_data_get(tb->idler_pos);
	if (!file)
	   break;
	if (ecore_file_exists(file) && ecore_file_is_dir(destination))
	  {
	     char dest[PATH_MAX], fp[PATH_MAX], extra[PATH_MAX];
	     int ret;

	     snprintf(fp, PATH_MAX, "%s", file);
	     snprintf(dest, PATH_MAX, "%s/%s", destination, basename(fp));
	     if (ecore_file_exists(dest))
	       {
		  snprintf(extra, PATH_MAX, "%s/CopyOf%s", destination,
		      basename(fp));
		  if (ecore_file_exists(extra))
		    {
		       int count;

		       for (count = 2; ecore_file_exists(extra); count++)
			 {
			    memset(extra, 0, sizeof(extra));
			    snprintf(extra, PATH_MAX, "%s/Copy%dOf%s",
				destination, count, basename(fp));
			 }
		    }
		  ret = ecore_file_mv(file, extra);
	       }
             else
	       ret = ecore_file_mv(file, dest);
	     if (!ret)
		tb->file_errors++;
	  }
	tb->idler_pos = eina_list_next(tb->idler_pos);
     }
   return EINA_TRUE;
}

static void
_delete_files(Ephoto_Thumb_Browser *tb, Eina_List *files)
{
   Evas_Object *popup = _processing(tb, _("Deleting Files"),
       _("Please wait while your files are deleted."));

   evas_object_data_set(popup, "thumb_browser", tb);
   evas_object_data_set(popup, "files", files);
   evas_object_show(popup);

   tb->idler_pos = eina_list_clone(files);
   eina_list_free(files);
   if (!tb->idler)
      tb->idler = ecore_idler_add(_delete_idler_cb, popup);
}

static Eina_Bool
_delete_dir_idler_cb(void *data)
{
   Evas_Object *popup = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(popup, "thumb_browser");
   Elm_Object_Item *item = elm_genlist_selected_item_get(tb->fsel);
   Elm_Object_Item *parent = elm_genlist_item_parent_get(item);
   const char *dir = evas_object_data_get(popup, "path");
   char destination[PATH_MAX];

   snprintf(destination, PATH_MAX, "%s/.config/ephoto/trash", getenv("HOME"));

   if (!ecore_file_exists(destination))
      ecore_file_mkpath(destination);

   if (dir)
     {
        char fp[PATH_MAX], dest[PATH_MAX], extra[PATH_MAX];
        int ret;

        snprintf(fp, PATH_MAX, "%s", dir);
        snprintf(dest, PATH_MAX, "%s/%s", destination, basename(fp));
        if (ecore_file_exists(dir) && ecore_file_is_dir(destination))
          {
             if (ecore_file_exists(dest))
               {
                  snprintf(extra, PATH_MAX, "%s/CopyOf%s", destination,
                      basename(fp));
                  if (ecore_file_exists(extra))
                    {
                       int count;
                       for (count = 2; ecore_file_exists(extra); count++)
                         {
                            memset(extra, 0, sizeof(extra));
                            snprintf(extra, PATH_MAX, "%s/Copy%dOf%s",
                                destination, count, basename(fp));
                         }
                     }
                  ret = ecore_file_mv(dir, extra);
               }
             else
               ret = ecore_file_mv(dir, dest);
             if (!ret)
                tb->file_errors++;
          }
     }
   if (!dir || tb->file_errors > 0)
     {
        char msg[PATH_MAX];

        snprintf(msg, PATH_MAX, "%s.",
            _("There was an error deleting this directory"));
        _complete(tb, _("Error"), msg);
     }
   ecore_idler_del(tb->idler);
   tb->idler = NULL;
   eina_list_free(tb->idler_pos);
        
   tb->file_errors = 0;
   tb->dirs_only = 0;
   tb->thumbs_only = 0;
   if (parent)
     {
        elm_genlist_item_subitems_clear(parent);
        ephoto_directory_set(tb->ephoto, elm_object_item_data_get(parent),
            parent, tb->dirs_only, tb->thumbs_only);
     }
   else
     {
        char fp[PATH_MAX];

        snprintf(fp, PATH_MAX, "%s", dir);
        elm_genlist_clear(tb->fsel);
        ephoto_directory_set(tb->ephoto, dirname(fp), NULL,
            tb->dirs_only, tb->thumbs_only);
     }
   ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);

   evas_object_del(popup);
   evas_object_freeze_events_set(tb->main, EINA_FALSE);
   elm_object_focus_set(tb->main, EINA_TRUE);

   return EINA_FALSE;
}

static void
_delete_dir(Ephoto_Thumb_Browser *tb, const char *path)
{
   Evas_Object *popup = _processing(tb, _("Deleting Directory"),
       _("Please wait while your directory is deleted."));

   evas_object_data_set(popup, "thumb_browser", tb);
   evas_object_data_set(popup, "path", path);
   evas_object_show(popup);

   tb->idler_pos = NULL;
   if (!tb->idler)
      tb->idler = ecore_idler_add(_delete_dir_idler_cb, popup);
}

static Eina_Bool
_empty_trash_idler_cb(void *data)
{
   Evas_Object *popup = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(popup, "thumb_browser");
   const char *file;
   char trash[PATH_MAX];
   int i = 0;

   snprintf(trash, PATH_MAX, "%s/.config/ephoto/trash", getenv("HOME"));

   if (!tb->idler_pos)
      tb->idler_pos = eina_list_nth(tb->idler_pos, 0);
   if (!tb->idler_pos)
     {
	ecore_idler_del(tb->idler);
	tb->idler = NULL;
	eina_list_free(tb->idler_pos);
	tb->idler_pos = NULL;
	if (tb->file_errors > 0)
	  {
	     char msg[PATH_MAX];

	     snprintf(msg, PATH_MAX, "%s %d %s.",
		 _("There was an error deleting"), tb->file_errors,
		 ngettext("file", "files", tb->file_errors));
	     _complete(tb, _("Error"), msg);
	  }
        tb->file_errors = 0;
        tb->dirs_only = 0;
        tb->thumbs_only = 0;
        elm_genlist_clear(tb->fsel);
	ephoto_directory_set(tb->ephoto, trash, NULL,
            tb->dirs_only, tb->thumbs_only);
	ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);

	evas_object_del(popup);
        evas_object_freeze_events_set(tb->main, EINA_FALSE);
        elm_object_focus_set(tb->main, EINA_TRUE);

	return EINA_FALSE;
     }
   for (i = 0; i < 5; i++)
     {
	file = eina_list_data_get(tb->idler_pos);
	if (!file)
	   break;
	if (ecore_file_exists(file))
	  {
	     int ret;

             if (ecore_file_is_dir(file))
               ret = ecore_file_recursive_rm(file);
             else
	       ret = ecore_file_unlink(file);
	     if (!ret)
		tb->file_errors++;
	  }
	tb->idler_pos = eina_list_next(tb->idler_pos);
     }
   return EINA_TRUE;
}

static void
_empty_trash(Ephoto_Thumb_Browser *tb, Eina_List *files)
{
   Evas_Object *popup = _processing(tb, _("Emptying Trash"),
       _("Please wait while your files are deleted."));

   evas_object_data_set(popup, "thumb_browser", tb);
   evas_object_show(popup);

   tb->idler_pos = eina_list_clone(files);
   eina_list_free(files);
   if (!tb->idler)
      tb->idler = ecore_idler_add(_empty_trash_idler_cb, popup);
}

static void
_prompt_empty_apply(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(popup, "thumb_browser");
   Eina_List *files = evas_object_data_get(popup, "files");

   evas_object_del(popup);
   evas_object_freeze_events_set(tb->main, EINA_FALSE);
   elm_object_focus_set(tb->main, EINA_TRUE);
   _empty_trash(tb, files);
}

static void
_prompt_delete_apply(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(popup, "thumb_browser");
   Eina_List *files = evas_object_data_get(popup, "files");

   evas_object_del(popup);
   evas_object_freeze_events_set(tb->main, EINA_FALSE);
   elm_object_focus_set(tb->main, EINA_TRUE);
   _delete_files(tb, files);
}

static void
_prompt_delete_dir_apply(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(popup, "thumb_browser");
   const char *path = evas_object_data_get(popup, "path");

   evas_object_del(popup);
   evas_object_freeze_events_set(tb->main, EINA_FALSE);
   elm_object_focus_set(tb->main, EINA_TRUE);
   _delete_dir(tb, path);
}

static void
_prompt_move_apply(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(popup, "thumb_browser");
   Eina_List *files = evas_object_data_get(popup, "files");
   const char *path = evas_object_data_get(popup, "path");

   evas_object_del(popup);
   evas_object_freeze_events_set(tb->main, EINA_FALSE);
   elm_object_focus_set(tb->main, EINA_TRUE);
   _move_files(tb, files, path);
}

static void
_prompt_copy_apply(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(popup, "thumb_browser");
   Eina_List *files = evas_object_data_get(popup, "files");
   const char *path = evas_object_data_get(popup, "path");

   evas_object_del(popup);
   evas_object_freeze_events_set(tb->main, EINA_FALSE);
   elm_object_focus_set(tb->main, EINA_TRUE);
   _copy_files(tb, files, path);
}

static void
_prompt_cancel(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(popup, "thumb_browser");

   evas_object_del(popup);
   evas_object_freeze_events_set(tb->main, EINA_FALSE);
   elm_object_focus_set(tb->main, EINA_TRUE);
}

static Evas_Object *
_prompt(Ephoto_Thumb_Browser *tb, const char *title, const char *text)
{
   Evas_Object *popup, *box, *label;

   evas_object_freeze_events_set(tb->main, EINA_TRUE);

   popup = elm_popup_add(tb->ephoto->win);
   elm_object_part_text_set(popup, "title,text", title);
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);

   label = elm_label_add(box);
   elm_object_text_set(label, text);
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, label);
   evas_object_show(label);

   evas_object_data_set(popup, "thumb_browser", tb);
   elm_object_part_content_set(popup, "default", box);

   return popup;
}

static char *
_drag_data_extract(char **drag_data)
{
   char *uri = NULL;

   if (!drag_data)
      return uri;

   char *p = *drag_data;

   if (!p)
      return uri;
   char *s = strstr(p, FILESEP);

   if (s)
      p += FILESEP_LEN;
   s = strchr(p, '\n');
   uri = p;
   if (s)
     {
	if (s - p > 0)
	  {
	     char *s1 = s - 1;

	     if (s1[0] == '\r')
		s1[0] = '\0';
	     else
	       {
		  char *s2 = s + 1;

		  if (s2[0] == '\r')
		    {
		       s[0] = '\0';
		       s++;
		    }
                  else
		     s[0] = '\0';
	       }
	  }
        else
	   s[0] = '\0';
	s++;
     }
   else
     p = NULL;
   *drag_data = s;

   return uri;
}

static Eina_Bool
_drop_dropcb(void *data EINA_UNUSED, Evas_Object *obj, Elm_Object_Item *it,
    Elm_Selection_Data *ev, int xposret EINA_UNUSED, int yposret EINA_UNUSED)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(it, EINA_TRUE);

   const char *path = elm_object_item_data_get(it);
   Eina_List *files = NULL;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(obj, "thumb_browser");

   if (!ev->data)
      return EINA_FALSE;
   if (ev->len <= 0)
      return EINA_FALSE;
   if (!path)
      return EINA_FALSE;

   char *dd = strdup(ev->data);

   if (!dd)
      return EINA_FALSE;

   char *s = _drag_data_extract(&dd);

   while (s)
     {
	files = eina_list_append(files, s);
	s = _drag_data_extract(&dd);
     }
   free(dd);

   if (tb->ephoto->config->prompts)
     {
	Evas_Object *ic, *button;
	Evas_Object *popup;
	char drop_dir[PATH_MAX];

	snprintf(drop_dir, PATH_MAX, "%s:<br> %s?",
	    _("Are you sure you want to drop these files in"), path);

	popup = _prompt(tb, _("Drop Files"), drop_dir);

	ic = elm_icon_add(popup);
	elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
	evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1,
	    1);
	elm_icon_standard_set(ic, "document-save");

	button = elm_button_add(popup);
	elm_object_text_set(button, _("Yes"));
	elm_object_part_content_set(button, "icon", ic);
	if (tb->ephoto->config->move_drop)
	   evas_object_smart_callback_add(button, "clicked", _prompt_move_apply,
	       popup);
	else
	   evas_object_smart_callback_add(button, "clicked", _prompt_copy_apply,
	       popup);
	elm_object_part_content_set(popup, "button1", button);
	evas_object_show(button);

	ic = elm_icon_add(popup);
	elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
	evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1,
	    1);
	elm_icon_standard_set(ic, "window-close");

	button = elm_button_add(popup);
	elm_object_text_set(button, _("No"));
	elm_object_part_content_set(button, "icon", ic);
	evas_object_smart_callback_add(button, "clicked", _prompt_cancel,
	    popup);
	elm_object_part_content_set(popup, "button2", button);
	evas_object_show(button);

	evas_object_data_set(popup, "files", files);
	evas_object_data_set(popup, "path", path);

	evas_object_show(popup);
     }
   else
     {
	if (tb->ephoto->config->move_drop)
	   _move_files(tb, files, path);
	else
	   _copy_files(tb, files, path);
     }
   if (tb->dir_current)
     elm_genlist_item_selected_set(tb->dir_current, EINA_TRUE);
   return EINA_TRUE;
}

static Elm_Object_Item *
_drop_item_getcb(Evas_Object *obj, Evas_Coord x, Evas_Coord y,
    int *xposret EINA_UNUSED, int *yposret)
{
   Elm_Object_Item *gli;

   gli = elm_genlist_at_xy_item_get(obj, x, y, yposret);

   return gli;
}

static void
_drop_enter(void *data, Evas_Object *obj EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   if (tb->dragging)
     elm_object_cursor_set(tb->main, ELM_CURSOR_TARGET);
}

static void
_drop_leave(void *data, Evas_Object *obj EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   if (tb->dragging)
     {
        elm_object_cursor_set(tb->main, ELM_CURSOR_HAND2);
        if (tb->dir_current)
          elm_genlist_item_selected_set(tb->dir_current, EINA_TRUE);
     }
}

static void
_drop_pos(void *data EINA_UNUSED, Evas_Object *cont EINA_UNUSED,
    Elm_Object_Item *it EINA_UNUSED, Evas_Coord x EINA_UNUSED,
    Evas_Coord y EINA_UNUSED, int xposret EINA_UNUSED,
    int yposret EINA_UNUSED, Elm_Xdnd_Action action EINA_UNUSED)
{
   elm_genlist_item_selected_set(it, EINA_TRUE);
}

static Eina_Bool
_5s_timeout_gone(void *data)
{
   elm_drag_cancel(data);
   _5s_timeout = NULL;

   return ECORE_CALLBACK_CANCEL;
}

static void
_dnd_drag_start(void *data EINA_UNUSED, Evas_Object *obj)
{
   Ephoto_Thumb_Browser *tb = evas_object_data_get(obj, "thumb_browser");

   if (_5s_cancel)
      _5s_timeout = ecore_timer_add(5.0, _5s_timeout_gone, obj);
   elm_object_cursor_set(tb->main, ELM_CURSOR_HAND2);
   tb->dragging = 1;
}

static void
_dnd_drag_done(void *data EINA_UNUSED, Evas_Object *obj,
    Eina_Bool doaccept EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = evas_object_data_get(obj, "thumb_browser");

   if (_5s_cancel)
     {
	ecore_timer_del(_5s_timeout);
	_5s_timeout = NULL;
     }
   eina_list_free(data);
   elm_object_cursor_unset(tb->main);
   tb->dragging = 0;
   return;
}

static const char *
_dnd_drag_data_build(Eina_List **items)
{
   const char *drag_data = NULL;

   if (*items)
     {
	Eina_List *l;
	Elm_Object_Item *it;
	Ephoto_Entry *e;
	unsigned int len = 0;

	EINA_LIST_FOREACH(*items, l, it)
	  {
	     e = elm_object_item_data_get(it);
	     if (e->path)
	        len += strlen(e->path);
	  }

	drag_data =
	    malloc(len + eina_list_count(*items) * (FILESEP_LEN + 1) + 1);
	strcpy((char *) drag_data, "");

	EINA_LIST_FOREACH(*items, l, it)
	  {
	     e = elm_object_item_data_get(it);
	     if (e->path)
	       {
		  strcat((char *) drag_data, FILESEP);
		  strcat((char *) drag_data, e->path);
	       }
	  }
     }
   return drag_data;
}

static Evas_Object *
_dnd_create_icon(void *data, Evas_Object *win, Evas_Coord *xoff,
    Evas_Coord *yoff)
{
   Evas_Object *icon = NULL;
   Evas_Object *o = elm_object_item_part_content_get(data, "elm.swallow.icon");

   if (o)
     {
	int xm, ym, w = 30, h = 30;
	const char *f;
	const char *g;

	elm_image_file_get(o, &f, &g);
	evas_pointer_canvas_xy_get(evas_object_evas_get(o), &xm, &ym);
	if (xoff)
	   *xoff = xm - (w / 2);
	if (yoff)
	   *yoff = ym - (h / 2);
	icon = elm_icon_add(win);
	elm_image_file_set(icon, f, g);
	evas_object_size_hint_align_set(icon, EVAS_HINT_FILL, EVAS_HINT_FILL);
	evas_object_size_hint_weight_set(icon, EVAS_HINT_EXPAND,
	    EVAS_HINT_EXPAND);
	if (xoff && yoff)
	   evas_object_move(icon, *xoff, *yoff);
	evas_object_resize(icon, w, h);
     }

   return icon;
}

static const char *
_dnd_get_drag_data(Evas_Object *obj, Elm_Object_Item *it, Eina_List **items)
{
   const char *drag_data = NULL;

   *items = eina_list_clone(elm_gengrid_selected_items_get(obj));
   if (it)
     {
	if (!elm_gengrid_item_selected_get(it))
	   *items = eina_list_append(*items, it);
     }
   drag_data = _dnd_drag_data_build(items);

   return drag_data;
}

static Elm_Object_Item *
_dnd_item_get(Evas_Object *obj, Evas_Coord x, Evas_Coord y, int *xposret,
    int *yposret)
{
   Elm_Object_Item *item;

   item = elm_gengrid_at_xy_item_get(obj, x, y, xposret, yposret);
   return item;
}

static Eina_Bool
_dnd_item_data_get(Evas_Object *obj, Elm_Object_Item *it,
    Elm_Drag_User_Info *info)
{
   info->format = ELM_SEL_FORMAT_TARGETS;
   info->createicon = _dnd_create_icon;
   info->createdata = it;
   info->dragstart = _dnd_drag_start;
   info->icons = NULL;
   info->dragdone = _dnd_drag_done;
   info->data = _dnd_get_drag_data(obj, it, (Eina_List **) & info->donecbdata);
   info->acceptdata = info->donecbdata;
   if (info->data)
      return EINA_TRUE;
   else
      return EINA_FALSE;
}

static void
_fsel_menu_new_dir_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Elm_Object_Item *item = elm_genlist_selected_item_get(tb->fsel);
   const char *path;

   if (item)
     path = elm_object_item_data_get(item);
   else
     path = tb->ephoto->config->directory;
   if (!path)
     return;
   _new_dir(tb, path);
}

static void
_grid_menu_select_all_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Elm_Object_Item *item;

   item = elm_gengrid_first_item_get(tb->grid);
   while (item)
     {
	elm_gengrid_item_selected_set(item, EINA_TRUE);
	item = elm_gengrid_item_next_get(item);
     }
}

static void
_grid_menu_clear_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Elm_Object_Item *item;

   item = elm_gengrid_first_item_get(tb->grid);
   while (item)
     {
	elm_gengrid_item_selected_set(item, EINA_FALSE);
	item = elm_gengrid_item_next_get(item);
     }
}

static void
_grid_menu_cut_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Eina_List *selection =
       eina_list_clone(elm_gengrid_selected_items_get(tb->grid));
   Eina_List *f;
   Elm_Object_Item *item;
   Ephoto_Entry *file;

   if (eina_list_count(selection) <= 0)
     return;

   if (tb->cut_items)
     {
	eina_list_free(tb->cut_items);
	tb->cut_items = NULL;
     }
   if (tb->copy_items)
     {
	eina_list_free(tb->copy_items);
	tb->copy_items = NULL;
     }
   EINA_LIST_FOREACH(selection, f, item)
     {
        file = elm_object_item_data_get(item);
        tb->cut_items = eina_list_append(tb->cut_items, strdup(file->path));
     }
   eina_list_free(selection);
}

static void
_grid_menu_copy_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Eina_List *selection =
       eina_list_clone(elm_gengrid_selected_items_get(tb->grid));
   Eina_List *f;
   Elm_Object_Item *item;
   Ephoto_Entry *file;

   if (eina_list_count(selection) <= 0)
     return;

   if (tb->cut_items)
     {
	eina_list_free(tb->cut_items);
	tb->cut_items = NULL;
     }
   if (tb->copy_items)
     {
	eina_list_free(tb->copy_items);
	tb->copy_items = NULL;
     }
   EINA_LIST_FOREACH(selection, f, item)
     {
        file = elm_object_item_data_get(item);
        tb->copy_items = eina_list_append(tb->copy_items, strdup(file->path));
     }
   eina_list_free(selection);
}

static void
_fsel_menu_paste_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Elm_Object_Item *item = elm_genlist_selected_item_get(tb->fsel);
   const char *path;

   if (item)
     path = elm_object_item_data_get(item);
   else
     path = tb->ephoto->config->directory;

   if (eina_list_count(tb->cut_items) > 0)
     {
        if (tb->ephoto->config->prompts)
          {
             Evas_Object *ic, *button;
             Evas_Object *popup;

             popup =
                 _prompt(tb, _("Move Files"),
                 _("Are you sure you want to move these files here?"));

             ic = elm_icon_add(popup);
             elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
             evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL,
                 1, 1);
             elm_icon_standard_set(ic, "document-save");

             button = elm_button_add(popup);
             elm_object_text_set(button, _("Yes"));
             elm_object_part_content_set(button, "icon", ic);
             evas_object_smart_callback_add(button, "clicked",
                 _prompt_move_apply, popup);
             elm_object_part_content_set(popup, "button1", button);
             evas_object_show(button);

             ic = elm_icon_add(popup);
             elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
             evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL,
                 1, 1);
             elm_icon_standard_set(ic, "window-close");

             button = elm_button_add(popup);
             elm_object_text_set(button, _("No"));
             elm_object_part_content_set(button, "icon", ic);
             evas_object_smart_callback_add(button, "clicked", _prompt_cancel,
                 popup);
             elm_object_part_content_set(popup, "button2", button);
             evas_object_show(button);

             evas_object_data_set(popup, "files",
                 eina_list_clone(tb->cut_items));
             evas_object_data_set(popup, "path", path);

             evas_object_show(popup);
          }
        else
          {
             _move_files(tb, eina_list_clone(tb->cut_items),
                 path);
          }
        eina_list_free(tb->cut_items);
        tb->cut_items = NULL;
     }
   else if (eina_list_count(tb->copy_items) > 0)
     {
        if (tb->ephoto->config->prompts)
          {
             Evas_Object *ic, *button;
             Evas_Object *popup;

             popup =
                 _prompt(tb, _("Paste Files"),
                 _("Are you sure you want to paste these files here?"));

             ic = elm_icon_add(popup);
             elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
             evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL,
                 1, 1);
             elm_icon_standard_set(ic, "document-save");
             
             button = elm_button_add(popup);
             elm_object_text_set(button, _("Yes"));
             elm_object_part_content_set(button, "icon", ic);
             evas_object_smart_callback_add(button, "clicked",
                 _prompt_copy_apply, popup);
             elm_object_part_content_set(popup, "button1", button);
             evas_object_show(button);

             ic = elm_icon_add(popup);
             elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
             evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL,
                 1, 1);
             elm_icon_standard_set(ic, "window-close");

             button = elm_button_add(popup);
             elm_object_text_set(button, _("No"));
             elm_object_part_content_set(button, "icon", ic);
             evas_object_smart_callback_add(button, "clicked", _prompt_cancel,
                 popup);
             elm_object_part_content_set(popup, "button2", button);
             evas_object_show(button);

             evas_object_data_set(popup, "files",
                 eina_list_clone(tb->copy_items));
             evas_object_data_set(popup, "path", path);

             evas_object_show(popup);
          }
        else
          {
             _copy_files(tb, eina_list_clone(tb->copy_items),
                 path);
          }
        eina_list_free(tb->copy_items);
        tb->copy_items = NULL;
     }
}

static void
_grid_menu_paste_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   if (eina_list_count(tb->cut_items) > 0)
     {
	if (tb->ephoto->config->prompts)
	  {
	     Evas_Object *ic, *button;
	     Evas_Object *popup;

	     popup =
		 _prompt(tb, _("Move Files"),
		 _("Are you sure you want to move these files here?"));

	     ic = elm_icon_add(popup);
	     elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
	     evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL,
		 1, 1);
	     elm_icon_standard_set(ic, "document-save");

	     button = elm_button_add(popup);
	     elm_object_text_set(button, _("Yes"));
	     elm_object_part_content_set(button, "icon", ic);
	     evas_object_smart_callback_add(button, "clicked",
		 _prompt_move_apply, popup);
	     elm_object_part_content_set(popup, "button1", button);
	     evas_object_show(button);

	     ic = elm_icon_add(popup);
	     elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
	     evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL,
		 1, 1);
	     elm_icon_standard_set(ic, "window-close");

	     button = elm_button_add(popup);
	     elm_object_text_set(button, _("No"));
	     elm_object_part_content_set(button, "icon", ic);
	     evas_object_smart_callback_add(button, "clicked", _prompt_cancel,
		 popup);
	     elm_object_part_content_set(popup, "button2", button);
	     evas_object_show(button);

	     evas_object_data_set(popup, "files",
		 eina_list_clone(tb->cut_items));
	     evas_object_data_set(popup, "path", tb->ephoto->config->directory);

	     evas_object_show(popup);
	  }
        else
	  {
	     _move_files(tb, eina_list_clone(tb->cut_items),
		 tb->ephoto->config->directory);
	  }
	eina_list_free(tb->cut_items);
	tb->cut_items = NULL;
     }
   else if (eina_list_count(tb->copy_items) > 0)
     {
	if (tb->ephoto->config->prompts)
	  {
	     Evas_Object *ic, *button;
	     Evas_Object *popup;

	     popup =
		 _prompt(tb, _("Paste Files"),
		 _("Are you sure you want to paste these files here?"));

	     ic = elm_icon_add(popup);
	     elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
	     evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL,
		 1, 1);
	     elm_icon_standard_set(ic, "document-save");

	     button = elm_button_add(popup);
	     elm_object_text_set(button, _("Yes"));
	     elm_object_part_content_set(button, "icon", ic);
	     evas_object_smart_callback_add(button, "clicked",
		 _prompt_copy_apply, popup);
	     elm_object_part_content_set(popup, "button1", button);
	     evas_object_show(button);

	     ic = elm_icon_add(popup);
	     elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
	     evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL,
		 1, 1);
	     elm_icon_standard_set(ic, "window-close");

	     button = elm_button_add(popup);
	     elm_object_text_set(button, _("No"));
	     elm_object_part_content_set(button, "icon", ic);
	     evas_object_smart_callback_add(button, "clicked", _prompt_cancel,
		 popup);
	     elm_object_part_content_set(popup, "button2", button);
	     evas_object_show(button);

	     evas_object_data_set(popup, "files",
		 eina_list_clone(tb->copy_items));
	     evas_object_data_set(popup, "path", tb->ephoto->config->directory);

	     evas_object_show(popup);
          }
        else
	  {
	     _copy_files(tb, eina_list_clone(tb->copy_items),
		 tb->ephoto->config->directory);
	  }
	eina_list_free(tb->copy_items);
	tb->copy_items = NULL;
     }
}

static void
_fsel_menu_rename_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Elm_Object_Item *item = elm_genlist_selected_item_get(tb->fsel);
   const char *path;

   if (!item)
     return;
   
   path = elm_object_item_data_get(item);
   if (!path)
     return;
   _rename_file(tb, path);
}

static void
_grid_menu_rename_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Elm_Object_Item *item = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(item, "thumb_browser");
   Ephoto_Entry *file;

   file = elm_object_item_data_get(item);
   _rename_file(tb, file->path);
   evas_object_data_del(item, "thumb_browser");
}

static void
_fsel_menu_delete_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Elm_Object_Item *item = elm_genlist_selected_item_get(tb->fsel);
   const char *path;

   if (!item)
     return;

   path = elm_object_item_data_get(item);
   if (!path)
     return;

   if (tb->ephoto->config->prompts)
     {
        Evas_Object *ic, *button;
        Evas_Object *popup;

        popup =
            _prompt(tb, _("Delete Directory"),
            _("Are you sure you want to delete this directory?"));

        ic = elm_icon_add(popup);
        elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
        evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1,
            1);
        elm_icon_standard_set(ic, "document-save");

        button = elm_button_add(popup);
        elm_object_text_set(button, _("Yes"));
        elm_object_part_content_set(button, "icon", ic);
        evas_object_smart_callback_add(button, "clicked", _prompt_delete_dir_apply,
            popup);
        elm_object_part_content_set(popup, "button1", button);
        evas_object_show(button);

        ic = elm_icon_add(popup);
        elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
        evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1,
            1);
        elm_icon_standard_set(ic, "window-close");

        button = elm_button_add(popup);
        elm_object_text_set(button, _("No"));
        elm_object_part_content_set(button, "icon", ic);
        evas_object_smart_callback_add(button, "clicked", _prompt_cancel,
            popup);
        elm_object_part_content_set(popup, "button2", button);
        evas_object_show(button);
        evas_object_data_set(popup, "path", path);

        evas_object_show(popup);
     }
   else
     {
        _delete_dir(tb, path);
     }
}  

static void
_grid_menu_delete_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Eina_List *paths = NULL, *f;
   Eina_List *selection =
       eina_list_clone(elm_gengrid_selected_items_get(tb->grid));
   Elm_Object_Item *item;
   Ephoto_Entry *file;

   if (eina_list_count(selection) <= 0)
     return;

   EINA_LIST_FOREACH(selection, f, item)
     {
        file = elm_object_item_data_get(item);
        if (ecore_file_exists(file->path))
          paths = eina_list_append(paths, strdup(file->path));
     }
   if (tb->ephoto->config->prompts)
     {
	Evas_Object *ic, *button;
	Evas_Object *popup;

	popup =
	    _prompt(tb, _("Delete Files"),
	    _("Are you sure you want to delete these files?"));

	ic = elm_icon_add(popup);
	elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
	evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1,
	    1);
	elm_icon_standard_set(ic, "document-save");

	button = elm_button_add(popup);
	elm_object_text_set(button, _("Yes"));
	elm_object_part_content_set(button, "icon", ic);
	evas_object_smart_callback_add(button, "clicked", _prompt_delete_apply,
	    popup);
	elm_object_part_content_set(popup, "button1", button);
	evas_object_show(button);

	ic = elm_icon_add(popup);
	elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
	evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1,
	    1);
	elm_icon_standard_set(ic, "window-close");

	button = elm_button_add(popup);
	elm_object_text_set(button, _("No"));
	elm_object_part_content_set(button, "icon", ic);
	evas_object_smart_callback_add(button, "clicked", _prompt_cancel,
	    popup);
	elm_object_part_content_set(popup, "button2", button);
	evas_object_show(button);
	evas_object_data_set(popup, "files", paths);

	evas_object_show(popup);
     }
   else
     {
	_delete_files(tb, paths);
     }
   eina_list_free(selection);
}

static void
_grid_menu_empty_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Eina_List *paths = NULL;
   Elm_Object_Item *item;
   Ephoto_Entry *file;
   const char *path;

   item = elm_gengrid_first_item_get(tb->grid);
   while (item)
     {
	file = elm_object_item_data_get(item);
	paths = eina_list_append(paths, strdup(file->path));
	item = elm_gengrid_item_next_get(item);
     }
   item = elm_genlist_first_item_get(tb->fsel);
   while (item)
     {
        path = elm_object_item_data_get(item);
        paths = eina_list_append(paths, strdup(path));
        item = elm_genlist_item_next_get(item);
     }
   if (eina_list_count(paths) <= 0)
     return;
   if (tb->ephoto->config->prompts)
     {
	Evas_Object *ic, *button;
	Evas_Object *popup;

	popup =
	    _prompt(tb, _("Empty Trash"),
	    _("Are you sure you want to empty the trash?"));

	ic = elm_icon_add(popup);
	elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
	evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1,
	    1);
	elm_icon_standard_set(ic, "document-save");

	button = elm_button_add(popup);
	elm_object_text_set(button, _("Yes"));
	elm_object_part_content_set(button, "icon", ic);
	evas_object_smart_callback_add(button, "clicked", _prompt_empty_apply,
	    popup);
	elm_object_part_content_set(popup, "button1", button);
	evas_object_show(button);

	ic = elm_icon_add(popup);
	elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
	evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1,
	    1);
	elm_icon_standard_set(ic, "window-close");

	button = elm_button_add(popup);
	elm_object_text_set(button, _("No"));
	elm_object_part_content_set(button, "icon", ic);
	evas_object_smart_callback_add(button, "clicked", _prompt_cancel,
	    popup);
	elm_object_part_content_set(popup, "button2", button);
	evas_object_show(button);
	evas_object_data_set(popup, "files", paths);

	evas_object_show(popup);
     }
   else
     {
	_empty_trash(tb, paths);
     }
}

static void
_menu_dismissed_cb(void *data, Evas_Object *obj,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   evas_object_del(obj);
   elm_object_focus_set(tb->main, EINA_TRUE);
}

static Eina_Bool
_click_timer_cb(void *data)
{
   Elm_Object_Item *item = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(item, "thumb_browser");

   printf("No\n");

   _on_list_selected(tb, NULL, item);
   tb->click_timer = NULL;

   return ECORE_CALLBACK_CANCEL;
}

static void
_fsel_mouse_up_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Object *menu;
   Elm_Object_Item *item;
   Evas_Event_Mouse_Up *info = event_info;
   char trash[PATH_MAX];
   int x, y;

   evas_pointer_canvas_xy_get(evas_object_evas_get(tb->fsel), &x, &y);
   item = elm_genlist_at_xy_item_get(tb->fsel, x, y, 0);

   if (info->button == 1 && item)
     {
        if (info->flags == EVAS_BUTTON_DOUBLE_CLICK)
          {
             if (elm_genlist_item_type_get(item) == ELM_GENLIST_ITEM_TREE)
               {
                  if (tb->click_timer)
                    {
                       ecore_timer_del(tb->click_timer);
                       tb->click_timer = NULL;
                       elm_genlist_item_expanded_set(item,
                           !elm_genlist_item_expanded_get(item));
                    }
               }
          }
        else
          {
             evas_object_data_set(item, "thumb_browser", tb);
             if (elm_genlist_item_type_get(item) == ELM_GENLIST_ITEM_TREE)
               tb->click_timer = ecore_timer_add(.3, _click_timer_cb, item);
             else
               _on_list_selected(tb, NULL, item);
          }
     }
   if (info->button != 3)
     return;

   snprintf(trash, PATH_MAX, "%s/.config/ephoto/trash", getenv("HOME"));
   if (item && strncmp(tb->ephoto->config->directory, trash, strlen(trash)))
     {
        elm_genlist_item_selected_set(item, EINA_TRUE);
        menu = elm_menu_add(tb->main);
        elm_menu_move(menu, x, y);
        elm_menu_item_add(menu, NULL, "folder-new", _("New Folder"),
            _fsel_menu_new_dir_cb, tb);
        if (tb->cut_items || tb->copy_items)
          elm_menu_item_add(menu, NULL, "edit-paste", _("Paste"),
              _fsel_menu_paste_cb, tb);
        elm_menu_item_add(menu, NULL, "edit", _("Rename"),
            _fsel_menu_rename_cb, tb);
        elm_menu_item_add(menu, NULL, "edit-delete", _("Delete"),
            _fsel_menu_delete_cb, tb);
        evas_object_smart_callback_add(menu, "dismissed", _menu_dismissed_cb,
            tb);
        evas_object_show(menu);
     }
   else if (item && !strncmp(tb->ephoto->config->directory, trash, strlen(trash)))
     {
        menu = elm_menu_add(tb->main);
        elm_menu_move(menu, x, y);
        elm_menu_item_add(menu, NULL, "edit-delete", _("Empty Trash"),
             _grid_menu_empty_cb, tb);
        evas_object_smart_callback_add(menu, "dismissed", _menu_dismissed_cb,
            tb);
        evas_object_show(menu);
     }
   else
     {
        menu = elm_menu_add(tb->main);
        elm_menu_move(menu, x, y);
        if (strncmp(tb->ephoto->config->directory, trash, strlen(trash)))
          elm_menu_item_add(menu, NULL, "folder-new", _("New Folder"),
              _fsel_menu_new_dir_cb, tb);
        if (tb->cut_items || tb->copy_items)
          elm_menu_item_add(menu, NULL, "edit-paste", _("Paste"),
              _fsel_menu_paste_cb, tb);
        if (!strncmp(tb->ephoto->config->directory, trash, strlen(trash)))
          elm_menu_item_add(menu, NULL, "edit-delete", _("Empty Trash"),
               _grid_menu_empty_cb, tb);
        evas_object_smart_callback_add(menu, "dismissed", _menu_dismissed_cb,
            tb);
        evas_object_show(menu);
     }
}       

static void
_grid_mouse_up_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Object *menu;
   Elm_Object_Item *item;
   Evas_Event_Mouse_Up *info = event_info;
   char trash[PATH_MAX];
   const Eina_List *selected = elm_gengrid_selected_items_get(tb->grid);
   int x, y;

   if (info->button != 3)
      return;

   snprintf(trash, PATH_MAX, "%s/.config/ephoto/trash", getenv("HOME"));

   evas_pointer_canvas_xy_get(evas_object_evas_get(tb->grid), &x, &y);
   item = elm_gengrid_at_xy_item_get(tb->grid, x, y, 0, 0);
   if (item)
      elm_gengrid_item_selected_set(item, EINA_TRUE);
   if (eina_list_count(selected) > 0 || item)
     {
	menu = elm_menu_add(tb->main);
	elm_menu_move(menu, x, y);
	elm_menu_item_add(menu, NULL, "edit-select-all", _("Select All"),
	    _grid_menu_select_all_cb, tb);
	elm_menu_item_add(menu, NULL, "edit-clear", _("Select None"),
	    _grid_menu_clear_cb, tb);
	elm_menu_item_add(menu, NULL, "edit-cut", _("Cut"), _grid_menu_cut_cb,
	    tb);
	elm_menu_item_add(menu, NULL, "edit-copy", _("Copy"),
	    _grid_menu_copy_cb, tb);
	if (tb->cut_items || tb->copy_items)
	   elm_menu_item_add(menu, NULL, "edit-paste", _("Paste"),
	       _grid_menu_paste_cb, tb);
	if (item)
	  {
	     evas_object_data_set(item, "thumb_browser", tb);
	     elm_menu_item_add(menu, NULL, "edit", _("Rename"),
		 _grid_menu_rename_cb, item);
	  }
	if (strcmp(tb->ephoto->config->directory, trash))
	   elm_menu_item_add(menu, NULL, "edit-delete", _("Delete"),
	       _grid_menu_delete_cb, tb);
	else
	   elm_menu_item_add(menu, NULL, "edit-delete", _("Empty Trash"),
	       _grid_menu_empty_cb, tb);
	evas_object_smart_callback_add(menu, "dismissed", _menu_dismissed_cb,
	    tb);
	evas_object_show(menu);
     }
   else if (tb->cut_items || tb->copy_items)
     {
	menu = elm_menu_add(tb->main);
	elm_menu_move(menu, x, y);
	elm_menu_item_add(menu, NULL, "edit-select-all", _("Select All"),
	    _grid_menu_select_all_cb, tb);
	elm_menu_item_add(menu, NULL, "edit-paste", _("Paste"),
	    _grid_menu_paste_cb, tb);
	if (!strcmp(tb->ephoto->config->directory, trash))
	   elm_menu_item_add(menu, NULL, "edit-delete", _("Empty Trash"),
	       _grid_menu_empty_cb, tb);
	evas_object_smart_callback_add(menu, "dismissed", _menu_dismissed_cb,
	    tb);
	evas_object_show(menu);
     }
   else if (!strcmp(tb->ephoto->config->directory, trash))
     {
	menu = elm_menu_add(tb->main);
	elm_menu_move(menu, x, y);
	elm_menu_item_add(menu, NULL, "edit-select-all", _("Select All"),
	    _grid_menu_select_all_cb, tb);
	elm_menu_item_add(menu, NULL, "edit-delete", _("Empty Trash"),
	    _grid_menu_empty_cb, tb);
	evas_object_smart_callback_add(menu, "dismissed", _menu_dismissed_cb,
	    tb);
	evas_object_show(menu);
     }
   else if (elm_gengrid_first_item_get(tb->grid))
     {
	menu = elm_menu_add(tb->main);
	elm_menu_move(menu, x, y);
	elm_menu_item_add(menu, NULL, "edit-select-all", _("Select All"),
	    _grid_menu_select_all_cb, tb);
	evas_object_smart_callback_add(menu, "dismissed", _menu_dismissed_cb,
	    tb);
	evas_object_show(menu);
     }
}

static void
_key_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Event_Key_Down *ev = event_info;
   Eina_Bool ctrl = evas_key_modifier_is_set(ev->modifiers, "Control");
   Eina_Bool shift = evas_key_modifier_is_set(ev->modifiers, "Shift");
   const char *k = ev->keyname;
   
   if (ctrl)
     {
        if (shift)
          {
             if (!strcmp(k, "f"))
               {
                  if (evas_object_visible_get(tb->leftbox))
                    _ephoto_dir_hide_folders(tb, NULL, NULL);
                  else
                    _ephoto_dir_show_folders(tb, NULL, NULL);
               }
          }
	else if ((!strcmp(k, "plus")) || (!strcmp(k, "equal")))
	  {
	     int zoom = tb->ephoto->config->thumb_size + ZOOM_STEP;

	     _zoom_set(tb, zoom);
	  }
	else if ((!strcmp(k, "minus")) || (!strcmp(k, "underscore")))
	  {
	     int zoom = tb->ephoto->config->thumb_size - ZOOM_STEP;

	     _zoom_set(tb, zoom);
	  }
	else if (!strcmp(k, "Tab"))
	  {
	     Elm_Object_Item *it = elm_gengrid_selected_item_get(tb->grid);
	     Ephoto_Entry *entry;

	     if (it)
		entry = elm_object_item_data_get(it);
	     else
		entry = eina_list_nth(tb->ephoto->entries, 0);

	     if (entry)
		evas_object_smart_callback_call(tb->main, "view", entry);
	  }
        else if (!strcmp(k, "c"))
          {
             _grid_menu_copy_cb(tb, NULL, NULL);
          }
        else if (!strcmp(k, "x"))
          {
             _grid_menu_cut_cb(tb, NULL, NULL);
          }
        else if (!strcmp(k, "v"))
          {
             _grid_menu_paste_cb(tb, NULL, NULL);
          }
        else if (!strcmp(k, "a"))
          {
             _grid_menu_select_all_cb(tb, NULL, NULL);
          }
     }
   else if (!strcmp(k, "F1"))
     {
        _settings(tb, NULL, NULL);
     }
   else if (!strcmp(k, "F2"))
     {
        Elm_Object_Item *it = NULL;

        it = eina_list_data_get(
            eina_list_last(elm_gengrid_selected_items_get(tb->grid)));
        if (it)
          {
             evas_object_data_set(it, "thumb_browser", tb);
             _grid_menu_rename_cb(it, NULL, NULL);
          }
     }
   else if (!strcmp(k, "F5"))
     {
	Elm_Object_Item *it = elm_gengrid_selected_item_get(tb->grid);
	Ephoto_Entry *entry;

	if (it)
	   entry = elm_object_item_data_get(it);
	else
	   entry = eina_list_nth(tb->ephoto->entries, 0);

	if (entry)
	   evas_object_smart_callback_call(tb->main, "slideshow", entry);
     }
   else if (!strcmp(k, "F11"))
     {
	Evas_Object *win = tb->ephoto->win;

	elm_win_fullscreen_set(win, !elm_win_fullscreen_get(win));
     }
   else if (!strcmp(k, "Delete"))
     {
        char path[PATH_MAX];
        char *trash;

        snprintf(path, PATH_MAX, "%s/.config/ephoto/trash",
            getenv("HOME"));
        trash = strdup(path);
        if ((strlen(trash)) == (strlen(tb->ephoto->config->directory)))
          {
             if (!strcmp(trash, tb->ephoto->config->directory))
               {
                  _grid_menu_empty_cb(tb, NULL, NULL);
                  free(trash);
                  return;
               }
          }
        else
          _grid_menu_delete_cb(tb, NULL, NULL);
        free(trash);
     }
}

static void
_main_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Ecore_Event_Handler *handler;

   _todo_items_free(tb);
   EINA_LIST_FREE(tb->handlers, handler) ecore_event_handler_del(handler);
   if (tb->animator.todo_items)
     {
	ecore_animator_del(tb->animator.todo_items);
	tb->animator.todo_items = NULL;
     }
   if (tb->ls)
     {
	tb->main_deleted = EINA_TRUE;
	eio_file_cancel(tb->ls);
	return;
     }
   if (tb->idler)
     {
	ecore_idler_del(tb->idler);
	tb->idler = NULL;
     }
   if (tb->idler_pos)
     {
	eina_list_free(tb->idler_pos);
	tb->idler_pos = NULL;
     }
   if (tb->cut_items)
      eina_list_free(tb->cut_items);
   else if (tb->copy_items)
      eina_list_free(tb->copy_items);
   free(tb);
}

static Eina_Bool
_ephoto_thumb_populate_start(void *data, int type EINA_UNUSED,
    void *event EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   evas_object_smart_callback_call(tb->main, "changed,directory", NULL);

   tb->dir_loading =
       _processing(tb, _("Loading Directory"),
       _("Please wait while the directory is loaded."));
   evas_object_show(tb->dir_loading);
   tb->animator.processed = 0;
   tb->animator.count = 0;

   _todo_items_free(tb);
   if (!tb->dirs_only)
     {
	elm_gengrid_clear(tb->grid);
        tb->totimages = 0;
        tb->totsize = 0;
     }
   elm_object_text_set(tb->direntry, tb->ephoto->config->directory);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ephoto_thumb_populate_end(void *data, int type EINA_UNUSED,
    void *event EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   tb->ls = NULL;
   if (tb->main_deleted)
     {
	free(tb);
	return ECORE_CALLBACK_PASS_ON;
     }
   if (!tb->ephoto->entries)
     {
        char buf[PATH_MAX];

        elm_object_text_set(tb->nolabel,
            _("There are no images in this directory"));
	snprintf(buf, PATH_MAX, "<b>%s:</b> 0 %s        <b>%s:</b> 0%s",
	    _("Total"), ngettext("image", "images", 0), _("Size"),
	ngettext("B", "B", 0));
	elm_object_text_set(tb->infolabel, buf);
     }
   else if (!tb->dirs_only)
     {
	char isize[PATH_MAX];
	char image_info[PATH_MAX];

        elm_object_text_set(tb->nolabel, " ");

	if (tb->totsize < 1024.0)
	   snprintf(isize, sizeof(isize), "%'.0f%s", tb->totsize, ngettext("B",
		   "B", tb->totsize));
	else
	  {
	     tb->totsize /= 1024.0;
	     if (tb->totsize < 1024)
		snprintf(isize, sizeof(isize), "%'.0f%s", tb->totsize,
		    ngettext("KB", "KB", tb->totsize));
	     else
	       {
		  tb->totsize /= 1024.0;
		  if (tb->totsize < 1024)
		     snprintf(isize, sizeof(isize), "%'.1f%s", tb->totsize,
			 ngettext("MB", "MB", tb->totsize));
		  else
		    {
		       tb->totsize /= 1024.0;
		       if (tb->totsize < 1024)
			  snprintf(isize, sizeof(isize), "%'.1f%s", tb->totsize,
			      ngettext("GB", "GB", tb->totsize));
		       else
			 {
			    tb->totsize /= 1024.0;
			    snprintf(isize, sizeof(isize), "%'.1f%s",
				tb->totsize, ngettext("TB", "TB", tb->totsize));
			 }
		    }
	       }
	  }
	snprintf(image_info, PATH_MAX, "<b>%s:</b> %d %s        <b>%s:</b> %s",
	    _("Total"), tb->totimages, ngettext("image", "images",
		tb->totimages), _("Size"), isize);
	elm_object_text_set(tb->infolabel, image_info);
     }
   if (tb->dir_loading && (tb->animator.processed == tb->animator.count))
     {
	evas_object_del(tb->dir_loading);
        evas_object_freeze_events_set(tb->main, EINA_FALSE);
        elm_object_focus_set(tb->main, EINA_TRUE);
     }
   tb->dirs_only = 0;
   tb->thumbs_only = 0;

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ephoto_thumb_populate_error(void *data, int type EINA_UNUSED,
    void *event EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   if (tb->dir_loading)
     {
        evas_object_del(tb->dir_loading);
        evas_object_freeze_events_set(tb->main, EINA_FALSE);
        elm_object_focus_set(tb->main, EINA_TRUE);
     }
   tb->dirs_only = 0;
   tb->thumbs_only = 0;

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ephoto_thumb_entry_create(void *data, int type EINA_UNUSED, void *event)
{
   Ephoto_Thumb_Browser *tb = data;
   Ephoto_Event_Entry_Create *ev = event;
   Ephoto_Entry *e;

   e = ev->entry;
   if (!e->is_dir && !tb->dirs_only)
     {
	Eina_File *f;

	tb->totimages += 1;
	f = eina_file_open(e->path, EINA_FALSE);
	tb->totsize += (double) eina_file_size_get(f);
	eina_file_close(f);
	tb->todo_items = eina_list_append(tb->todo_items, e);
	tb->animator.count++;
     }
   else if (e->is_dir && !tb->thumbs_only)
     {
	tb->todo_items = eina_list_append(tb->todo_items, e);
	tb->animator.count++;
     }
   if (!tb->animator.todo_items)
      tb->animator.todo_items = ecore_animator_add(_todo_items_process, tb);

   return ECORE_CALLBACK_PASS_ON;
}

void
ephoto_thumb_browser_fsel_clear(Ephoto *ephoto)
{
   Ephoto_Thumb_Browser *tb =
       evas_object_data_get(ephoto->thumb_browser, "thumb_browser");

   if (tb)
      elm_genlist_clear(tb->fsel);
}

Evas_Object *
ephoto_thumb_browser_add(Ephoto *ephoto, Evas_Object *parent)
{
   Evas_Object *box = elm_box_add(parent);
   Evas_Object *icon, *hbox, *but, *ic, *menu;  
   Ephoto_Thumb_Browser *tb;

   EINA_SAFETY_ON_NULL_RETURN_VAL(box, NULL);

   tb = calloc(1, sizeof(Ephoto_Thumb_Browser));
   EINA_SAFETY_ON_NULL_GOTO(tb, error);

   _ephoto_thumb_file_class.item_style = "thumb";
   _ephoto_thumb_file_class.func.text_get = _ephoto_thumb_item_text_get;
   _ephoto_thumb_file_class.func.content_get = _ephoto_thumb_file_icon_get;
   _ephoto_thumb_file_class.func.state_get = NULL;
   _ephoto_thumb_file_class.func.del = _ephoto_thumb_item_del;

   _ephoto_dir_class.item_style = "tree_effect";
   _ephoto_dir_class.func.text_get = _ephoto_dir_item_text_get;
   _ephoto_dir_class.func.content_get = _ephoto_dir_item_icon_get;
   _ephoto_dir_class.func.state_get = NULL;
   _ephoto_dir_class.func.del = _ephoto_dir_item_del;

   tb->ephoto = ephoto;
   tb->thumbs_only = 0;
   tb->dirs_only = 0;
   tb->dragging = 0;
   tb->cut_items = NULL;
   tb->copy_items = NULL;
   tb->dir_current = NULL;
   tb->change_dir_job = NULL;
   tb->sort = EPHOTO_SORT_ALPHABETICAL_ASCENDING;
   tb->main = box;

   elm_box_horizontal_set(tb->main, EINA_FALSE);
   evas_object_size_hint_weight_set(tb->main, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->main, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_event_callback_add(tb->main, EVAS_CALLBACK_DEL, _main_del, tb);
   evas_object_event_callback_add(tb->main, EVAS_CALLBACK_KEY_DOWN, _key_down,
       tb);
   evas_object_data_set(tb->main, "thumb_browser", tb);

   tb->bar = elm_toolbar_add(tb->ephoto->win);
   elm_toolbar_horizontal_set(tb->bar, EINA_TRUE);
   elm_toolbar_homogeneous_set(tb->bar, EINA_TRUE);
   elm_toolbar_shrink_mode_set(tb->bar, ELM_TOOLBAR_SHRINK_NONE);
   elm_toolbar_select_mode_set(tb->bar, ELM_OBJECT_SELECT_MODE_NONE);
   elm_object_tree_focus_allow_set(tb->bar, EINA_FALSE);
   elm_toolbar_icon_order_lookup_set(tb->bar, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_weight_set(tb->bar, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(tb->bar, EVAS_HINT_FILL, EVAS_HINT_FILL);

   if (!tb->ephoto->config->fsel_hide)
     tb->ficon = elm_toolbar_item_prepend(tb->bar, "system-file-manager", _("Folders"),
         _ephoto_dir_hide_folders, tb);
   else
     tb->ficon = elm_toolbar_item_prepend(tb->bar, "system-file-manager", _("Folders"),
         _ephoto_dir_show_folders, tb);
   icon =
       elm_toolbar_item_append(tb->bar, "view-sort-descending", _("Sort"), NULL, NULL);
   elm_toolbar_item_menu_set(icon, EINA_TRUE);
   elm_toolbar_menu_parent_set(tb->bar, tb->ephoto->win);
   menu = elm_toolbar_item_menu_get(icon);
   elm_menu_item_add(menu, NULL, "view-sort-ascending",
       _("Alphabetical Ascending"), _sort_alpha_asc, tb);
   elm_menu_item_add(menu, NULL, "view-sort-descending",
       _("Alphabetical Descending"), _sort_alpha_desc, tb);
   elm_menu_item_add(menu, NULL, "view-sort-ascending",
       _("Modification Time Ascending"), _sort_mod_asc, tb);
   elm_menu_item_add(menu, NULL, "view-sort-descending",
       _("Modification Time Descending"), _sort_mod_desc, tb); 
   icon =
       elm_toolbar_item_append(tb->bar, "zoom-in", _("Zoom In"), _zoom_in, tb);
   tb->max = elm_object_item_widget_get(icon);
   icon =
       elm_toolbar_item_append(tb->bar, "zoom-out", _("Zoom Out"), _zoom_out,
       tb);
   tb->min = elm_object_item_widget_get(icon);
   evas_object_data_set(tb->max, "min", tb->min);
   evas_object_data_set(tb->min, "max", tb->max);
   elm_toolbar_item_append(tb->bar, "media-playback-start", _("Slideshow"),
       _slideshow, tb);
   elm_toolbar_item_append(tb->bar, "preferences-system", _("Settings"),
       _settings, tb);

   elm_box_pack_end(tb->main, tb->bar);
   evas_object_show(tb->bar);

   tb->table = elm_table_add(tb->main);
   evas_object_size_hint_weight_set(tb->table, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(tb->main, tb->table);
   evas_object_show(tb->table);

   tb->leftbox = elm_box_add(tb->table);
   evas_object_size_hint_weight_set(tb->leftbox, 0.1, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->leftbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   
   hbox = elm_box_add(tb->leftbox);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(tb->leftbox, hbox);
   evas_object_show(hbox);

   ic = elm_icon_add(hbox);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   elm_icon_standard_set(ic, "go-up");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   but = elm_button_add(hbox);
   elm_object_part_content_set(but, "icon", ic);
   elm_object_text_set(but, _("Up"));
   evas_object_size_hint_weight_set(but, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(but, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_callback_add(but, "clicked", _ephoto_dir_go_up, tb);
   elm_box_pack_end(hbox, but);
   evas_object_show(but);

   ic = elm_icon_add(hbox);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   elm_icon_standard_set(ic, "go-home");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   but = elm_button_add(hbox);
   elm_object_part_content_set(but, "icon", ic);
   elm_object_text_set(but, _("Home"));
   evas_object_size_hint_weight_set(but, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(but, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_callback_add(but, "clicked", _ephoto_dir_go_home, tb);
   elm_box_pack_end(hbox, but);
   evas_object_show(but);

   ic = elm_icon_add(hbox);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   elm_icon_standard_set(ic, "user-trash");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   but = elm_button_add(hbox);
   elm_object_part_content_set(but, "icon", ic);
   elm_object_text_set(but, _("Trash"));
   evas_object_size_hint_weight_set(but, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(but, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_callback_add(but, "clicked", _ephoto_dir_go_trash, tb);
   elm_box_pack_end(hbox, but);
   evas_object_show(but);

   tb->direntry = elm_entry_add(tb->leftbox);
   elm_entry_single_line_set(tb->direntry, EINA_TRUE);
   elm_entry_scrollable_set(tb->direntry, EINA_TRUE);
   elm_scroller_policy_set(tb->direntry, ELM_SCROLLER_POLICY_OFF,
       ELM_SCROLLER_POLICY_OFF);
   evas_object_size_hint_weight_set(tb->direntry, EVAS_HINT_EXPAND,
       EVAS_HINT_FILL);
   evas_object_size_hint_align_set(tb->direntry, EVAS_HINT_FILL,
       EVAS_HINT_FILL);
   elm_box_pack_end(tb->leftbox, tb->direntry);
   evas_object_smart_callback_add(tb->direntry, "activated",
       _ephoto_direntry_go, tb);
   evas_object_show(tb->direntry);

   tb->fsel = elm_genlist_add(tb->leftbox);
   elm_genlist_homogeneous_set(tb->fsel, EINA_TRUE);
   elm_genlist_select_mode_set(tb->fsel, ELM_OBJECT_SELECT_MODE_ALWAYS);
   evas_object_size_hint_weight_set(tb->fsel, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->fsel, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(tb->leftbox, tb->fsel);
   evas_object_smart_callback_add(tb->fsel, "expand,request",
       _on_list_expand_req, tb);
   evas_object_smart_callback_add(tb->fsel, "contract,request",
       _on_list_contract_req, tb);
   evas_object_smart_callback_add(tb->fsel, "expanded", _on_list_expanded, tb);
   evas_object_smart_callback_add(tb->fsel, "contracted", _on_list_contracted,
       tb); 
   evas_object_event_callback_add(tb->fsel, EVAS_CALLBACK_MOUSE_UP,
       _fsel_mouse_up_cb, tb);
   evas_object_data_set(tb->fsel, "thumb_browser", tb);
   evas_object_show(tb->fsel);
   elm_drop_item_container_add(tb->fsel, ELM_SEL_FORMAT_TARGETS,
       _drop_item_getcb, _drop_enter, tb, _drop_leave, tb, _drop_pos, tb,
       _drop_dropcb, NULL);

   if (!tb->ephoto->config->fsel_hide)
     {
        elm_table_pack(tb->table, tb->leftbox, 0, 0, 1, 1);
        evas_object_show(tb->leftbox);
     }
   else
        evas_object_hide(tb->leftbox);

   tb->nolabel = elm_label_add(tb->table);
   elm_label_line_wrap_set(tb->nolabel, ELM_WRAP_WORD);
   elm_object_text_set(tb->nolabel,
       _("There are no images in this directory"));
   evas_object_size_hint_weight_set(tb->nolabel, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->nolabel, EVAS_HINT_FILL,
       EVAS_HINT_FILL);
   evas_object_size_hint_aspect_set(tb->nolabel, EVAS_ASPECT_CONTROL_VERTICAL,
       1, 1);
   evas_object_show(tb->nolabel);
   if (!tb->ephoto->config->fsel_hide)
     elm_table_pack(tb->table, tb->nolabel, 1, 0, 4, 1);
   else
     elm_table_pack(tb->table, tb->nolabel, 0, 0, 5, 1);

   tb->grid = elm_gengrid_add(tb->table);
   evas_object_size_hint_weight_set(tb->grid, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->grid, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_gengrid_align_set(tb->grid, 0.5, 0.0);
   elm_gengrid_multi_select_set(tb->grid, EINA_TRUE);
   elm_gengrid_multi_select_mode_set(tb->grid,
       ELM_OBJECT_MULTI_SELECT_MODE_WITH_CONTROL);
   elm_scroller_bounce_set(tb->grid, EINA_FALSE, EINA_TRUE);
   evas_object_smart_callback_add(tb->grid, "activated",
       _ephoto_thumb_activated, tb);
   evas_object_event_callback_add(tb->grid, EVAS_CALLBACK_MOUSE_UP,
       _grid_mouse_up_cb, tb); 
   elm_drag_item_container_add(tb->grid, ANIM_TIME, DRAG_TIMEOUT, _dnd_item_get,
       _dnd_item_data_get);
   evas_object_data_set(tb->grid, "thumb_browser", tb);

   _zoom_set(tb, tb->ephoto->config->thumb_size);

   evas_object_show(tb->grid);
   if (!tb->ephoto->config->fsel_hide)
     elm_table_pack(tb->table, tb->grid, 1, 0, 4, 1);
   else
     elm_table_pack(tb->table, tb->grid, 0, 0, 5, 1);

   tb->infolabel = elm_label_add(tb->table);
   elm_label_line_wrap_set(tb->infolabel, ELM_WRAP_WORD);
   elm_object_text_set(tb->infolabel, "Info Label");
   evas_object_size_hint_weight_set(tb->infolabel, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(tb->infolabel, EVAS_HINT_FILL,
       EVAS_HINT_FILL);
   evas_object_size_hint_aspect_set(tb->infolabel, EVAS_ASPECT_CONTROL_VERTICAL,
       1, 1);
   evas_object_show(tb->infolabel);
   elm_table_pack(tb->table, tb->infolabel, 0, 1, 5, 1);

   tb->handlers =
       eina_list_append(tb->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_POPULATE_START,
	   _ephoto_thumb_populate_start, tb));

   tb->handlers =
       eina_list_append(tb->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_POPULATE_END,
	   _ephoto_thumb_populate_end, tb));

   tb->handlers =
       eina_list_append(tb->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_POPULATE_ERROR,
	   _ephoto_thumb_populate_error, tb));

   tb->handlers =
       eina_list_append(tb->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_ENTRY_CREATE,
	   _ephoto_thumb_entry_create, tb));

   return tb->main;

  error:
   evas_object_del(tb->main);
   return NULL;
}
