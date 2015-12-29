#include "ephoto.h"

int EPHOTO_EVENT_ENTRY_CREATE = 0;
int EPHOTO_EVENT_POPULATE_START = 0;
int EPHOTO_EVENT_POPULATE_END = 0;
int EPHOTO_EVENT_POPULATE_ERROR = 0;

typedef struct _Ephoto_Entry_Free_Listener Ephoto_Entry_Free_Listener;
struct _Ephoto_Entry_Free_Listener
{
   void (*cb) (void *data, const Ephoto_Entry *dead);
   const void *data;
};

typedef struct _Ephoto_Dir_Data Ephoto_Dir_Data;
struct _Ephoto_Dir_Data
{
   Ephoto *ephoto;
   Elm_Object_Item *expanded;
   Eina_Bool dirs_only;
   Eina_Bool thumbs_only;
};

static void
_ephoto_state_set(Ephoto *ephoto, Ephoto_State state)
{
   ephoto->prev_state = ephoto->state;
   ephoto->state = state;
}

static void
_ephoto_thumb_browser_show(Ephoto *ephoto, Ephoto_Entry *entry)
{
   ephoto_single_browser_entry_set(ephoto->single_browser, NULL);
   ephoto_slideshow_entry_set(ephoto->slideshow, NULL);
   elm_naviframe_item_promote(ephoto->tb);
   elm_object_focus_set(ephoto->thumb_browser, EINA_TRUE);
   _ephoto_state_set(ephoto, EPHOTO_STATE_THUMB);
   ephoto_title_set(ephoto, ephoto->config->directory);

   if ((entry) && (entry->item))
     {
        elm_gengrid_item_selected_set(entry->item, EINA_TRUE);
        elm_gengrid_item_bring_in(entry->item, ELM_GENGRID_ITEM_SCROLLTO_IN);
     }
}

static void
_ephoto_single_browser_show(Ephoto *ephoto, Ephoto_Entry *entry)
{
   if (ephoto->selentries)
     ephoto_single_browser_entries_set(ephoto->single_browser, ephoto->selentries);
   else
     ephoto_single_browser_entries_set(ephoto->single_browser, ephoto->entries);
   ephoto_single_browser_entry_set(ephoto->single_browser, entry);
   elm_naviframe_item_simple_promote(ephoto->pager, ephoto->single_browser);
   elm_object_focus_set(ephoto->single_browser, EINA_TRUE);
   _ephoto_state_set(ephoto, EPHOTO_STATE_SINGLE);
}

static void
_ephoto_slideshow_show(Ephoto *ephoto, Ephoto_Entry *entry)
{
   if (ephoto->selentries)
     ephoto_slideshow_entries_set(ephoto->slideshow, ephoto->selentries);
   else
     ephoto_slideshow_entries_set(ephoto->slideshow, ephoto->entries);
   ephoto_slideshow_entry_set(ephoto->slideshow, entry);
   elm_naviframe_item_simple_promote(ephoto->pager, ephoto->slideshow);
   elm_object_focus_set(ephoto->slideshow, EINA_TRUE);
   _ephoto_state_set(ephoto, EPHOTO_STATE_SLIDESHOW);
}

static void
_ephoto_single_browser_back(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto *ephoto = data;
   Ephoto_Entry *entry = event_info;

   ephoto->selentries = NULL;
   _ephoto_thumb_browser_show(ephoto, entry);
}

static void
_ephoto_slideshow_back(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto *ephoto = data;
   Ephoto_Entry *entry = event_info;

   switch (ephoto->prev_state)
     {
       case EPHOTO_STATE_SINGLE:
	  _ephoto_single_browser_show(ephoto, entry);
	  break;

       case EPHOTO_STATE_THUMB:
          ephoto->selentries = NULL;
	  _ephoto_thumb_browser_show(ephoto, entry);
	  break;

       default:
          ephoto->selentries = NULL;
	  _ephoto_thumb_browser_show(ephoto, entry);
     }
}

static void
_ephoto_thumb_browser_view(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto *ephoto = data;
   Ephoto_Entry *entry = event_info;

   _ephoto_single_browser_show(ephoto, entry);
}

static void
_ephoto_thumb_browser_changed_directory(void *data,
    Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;

   ephoto->selentries = NULL;
   ephoto_single_browser_entry_set(ephoto->single_browser, NULL);
   ephoto_slideshow_entry_set(ephoto->slideshow, NULL);
}

static void
_ephoto_thumb_browser_slideshow(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto *ephoto = data;
   Ephoto_Entry *entry = event_info;

   _ephoto_slideshow_show(ephoto, entry);
}

static void
_ephoto_single_browser_slideshow(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto *ephoto = data;
   Ephoto_Entry *entry = event_info;

   _ephoto_slideshow_show(ephoto, entry);
}

static void
_win_free(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;

   if (ephoto->timer.thumb_regen)
      ecore_timer_del(ephoto->timer.thumb_regen);
   ephoto_config_save(ephoto);
   free(ephoto);
}

static void
_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;
   int w, h;

   evas_object_geometry_get(ephoto->win, 0, 0, &w, &h);
   if (w && h)
     {
	ephoto->config->window_width = w;
	ephoto->config->window_height = h;
     }
}

Evas_Object *
ephoto_window_add(const char *path)
{
   Ephoto *ephoto = calloc(1, sizeof(Ephoto));
   char buf[PATH_MAX];

   EINA_SAFETY_ON_NULL_RETURN_VAL(ephoto, NULL);

   EPHOTO_EVENT_ENTRY_CREATE = ecore_event_type_new();
   EPHOTO_EVENT_POPULATE_START = ecore_event_type_new();
   EPHOTO_EVENT_POPULATE_END = ecore_event_type_new();
   EPHOTO_EVENT_POPULATE_ERROR = ecore_event_type_new();

   ephoto->selentries = NULL;
   ephoto->win = elm_win_util_standard_add("ephoto", "Ephoto");
   if (!ephoto->win)
     {
	free(ephoto);
	return NULL;
     }

   evas_object_event_callback_add(ephoto->win, EVAS_CALLBACK_FREE, _win_free,
       ephoto);
   evas_object_event_callback_add(ephoto->win, EVAS_CALLBACK_RESIZE, _resize_cb,
       ephoto);
   elm_win_autodel_set(ephoto->win, EINA_TRUE);

   if (!ephoto_config_init(ephoto))
     {
	evas_object_del(ephoto->win);
	return NULL;
     }

   if ((ephoto->config->thumb_gen_size != 128) &&
       (ephoto->config->thumb_gen_size != 256) &&
       (ephoto->config->thumb_gen_size != 512))
      ephoto_thumb_size_set(ephoto, ephoto->config->thumb_size);

   ephoto->pager = elm_naviframe_add(ephoto->win);
   elm_naviframe_prev_btn_auto_pushed_set(ephoto->pager, EINA_FALSE);
   evas_object_size_hint_weight_set(ephoto->pager, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(ephoto->pager, EVAS_HINT_FILL,
       EVAS_HINT_FILL);
   elm_win_resize_object_add(ephoto->win, ephoto->pager);
   evas_object_show(ephoto->pager);

   ephoto->thumb_browser = ephoto_thumb_browser_add(ephoto, ephoto->pager);
   if (!ephoto->thumb_browser)
     {
	evas_object_del(ephoto->win);
	return NULL;
     }
   ephoto->tb =
       elm_naviframe_item_push(ephoto->pager, NULL, NULL, NULL,
       ephoto->thumb_browser, "overlap");
   elm_naviframe_item_title_enabled_set(ephoto->tb, EINA_FALSE, EINA_FALSE);
   evas_object_smart_callback_add(ephoto->thumb_browser, "view",
       _ephoto_thumb_browser_view, ephoto);
   evas_object_smart_callback_add(ephoto->thumb_browser, "changed,directory",
       _ephoto_thumb_browser_changed_directory, ephoto);
   evas_object_smart_callback_add(ephoto->thumb_browser, "slideshow",
       _ephoto_thumb_browser_slideshow, ephoto);

   ephoto->single_browser = ephoto_single_browser_add(ephoto, ephoto->pager);
   if (!ephoto->single_browser)
     {
	evas_object_del(ephoto->win);
	return NULL;
     }
   ephoto->sb =
       elm_naviframe_item_insert_after(ephoto->pager, ephoto->tb, NULL, NULL,
       NULL, ephoto->single_browser, "overlap");
   elm_naviframe_item_title_enabled_set(ephoto->sb, EINA_FALSE, EINA_FALSE);
   evas_object_smart_callback_add(ephoto->single_browser, "back",
       _ephoto_single_browser_back, ephoto);
   evas_object_smart_callback_add(ephoto->single_browser, "slideshow",
       _ephoto_single_browser_slideshow, ephoto);

   ephoto->slideshow = ephoto_slideshow_add(ephoto, ephoto->pager);
   if (!ephoto->slideshow)
     {
	evas_object_del(ephoto->win);
	return NULL;
     }
   ephoto->sl =
       elm_naviframe_item_insert_after(ephoto->pager, ephoto->sb, NULL, NULL,
       NULL, ephoto->slideshow, "overlap");
   elm_naviframe_item_title_enabled_set(ephoto->sl, EINA_FALSE, EINA_FALSE);
   evas_object_smart_callback_add(ephoto->slideshow, "back",
       _ephoto_slideshow_back, ephoto);

   if ((!path) || (!ecore_file_exists(path)))
     {
	if (ephoto->config->open)
	  {
	     if (!strcmp(ephoto->config->open, "Last"))
		path = ephoto->config->directory;
	     else
		path = ephoto->config->open;
	     if ((path) && (!ecore_file_exists(path)))
		path = NULL;
	  }
        else if (!ephoto->config->open || path)
	  {
	     if (getcwd(buf, sizeof(buf)))
		path = buf;
	     else
		path = getenv("HOME");
	  }
     }

   if (ecore_file_is_dir(path))
     {
	ephoto_directory_set(ephoto, path, NULL, EINA_FALSE, EINA_FALSE);
	_ephoto_thumb_browser_show(ephoto, NULL);
     }
   else
     {
	char *dir = ecore_file_dir_get(path);

	ephoto_directory_set(ephoto, dir, NULL, EINA_FALSE, EINA_FALSE);
	free(dir);
	ephoto_single_browser_path_pending_set(ephoto->single_browser, path);

	elm_naviframe_item_simple_promote(ephoto->pager,
	    ephoto->single_browser);
	ephoto->state = EPHOTO_STATE_SINGLE;
     }

   evas_object_resize(ephoto->win, ephoto->config->window_width,
       ephoto->config->window_height);
   evas_object_show(ephoto->win);

   return ephoto->win;
}

void
ephoto_title_set(Ephoto *ephoto, const char *title)
{
   char buf[1024] = "Ephoto";

   if (title)
     {
	snprintf(buf, sizeof(buf), "Ephoto - %s", title);
	elm_win_title_set(ephoto->win, buf);
     }
   else
      elm_win_title_set(ephoto->win, "Ephoto");
}

int
ephoto_entries_cmp(const void *pa, const void *pb)
{
   const Ephoto_Entry *a = pa, *b = pb;

   return strcoll(a->basename, b->basename);
}

static void
_ephoto_populate_main(void *data, Eio_File *handler EINA_UNUSED,
    const Eina_File_Direct_Info *info)
{
   Ephoto_Dir_Data *ed = data;
   Ephoto_Entry *e;
   Ephoto_Event_Entry_Create *ev;

   e = ephoto_entry_new(ed->ephoto, info->path, info->path + info->name_start,
       info->type);

   if (e->is_dir)
     {
	if (!ed->ephoto->direntries)
	   ed->ephoto->direntries = eina_list_append(ed->ephoto->direntries, e);
	else
	  {
	     int near_cmp;
	     Eina_List *near_node =
		 eina_list_search_sorted_near_list(ed->ephoto->direntries,
		 ephoto_entries_cmp, e, &near_cmp);

	     if (near_cmp < 0)
		ed->ephoto->direntries =
		    eina_list_append_relative_list(ed->ephoto->direntries, e,
		    near_node);
	     else
		ed->ephoto->direntries =
		    eina_list_prepend_relative_list(ed->ephoto->direntries, e,
		    near_node);
	  }
	e->parent = ed->expanded;
     }
   else
     {
	if (!ed->ephoto->entries)
	   ed->ephoto->entries = eina_list_append(ed->ephoto->entries, e);
	else
	  {
	     int near_cmp;
	     Eina_List *near_node =
		 eina_list_search_sorted_near_list(ed->ephoto->entries,
		 ephoto_entries_cmp, e, &near_cmp);

	     if (near_cmp < 0)
		ed->ephoto->entries =
		    eina_list_append_relative_list(ed->ephoto->entries, e,
		    near_node);
	     else
		ed->ephoto->entries =
		    eina_list_prepend_relative_list(ed->ephoto->entries, e,
		    near_node);
	  }
	e->parent = NULL;
     }
   ev = calloc(1, sizeof(Ephoto_Event_Entry_Create));
   ev->entry = e;

   ecore_event_add(EPHOTO_EVENT_ENTRY_CREATE, ev, NULL, NULL);
}

static Eina_Bool
_ephoto_populate_filter(void *data, Eio_File *handler EINA_UNUSED,
    const Eina_File_Direct_Info *info)
{
   Ephoto_Dir_Data *ed = data;
   const char *bname = info->path + info->name_start;

   if (bname[0] == '.')
      return EINA_FALSE;
   if (info->type == EINA_FILE_DIR)
     {
        return EINA_TRUE;
     }
   if (!ed->dirs_only)
     return _ephoto_eina_file_direct_info_image_useful(info);
   else
     return EINA_FALSE;
}

static void
_ephoto_populate_end(void *data, Eio_File *handler EINA_UNUSED)
{
   Ephoto_Dir_Data *ed = data;

   ed->ephoto->ls = NULL;

   if (!ed->ephoto->selentries)
     ephoto_single_browser_entries_set(ed->ephoto->single_browser,
         ed->ephoto->entries);
   ecore_event_add(EPHOTO_EVENT_POPULATE_END, NULL, NULL, NULL);
   free(ed);
}

static void
_ephoto_populate_error(void *data, Eio_File *handler, int error EINA_UNUSED)
{
   Ephoto_Dir_Data *ed = data;

   ecore_event_add(EPHOTO_EVENT_POPULATE_ERROR, NULL, NULL, NULL);
   _ephoto_populate_end(ed->ephoto, handler);
}

static void
_ephoto_populate_entries(Ephoto_Dir_Data *ed)
{
   Ephoto_Entry *entry;

   if (ed->dirs_only)
     EINA_LIST_FREE(ed->ephoto->direntries, entry)
       ephoto_entry_free(entry->ephoto, entry);
   else if (ed->thumbs_only)
     EINA_LIST_FREE(ed->ephoto->entries, entry)
       ephoto_entry_free(entry->ephoto, entry);
   else
     ephoto_entries_free(ed->ephoto);

   ed->ephoto->ls =
       eio_file_stat_ls(ed->ephoto->config->directory, _ephoto_populate_filter,
       _ephoto_populate_main, _ephoto_populate_end, _ephoto_populate_error, ed);

   ecore_event_add(EPHOTO_EVENT_POPULATE_START, NULL, NULL, NULL);
}

static void
_ephoto_change_dir(void *data)
{
   Ephoto_Dir_Data *ed = data;

   ed->ephoto->job.change_dir = NULL;
   _ephoto_populate_entries(ed);
}

void
ephoto_directory_set(Ephoto *ephoto, const char *path, Evas_Object *expanded,
    Eina_Bool dirs_only, Eina_Bool thumbs_only)
{
   Ephoto_Dir_Data *ed;

   ed = malloc(sizeof(Ephoto_Dir_Data));
   ed->ephoto = ephoto;
   ed->expanded = expanded;
   ed->dirs_only = dirs_only;
   ed->thumbs_only = thumbs_only;

   ephoto_title_set(ephoto, NULL);
   eina_stringshare_replace(&ephoto->config->directory, ecore_file_realpath(path));

   if (ed->ephoto->job.change_dir)
      ecore_job_del(ed->ephoto->job.change_dir);
   ed->ephoto->job.change_dir = ecore_job_add(_ephoto_change_dir, ed);
}

static Eina_Bool
_thumb_gen_size_changed_timer_cb(void *data)
{
   Ephoto *ephoto = data;
   const Eina_List *l;
   Evas_Object *o;

   if (ephoto->config->thumb_gen_size == ephoto->thumb_gen_size)
      goto end;

   ephoto->config->thumb_gen_size = ephoto->thumb_gen_size;

   EINA_LIST_FOREACH(ephoto->thumbs, l, o)
     {
        Ethumb_Thumb_Format format;

        format = (long) evas_object_data_get(o, "ephoto_format");
        if (format)
	  {
	     elm_thumb_format_set(o, format);
	     if (format == ETHUMB_THUMB_FDO)
	       {
	          if (ephoto->config->thumb_gen_size < 256)
		    elm_thumb_fdo_size_set(o, ETHUMB_THUMB_NORMAL);
		  else
		    elm_thumb_fdo_size_set(o, ETHUMB_THUMB_LARGE);
	       }
             else
	       elm_thumb_size_set(o, ephoto->thumb_gen_size,
		   ephoto->thumb_gen_size);
	     elm_thumb_reload(o);
	  }
     }
  end:
   ephoto->timer.thumb_regen = NULL;
   return EINA_FALSE;
}

void
ephoto_thumb_size_set(Ephoto *ephoto, int size)
{
   if (ephoto->config->thumb_size != size)
     {
	ephoto->config->thumb_size = size;
     }

   if (size <= 128)
      ephoto->thumb_gen_size = 128;
   else if (size <= 256)
      ephoto->thumb_gen_size = 256;
   else
      ephoto->thumb_gen_size = 512;

   if (ephoto->timer.thumb_regen)
      ecore_timer_del(ephoto->timer.thumb_regen);
   ephoto->timer.thumb_regen =
       ecore_timer_add(0.1, _thumb_gen_size_changed_timer_cb, ephoto);
}

static void
_thumb_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj,
    void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;

   ephoto->thumbs = eina_list_remove(ephoto->thumbs, obj);
}

Evas_Object *
ephoto_thumb_add(Ephoto *ephoto, Evas_Object *parent, const char *path)
{
   Evas_Object *o;

   if (path)
     {
	const char *ext = strrchr(path, '.');

	if (ext)
	  {
	     ext++;
	     if ((strcasecmp(ext, "edj") == 0))
		o = elm_icon_add(parent);
	     else
		o = elm_thumb_add(parent);
	  }
        else
	   o = elm_thumb_add(parent);
	ephoto_thumb_path_set(o, path);
     }
   else
      o = elm_thumb_add(parent);
   if (!o)
      return NULL;

   elm_object_style_set(o, "noframe");
   ephoto->thumbs = eina_list_append(ephoto->thumbs, o);
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, _thumb_del, ephoto);

   return o;
}

void
ephoto_thumb_path_set(Evas_Object *obj, const char *path)
{
   Ethumb_Thumb_Format format = ETHUMB_THUMB_FDO;
   const char *group = NULL;
   const char *ext = strrchr(path, '.');

   if (ext)
     {
	ext++;
	if ((strcasecmp(ext, "jpg") == 0) || (strcasecmp(ext, "jpeg") == 0))
	   format = ETHUMB_THUMB_JPEG;
	else if ((strcasecmp(ext, "edj") == 0))
	  {
	     if (edje_file_group_exists(path, "e/desktop/background"))
		group = "e/desktop/background";
	     else
	       {
		  Eina_List *g = edje_file_collection_list(path);

		  group = eina_list_data_get(g);
		  edje_file_collection_list_free(g);
	       }
	     elm_image_file_set(obj, path, group);
	     evas_object_data_set(obj, "ephoto_format", NULL);
	     return;
	  }
     }
   elm_thumb_format_set(obj, format);
   evas_object_data_set(obj, "ephoto_format", (void *) (long) format);
   elm_thumb_crop_align_set(obj, 0.5, 0.5);
   elm_thumb_aspect_set(obj, ETHUMB_THUMB_CROP);
   elm_thumb_orientation_set(obj, ETHUMB_THUMB_ORIENT_ORIGINAL);
   elm_thumb_file_set(obj, path, group);
}

Ephoto_Entry *
ephoto_entry_new(Ephoto *ephoto, const char *path, const char *label,
    Eina_File_Type type)
{
   Ephoto_Entry *entry;

   entry = calloc(1, sizeof(Ephoto_Entry));
   entry->ephoto = ephoto;
   entry->path = eina_stringshare_add(path);
   entry->basename = ecore_file_file_get(entry->path);
   entry->label = eina_stringshare_add(label);
   if (type == EINA_FILE_DIR)
      entry->is_dir = EINA_TRUE;
   else
      entry->is_dir = EINA_FALSE;
   return entry;
}

void
ephoto_entry_free(Ephoto *ephoto, Ephoto_Entry *entry)
{
   Ephoto_Entry_Free_Listener *fl;
   Eina_List *node;

   EINA_LIST_FREE(entry->free_listeners, fl)
     {
        fl->cb((void *) fl->data, entry);
        free(fl);
     }
   if (entry->is_dir)
     {
        node = eina_list_data_find_list(ephoto->direntries, entry);
        ephoto->direntries = eina_list_remove_list(ephoto->direntries, node);
     }
   else
     {
        node = eina_list_data_find_list(ephoto->entries, entry);
        ephoto->entries = eina_list_remove_list(ephoto->entries, node);
        if (ephoto->selentries)
          {
             node = eina_list_data_find_list(ephoto->selentries, entry);
             ephoto->selentries = eina_list_remove_list(ephoto->selentries, node);
          }
     }
   eina_stringshare_del(entry->path);
   eina_stringshare_del(entry->label);
   free(entry);
}

void
ephoto_entry_free_listener_add(Ephoto_Entry *entry, void (*cb) (void *data,
	const Ephoto_Entry *entry), const void *data)
{
   Ephoto_Entry_Free_Listener *fl;

   fl = malloc(sizeof(Ephoto_Entry_Free_Listener));
   fl->cb = cb;
   fl->data = data;
   entry->free_listeners = eina_list_append(entry->free_listeners, fl);
}

void
ephoto_entry_free_listener_del(Ephoto_Entry *entry, void (*cb) (void *data,
	const Ephoto_Entry *entry), const void *data)
{
   Eina_List *l;
   Ephoto_Entry_Free_Listener *fl;

   EINA_LIST_FOREACH(entry->free_listeners, l, fl)
     {
        if ((fl->cb == cb) && (fl->data == data))
	  {
	     entry->free_listeners =
	         eina_list_remove_list(entry->free_listeners, l);
	     break;
          }
     }
}

void
ephoto_entries_free(Ephoto *ephoto)
{
   Ephoto_Entry *entry;

   EINA_LIST_FREE(ephoto->entries, entry) ephoto_entry_free(ephoto, entry);
   EINA_LIST_FREE(ephoto->direntries, entry) ephoto_entry_free(ephoto, entry);
}
