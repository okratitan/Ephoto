#include "ephoto.h"
#include <elm_interface_scrollable.h>

#define ZOOM_MAX            512
#define ZOOM_MIN            128
#define ZOOM_STEP           32

#define FILESEP             "file://"
#define FILESEP_LEN         sizeof(FILESEP) - 1

#define TODO_ITEM_MIN_BATCH 5

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
   Evas_Object *gridbox;
   Evas_Object *grid;
   Evas_Object *original_grid;
   Evas_Object *nolabel;
   Evas_Object *search;
   Evas_Object *menu;
   Evas_Object *hover;
   Elm_Object_Item *similarity;
   Elm_Object_Item *last_sel;
   Ephoto_Sort sort;
   Eio_File *ls;
   Eina_Bool dirs_only;
   Eina_Bool thumbs_only;
   Eina_List *cut_items;
   Eina_List *copy_items;
   Eina_List *handlers;
   Eina_List *todo_items;
   Eina_List *entries;
   Eina_List *searchentries;
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

/*Main Callbacks*/
static void _ephoto_show_settings(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED);
static void _ephoto_main_key_down(void *data, Evas *e EINA_UNUSED,
    Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED);
static void _ephoto_main_del(void *data, Evas *e EINA_UNUSED,
    Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED);

/*Thumb Pane Functions*/
static void _ephoto_thumb_activated(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info);
static void _ephoto_thumb_zoom_set(Ephoto_Thumb_Browser *tb, int zoom);
static void _ephoto_thumb_search_go(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED);
static void _ephoto_thumb_search_cancel(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED);
static void _ephoto_thumb_search_start(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED);
static char * _drag_data_extract(char **drag_data);

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
   if (eina_list_count(paths) <= 0)
     return;
   ephoto_file_empty_trash(tb->ephoto, paths);
}

static Eina_Bool
_5s_timeout_gone(void *data)
{
   elm_drag_cancel(data);
   _5s_timeout = NULL;

   return ECORE_CALLBACK_CANCEL;
}

static Eina_Bool
_drop_dropcb(void *data EINA_UNUSED, Evas_Object *obj, Elm_Object_Item *it EINA_UNUSED,
    Elm_Selection_Data *ev, int xposret EINA_UNUSED, int yposret EINA_UNUSED)
{
   Eina_List *files = NULL;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(obj, "thumb_browser");
   const char *path = tb->ephoto->config->directory;
   char *dir;

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
        dir = ecore_file_dir_get(s);
        if (!strcmp(path, dir))
          {
             free(dir);
             break;
          }
        if (evas_object_image_extension_can_load_get(basename(s)))
          files = eina_list_append(files, s);
        free(dir);
        s = _drag_data_extract(&dd);
     }
   free(dd);

   if (eina_list_count(files) <= 0)
     return EINA_TRUE;
   if (tb->ephoto->config->move_drop)
     ephoto_file_move(tb->ephoto, files, path);
   else
     ephoto_file_copy(tb->ephoto, files, path);
   return EINA_TRUE;
}

static Elm_Object_Item *
_drop_item_getcb(Evas_Object *obj EINA_UNUSED, Evas_Coord x EINA_UNUSED,
    Evas_Coord y EINA_UNUSED,int *xposret EINA_UNUSED, int *yposret EINA_UNUSED)
{
   return NULL;
}

static void
_drop_enter(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED)
{
   return;
}

static void
_drop_leave(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED)
{
   return;
}

static void
_drop_pos(void *data EINA_UNUSED, Evas_Object *cont EINA_UNUSED,
    Elm_Object_Item *it EINA_UNUSED, Evas_Coord x EINA_UNUSED,
    Evas_Coord y EINA_UNUSED, int xposret EINA_UNUSED,
    int yposret EINA_UNUSED, Elm_Xdnd_Action action EINA_UNUSED)
{
   return;
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

static void
_dnd_drag_start(void *data EINA_UNUSED, Evas_Object *obj)
{
   Ephoto_Thumb_Browser *tb = evas_object_data_get(obj, "thumb_browser");

   if (_5s_cancel)
      _5s_timeout = ecore_timer_add(5.0, _5s_timeout_gone, obj);
   elm_object_cursor_set(tb->main, ELM_CURSOR_HAND2);

   ephoto_show_folders(tb->ephoto, EINA_FALSE);
   
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
   if (eina_list_count(data))
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
        thumb = ephoto_thumb_add(e->ephoto, obj, e);
        evas_object_show(thumb);
     }
   return thumb;
}

static void
_thumb_item_del(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED)
{
   /*The entry is already freed when changing directories*/
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

static int
_entry_cmp_grid_similarity(const void *pa, const void *pb)
{
   const Ephoto_Entry *a, *b;

   a = elm_object_item_data_get(pa);
   b = elm_object_item_data_get(pb);

   if (!a->sort_id || !b->sort_id)
     return 0;
   else
     return strcmp(a->sort_id, b->sort_id);
}

static void
_sort_alpha_asc(void *data, Evas_Object *obj,
    void *event_data EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Object *ic;

   tb->sort = EPHOTO_SORT_ALPHABETICAL_ASCENDING;
   tb->ephoto->sort = tb->sort;
   tb->thumbs_only = 1;
   tb->dirs_only = 0;
   ic = elm_icon_add(obj);
   elm_icon_standard_set(ic, "view-sort-ascending");
   elm_object_part_content_set(obj, "icon", ic);
   evas_object_show(ic);
   ephoto_directory_set(tb->ephoto, tb->ephoto->config->directory,
       NULL, tb->dirs_only, tb->thumbs_only);
   ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);
}

static void
_sort_alpha_desc(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Object *ic;

   tb->sort = EPHOTO_SORT_ALPHABETICAL_DESCENDING;
   tb->ephoto->sort = tb->sort;
   tb->thumbs_only = 1;
   tb->dirs_only = 0;
   ic = elm_icon_add(obj);
   elm_icon_standard_set(ic, "view-sort-descending");
   elm_object_part_content_set(obj, "icon", ic);
   evas_object_show(ic);
   ephoto_directory_set(tb->ephoto, tb->ephoto->config->directory,
       NULL, tb->dirs_only, tb->thumbs_only);
   ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);
}

static void
_sort_mod_asc(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Object *ic;

   tb->sort = EPHOTO_SORT_MODTIME_ASCENDING;
   tb->ephoto->sort = tb->sort;
   tb->thumbs_only = 1;
   tb->dirs_only = 0;
   ic = elm_icon_add(obj);
   elm_icon_standard_set(ic, "view-sort-ascending");
   elm_object_part_content_set(obj, "icon", ic);
   evas_object_show(ic);
   ephoto_directory_set(tb->ephoto, tb->ephoto->config->directory,
       NULL, tb->dirs_only, tb->thumbs_only);
   ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);
}

static void
_sort_mod_desc(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Object *ic;

   tb->sort = EPHOTO_SORT_MODTIME_DESCENDING;
   tb->ephoto->sort = tb->sort;
   tb->thumbs_only = 1;
   tb->dirs_only = 0;
   ic = elm_icon_add(obj);
   elm_icon_standard_set(ic, "view-sort-descending");
   elm_object_part_content_set(obj, "icon", ic);
   evas_object_show(ic);
   ephoto_directory_set(tb->ephoto, tb->ephoto->config->directory,
       NULL, tb->dirs_only, tb->thumbs_only);
   ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);
}

static void
_sort_similarity(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Object *ic;

   tb->sort = EPHOTO_SORT_SIMILARITY;
   tb->ephoto->sort = EPHOTO_SORT_SIMILARITY;
   tb->thumbs_only = 1;
   tb->dirs_only = 0;
   ic = elm_icon_add(obj);
   elm_icon_standard_set(ic, "view-sort-ascending");
   elm_object_part_content_set(obj, "icon", ic);
   evas_object_show(ic);
   ephoto_directory_set(tb->ephoto, tb->ephoto->config->directory,
       NULL, tb->dirs_only, tb->thumbs_only);
   ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);
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
_view_single(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Elm_Object_Item *it = elm_gengrid_selected_item_get(tb->grid);
   Ephoto_Entry *entry;
   Eina_List *selected, *s;
   Elm_Object_Item *item;

   if (it)
     entry = elm_object_item_data_get(it);
   else
     entry = eina_list_nth(tb->entries, 0);
   selected =
       eina_list_clone(elm_gengrid_selected_items_get(tb->grid));
   if (eina_list_count(selected) <= 1 && tb->searchentries)
     {
        if (eina_list_count(tb->ephoto->selentries))
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
        if (eina_list_count(tb->ephoto->selentries))
          eina_list_free(tb->ephoto->selentries);
        if (eina_list_count(tb->ephoto->searchentries))
          eina_list_free(tb->ephoto->searchentries);
        tb->ephoto->selentries = NULL;
        tb->ephoto->searchentries = NULL;
     }
   if (entry)
     {
        evas_object_smart_callback_call(tb->main, "view", entry);
     }
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
   if (eina_list_count(selection))
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

   if (eina_list_count(tb->cut_items))
     {
	eina_list_free(tb->cut_items);
	tb->cut_items = NULL;
     }
   if (eina_list_count(tb->copy_items))
     {
	eina_list_free(tb->copy_items);
	tb->copy_items = NULL;
     }
   EINA_LIST_FOREACH(selection, f, item)
     {
        file = elm_object_item_data_get(item);
        tb->copy_items = eina_list_append(tb->copy_items, strdup(file->path));
     }
   if (eina_list_count(selection))
     eina_list_free(selection);
}

static void
_grid_menu_paste_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   if (eina_list_count(tb->cut_items))
     {
        ephoto_file_paste(tb->ephoto, eina_list_clone(tb->cut_items), EINA_FALSE,
            tb->ephoto->config->directory);
        eina_list_free(tb->cut_items);
        tb->cut_items = NULL;
     }
   else if (eina_list_count(tb->copy_items))
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
   if (eina_list_count(selection))
     eina_list_free(selection);
}

static void
_grid_mouse_up_cb(void *data, Evas *e EINA_UNUSED,
    Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Object *menu;
   Elm_Object_Item *item;
   Evas_Event_Mouse_Up *info = event_info;
   Eina_Bool ctrl = evas_key_modifier_is_set(info->modifiers, "Control");
   Eina_Bool shift = evas_key_modifier_is_set(info->modifiers, "Shift");
   Eina_Bool clear_selection = EINA_FALSE;
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
             clear_selection = EINA_TRUE;
             tb->last_sel = item;
          }
     }
   if (info->button == 1 && !item)
     clear_selection = EINA_TRUE;

   else if (info->button == 3 && item)
     {
        if (!elm_gengrid_item_selected_get(item))
          clear_selection = EINA_TRUE;
     }
   if (clear_selection)
     {
        Eina_List *sel = eina_list_clone(selected); 
        Elm_Object_Item *it;
        if (eina_list_count(sel) > 0)
          {
             EINA_LIST_FREE(sel, it)
               {
                  elm_gengrid_item_selected_set(it, EINA_FALSE);
               }
          }
     }
   if (info->button != 3)
      return;

   if (!elm_gengrid_first_item_get(tb->grid) && !tb->cut_items)
     {
        if (!tb->copy_items)
          return;
     }

   snprintf(trash, PATH_MAX, "%s/.config/ephoto/trash", getenv("HOME"));

   if (item)
     {
        elm_gengrid_item_selected_set(item, EINA_TRUE);
        tb->last_sel = item;
     }

   selected = elm_gengrid_selected_items_get(tb->grid);

   menu = elm_menu_add(tb->ephoto->win);
   elm_menu_move(menu, x, y);

   if (elm_gengrid_first_item_get(tb->grid))
     {
        elm_menu_item_add(menu, NULL, "system-search", _("Search"),
            _ephoto_thumb_search_start, tb);
        elm_menu_item_add(menu, NULL, "edit-select-all", _("Select All"),
            _grid_menu_select_all_cb, tb);
        if (eina_list_count(selected) || item)
          elm_menu_item_add(menu, NULL, "edit-clear", _("Select None"),
              _grid_menu_clear_cb, tb);
        if (item)
          {
             elm_menu_item_add(menu, NULL, "edit", _("Rename"),
                  _grid_menu_rename_cb, item);
             evas_object_data_set(item, "thumb_browser", tb);
          }
        if (eina_list_count(selected))
          {
             elm_menu_item_add(menu, NULL, "edit-cut", _("Cut"),
                 _grid_menu_cut_cb, tb);
             elm_menu_item_add(menu, NULL, "edit-copy", _("Copy"),
                 _grid_menu_copy_cb, tb);
          }
     }
   if (tb->cut_items || tb->copy_items)
     {
        elm_menu_item_add(menu, NULL, "edit-paste", _("Paste"),
            _grid_menu_paste_cb, tb);
     }
   if (elm_gengrid_first_item_get(tb->grid))
     {
        if (!strcmp(tb->ephoto->config->directory, trash))
          {
	     elm_menu_item_add(menu, NULL, "edit-delete", _("Empty Trash"),
                 _menu_empty_cb, tb);
          }
        else
          {
             elm_menu_item_add(menu, NULL, "edit-delete", _("Delete"),
                 _grid_menu_delete_cb, tb);
          }
     }
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
_grid_changed(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Coord w, h, gw, gh;
   Edje_Message_Int_Set *msg;

   if (tb->ephoto->state != EPHOTO_STATE_THUMB)
     return;

   elm_scroller_region_get(tb->grid, 0, 0, &w, &h);
   evas_object_geometry_get(tb->grid, 0, 0, &gw, &gh);
   gw -= w;
   gh -= h;
   msg = alloca(sizeof(Edje_Message_Int_Set) + (2 * sizeof(int)));
   msg->count = 2;
   msg->val[0] = gw;
   msg->val[1] = gh;
   edje_object_message_send(elm_layout_edje_get(tb->ephoto->layout),
       EDJE_MESSAGE_INT_SET, 1, msg);
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
        if (eina_list_count(tb->ephoto->selentries))
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
        if (eina_list_count(tb->ephoto->selentries))
          eina_list_free(tb->ephoto->selentries);
        if (eina_list_count(tb->ephoto->searchentries))
          eina_list_free(tb->ephoto->searchentries);
        tb->ephoto->selentries = NULL;
        tb->ephoto->searchentries = NULL;
     }
   evas_object_smart_callback_call(tb->main, "view", e);
   if (eina_list_count(selected))
     eina_list_free(selected);
}

/*Thumb Pane Functions*/
void
ephoto_thumb_browser_update_info_label(Ephoto *ephoto)
{
   char buf[PATH_MAX];
   char isize[PATH_MAX];
   char image_info[PATH_MAX];
   double totsize;
   Ephoto_Thumb_Browser *tb = evas_object_data_get(ephoto->thumb_browser,
       "thumb_browser");

   if (!tb->totimages)
     {
        if (tb->searching)
          elm_object_text_set(tb->nolabel,
              _("No images matched your search"));
        else
          elm_object_text_set(tb->nolabel,
              _("There are no images in this directory"));
        snprintf(buf, PATH_MAX, "<b>%s:</b> 0 %s        <b>%s:</b> 0%s",
            _("Total"), ngettext("image", "images", 0), _("Size"),
        ngettext("B", "B", 0));
        elm_object_text_set(tb->ephoto->infolabel, buf);
     }
   else
     {
        if (tb->nolabel)
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
        elm_object_text_set(tb->ephoto->infolabel, image_info);
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
        ephoto_thumb_browser_clear(tb->ephoto);
        elm_box_unpack(tb->gridbox, tb->grid);
        evas_object_del(tb->grid);
        tb->grid = tb->original_grid;
        elm_box_pack_end(tb->gridbox, tb->grid);
        evas_object_show(tb->grid);
        next = elm_gengrid_first_item_get(tb->grid);
     }
   snprintf(pattern, PATH_MAX, "*%s*", search_text);
   EINA_LIST_FREE(sel, o)
     {
        elm_gengrid_item_selected_set(o, EINA_FALSE);
     }
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
   if (eina_list_count(tb->searchentries))
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
             else if (tb->sort == EPHOTO_SORT_SIMILARITY)
               e->item =
                   elm_gengrid_item_sorted_insert(tb->grid, ic, e,
                   _entry_cmp_grid_similarity, NULL, NULL);
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
        ephoto_thumb_browser_update_info_label(tb->ephoto);
        if (eina_list_count(results))
          eina_list_free(results);
     }
   else
     {
        tb->totimages = 0;
        tb->totsize = 0;
        ephoto_thumb_browser_update_info_label(tb->ephoto);
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
   if (eina_list_count(tb->ephoto->searchentries))
      eina_list_free(tb->ephoto->searchentries);
   if (eina_list_count(tb->searchentries))
      eina_list_free(tb->searchentries);
   tb->ephoto->searchentries = NULL;
   tb->searchentries = NULL;
   if (tb->original_grid)
     {
        ephoto_thumb_browser_clear(tb->ephoto);
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
   ephoto_thumb_browser_update_info_label(tb->ephoto);
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

static void
_ephoto_thumb_view_add(Ephoto_Thumb_Browser *tb)
{
   tb->table = elm_table_add(tb->main);
   evas_object_size_hint_weight_set(tb->table, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(tb->main, tb->table);
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
   elm_drop_item_container_add(tb->grid, ELM_SEL_FORMAT_TARGETS,
       _drop_item_getcb, _drop_enter, tb, _drop_leave, tb, _drop_pos, tb,
       _drop_dropcb, NULL);
   evas_object_data_set(tb->grid, "thumb_browser", tb);
   elm_box_pack_end(tb->gridbox, tb->grid);
   evas_object_smart_callback_add(tb->grid, "changed", _grid_changed, tb);
   evas_object_show(tb->grid);
   elm_table_pack(tb->table, tb->gridbox, 0, 0, 5, 1);

   _ephoto_thumb_zoom_set(tb, tb->ephoto->config->thumb_size);
}

/*Ephoto Populating Functions*/
static void
_todo_items_free(Ephoto_Thumb_Browser *tb)
{
   if (eina_list_count(tb->todo_items))
     eina_list_free(tb->todo_items);
   tb->todo_items = NULL;
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
      else if (!entry->is_dir && !entry->item)
        {
	   const Elm_Gengrid_Item_Class *ic;

           entry->gengrid = tb->grid;

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
           else if (tb->sort == EPHOTO_SORT_SIMILARITY)
             entry->thumb =
                 ephoto_thumb_add(entry->ephoto, tb->grid, entry);

	   if (entry->item)
	     {
		elm_object_item_data_set(entry->item, entry);

             }
           else if (tb->sort != EPHOTO_SORT_SIMILARITY)
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

   if (tb->dirs_only)
     return ECORE_CALLBACK_PASS_ON;

   tb->animator.processed = 0;
   tb->animator.count = 0;
   if (eina_list_count(tb->ephoto->selentries))
     eina_list_free(tb->ephoto->selentries);
   if (tb->searching)
     _ephoto_thumb_search_cancel(tb->search, NULL, NULL);
   _todo_items_free(tb);
   ephoto_thumb_browser_clear(tb->ephoto);
   tb->totimages = 0;
   tb->totsize = 0;

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ephoto_thumb_populate_end(void *data, int type EINA_UNUSED,
    void *event EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   if (tb->dirs_only)
     return ECORE_CALLBACK_PASS_ON;

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
   if (tb->ephoto->state == EPHOTO_STATE_THUMB)
     {
        evas_object_smart_callback_call(tb->main, "changed,directory", NULL);        
        ephoto_thumb_browser_update_info_label(tb->ephoto);
     }
   tb->entries = tb->ephoto->entries;
   ephoto_single_browser_entries_set(tb->ephoto->single_browser,
                 tb->ephoto->entries);
   if (eina_list_count(tb->entries) < 1 && tb->ephoto->config->folders)
     {
        ephoto_show_folders(tb->ephoto, EINA_FALSE);
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

   if (tb->dirs_only)
     return ECORE_CALLBACK_PASS_ON;

   tb->thumbs_only = 0;
   tb->dirs_only = 0;

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ephoto_thumb_entry_create(void *data, int type EINA_UNUSED, void *event)
{
   Ephoto_Thumb_Browser *tb = data;
   Ephoto_Event_Entry_Create *ev = event;
   Ephoto_Entry *e;

   if (tb->dirs_only)
     return ECORE_CALLBACK_PASS_ON;

   e = ev->entry;
   if (!e->is_dir && !ecore_file_is_dir(ecore_file_realpath(e->path)))
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
   if (!tb->animator.todo_items)
      tb->animator.todo_items = ecore_animator_add(_todo_items_process, tb);

   return ECORE_CALLBACK_PASS_ON;
}

/*Ephoto Thumb Browser Main Callbacks*/
void
ephoto_thumb_browser_slideshow(Evas_Object *obj)
{
   Ephoto_Thumb_Browser *tb = evas_object_data_get(obj, "thumb_browser");
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
        if (eina_list_count(tb->ephoto->selentries))
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
        if (eina_list_count(tb->ephoto->selentries))
          eina_list_free(tb->ephoto->selentries);
        if (eina_list_count(tb->ephoto->searchentries))
          eina_list_free(tb->ephoto->searchentries);
        tb->ephoto->selentries = NULL;
        tb->ephoto->searchentries = NULL;
     }
   evas_object_smart_callback_call(tb->main, "slideshow", entry);
   if (eina_list_count(selected))
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
        if ((!strcasecmp(k, "plus")) || (!strcasecmp(k, "equal")))
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
	     _view_single(tb, NULL, NULL);
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
             if (shift)
               ephoto_show_folders(tb->ephoto, EINA_TRUE);
             else
               {
                  if (tb->searching)
                    _ephoto_thumb_search_cancel(tb->search, NULL, NULL);
                  else
                    _ephoto_thumb_search_start(tb, NULL, NULL);
               }
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
             if (eina_list_count(tb->ephoto->selentries))
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
             if (eina_list_count(tb->ephoto->selentries))
               eina_list_free(tb->ephoto->selentries);
             if (eina_list_count(tb->ephoto->searchentries))
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
   if (eina_list_count(selected))
     eina_list_free(selected);
}

static void
_hover_dismissed_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   tb->ephoto->hover_blocking = EINA_FALSE;
}

static void
_hover_expand_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   tb->ephoto->hover_blocking = EINA_TRUE;
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
   if (eina_list_count(tb->cut_items))
      eina_list_free(tb->cut_items);
   else if (eina_list_count(tb->copy_items))
      eina_list_free(tb->copy_items);
   if (eina_list_count(tb->ephoto->selentries))
      eina_list_free(tb->ephoto->selentries);
   if (eina_list_count(tb->ephoto->searchentries))
      eina_list_free(tb->ephoto->searchentries);
   if (eina_list_count(tb->searchentries))
      eina_list_free(tb->searchentries);
   free(tb);
}


/*Ephoto Thumb Browser Public Functions*/
void 
ephoto_thumb_browser_dirs_only_set(Ephoto *ephoto, Eina_Bool dirs_only)
{
   Ephoto_Thumb_Browser *tb =
       evas_object_data_get(ephoto->thumb_browser, "thumb_browser");

   tb->dirs_only = dirs_only;
}

void
ephoto_thumb_browser_clear(Ephoto *ephoto)
{
   Ephoto_Thumb_Browser *tb =
       evas_object_data_get(ephoto->thumb_browser, "thumb_browser");

   elm_gengrid_clear(tb->grid);
}  

void
ephoto_thumb_browser_paste(Ephoto *ephoto, Elm_Object_Item *item)
{
   Ephoto_Thumb_Browser *tb =
       evas_object_data_get(ephoto->thumb_browser, "thumb_browser");
   Ephoto_Entry *entry;
   const char *path;

   if (item)
     {
        entry = elm_object_item_data_get(item);
        path = entry->path;
     }
   else
     path = tb->ephoto->config->directory;

   if (eina_list_count(tb->cut_items))
     {
        ephoto_file_paste(tb->ephoto, eina_list_clone(tb->cut_items), EINA_FALSE, path);
        eina_list_free(tb->cut_items);
        tb->cut_items = NULL;
     }
   else if (eina_list_count(tb->copy_items))
     {
        ephoto_file_paste(tb->ephoto, eina_list_clone(tb->copy_items), EINA_TRUE, path);
        eina_list_free(tb->copy_items);
        tb->copy_items = NULL;
     }
}

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

        entry->gengrid = tb->grid;

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
        else if (tb->sort == EPHOTO_SORT_SIMILARITY)
          entry->item =
              elm_gengrid_item_sorted_insert(tb->grid, ic, entry,
              _entry_cmp_grid_similarity, NULL, NULL);
        if (entry->item)
          {
             elm_object_item_data_set(entry->item, entry);
          }
        else
          {
             ephoto_entry_free(tb->ephoto, entry);
          }
        ephoto_thumb_browser_update_info_label(tb->ephoto);
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
        ephoto_thumb_browser_update_info_label(tb->ephoto);
        elm_object_item_del(entry->item);
        ephoto_entry_free(tb->ephoto, entry);
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
        ephoto_thumb_browser_update_info_label(tb->ephoto);
     }
}

void
ephoto_thumb_browser_show_controls(Ephoto *ephoto)
{
   Ephoto_Thumb_Browser *tb = evas_object_data_get(ephoto->thumb_browser,
       "thumb_browser");
   Evas_Object *but, *ic, *hover;
   int ret;

   ic = elm_icon_add(ephoto->controls_left);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   ret = elm_icon_standard_set(ic, "view-list-details");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   if (!ret)
     ret = elm_image_file_set(ic, PACKAGE_DATA_DIR "/images/single.png", NULL);

   but = elm_button_add(ephoto->controls_left);
   if (!ret)
     elm_object_text_set(but, _("View Images"));
   elm_object_part_content_set(but, "icon", ic);
   elm_object_tooltip_text_set(but, _("View Images"));
   elm_object_tooltip_orient_set(but, ELM_TOOLTIP_ORIENT_TOP);
   evas_object_smart_callback_add(but, "clicked", _view_single, tb);
   elm_box_pack_end(ephoto->controls_left, but);
   evas_object_show(but);

   ic = elm_icon_add(ephoto->controls_left);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   ret = elm_icon_standard_set(ic, "zoom-in");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   
   but = elm_button_add(ephoto->controls_left);
   if (!ret)
     elm_object_text_set(but, _("Zoom In"));
   elm_object_part_content_set(but, "icon", ic);
   elm_object_tooltip_text_set(but, _("Zoom In"));
   elm_object_tooltip_orient_set(but, ELM_TOOLTIP_ORIENT_TOP);
   evas_object_smart_callback_add(but, "clicked", _zoom_in, tb);
   elm_box_pack_end(ephoto->controls_left, but);
   evas_object_show(but);

   ic = elm_icon_add(ephoto->controls_left);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   elm_icon_standard_set(ic, "zoom-out");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   
   but = elm_button_add(ephoto->controls_left);
   if (!ret)
     elm_object_text_set(but, _("Zoom Out"));
   elm_object_part_content_set(but, "icon", ic);
   elm_object_tooltip_text_set(but, _("Zoom Out"));
   elm_object_tooltip_orient_set(but, ELM_TOOLTIP_ORIENT_TOP);
   evas_object_smart_callback_add(but, "clicked", _zoom_out, tb);
   elm_box_pack_end(ephoto->controls_left, but);
   evas_object_show(but);

   hover = elm_hoversel_add(ephoto->controls_right);
   elm_hoversel_hover_parent_set(hover, ephoto->win);
   elm_hoversel_item_add(hover, _("Alphabetical Ascending"),
       "view-sort-ascending", ELM_ICON_STANDARD, _sort_alpha_asc, tb);
   elm_hoversel_item_add(hover, _("Alphabetical Descending"),
       "view-sort-descending", ELM_ICON_STANDARD, _sort_alpha_desc, tb);
   elm_hoversel_item_add(hover, _("Modification Time Ascending"),
       "view-sort-ascending", ELM_ICON_STANDARD, _sort_mod_asc, tb);
   elm_hoversel_item_add(hover, _("Modification Time Descending"),
       "view-sort-descending", ELM_ICON_STANDARD, _sort_mod_desc, tb);
   tb->similarity = elm_hoversel_item_add(hover, _("Image Simalarity"),
       "view-sort-ascending", ELM_ICON_STANDARD, _sort_similarity, tb);
   elm_object_text_set(hover, _("Sort"));
   ic = elm_icon_add(hover);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   elm_icon_standard_set(ic, "view-sort-ascending");
   elm_object_part_content_set(hover, "icon", ic);
   evas_object_show(ic);
   elm_object_tooltip_text_set(hover, _("Sort"));
   elm_object_tooltip_orient_set(hover, ELM_TOOLTIP_ORIENT_TOP);
   evas_object_smart_callback_add(hover, "expanded", _hover_expand_cb, tb);
   evas_object_smart_callback_add(hover, "dismissed", _hover_dismissed_cb, tb);
   elm_box_pack_end(ephoto->controls_right, hover);
   evas_object_show(hover);
   tb->hover = hover;
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

   tb->ephoto = ephoto;
   tb->dragging = 0;
   tb->searching = 0;
   tb->cut_items = NULL;
   tb->copy_items = NULL;
   tb->last_sel = NULL;
   tb->entries = NULL;
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

   _ephoto_thumb_view_add(tb);
   elm_box_pack_end(tb->main, tb->table);

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

