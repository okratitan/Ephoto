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
   Evas_Object *panes;
   Evas_Object *table;
   Evas_Object *gridbox;
   Evas_Object *grid;
   Evas_Object *original_grid;
   Evas_Object *nolabel;
   Evas_Object *infolabel;
   Evas_Object *fsel;
   Evas_Object *leftbox;
   Evas_Object *direntry;
   Evas_Object *search;
   Evas_Object *hover;
   Evas_Object *toggle;
   Elm_Object_Item *dir_current;
   Elm_Object_Item *last_sel;
   Ephoto_Sort sort;
   Eio_File *ls;
   Ecore_File_Monitor *monitor;
   Eina_List *cut_items;
   Eina_List *copy_items;
   Eina_List *handlers;
   Eina_List *todo_items;
   Eina_List *entries;
   Eina_List *searchentries;
   Ecore_Job *change_dir_job;
   Ecore_Timer *click_timer;
   Eina_Bool thumbs_only;
   Eina_Bool dirs_only;
   double totsize;
   double totsize_old;
   int totimages;
   int totimages_old;
   Eina_Bool dragging;
   Eina_Bool searching;
   Eina_Bool processing;
   struct
   {
      Ecore_Animator *todo_items;
      int count;
      int processed;
   } animator;
   Eina_Bool main_deleted:1;
};

/*Item Classes*/
static Elm_Gengrid_Item_Class _ephoto_thumb_file_class;
static Elm_Genlist_Item_Class _ephoto_dir_class;

/*Main Callbacks*/
static void _ephoto_show_slideshow(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED);
static void _ephoto_show_settings(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED);
static void _ephoto_main_key_down(void *data, Evas *e EINA_UNUSED,
    Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED);
static void _ephoto_panes_unpress(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED);
static void _ephoto_panes_double_clicked(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED);
static void _ephoto_main_del(void *data, Evas *e EINA_UNUSED,
    Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED);

/*File Pane Functions*/
static void _ephoto_dir_hide_folders(void *data, Evas_Object *obj,
    void *event_info);
static void _ephoto_dir_show_folders(void *data, Evas_Object *obj,
    void *event_info);

/*Thumb Pane Functions*/
static void _ephoto_thumb_update_info_label(Ephoto_Thumb_Browser *tb);
static void _ephoto_thumb_activated(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info);
static void _ephoto_thumb_zoom_set(Ephoto_Thumb_Browser *tb, int zoom);
static void _ephoto_thumb_search_go(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED);
static void _ephoto_thumb_search_cancel(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED);
static void _ephoto_thumb_search_start(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED);

/*Common Callbacks*/
static void
_menu_dismissed_cb(void *data, Evas_Object *obj,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   evas_object_del(obj);
   elm_object_focus_set(tb->main, EINA_TRUE);
}

static void
_menu_empty_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Eina_List *paths = NULL;
   Elm_Object_Item *item;
   Ephoto_Entry *file;
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
        file = elm_object_item_data_get(item);
        paths = eina_list_append(paths, strdup(file->path));
        item = elm_genlist_item_next_get(item);
     }
   if (eina_list_count(paths) <= 0)
     return;
   ephoto_file_empty_trash(tb->ephoto, paths);
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

   Ephoto_Entry *entry = elm_object_item_data_get(it);
   const char *path = entry->path;
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

   if (tb->ephoto->config->move_drop)
     ephoto_file_move(tb->ephoto, files, path);
   else
     ephoto_file_copy(tb->ephoto, files, path);
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
                  strcat((char *) drag_data, "\n");
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

/*File Pane Callbacks*/
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
   Ephoto_Entry *entry;
   const char *path;

   entry = elm_object_item_data_get(it);
   path = entry->path;
   tb->dirs_only = 0;
   if (!strcmp(path, tb->ephoto->config->directory))
     tb->dirs_only = 1;
   else
     tb->dirs_only = 0;
   tb->thumbs_only = 0;
   ephoto_directory_set(tb->ephoto, path, it, tb->dirs_only, tb->thumbs_only);
   ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);
}

static void
_on_list_contracted(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Ephoto_Thumb_Browser *tb = data;
   Elm_Object_Item *it = event_info;
   Ephoto_Entry *entry;
   const char *path;

   entry = elm_object_item_data_get(it);
   path = entry->path;
   elm_genlist_item_subitems_clear(it);
   if (!strcmp(path, tb->ephoto->config->directory))
     return;
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
   Ephoto_Entry *entry;
   const char *path;

   entry = elm_object_item_data_get(it);
   path = entry->path;
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
_dir_item_text_get(void *data, Evas_Object *obj EINA_UNUSED,
    const char *part EINA_UNUSED)
{
   Ephoto_Entry *e = data;

   return strdup(e->label);
}

static Evas_Object *
_dir_item_icon_get(void *data EINA_UNUSED, Evas_Object *obj,
    const char *part)
{
   if (!strcmp(part, "elm.swallow.end"))
      return NULL;
   Evas_Object *ic = elm_icon_add(obj);

   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   elm_icon_standard_set(ic, "folder");
   return ic;
}

static void
_dir_item_del(void *data, Evas_Object *obj EINA_UNUSED)
{
   Ephoto_Entry *e = data;

   if (!e->no_delete)
     ephoto_entry_free(e->ephoto, e);
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

static void
_dir_go_home(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   elm_genlist_clear(tb->fsel);
   tb->thumbs_only = 0;
   tb->dirs_only = 0;
   ephoto_directory_set(tb->ephoto, getenv("HOME"), NULL,
       tb->dirs_only, tb->thumbs_only);
   ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);
   ephoto_thumb_browser_top_dir_set(tb->ephoto, tb->ephoto->config->directory);
}

static void
_dir_go_up(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   if (strcmp(tb->ephoto->config->directory, "/"))
     {
	char path[PATH_MAX];
        Elm_Object_Item *it, *next;
        Ephoto_Entry *e;

	snprintf(path, PATH_MAX, "%s", tb->ephoto->config->directory);
        it = elm_genlist_first_item_get(tb->fsel);
        while (it)
          {
             e = elm_object_item_data_get(it);
             if (!strcmp(e->path, path))
               {
                  if (e->parent)
                    {
                       elm_genlist_item_expanded_set(e->parent, EINA_FALSE);
                       return;
                    }
               }
             next = elm_genlist_item_next_get(it);
             it = next;
          }
	elm_genlist_clear(tb->fsel);
	tb->thumbs_only = 0;
        tb->dirs_only = 0;
	ephoto_directory_set(tb->ephoto, ecore_file_dir_get(path), NULL,
            tb->dirs_only, tb->thumbs_only);
	ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);
        ephoto_thumb_browser_top_dir_set(tb->ephoto,
            tb->ephoto->config->directory);
     }
}

static void
_dir_go_trash(void *data, Evas_Object *obj EINA_UNUSED,
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
   ephoto_thumb_browser_top_dir_set(tb->ephoto, tb->ephoto->config->directory);
}

static void
_dir_go_entry(void *data, Evas_Object *obj EINA_UNUSED,
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
        ephoto_thumb_browser_top_dir_set(tb->ephoto,
            tb->ephoto->config->directory);
     }
}

static Eina_Bool
_click_timer_cb(void *data)
{
   Elm_Object_Item *item = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(item, "thumb_browser");

   _on_list_selected(tb, NULL, item);
   tb->click_timer = NULL;

   return ECORE_CALLBACK_CANCEL;
}

static void
_fsel_menu_new_dir_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Elm_Object_Item *item = elm_genlist_selected_item_get(tb->fsel);
   Ephoto_Entry *entry;
   const char *path;

   if (item)
     {
        entry = elm_object_item_data_get(item);
        path = entry->path;
     }
   else
     path = tb->ephoto->config->directory;
   if (!path)
     return;
   ephoto_file_new_dir(tb->ephoto, path);
}

static void
_fsel_menu_paste_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Elm_Object_Item *item = elm_genlist_selected_item_get(tb->fsel);
   Ephoto_Entry *entry;
   const char *path;

   if (item)
     {
        entry = elm_object_item_data_get(item);
        path = entry->path;
     }
   else
     path = tb->ephoto->config->directory;

   if (eina_list_count(tb->cut_items) > 0)
     {
        ephoto_file_paste(tb->ephoto, eina_list_clone(tb->cut_items), EINA_FALSE, path);
        eina_list_free(tb->cut_items);
        tb->cut_items = NULL;
     }
   else if (eina_list_count(tb->copy_items) > 0)
     {
        ephoto_file_paste(tb->ephoto, eina_list_clone(tb->copy_items), EINA_TRUE, path);
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
   Ephoto_Entry *entry;
   const char *path;

   if (!item)
     return;
   entry = elm_object_item_data_get(item);
   path = entry->path;
   if (!path)
     return;
   ephoto_file_rename(tb->ephoto, path);
}

static void
_fsel_menu_delete_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Elm_Object_Item *item = elm_genlist_selected_item_get(tb->fsel);
   Ephoto_Entry *entry;
   Eina_List *files = NULL;
   const char *path;

   if (!item)
     return;
   entry = elm_object_item_data_get(item);
   path = entry->path;
   if (!path)
     return;
   files = eina_list_append(files, path);
   ephoto_file_delete(tb->ephoto, files, EINA_FILE_DIR);
}

static void
_fsel_mouse_up_cb(void *data, Evas *e EINA_UNUSED,
    Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Object *menu;
   Elm_Object_Item *item, *menu_it;
   Evas_Event_Mouse_Up *info = event_info;
   char trash[PATH_MAX];
   Evas_Coord x, y;

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
   else if (!item)
     {
        Elm_Object_Item *it;

        it = elm_genlist_selected_item_get(tb->fsel);
        if (it)
          elm_genlist_item_selected_set(it, EINA_FALSE);
        ephoto_directory_set(tb->ephoto, tb->ephoto->top_directory, NULL, 0, 1);
     }

   if (info->button != 3)
      return;

   snprintf(trash, PATH_MAX, "%s/.config/ephoto/trash", getenv("HOME"));

   if (item)
      elm_genlist_item_selected_set(item, EINA_TRUE);
   menu = elm_menu_add(tb->ephoto->win);
   elm_menu_move(menu, x, y);
   menu_it = elm_menu_item_add(menu, NULL, "document-properties", _("Edit"),
       NULL, NULL);
   elm_menu_item_separator_add(menu, NULL);
   if (strcmp(tb->ephoto->config->directory, trash))
     {
        elm_menu_item_add(menu, menu_it, "folder-new", _("New Folder"),
              _fsel_menu_new_dir_cb, tb);
     }
   if (item)
     {
             evas_object_data_set(item, "thumb_browser", tb);
             elm_menu_item_add(menu, menu_it, "edit", _("Rename"),
                 _fsel_menu_rename_cb, item);
     }
   if (tb->cut_items || tb->copy_items)
     {
	elm_menu_item_add(menu, menu_it, "edit-paste", _("Paste"),
	    _fsel_menu_paste_cb, tb);
     }
   if (!strcmp(tb->ephoto->config->directory, trash) &&
     elm_gengrid_first_item_get(tb->grid))
     {
	elm_menu_item_add(menu, menu_it, "edit-delete", _("Empty Trash"),
	    _menu_empty_cb, tb);
     }
   else if (!strcmp(tb->ephoto->config->directory, trash) &&
     elm_genlist_first_item_get(tb->fsel))
     {
        elm_menu_item_add(menu, menu_it, "edit-delete", _("Empty Trash"),
            _menu_empty_cb, tb);
     }
   if (strcmp(tb->ephoto->config->directory, trash) && item)
     {
        elm_menu_item_add(menu, menu_it, "edit-delete", _("Delete"),
             _fsel_menu_delete_cb, tb);
     }
   if (strcmp(tb->ephoto->config->directory, trash) &&
     elm_gengrid_first_item_get(tb->grid))
     {
        elm_menu_item_add(menu, NULL, "media-playback-start", _("Slideshow"),
           _ephoto_show_slideshow, tb);
     }
   elm_menu_item_add(menu, NULL, "preferences-system", _("Settings"),
       _ephoto_show_settings, tb);
   evas_object_smart_callback_add(menu, "dismissed", _menu_dismissed_cb,
            tb);
   evas_object_show(menu);
}

/*File Pane Functions*/
static void
_ephoto_dir_show_folders(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   elm_check_state_set(tb->toggle, EINA_FALSE);
   elm_panes_content_left_min_size_set(tb->panes, 100);
   elm_panes_content_left_size_set(tb->panes, tb->ephoto->config->lpane_size);
   tb->ephoto->config->fsel_hide = 0;
   evas_object_smart_callback_del(tb->toggle, "changed", _ephoto_dir_show_folders);
   evas_object_smart_callback_add(tb->toggle, "changed", _ephoto_dir_hide_folders, tb);
}

static void
_ephoto_dir_hide_folders(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   elm_check_state_set(tb->toggle, EINA_TRUE);
   elm_panes_content_left_min_size_set(tb->panes, 0);
   elm_panes_content_left_size_set(tb->panes, 0.0);
   tb->ephoto->config->fsel_hide = 1;
   evas_object_smart_callback_del(tb->toggle, "changed", _ephoto_dir_hide_folders);
   evas_object_smart_callback_add(tb->toggle, "changed", _ephoto_dir_show_folders, tb);
}

void
_ephoto_file_pane_add(Ephoto_Thumb_Browser *tb)
{
   Evas_Object *hbox, *but, *ic;

   tb->leftbox = elm_box_add(tb->panes);
   elm_box_horizontal_set(tb->leftbox, EINA_FALSE);
   elm_box_homogeneous_set(tb->leftbox, EINA_FALSE);
   evas_object_size_hint_weight_set(tb->leftbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->leftbox,
       EVAS_HINT_FILL, EVAS_HINT_FILL);

   hbox = elm_box_add(tb->leftbox);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   elm_box_homogeneous_set(hbox, EINA_TRUE);
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
   evas_object_smart_callback_add(but, "clicked", _dir_go_up, tb);
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
   evas_object_smart_callback_add(but, "clicked", _dir_go_home, tb);
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
   evas_object_smart_callback_add(but, "clicked", _dir_go_trash, tb);
   elm_box_pack_end(hbox, but);
   evas_object_show(but);

   tb->direntry = elm_entry_add(tb->main);
   elm_entry_single_line_set(tb->direntry, EINA_TRUE);
   elm_entry_scrollable_set(tb->direntry, EINA_TRUE);
   elm_scroller_policy_set(tb->direntry, ELM_SCROLLER_POLICY_OFF,
       ELM_SCROLLER_POLICY_OFF);
   evas_object_size_hint_weight_set(tb->direntry, EVAS_HINT_EXPAND,
       EVAS_HINT_FILL);
   evas_object_size_hint_align_set(tb->direntry, EVAS_HINT_FILL,
       EVAS_HINT_FILL);
   evas_object_smart_callback_add(tb->direntry, "activated",
       _dir_go_entry, tb);
   elm_box_pack_end(tb->leftbox, tb->direntry);
   evas_object_show(tb->direntry);

   tb->fsel = elm_genlist_add(tb->leftbox);
   elm_genlist_homogeneous_set(tb->fsel, EINA_TRUE);
   elm_genlist_select_mode_set(tb->fsel, ELM_OBJECT_SELECT_MODE_ALWAYS);
   evas_object_size_hint_weight_set(tb->fsel, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->fsel, EVAS_HINT_FILL, EVAS_HINT_FILL);
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
   elm_box_pack_end(tb->leftbox, tb->fsel);
   evas_object_show(tb->fsel);

   elm_drop_item_container_add(tb->fsel, ELM_SEL_FORMAT_TARGETS,
       _drop_item_getcb, _drop_enter, tb, _drop_leave, tb, _drop_pos, tb,
       _drop_dropcb, NULL);

   evas_object_raise(hbox);
   evas_object_raise(tb->direntry);
}

/*Thumb Pane Callbacks*/
static char *
_thumb_item_text_get(void *data, Evas_Object *obj EINA_UNUSED,
    const char *part EINA_UNUSED)
{
   Ephoto_Entry *e = data;

   return strdup(e->label);
}

static Evas_Object *
_thumb_file_icon_get(void *data, Evas_Object *obj,
    const char *part)
{
   Ephoto_Entry *e = data;
   Evas_Object *thumb = NULL;

   if (strcmp(part, "elm.swallow.icon"))
     return NULL;

   if (e)
     {
        thumb = ephoto_thumb_add(e->ephoto, obj, e->path);
        evas_object_show(thumb);
     }
   return thumb;
}

static void
_thumb_item_del(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED)
{
   /* The entry is already freed when changing directories. */
}

static int
_entry_cmp(const void *pa, const void *pb)
{
   const Ephoto_Entry *a, *b;

   a = elm_object_item_data_get(pa);
   b = elm_object_item_data_get(pb);

   return strcasecmp(a->basename, b->basename);
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
   Evas_Object *ic;

   tb->sort = EPHOTO_SORT_ALPHABETICAL_ASCENDING;
   tb->thumbs_only = 1;
   tb->dirs_only = 0;
   ic = elm_icon_add(tb->hover);
   elm_icon_standard_set(ic, "view-sort-ascending");
   elm_object_part_content_set(tb->hover, "icon", ic);
   evas_object_show(ic);
   ephoto_directory_set(tb->ephoto, tb->ephoto->config->directory,
       NULL, tb->dirs_only, tb->thumbs_only);
}

static void
_sort_alpha_desc(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Object *ic;

   tb->sort = EPHOTO_SORT_ALPHABETICAL_DESCENDING;
   tb->thumbs_only = 1;
   tb->dirs_only = 0;
   ic = elm_icon_add(tb->hover);
   elm_icon_standard_set(ic, "view-sort-descending");
   elm_object_part_content_set(tb->hover, "icon", ic);
   evas_object_show(ic);
   ephoto_directory_set(tb->ephoto, tb->ephoto->config->directory,
       NULL, tb->dirs_only, tb->thumbs_only);
}

static void
_sort_mod_asc(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Object *ic;

   tb->sort = EPHOTO_SORT_MODTIME_ASCENDING;
   tb->thumbs_only = 1;
   tb->dirs_only = 0;
   ic = elm_icon_add(tb->hover);
   elm_icon_standard_set(ic, "view-sort-ascending");
   elm_object_part_content_set(tb->hover, "icon", ic);
   evas_object_show(ic);
   ephoto_directory_set(tb->ephoto, tb->ephoto->config->directory,
       NULL, tb->dirs_only, tb->thumbs_only);
}

static void
_sort_mod_desc(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Object *ic;

   tb->sort = EPHOTO_SORT_MODTIME_DESCENDING;
   tb->thumbs_only = 1;
   tb->dirs_only = 0;
   ic = elm_icon_add(tb->hover);
   elm_icon_standard_set(ic, "view-sort-descending");
   elm_object_part_content_set(tb->hover, "icon", ic);
   evas_object_show(ic);
   ephoto_directory_set(tb->ephoto, tb->ephoto->config->directory,
       NULL, tb->dirs_only, tb->thumbs_only);
}

static void
_zoom_in(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   int zoom = tb->ephoto->config->thumb_size + ZOOM_STEP;

   _ephoto_thumb_zoom_set(tb, zoom);
}

static void
_zoom_out(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   int zoom = tb->ephoto->config->thumb_size - ZOOM_STEP;

   _ephoto_thumb_zoom_set(tb, zoom);
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
_grid_menu_paste_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   if (eina_list_count(tb->cut_items) > 0)
     {
        ephoto_file_paste(tb->ephoto, eina_list_clone(tb->cut_items), EINA_FALSE,
            tb->ephoto->config->directory);
        eina_list_free(tb->cut_items);
        tb->cut_items = NULL;
     }
   else if (eina_list_count(tb->copy_items) > 0)
     {
        ephoto_file_paste(tb->ephoto, eina_list_clone(tb->copy_items), EINA_TRUE,
            tb->ephoto->config->directory);
        eina_list_free(tb->copy_items);
        tb->copy_items = NULL;
     }
}

static void
_grid_menu_rename_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Elm_Object_Item *item = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(item, "thumb_browser");
   Ephoto_Entry *file;

   file = elm_object_item_data_get(item);
   ephoto_file_rename(tb->ephoto, file->path);
   evas_object_data_del(item, "thumb_browser");
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
   ephoto_file_delete(tb->ephoto, paths, EINA_FILE_REG);
   eina_list_free(selection);
}

static void
_grid_mouse_up_cb(void *data, Evas *e EINA_UNUSED,
    Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Object *menu, *menu_it;
   Elm_Object_Item *item;
   Evas_Event_Mouse_Up *info = event_info;
   Eina_Bool ctrl = evas_key_modifier_is_set(info->modifiers, "Control");
   Eina_Bool shift = evas_key_modifier_is_set(info->modifiers, "Shift");
   char trash[PATH_MAX];
   const Eina_List *selected = elm_gengrid_selected_items_get(tb->grid);
   int x, y;

   evas_pointer_canvas_xy_get(evas_object_evas_get(tb->grid), &x, &y);
   item = elm_gengrid_at_xy_item_get(tb->grid, x, y, 0, 0);

   if (info->button == 1 && item)
     {
        if (ctrl)
          {
             tb->last_sel = item;
          }
        else if (shift)
          {
             if (tb->last_sel)
               {
                  int one, two, i;
                  Elm_Object_Item *it, *cur = tb->last_sel;
                  one = elm_gengrid_item_index_get(tb->last_sel);
                  two = elm_gengrid_item_index_get(item);
                  if (two < one)
                    {
                       for (i = one; i > (two+1); i--)
                         {
                            it = elm_gengrid_item_prev_get(cur);
                            elm_gengrid_item_selected_set(it, EINA_TRUE);
                            cur = it;
                         }
                    }
                  else if (two > one)
                    {
                       for (i = one; i < (two-1); i++)
                         {
                            it = elm_gengrid_item_next_get(cur);
                            elm_gengrid_item_selected_set(it, EINA_TRUE);
                            cur = it;
                         }
                    }
                  tb->last_sel = item;
               }
             else
               {
                  tb->last_sel = item;
               }
          }
        else
          {
             Eina_List *sel = eina_list_clone(selected);
             Eina_List *node;
             Elm_Object_Item *it;
             if (eina_list_count(sel) > 0)
               {
                  EINA_LIST_FOREACH(sel, node, it)
                    {
                       elm_gengrid_item_selected_set(it, EINA_FALSE);
                    }
                  eina_list_free(sel);
               }
             tb->last_sel = item;
          }
     }
   if (info->button == 1 && !item)
     {
        Eina_List *sel = eina_list_clone(selected);
        Eina_List *node;
        Elm_Object_Item *it;
        if (eina_list_count(sel) > 0)
          {
             EINA_LIST_FOREACH(sel, node, it)
               {
                  elm_gengrid_item_selected_set(it, EINA_FALSE);
               }
             eina_list_free(sel);
          }
     }
   if (info->button != 3)
      return;

   snprintf(trash, PATH_MAX, "%s/.config/ephoto/trash", getenv("HOME"));

   if (item)
      elm_gengrid_item_selected_set(item, EINA_TRUE);
   menu = elm_menu_add(tb->ephoto->win);
   elm_menu_move(menu, x, y);
   if (elm_gengrid_first_item_get(tb->grid))
     {
        menu_it = elm_menu_item_add(menu, NULL, "document-properties", _("Edit"),
            NULL, NULL);
        elm_menu_item_separator_add(menu, NULL);
        elm_menu_item_add(menu, menu_it, "system-search", _("Search"),
            _ephoto_thumb_search_start, tb);
        elm_menu_item_separator_add(menu, menu_it);
        elm_menu_item_add(menu, menu_it, "edit-select-all", _("Select All"),
            _grid_menu_select_all_cb, tb);
     }
   else
     {
        menu_it = NULL;
     }
   if (eina_list_count(selected) > 0 || item)
     {
	elm_menu_item_add(menu, menu_it, "edit-clear", _("Select None"),
	    _grid_menu_clear_cb, tb);
        elm_menu_item_separator_add(menu, menu_it);
        if (item)
          {
             evas_object_data_set(item, "thumb_browser", tb);
             elm_menu_item_add(menu, menu_it, "edit", _("Rename"),
                 _grid_menu_rename_cb, item);
          }
	elm_menu_item_add(menu, menu_it, "edit-cut", _("Cut"), _grid_menu_cut_cb,
	    tb);
	elm_menu_item_add(menu, menu_it, "edit-copy", _("Copy"),
	    _grid_menu_copy_cb, tb);
     }
   if (tb->cut_items || tb->copy_items)
     {
	elm_menu_item_add(menu, menu_it, "edit-paste", _("Paste"),
	    _grid_menu_paste_cb, tb);
     }
   if (!strcmp(tb->ephoto->config->directory, trash) &&
     elm_gengrid_first_item_get(tb->grid))
     {
	elm_menu_item_add(menu, menu_it, "edit-delete", _("Empty Trash"),
	    _menu_empty_cb, tb);
     }
   else
     {
        if (elm_gengrid_first_item_get(tb->grid))
          {
             elm_menu_item_add(menu, menu_it, "edit-delete", _("Delete"),
                 _grid_menu_delete_cb, tb);
             elm_menu_item_add(menu, NULL, "media-playback-start",
                 _("Slideshow"), _ephoto_show_slideshow, tb);
          }
     }
   elm_menu_item_add(menu, NULL, "preferences-system", _("Settings"),
       _ephoto_show_settings, tb);
   evas_object_smart_callback_add(menu, "dismissed", _menu_dismissed_cb,
       tb);
   evas_object_show(menu);
}

static void
_grid_mouse_wheel(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Event_Mouse_Wheel *ev = event_info;
   Eina_Bool ctrl = evas_key_modifier_is_set(ev->modifiers, "Control");

   if (ctrl)
     {
        if (ev->z > 0)
           _zoom_out(tb, NULL, NULL);
        else
           _zoom_in(tb, NULL, NULL);
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
   if (eina_list_count(selected) <= 1 && tb->searchentries)
     {
        if (tb->ephoto->selentries)
          eina_list_free(tb->ephoto->selentries);
        tb->ephoto->selentries = NULL;
        tb->ephoto->searchentries = eina_list_clone(tb->searchentries);
     }
   else if (eina_list_count(selected) > 1)
     {
        EINA_LIST_FOREACH(selected, s, item)
          {
             elm_gengrid_item_selected_set(item, EINA_TRUE);
             tb->ephoto->selentries = eina_list_append(tb->ephoto->selentries,
                 elm_object_item_data_get(item));
          }
     }
   else
     {
        if (tb->ephoto->selentries)
          eina_list_free(tb->ephoto->selentries);
        if (tb->ephoto->searchentries)
          eina_list_free(tb->ephoto->searchentries);
        tb->ephoto->selentries = NULL;
        tb->ephoto->searchentries = NULL;
     }
   evas_object_smart_callback_call(tb->main, "view", e);
   if (selected)
     eina_list_free(selected);
}

/*Thumb Pane Functions*/
static void
_ephoto_thumb_update_info_label(Ephoto_Thumb_Browser *tb)
{
   char buf[PATH_MAX];
   char isize[PATH_MAX];
   char image_info[PATH_MAX];
   double totsize;


   if (!tb->totimages)
     {
        elm_object_text_set(tb->nolabel,
            _("No images matched your search"));
        snprintf(buf, PATH_MAX, "<b>%s:</b> 0 %s        <b>%s:</b> 0%s",
            _("Total"), ngettext("image", "images", 0), _("Size"),
        ngettext("B", "B", 0));
        elm_object_text_set(tb->infolabel, buf);
     }
   else
     {
        elm_object_text_set(tb->nolabel, " ");
        totsize = tb->totsize;
        if (totsize < 1024.0)
           snprintf(isize, sizeof(isize), "%'.0f%s", totsize, ngettext("B",
                   "B", totsize));
        else
          {
             totsize /= 1024.0;
             if (totsize < 1024)
                snprintf(isize, sizeof(isize), "%'.0f%s", totsize,
                    ngettext("KB", "KB", totsize));
             else
               {
                  totsize /= 1024.0;
                  if (totsize < 1024)
                     snprintf(isize, sizeof(isize), "%'.1f%s", totsize,
                         ngettext("MB", "MB", totsize));
                  else
                    {
                       totsize /= 1024.0;
                       if (totsize < 1024)
                          snprintf(isize, sizeof(isize), "%'.1f%s", totsize,
                              ngettext("GB", "GB", totsize));
                       else
                         {
                            totsize /= 1024.0;
                            snprintf(isize, sizeof(isize), "%'.1f%s",
                                totsize, ngettext("TB", "TB", totsize));
                         }
                    }
               }
          }
        snprintf(image_info, PATH_MAX, "<b>%s:</b> %d %s        <b>%s:</b> %s",
            _("Total"), tb->totimages, ngettext("image", "images",
                tb->totimages), _("Size"), isize);
        elm_object_text_set(tb->infolabel, image_info);
     }
}

static void
_ephoto_thumb_zoom_set(Ephoto_Thumb_Browser *tb, int zoom)
{
   double scale = elm_config_scale_get();

   if (zoom > ZOOM_MAX)
      zoom = ZOOM_MAX;
   else if (zoom < ZOOM_MIN)
      zoom = ZOOM_MIN;
   ephoto_thumb_size_set(tb->ephoto, zoom);
   elm_gengrid_item_size_set(tb->grid, zoom * scale, zoom * scale);
}

static void
_ephoto_thumb_search_go(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *search = data;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(search, "thumb_browser");
   if (tb->processing)
     return;
   Elm_Object_Item *next = NULL;
   Elm_Object_Item *found = NULL;
   Elm_Object_Item *o = NULL;
   Eina_List *sel = eina_list_clone(elm_gengrid_selected_items_get(tb->grid));
   Eina_List *l, *results = NULL;
   const char *search_text = elm_object_text_get(search);
   char pattern[PATH_MAX];

   if (tb->original_grid)
     {
        elm_gengrid_clear(tb->grid);
        elm_box_unpack(tb->gridbox, tb->grid);
        evas_object_del(tb->grid);
        tb->grid = tb->original_grid;
        elm_box_pack_end(tb->gridbox, tb->grid);
        evas_object_show(tb->grid);
        next = elm_gengrid_first_item_get(tb->grid);
     }
   snprintf(pattern, PATH_MAX, "*%s*", search_text);
   EINA_LIST_FOREACH(sel, l, o)
     {
        elm_gengrid_item_selected_set(o, EINA_FALSE);
     }
   eina_list_free(sel);
   found = elm_gengrid_search_by_text_item_get(tb->grid, next, NULL, pattern,
       ELM_GLOB_MATCH_NOCASE);
   while (found)
     {
        results = eina_list_append(results, found);
        if (found == elm_gengrid_last_item_get(tb->grid))
          break;
        next = elm_gengrid_item_next_get(found);
        found = elm_gengrid_search_by_text_item_get(tb->grid, next, NULL,
            pattern, ELM_GLOB_MATCH_NOCASE);
     }
   tb->original_grid = tb->grid;
   elm_box_unpack(tb->gridbox, tb->original_grid);
   evas_object_hide(tb->original_grid);

   tb->grid = elm_gengrid_add(tb->gridbox);
   evas_object_size_hint_weight_set(tb->grid, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->grid, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_gengrid_align_set(tb->grid, 0.5, 0.0);
   elm_gengrid_multi_select_set(tb->grid, EINA_TRUE);
   elm_gengrid_multi_select_mode_set(tb->grid,
       ELM_OBJECT_MULTI_SELECT_MODE_DEFAULT);
   elm_scroller_bounce_set(tb->grid, EINA_FALSE, EINA_TRUE);
   evas_object_smart_callback_add(tb->grid, "activated",
       _ephoto_thumb_activated, tb);
   evas_object_event_callback_add(tb->grid, EVAS_CALLBACK_MOUSE_UP,
       _grid_mouse_up_cb, tb);
   elm_drag_item_container_add(tb->grid, ANIM_TIME, DRAG_TIMEOUT,
       _dnd_item_get, _dnd_item_data_get);
   evas_object_data_set(tb->grid, "thumb_browser", tb);
   _ephoto_thumb_zoom_set(tb, tb->ephoto->config->thumb_size);
   elm_box_pack_end(tb->gridbox, tb->grid);
   evas_object_show(tb->grid);

   elm_table_pack(tb->table, tb->gridbox, 0, 0, 5, 1);
   if (tb->searchentries)
      eina_list_free(tb->searchentries);
   tb->searchentries = NULL;
   if (results)
     {
        tb->totimages = 0;
        tb->totsize = 0;
        EINA_LIST_FOREACH(results, l, o)
          {
             const Elm_Gengrid_Item_Class *ic = NULL;
             Ephoto_Entry *entry = NULL, *e = NULL;

             ic = &_ephoto_thumb_file_class;
             entry = elm_object_item_data_get(o);
             e = ephoto_entry_new(tb->ephoto, entry->path, entry->label,
                 EINA_FILE_REG);
             if (tb->sort == EPHOTO_SORT_ALPHABETICAL_ASCENDING)
               e->item =
                   elm_gengrid_item_sorted_insert(tb->grid, ic, e,
                   _entry_cmp_grid_alpha_asc, NULL, NULL);
             else if (tb->sort == EPHOTO_SORT_ALPHABETICAL_DESCENDING)
               e->item =
                   elm_gengrid_item_sorted_insert(tb->grid, ic, e,
                   _entry_cmp_grid_alpha_desc, NULL, NULL);
             else if (tb->sort == EPHOTO_SORT_MODTIME_ASCENDING)
               e->item =
                   elm_gengrid_item_sorted_insert(tb->grid, ic, e,
                   _entry_cmp_grid_mod_asc, NULL, NULL);
             else if (tb->sort == EPHOTO_SORT_MODTIME_DESCENDING)
               e->item =
                   elm_gengrid_item_sorted_insert(tb->grid, ic, e,
                   _entry_cmp_grid_mod_desc, NULL, NULL);
             if (e->item)
               {
                  Eina_File *f;
                  elm_object_item_data_set(e->item, e);
                  tb->totimages++;
                  f = eina_file_open(e->path, EINA_FALSE);
                  tb->totsize += (double) eina_file_size_get(f);
                  eina_file_close(f);
                  tb->searchentries = eina_list_append(tb->searchentries, e);
               }
             else
               {
                  ephoto_entry_free(tb->ephoto, e);
               }
          }
        tb->entries = tb->searchentries;
        _ephoto_thumb_update_info_label(tb);
        eina_list_free(results);
     }
   else
     {
        tb->totimages = 0;
        tb->totsize = 0;
        _ephoto_thumb_update_info_label(tb);
        tb->searchentries = NULL;
        tb->entries = NULL;
     }
}

static void
_ephoto_thumb_search_cancel(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *search = data;
   if (!search)
     return;
   Evas_Object *hbox = evas_object_data_get(search, "parent");
   Ephoto_Thumb_Browser *tb = evas_object_data_get(search, "thumb_browser");

   tb->entries = tb->ephoto->entries;
   if (tb->ephoto->searchentries)
      eina_list_free(tb->ephoto->searchentries);
   if (tb->searchentries)
      eina_list_free(tb->searchentries);
   tb->ephoto->searchentries = NULL;
   tb->searchentries = NULL;
   if (tb->original_grid)
     {
        elm_gengrid_clear(tb->grid);
        elm_box_unpack(tb->gridbox, tb->grid);
        evas_object_del(tb->grid);
        tb->grid = tb->original_grid;
        elm_box_pack_end(tb->gridbox, tb->grid);
        evas_object_show(tb->grid);
        tb->original_grid = NULL;
        tb->totimages = tb->totimages_old;
        tb->totsize = tb->totsize_old;
     }
   if (!tb->ephoto->entries)
     {
        tb->totimages = 0;
        tb->totsize = 0;
     }
   elm_object_focus_set(tb->main, EINA_TRUE);
   evas_object_del(tb->search);
   tb->search = NULL;
   elm_box_unpack(tb->gridbox, hbox);
   evas_object_del(hbox);
   tb->searching = 0;
   _ephoto_thumb_update_info_label(tb);
   tb->totimages_old = 0;
   tb->totsize_old = 0;
}

static void
_ephoto_thumb_search_start(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Object *hbox, *search;

   if (tb->processing)
     return;
   if (!tb->searching)
     tb->searching = 1;
   else
     {
        _ephoto_thumb_search_cancel(tb->search, NULL, NULL);
        return;
     }
   hbox = elm_box_add(tb->gridbox);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_start(tb->gridbox, hbox);
   evas_object_show(hbox);

   search = elm_entry_add(hbox);
   elm_entry_single_line_set(search, EINA_TRUE);
   elm_entry_scrollable_set(search, EINA_TRUE);
   elm_object_part_text_set(search, "guide", _("Search"));
   elm_scroller_policy_set(search, ELM_SCROLLER_POLICY_OFF,
       ELM_SCROLLER_POLICY_OFF);
   evas_object_size_hint_weight_set(search, EVAS_HINT_EXPAND,
       EVAS_HINT_FILL);
   evas_object_size_hint_align_set(search, EVAS_HINT_FILL,
       EVAS_HINT_FILL);
   evas_object_data_set(search, "thumb_browser", tb);
   evas_object_data_set(search, "parent", hbox);
   evas_object_smart_callback_add(search, "activated",
       _ephoto_thumb_search_go, search);
   elm_box_pack_end(hbox, search);
   evas_object_show(search);

   tb->search = search;
   tb->totimages_old = tb->totimages;
   tb->totsize_old = tb->totsize;

   elm_object_focus_set(search, EINA_TRUE);
}

void _ephoto_thumb_pane_add(Ephoto_Thumb_Browser *tb)
{
   Evas_Object *hbox, *but, *ic;
   int ret;

   tb->table = elm_table_add(tb->panes);
   evas_object_size_hint_weight_set(tb->table, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(tb->table);

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
   elm_table_pack(tb->table, tb->nolabel, 0, 0, 5, 1);
   evas_object_show(tb->nolabel);

   tb->gridbox = elm_box_add(tb->table);
   elm_box_horizontal_set(tb->gridbox, EINA_FALSE);
   evas_object_size_hint_weight_set(tb->gridbox, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->gridbox, EVAS_HINT_FILL,
       EVAS_HINT_FILL);
   evas_object_show(tb->gridbox);

   tb->grid = elm_gengrid_add(tb->gridbox);
   evas_object_size_hint_weight_set(tb->grid, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->grid, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_gengrid_align_set(tb->grid, 0.5, 0.0);
   elm_gengrid_multi_select_set(tb->grid, EINA_TRUE);
   elm_gengrid_multi_select_mode_set(tb->grid,
       ELM_OBJECT_MULTI_SELECT_MODE_DEFAULT);
   elm_scroller_bounce_set(tb->grid, EINA_FALSE, EINA_TRUE);
   evas_object_smart_callback_add(tb->grid, "activated",
       _ephoto_thumb_activated, tb);
   evas_object_event_callback_add(tb->grid, EVAS_CALLBACK_MOUSE_UP,
       _grid_mouse_up_cb, tb);
   evas_object_event_callback_add(tb->grid, EVAS_CALLBACK_MOUSE_WHEEL,
       _grid_mouse_wheel, tb);
   elm_drag_item_container_add(tb->grid, ANIM_TIME, DRAG_TIMEOUT,
       _dnd_item_get, _dnd_item_data_get);
   evas_object_data_set(tb->grid, "thumb_browser", tb);
   elm_box_pack_end(tb->gridbox, tb->grid);
   evas_object_show(tb->grid);
   elm_table_pack(tb->table, tb->gridbox, 0, 0, 5, 1);

   _ephoto_thumb_zoom_set(tb, tb->ephoto->config->thumb_size);

   hbox = elm_box_add(tb->main);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(tb->main, hbox);
   evas_object_show(hbox);

   tb->toggle = elm_check_add(hbox);
   elm_object_style_set(tb->toggle, "toggle");
   elm_object_part_text_set(tb->toggle, "on", _("Show Folders"));
   elm_object_part_text_set(tb->toggle, "off", _("Hide Folders"));
   if (!tb->ephoto->config->fsel_hide)
     {
        elm_check_state_set(tb->toggle, EINA_FALSE);
        evas_object_smart_callback_add(tb->toggle, "changed",
            _ephoto_dir_hide_folders, tb);
     }
   else
     {
        elm_check_state_set(tb->toggle, EINA_TRUE);
        evas_object_smart_callback_add(tb->toggle, "changed",
            _ephoto_dir_show_folders, tb);
     }
   elm_box_pack_end(hbox, tb->toggle);
   evas_object_show(tb->toggle);

   tb->infolabel = elm_label_add(hbox);
   elm_label_line_wrap_set(tb->infolabel, ELM_WRAP_WORD);
   elm_object_text_set(tb->infolabel, "Info Label");
   evas_object_size_hint_weight_set(tb->infolabel, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(tb->infolabel, EVAS_HINT_FILL,
       EVAS_HINT_FILL);
   evas_object_size_hint_aspect_set(tb->infolabel, EVAS_ASPECT_CONTROL_HORIZONTAL, 1, 1);
   elm_box_pack_end(hbox, tb->infolabel);
   evas_object_show(tb->infolabel);

   ic = elm_icon_add(hbox);
   evas_object_size_hint_min_set(ic, 20, 20);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   ret = elm_icon_standard_set(ic, "zoom-in");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   but = elm_button_add(hbox);
   if (!ret)
     elm_object_text_set(but, _("Zoom In"));
   else
     {
        elm_object_part_content_set(but, "icon", ic);
        elm_object_tooltip_text_set(but, _("Zoom In"));
        elm_object_tooltip_orient_set(but, ELM_TOOLTIP_ORIENT_TOP);
     }
   evas_object_smart_callback_add(but, "clicked", _zoom_in, tb);
   elm_box_pack_end(hbox, but);
   evas_object_show(but);

   ic = elm_icon_add(hbox);
   evas_object_size_hint_min_set(ic, 20, 20);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   elm_icon_standard_set(ic, "zoom-out");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   but = elm_button_add(hbox);
   if (!ret)
     elm_object_text_set(but, _("Zoom Out"));
   else
     {
        elm_object_part_content_set(but, "icon", ic);
        elm_object_tooltip_text_set(but, _("Zoom Out"));
        elm_object_tooltip_orient_set(but, ELM_TOOLTIP_ORIENT_TOP);
     }
   evas_object_smart_callback_add(but, "clicked", _zoom_out, tb);
   elm_box_pack_end(hbox, but);
   evas_object_show(but);

   tb->hover = elm_hoversel_add(hbox);
   elm_hoversel_hover_parent_set(tb->hover, tb->ephoto->win);
   elm_hoversel_item_add(tb->hover, _("Alphabetical Ascending"),
       "view-sort-ascending", ELM_ICON_STANDARD, _sort_alpha_asc, tb);
   elm_hoversel_item_add(tb->hover, _("Alphabetical Descending"),
       "view-sort-descending", ELM_ICON_STANDARD, _sort_alpha_desc, tb);
   elm_hoversel_item_add(tb->hover, _("Modification Time Ascending"),
       "view-sort-ascending", ELM_ICON_STANDARD, _sort_mod_asc, tb);
   elm_hoversel_item_add(tb->hover, _("Modification Time Descending"),
       "view-sort-descending", ELM_ICON_STANDARD, _sort_mod_desc, tb);
   elm_object_text_set(tb->hover, _("Sort"));
   ic = elm_icon_add(tb->hover);
   evas_object_size_hint_min_set(ic, 20, 20);
   elm_icon_standard_set(ic, "view-sort-ascending");
   elm_object_part_content_set(tb->hover, "icon", ic);
   evas_object_show(ic);
   elm_box_pack_end(hbox, tb->hover);
   evas_object_show(tb->hover);
}

/*Ephoto Populating Functions*/
static void
_todo_items_free(Ephoto_Thumb_Browser *tb)
{
   eina_list_free(tb->todo_items);
   tb->todo_items = NULL;
}

static void
_monitor_cb(void *data, Ecore_File_Monitor *em EINA_UNUSED,
    Ecore_File_Event event, const char *path)
{
   Elm_Object_Item *item;
   Ephoto_Entry *entry = data;
   Ephoto_Entry *e;
   char file[PATH_MAX], dir[PATH_MAX];
   const Elm_Genlist_Item_Class *ic;
   char buf[PATH_MAX];

   if (!entry)
     return;

   snprintf(file, PATH_MAX, "%s", path);
   snprintf(dir, PATH_MAX, "%s", ecore_file_dir_get(file));

   if (strcmp(entry->path, dir))
     return;
   if (event == ECORE_FILE_EVENT_CREATED_DIRECTORY)
     {
        if (!ecore_file_is_dir(path))
          return;
        if (ephoto_entry_exists(entry->ephoto, path))
          return;
 
        if (elm_genlist_item_type_get(entry->item) == ELM_GENLIST_ITEM_TREE &&
            elm_genlist_item_expanded_get(entry->item) == EINA_TRUE)
          {
             ic = &_ephoto_dir_class;
             snprintf(buf, PATH_MAX, "%s", path);
             e = ephoto_entry_new(entry->ephoto, path, basename(buf),
                 EINA_FILE_DIR);
             e->genlist = entry->genlist;
             e->parent = entry->item;
             e->item =
                 elm_genlist_item_sorted_insert(entry->genlist, ic, e,
                 e->parent, ELM_GENLIST_ITEM_NONE, _entry_cmp, NULL, NULL);
             if (e->item)
               e->monitor = ecore_file_monitor_add(e->path, _monitor_cb, e);
          }
        if (elm_genlist_item_type_get(entry->item) == ELM_GENLIST_ITEM_NONE)
          {
             Elm_Object_Item *parent;

             ic = &_ephoto_dir_class;
             parent =
                 elm_genlist_item_insert_before(entry->genlist, ic, entry,
                     entry->parent, entry->item, ELM_GENLIST_ITEM_TREE, NULL, NULL);
             entry->no_delete = EINA_TRUE;
             if (entry->monitor)
               ecore_file_monitor_del(entry->monitor);
             elm_object_item_del(entry->item);
             entry->item = parent;
             entry->no_delete = EINA_FALSE;
             entry->monitor = ecore_file_monitor_add(entry->path, _monitor_cb, entry);
          }
        return;
     }
   else if (event == ECORE_FILE_EVENT_DELETED_DIRECTORY)
     {
        item = elm_genlist_first_item_get(entry->genlist);
        while (item)
          {
             e = elm_object_item_data_get(item);
             if (!strcmp(e->path, path))
               {
                  elm_object_item_del(e->item);
                  //if (!strcmp(e->path, e->ephoto->config->directory))
                  break;
               }
             item = elm_genlist_item_next_get(item);
          }
        if (elm_genlist_item_type_get(entry->item) == ELM_GENLIST_ITEM_TREE &&
            _check_for_subdirs(entry) == EINA_FALSE)
          {
             Elm_Object_Item *parent;

             ic = &_ephoto_dir_class;
             parent =
                 elm_genlist_item_insert_before(entry->genlist, ic, entry,
                 entry->parent, entry->item, ELM_GENLIST_ITEM_NONE, NULL, NULL);
             entry->no_delete = EINA_TRUE;
             elm_object_item_del(entry->item);
             entry->item = parent;
             entry->no_delete = EINA_FALSE;
          }
        if (!ecore_file_exists(entry->ephoto->config->directory))
          {
             ephoto_directory_set(entry->ephoto, entry->path, entry->parent, 0, 1);
             ephoto_title_set(entry->ephoto, entry->path);
          }
        return;
     }
   else if (event == ECORE_FILE_EVENT_MODIFIED)
     {
        if (!ecore_file_is_dir(path))
          return;
        if ((elm_genlist_item_expanded_get(entry->item) == EINA_TRUE))
          {
             item = elm_genlist_first_item_get(entry->genlist);
             while (item)
               {
                  e = elm_object_item_data_get(item);
                  if (!strcmp(e->path, path))
                    {
                       elm_genlist_item_update(e->item);
                       break;
                    }
                  item = elm_genlist_item_next_get(item);
               }
          }
        return;
     }
}

static void
_top_monitor_cb(void *data, Ecore_File_Monitor *em EINA_UNUSED,
    Ecore_File_Event event, const char *path)
{
   Elm_Object_Item *item;
   Ephoto_Thumb_Browser *tb = data;
   Ephoto_Entry *e;
   const Elm_Genlist_Item_Class *ic;
   char buf[PATH_MAX], file[PATH_MAX], dir[PATH_MAX];

   if (!tb)
     return;
   snprintf(file, PATH_MAX, "%s", path);
   snprintf(dir, PATH_MAX, "%s", ecore_file_dir_get(file));

   if (strcmp(tb->ephoto->top_directory, dir))
     return;
   if (event == ECORE_FILE_EVENT_CREATED_DIRECTORY)
     {
       if (!ecore_file_is_dir(path))
         return; 
        if (ephoto_entry_exists(tb->ephoto, path))
          return;
        snprintf(buf, PATH_MAX, "%s", path);
        e = ephoto_entry_new(tb->ephoto, path, basename(buf),
            EINA_FILE_DIR);
        e->genlist = tb->fsel;
        ic = &_ephoto_dir_class;
        e->item =
            elm_genlist_item_append(tb->fsel, ic, e,
            NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);
        if (e->item)
          e->monitor = ecore_file_monitor_add(e->path, _monitor_cb, e);
        return;
     }
   else if (event == ECORE_FILE_EVENT_DELETED_DIRECTORY)
     {
        item = elm_genlist_first_item_get(tb->fsel);
        while (item)
          {
             e = elm_object_item_data_get(item);
             if (!strcmp(e->path, path))
               {
                  if (!strcmp(path, tb->ephoto->config->directory))
                    _dir_go_up(tb, NULL, NULL);
                  else
                    elm_object_item_del(e->item);
                  break;
               }
             item = elm_genlist_item_next_get(item);
          }
        return;
     }
   else if (event == ECORE_FILE_EVENT_MODIFIED)
     {
        if (!ecore_file_is_dir(path))
          return;
        item = elm_genlist_first_item_get(tb->fsel);
        while (item)
          {
             e = elm_object_item_data_get(item);
             if (!strcmp(e->path, path))
               {
                  elm_genlist_item_update(e->item);
                  break;
               }
             item = elm_genlist_item_next_get(item);
          }
        return;
     }
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
        tb->animator.todo_items = NULL;
        tb->processing = 0;
	return EINA_FALSE;
     }
   if ((tb->ls) && (eina_list_count(tb->todo_items) < TODO_ITEM_MIN_BATCH))
      return EINA_TRUE;

   tb->animator.todo_items = NULL;
   tb->processing = 1;
   EINA_LIST_FREE(tb->todo_items, entry)
   {
      i++;
      if (i > TODO_ITEM_MIN_BATCH)
	 return EINA_TRUE;
      if (entry->is_dir && !entry->item)
        {
	   const Elm_Genlist_Item_Class *ic;

	   ic = &_ephoto_dir_class;
	   if (_check_for_subdirs(entry))
             entry->item =
                 elm_genlist_item_sorted_insert(tb->fsel, ic, entry,
                 entry->parent, ELM_GENLIST_ITEM_TREE, _entry_cmp, NULL, NULL);
           else
             entry->item =
                 elm_genlist_item_sorted_insert(tb->fsel, ic, entry,
                 entry->parent, ELM_GENLIST_ITEM_NONE, _entry_cmp, NULL, NULL);
	   if (!entry->item)
	     {
		ephoto_entry_free(tb->ephoto, entry);
	     }
           else
             {
               entry->monitor = ecore_file_monitor_add(entry->path, _monitor_cb, entry);
               entry->genlist = tb->fsel;
             }
        }
      else if (!entry->is_dir && !entry->item)
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

static Eina_Bool
_ephoto_thumb_populate_start(void *data, int type EINA_UNUSED,
    void *event EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   evas_object_smart_callback_call(tb->main, "changed,directory", NULL);

   tb->animator.processed = 0;
   tb->animator.count = 0;
   if (tb->ephoto->selentries)
     eina_list_free(tb->ephoto->selentries);
   if (tb->searching)
     _ephoto_thumb_search_cancel(tb->search, NULL, NULL);
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
        tb->totimages = 0;
        tb->totsize = 0;
     }
   _ephoto_thumb_update_info_label(tb);
   tb->dirs_only = 0;
   tb->thumbs_only = 0;
   tb->entries = tb->ephoto->entries;

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ephoto_thumb_populate_error(void *data, int type EINA_UNUSED,
    void *event EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

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
   if (!e->is_dir)
     {
	Eina_File *f;

	tb->totimages += 1;
	f = eina_file_open(e->path, EINA_FALSE);
        e->size = eina_file_size_get(f);
	tb->totsize += (double) e->size;
	eina_file_close(f);
	tb->todo_items = eina_list_append(tb->todo_items, e);
	tb->animator.count++;
     }
   else if (e->is_dir)
     {
	tb->todo_items = eina_list_append(tb->todo_items, e);
	tb->animator.count++;
     }
   if (!tb->animator.todo_items)
      tb->animator.todo_items = ecore_animator_add(_todo_items_process, tb);

   return ECORE_CALLBACK_PASS_ON;
}

/*Ephoto Thumb Browser Main Callbacks*/
static void
_ephoto_show_slideshow(void *data, Evas_Object *obj EINA_UNUSED,
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
      entry = eina_list_nth(tb->entries, 0);
   if (!entry)
      return;
   selected =
       eina_list_clone(elm_gengrid_selected_items_get(tb->grid));
   if (eina_list_count(selected) <= 1  && tb->searchentries)
     {
        if (tb->ephoto->selentries)
          eina_list_free(tb->ephoto->selentries);
        tb->ephoto->selentries = NULL;
        tb->ephoto->searchentries = eina_list_clone(tb->searchentries);
     }
   else if (eina_list_count(selected) > 1)
     {
        EINA_LIST_FOREACH(selected, s, item)
          {
             tb->ephoto->selentries = eina_list_append(tb->ephoto->selentries,
                 elm_object_item_data_get(item));
          }
     }
   else
     {
        if (tb->ephoto->selentries)
          eina_list_free(tb->ephoto->selentries);
        if (tb->ephoto->searchentries)
          eina_list_free(tb->ephoto->searchentries);
        tb->ephoto->selentries = NULL;
        tb->ephoto->searchentries = NULL;
     }
   evas_object_smart_callback_call(tb->main, "slideshow", entry);
   if (selected)
     eina_list_free(selected);
}

static void
_ephoto_show_settings(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   ephoto_config_main(tb->ephoto);
}

static void
_ephoto_main_key_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Event_Key_Down *ev = event_info;
   Eina_Bool ctrl = evas_key_modifier_is_set(ev->modifiers, "Control");
   Eina_Bool shift = evas_key_modifier_is_set(ev->modifiers, "Shift");
   Eina_List *selected =
       eina_list_clone(elm_gengrid_selected_items_get(tb->grid));
   const char *k = ev->keyname;

   if (ctrl)
     {
        if (shift)
          {
             if (!strcasecmp(k, "f"))
               {
                  if (!elm_check_state_get(tb->toggle))
                    _ephoto_dir_hide_folders(tb, NULL, NULL);
                  else
                    _ephoto_dir_show_folders(tb, NULL, NULL);
               }
          }
	else if ((!strcasecmp(k, "plus")) || (!strcasecmp(k, "equal")))
	  {
	     int zoom = tb->ephoto->config->thumb_size + ZOOM_STEP;

	     _ephoto_thumb_zoom_set(tb, zoom);
	  }
	else if ((!strcasecmp(k, "minus")) || (!strcasecmp(k, "underscore")))
	  {
	     int zoom = tb->ephoto->config->thumb_size - ZOOM_STEP;

	     _ephoto_thumb_zoom_set(tb, zoom);
	  }
	else if (!strcasecmp(k, "Tab"))
	  {
	     Elm_Object_Item *it = elm_gengrid_selected_item_get(tb->grid);
	     Ephoto_Entry *entry;
             Eina_List *s;
             Elm_Object_Item *item;

	     if (it)
		entry = elm_object_item_data_get(it);
	     else
		entry = eina_list_nth(tb->entries, 0);
             if (eina_list_count(selected) <= 1 && tb->searchentries)
               {
                  if (tb->ephoto->selentries)
                    eina_list_free(tb->ephoto->selentries);
                  tb->ephoto->selentries = NULL;
                  tb->ephoto->searchentries =
                      eina_list_clone(tb->searchentries);
               }
             else if (eina_list_count(selected) > 1)
               {
                  EINA_LIST_FOREACH(selected, s, item)
                    {
                       tb->ephoto->selentries =
                           eina_list_append(tb->ephoto->selentries,
                           elm_object_item_data_get(item));
                    }
               }
             else
               {
                  if (tb->ephoto->selentries)
                    eina_list_free(tb->ephoto->selentries);
                  if (tb->ephoto->searchentries)
                    eina_list_free(tb->ephoto->searchentries);
                  tb->ephoto->selentries = NULL;
                  tb->ephoto->searchentries = NULL;
               }
	     if (entry)
               {
		   evas_object_smart_callback_call(tb->main, "view", entry);
               }
	  }
        else if (!strcasecmp(k, "c"))
          {
             _grid_menu_copy_cb(tb, NULL, NULL);
          }
        else if (!strcasecmp(k, "x"))
          {
             _grid_menu_cut_cb(tb, NULL, NULL);
          }
        else if (!strcasecmp(k, "v"))
          {
             _grid_menu_paste_cb(tb, NULL, NULL);
          }
        else if (!strcasecmp(k, "a"))
          {
             _grid_menu_select_all_cb(tb, NULL, NULL);
          }
        else if (!strcasecmp(k, "f") && !tb->processing)
          {
             if (tb->searching)
               _ephoto_thumb_search_cancel(tb->search, NULL, NULL);
             else
               _ephoto_thumb_search_start(tb, NULL, NULL);
          }
        else if (!strcasecmp(k, "Delete"))
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
                       _menu_empty_cb(tb, NULL, NULL);
                       free(trash);
                       return;
                    }
               }
             else
               _grid_menu_delete_cb(tb, NULL, NULL);
             free(trash);
          }
     }
   else if (!strcasecmp(k, "F1"))
     {
        _ephoto_show_settings(tb, NULL, NULL);
     }
   else if (!strcasecmp(k, "F2"))
     {
        Elm_Object_Item *it = NULL;

        it = eina_list_data_get(
            eina_list_last(selected));
        if (it)
          {
             evas_object_data_set(it, "thumb_browser", tb);
             _grid_menu_rename_cb(it, NULL, NULL);
          }
     }
   else if (!strcasecmp(k, "F5"))
     {
	Elm_Object_Item *it = elm_gengrid_selected_item_get(tb->grid);
	Ephoto_Entry *entry;
        Eina_List *s;
        Elm_Object_Item *item;

	if (it)
	   entry = elm_object_item_data_get(it);
	else
	   entry = eina_list_nth(tb->entries, 0);
        if (eina_list_count(selected) <= 1 && tb->searchentries)
          {
             if (tb->ephoto->selentries)
               eina_list_free(tb->ephoto->selentries);
             tb->ephoto->selentries = NULL;
             tb->ephoto->searchentries = eina_list_clone(tb->searchentries);
          }
        else if (eina_list_count(selected) > 1)
          {
             EINA_LIST_FOREACH(selected, s, item)
               {
                  tb->ephoto->selentries =
                      eina_list_append(tb->ephoto->selentries,
                      elm_object_item_data_get(item));
               }
          }
        else
          {
             if (tb->ephoto->selentries)
               eina_list_free(tb->ephoto->selentries);
             if (tb->ephoto->searchentries)
               eina_list_free(tb->ephoto->searchentries);
             tb->ephoto->selentries = NULL;
             tb->ephoto->searchentries = NULL;
          }
	if (entry)
	   evas_object_smart_callback_call(tb->main, "slideshow", entry);
     }
   else if (!strcasecmp(k, "F11"))
     {
	Evas_Object *win = tb->ephoto->win;

	elm_win_fullscreen_set(win, !elm_win_fullscreen_get(win));
     }
   else if (!strcasecmp(k, "Escape"))
     {
        if (tb->searching)
          _ephoto_thumb_search_cancel(tb->search, NULL, NULL);
        else
          _grid_menu_clear_cb(tb, NULL, NULL);
     }
   else if (ev->compose && (((ev->compose[0] != '\\')
       && (ev->compose[0] >= ' ')) || ev->compose[1]))
     {
        if (elm_object_focus_get(tb->direntry))
          return;
        if (!tb->searching)
          {
             _ephoto_thumb_search_start(tb, NULL, NULL);
             elm_entry_entry_append(tb->search, ev->compose);
             elm_entry_cursor_end_set(tb->search);
          }
        else if (!elm_object_focus_get(tb->search))
          {
             elm_object_focus_set(tb->search, EINA_TRUE);
             elm_entry_entry_append(tb->search, ev->compose);
             elm_entry_cursor_end_set(tb->search);
          }
        _ephoto_thumb_search_go(tb->search, NULL, NULL);
     }
   else if (tb->searching && ((!strcasecmp(k, "Backspace")) ||
       !strcasecmp(k, "Delete")))
     {
        _ephoto_thumb_search_go(tb->search, NULL, NULL);
     }
   if (selected)
     eina_list_free(selected);
}

static void _ephoto_panes_unpress(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   if (!tb->ephoto->config->fsel_hide)
     tb->ephoto->config->lpane_size = elm_panes_content_left_size_get(tb->panes);
}

static void _ephoto_panes_double_clicked(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   double tmp_size = 0.0;

   tmp_size = elm_panes_content_left_size_get(tb->panes);

   if (tmp_size > 0)
     {
        _ephoto_dir_hide_folders(tb, NULL, NULL);
     }
   else
     {
        _ephoto_dir_show_folders(tb, NULL, NULL);
     }
}

static void
_ephoto_main_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
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
   if (tb->cut_items)
      eina_list_free(tb->cut_items);
   else if (tb->copy_items)
      eina_list_free(tb->copy_items);
   if (tb->ephoto->selentries)
      eina_list_free(tb->ephoto->selentries);
   if (tb->ephoto->searchentries)
      eina_list_free(tb->ephoto->searchentries);
   if (tb->searchentries)
      eina_list_free(tb->searchentries);
   if (tb->monitor)
     ecore_file_monitor_del(tb->monitor);
   free(tb);
}


/*Ephoto Thumb Browser Public Functions*/
void
ephoto_thumb_browser_insert(Ephoto *ephoto, Ephoto_Entry *entry)
{
   Ephoto_Thumb_Browser *tb =
       evas_object_data_get(ephoto->thumb_browser, "thumb_browser");

   if (!entry->is_dir && !entry->item)
     {
        Eina_File *f;
        const Elm_Gengrid_Item_Class *ic;

        tb->totimages += 1;
        f = eina_file_open(entry->path, EINA_FALSE);
        entry->size = eina_file_size_get(f);
        tb->totsize += (double) entry->size;
        eina_file_close(f);

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
        _ephoto_thumb_update_info_label(tb);
     }
}

void
ephoto_thumb_browser_remove(Ephoto *ephoto, Ephoto_Entry *entry)
{
    Ephoto_Thumb_Browser *tb =
       evas_object_data_get(ephoto->thumb_browser, "thumb_browser");

   if (!entry->is_dir)
     {
        tb->totimages -= 1;
        tb->totsize -= entry->size;

        if (eina_list_count(tb->ephoto->entries) == 1)
          {
             tb->totimages = 0;
             tb->totsize = 0;
          }
        _ephoto_thumb_update_info_label(tb);
        elm_object_item_del(entry->item);
     }
}

void
ephoto_thumb_browser_update(Ephoto *ephoto, Ephoto_Entry *entry)
{
   Ephoto_Thumb_Browser *tb =
      evas_object_data_get(ephoto->thumb_browser, "thumb_browser");

   if (!entry->is_dir)
     {
        Eina_File *f;

        tb->totsize -= entry->size;

        f = eina_file_open(entry->path, EINA_FALSE);
        entry->size = eina_file_size_get(f);
        tb->totsize += (double) entry->size;
        eina_file_close(f);

        elm_gengrid_item_update(entry->item);
        tb->totsize += entry->size;
        _ephoto_thumb_update_info_label(tb);
     }
}

void
ephoto_thumb_browser_top_dir_set(Ephoto *ephoto, const char *dir)
{
   Ephoto_Thumb_Browser *tb =
       evas_object_data_get(ephoto->thumb_browser, "thumb_browser");

   if (tb->monitor)
     ecore_file_monitor_del(tb->monitor);
   if (ephoto->top_directory)
     eina_stringshare_replace(&ephoto->top_directory, dir);
   else
     ephoto->top_directory = eina_stringshare_add(dir);
   tb->monitor = ecore_file_monitor_add(dir, _top_monitor_cb, tb);
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
   Ephoto_Thumb_Browser *tb;

   EINA_SAFETY_ON_NULL_RETURN_VAL(box, NULL);

   tb = calloc(1, sizeof(Ephoto_Thumb_Browser));
   EINA_SAFETY_ON_NULL_GOTO(tb, error);

   _ephoto_thumb_file_class.item_style = "thumb";
   _ephoto_thumb_file_class.func.text_get = _thumb_item_text_get;
   _ephoto_thumb_file_class.func.content_get = _thumb_file_icon_get;
   _ephoto_thumb_file_class.func.state_get = NULL;
   _ephoto_thumb_file_class.func.del = _thumb_item_del;

   _ephoto_dir_class.item_style = "tree_effect";
   _ephoto_dir_class.func.text_get = _dir_item_text_get;
   _ephoto_dir_class.func.content_get = _dir_item_icon_get;
   _ephoto_dir_class.func.state_get = NULL;
   _ephoto_dir_class.func.del = _dir_item_del;

   tb->ephoto = ephoto;
   tb->thumbs_only = 0;
   tb->dirs_only = 0;
   tb->dragging = 0;
   tb->searching = 0;
   tb->cut_items = NULL;
   tb->copy_items = NULL;
   tb->dir_current = NULL;
   tb->change_dir_job = NULL;
   tb->last_sel = NULL;
   tb->sort = EPHOTO_SORT_ALPHABETICAL_ASCENDING;
   tb->main = box;

   elm_box_horizontal_set(tb->main, EINA_FALSE);
   evas_object_size_hint_weight_set(tb->main, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->main, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_event_callback_add(tb->main, EVAS_CALLBACK_DEL,
       _ephoto_main_del, tb);
   evas_object_event_callback_add(tb->main, EVAS_CALLBACK_KEY_DOWN,
       _ephoto_main_key_down, tb);
   evas_object_data_set(tb->main, "thumb_browser", tb);

   tb->panes = elm_panes_add(tb->main);
   evas_object_size_hint_weight_set(tb->panes, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->panes, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_panes_content_left_min_size_set(tb->panes, 100);
   elm_panes_content_left_size_set(tb->panes, tb->ephoto->config->lpane_size);
   evas_object_smart_callback_add(tb->panes, "clicked,double",
       _ephoto_panes_double_clicked, tb);
   evas_object_smart_callback_add(tb->panes, "unpress",
       _ephoto_panes_unpress, tb);
   elm_box_pack_end(tb->main, tb->panes);
   evas_object_show(tb->panes);

   _ephoto_file_pane_add(tb);
   elm_object_part_content_set(tb->panes, "left", tb->leftbox);
   if (!tb->ephoto->config->fsel_hide)
     {
        evas_object_show(tb->leftbox);
     }
   else
     {
        evas_object_hide(tb->leftbox);
        elm_panes_content_left_min_size_set(tb->panes, 0);
        elm_panes_content_left_size_set(tb->panes, 0.0);
     }

   _ephoto_thumb_pane_add(tb);
   elm_object_part_content_set(tb->panes, "right", tb->table);

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

