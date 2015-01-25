#include "ephoto.h"

#define ZOOM_MAX 512
#define ZOOM_MIN 128
#define ZOOM_STEP 32

#define TODO_ITEM_MIN_BATCH 16

#define PARENT_DIR "Up"

#define FILESEP "file://"
#define FILESEP_LEN sizeof(FILESEP) - 1

#define DRAG_TIMEOUT 0.3
#define ANIM_TIME 0.2

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
   Evas_Object *vbar;
   Evas_Object *fsel;
   Evas_Object *leftbox;
   Evas_Object *bleftbox;
   Eio_File *ls;
   Eina_List *todo_items;
   Eina_List *grid_items;
   Eina_List *dir_items;
   Eina_List *handlers;
   int totimages;
   double totsize;
   struct {
      Ecore_Animator *todo_items;
   } animator;
   Eina_Bool main_deleted : 1;
};

static Elm_Gengrid_Item_Class _ephoto_thumb_file_class;
static Elm_Genlist_Item_Class _ephoto_dir_class;

static void
_todo_items_free(Ephoto_Thumb_Browser *tb)
{
   eina_list_free(tb->todo_items);
   tb->todo_items = NULL;
}

static void
_dir_items_free(Ephoto_Thumb_Browser *tb)
{
   eina_list_free(tb->dir_items);
   tb->dir_items = NULL;
}

static void
_grid_items_free(Ephoto_Thumb_Browser *tb)
{
   eina_list_free(tb->grid_items);
   tb->grid_items = NULL;
}

static char *
_ephoto_dir_item_text_get(void *data, Evas_Object *obj EINA_UNUSED, const char *part EINA_UNUSED)
{
   Ephoto_Entry *e = data;
   return strdup(e->label);
}

static char *
_ephoto_thumb_item_text_get(void *data, Evas_Object *obj EINA_UNUSED, const char *part EINA_UNUSED)
{
   Ephoto_Entry *e = data;
   return strdup(e->label);
}

static Evas_Object *
_ephoto_dir_item_icon_get(void *data EINA_UNUSED, Evas_Object *obj, const char *part)
{
   if (!strcmp(part, "elm.swallow.end"))
     return NULL;
   Evas_Object *ic = elm_icon_add(obj);
   elm_icon_standard_set(ic, "stock_folder");
   return ic;
}

static Evas_Object *
_ephoto_thumb_file_icon_get(void *data, Evas_Object *obj, const char *part EINA_UNUSED)
{
   Ephoto_Entry *e = data;
   return ephoto_thumb_add(e->ephoto, obj, e->path);
}

static void
_ephoto_dir_item_del(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED)
{
}

static void
_ephoto_thumb_item_del(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED)
{
   /* FIXME: the entry is already freed when changing directories
    * One solution is to take care of this cleaning when manually removing
    * some grid items
   Ephoto_Entry *e = data;
   e->item = NULL;
   */
}

static int
_entry_cmp(const void *pa, const void *pb)
{
   const Elm_Object_Item *ia = pa;
   const Ephoto_Entry *a, *b = pb;

   a = elm_object_item_data_get(ia);

   return strcoll(a->basename, b->basename);
}

static void
_entry_dir_item_add(Ephoto_Thumb_Browser *tb, Ephoto_Entry *e)
{
   const Elm_Genlist_Item_Class *ic;
   int near_cmp;
   Elm_Object_Item *near_item = NULL;
   Eina_List *near_node = NULL;

   near_node = eina_list_search_sorted_near_list
     (tb->dir_items, _entry_cmp, e, &near_cmp);
   if (near_node)
     near_item = near_node->data;

   ic = &_ephoto_dir_class;

   if (!near_item)
     {
        e->item = elm_genlist_item_append(tb->fsel, ic, e, NULL, ELM_GENLIST_ITEM_NONE, NULL, NULL);
        tb->dir_items = eina_list_append(tb->dir_items, e->item);
     }
   else
     {
        if (near_cmp < 0)
          {
             e->item = elm_genlist_item_insert_after
               (tb->fsel, ic, e, NULL, near_item, ELM_GENLIST_ITEM_NONE, NULL, NULL);
             tb->dir_items = eina_list_append_relative
               (tb->dir_items, e->item, near_item);
          }
        else
          {
             e->item = elm_genlist_item_insert_before
               (tb->fsel, ic, e, NULL, near_item, ELM_GENLIST_ITEM_NONE, NULL, NULL);
             tb->dir_items = eina_list_prepend_relative
               (tb->dir_items, e->item, near_item);
          }
     }
   if (e->item)
     elm_object_item_data_set(e->item, e);
   else
     {
        ERR("could not add item to fsel: path '%s'", e->path);
        ephoto_entry_free(e);
        return;
     }
}

static void
_entry_thumb_item_add(Ephoto_Thumb_Browser *tb, Ephoto_Entry *e)
{
   const Elm_Gengrid_Item_Class *ic;
   int near_cmp;
   Elm_Object_Item *near_item = NULL;
   Eina_List *near_node = NULL;

   near_node = eina_list_search_sorted_near_list
     (tb->grid_items, _entry_cmp, e, &near_cmp);
   if (near_node)
     near_item = near_node->data;

   ic = &_ephoto_thumb_file_class;

   if (!near_item)
     {
        e->item = elm_gengrid_item_append(tb->grid, ic, e, NULL, NULL);
        tb->grid_items = eina_list_append(tb->grid_items, e->item);
     }
   else
     {
        if (near_cmp < 0)
          {
             e->item = elm_gengrid_item_insert_after
               (tb->grid, ic, e, near_item, NULL, NULL);
             tb->grid_items = eina_list_append_relative
               (tb->grid_items, e->item, near_item);
          }
        else
          {
             e->item = elm_gengrid_item_insert_before
               (tb->grid, ic, e, near_item, NULL, NULL);
             tb->grid_items = eina_list_prepend_relative
               (tb->grid_items, e->item, near_item);
          }
     }
   if (e->item)
     elm_object_item_data_set(e->item, e);
   else
     {
        ERR("could not add item to grid: path '%s'", e->path);
        ephoto_entry_free(e);
        return;
     }
}

static Eina_Bool
_todo_items_process(void *data)
{
   Ephoto_Thumb_Browser *tb = data;
   Ephoto_Entry *entry;

   if ((tb->ls) && (eina_list_count(tb->todo_items) < TODO_ITEM_MIN_BATCH))
     return EINA_TRUE;

   tb->animator.todo_items = NULL;

   EINA_LIST_FREE(tb->todo_items, entry)
     {
        if (entry->is_dir)
          _entry_dir_item_add(tb, entry);
        else
          _entry_thumb_item_add(tb, entry);
     }

   return EINA_FALSE;
}

static void
_ephoto_dir_selected(void *data, Evas_Object *o EINA_UNUSED, void *event_info)
{
   Ephoto_Thumb_Browser *tb = data;
   Elm_Object_Item *it = event_info;
   Ephoto_Entry *e = elm_object_item_data_get(it);

   ephoto_directory_set(tb->ephoto, e->path);
   ephoto_title_set(tb->ephoto, e->path);
}

static void
_ephoto_dir_go_home(void *data, Evas_Object *o EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   ephoto_directory_set(tb->ephoto, getenv("HOME"));
   ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);
}

static void
_ephoto_dir_go_up(void *data, Evas_Object *o EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   if (strcmp(tb->ephoto->config->directory, "/"))
     {
        char path[PATH_MAX];
        snprintf(path, PATH_MAX, "%s", tb->ephoto->config->directory);
        ephoto_directory_set(tb->ephoto, dirname(path));
        ephoto_title_set(tb->ephoto, tb->ephoto->config->directory);
     }
}

static void
_ephoto_thumb_selected(void *data, Evas_Object *o EINA_UNUSED, void *event_info)
{
   Ephoto_Thumb_Browser *tb = data;
   Elm_Object_Item *it = event_info;
   Ephoto_Entry *e = elm_object_item_data_get(it);

   elm_gengrid_item_selected_set(it, EINA_FALSE);

   evas_object_smart_callback_call(tb->main, "view", e);
}

static void
_zoom_set(Ephoto_Thumb_Browser *tb, int zoom)
{
   double scale = elm_config_scale_get();

   if (zoom > ZOOM_MAX) zoom = ZOOM_MAX;
   else if (zoom < ZOOM_MIN) zoom = ZOOM_MIN;

   ephoto_thumb_size_set(tb->ephoto, zoom);
   elm_gengrid_item_size_set(tb->grid, zoom * scale, zoom * scale);
}

static void
_zoom_in(void *data, Evas_Object *o, void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Object *min = evas_object_data_get(o, "min");
   int zoom = tb->ephoto->config->thumb_size + ZOOM_STEP;
   _zoom_set(tb, zoom);
   if (zoom >= ZOOM_MAX) elm_object_disabled_set(o, EINA_TRUE);
   if (zoom > ZOOM_MIN) elm_object_disabled_set(min, EINA_FALSE);
}

static void
_zoom_out(void *data, Evas_Object *o, void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Object *max = evas_object_data_get(o, "max");
   int zoom = tb->ephoto->config->thumb_size - ZOOM_STEP;
   _zoom_set(tb, zoom);
   if (zoom <= ZOOM_MIN) elm_object_disabled_set(o, EINA_TRUE);
   if (zoom < ZOOM_MAX) elm_object_disabled_set(max, EINA_FALSE);
}

static void
_slideshow(void *data, Evas_Object *o EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Elm_Object_Item *it = elm_gengrid_selected_item_get(tb->grid);
   Ephoto_Entry *entry;

   if (it) entry = elm_object_item_data_get(it);
   else entry = eina_list_nth(tb->ephoto->entries, 0);

   if (!entry) return;
   evas_object_smart_callback_call(tb->main, "slideshow", entry);
}

static void
_settings(void *data, Evas_Object *o EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   if (tb->ephoto)
     ephoto_config_window(tb->ephoto);
}

static void
_about(void *data, Evas_Object *o EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   if (tb->ephoto)
     ephoto_about_window(tb->ephoto);
}

static void
_ephoto_dir_show_folders(void *data, Evas_Object *o EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   elm_box_unpack(tb->main, tb->bleftbox);
   evas_object_del(tb->bleftbox);
   tb->bleftbox = NULL;
   tb->vbar = NULL;

   evas_object_show(tb->leftbox);
   elm_box_pack_start(tb->main, tb->leftbox);
}

static void
_ephoto_dir_hide_folders(void *data, Evas_Object *o EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Object *icon, *max, *min, *but, *ic;

   evas_object_hide(tb->leftbox);
   elm_box_unpack(tb->main, tb->leftbox);

   tb->bleftbox = elm_box_add(tb->main);
   evas_object_size_hint_weight_set(tb->bleftbox, 0.0, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->bleftbox, EVAS_HINT_FILL, EVAS_HINT_FILL);  
   elm_box_pack_start(tb->main, tb->bleftbox);
   evas_object_show(tb->bleftbox);

   tb->vbar = elm_toolbar_add(tb->main);
   elm_toolbar_horizontal_set(tb->vbar, EINA_FALSE);
   elm_toolbar_homogeneous_set(tb->vbar, EINA_TRUE);
   elm_toolbar_shrink_mode_set(tb->vbar, ELM_TOOLBAR_SHRINK_NONE);
   elm_toolbar_select_mode_set(tb->vbar, ELM_OBJECT_SELECT_MODE_NONE);
   elm_toolbar_align_set(tb->vbar, 0.0);
   evas_object_size_hint_weight_set(tb->vbar, 0.0, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->vbar, EVAS_HINT_FILL, EVAS_HINT_FILL);

   icon = elm_toolbar_item_append(tb->vbar, "zoom-in", "Zoom In", _zoom_in, tb);
   max = elm_object_item_widget_get(icon);
   icon = elm_toolbar_item_append(tb->vbar, "zoom-out", "Zoom Out", _zoom_out, tb);
   min = elm_object_item_widget_get(icon);
   evas_object_data_set(max, "min", min);
   evas_object_data_set(min, "max", max);
   elm_toolbar_item_append(tb->vbar, "stock_media-play", "Slideshow", _slideshow, tb);
   elm_toolbar_item_append(tb->vbar, "emblem-system", "Settings", _settings, tb);
   elm_toolbar_item_append(tb->vbar, "help-about", "About", _about, tb);

   elm_box_pack_end(tb->bleftbox, tb->vbar);
   evas_object_show(tb->vbar);

   ic = elm_icon_add(tb->bleftbox);
   elm_icon_standard_set(ic, "go-next");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   but = elm_button_add(tb->bleftbox);
   elm_object_part_content_set(but, "icon", ic);
   elm_object_tooltip_text_set(but, "Show the file selector");
   evas_object_size_hint_weight_set(but, 0.0, 0.0);
   evas_object_size_hint_align_set(but, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_callback_add(but, "clicked", _ephoto_dir_show_folders, tb);
   elm_box_pack_end(tb->bleftbox, but);
   evas_object_show(but);

}

static void
_key_down(void *data, Evas *e EINA_UNUSED, Evas_Object *o EINA_UNUSED, void *event_info)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Event_Key_Down *ev = event_info;
   const char *k = ev->keyname;

   if (!strcmp(k, "F5"))
     {
        Elm_Object_Item *it = elm_gengrid_selected_item_get(tb->grid);
        Ephoto_Entry *entry;
        if (it) entry = elm_object_item_data_get(it);
        else entry = eina_list_nth(tb->ephoto->entries, 0);

        if (entry)
          evas_object_smart_callback_call(tb->main, "slideshow", entry);
     }
   else if (!strcmp(k, "F11"))
     {
        Evas_Object *win = tb->ephoto->win;
        elm_win_fullscreen_set(win, !elm_win_fullscreen_get(win));
     }
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
   if (_5s_cancel)
     _5s_timeout = ecore_timer_add(5.0, _5s_timeout_gone, obj);
}

static void
_dnd_drag_done(void *data, Evas_Object *obj EINA_UNUSED, Eina_Bool doaccept EINA_UNUSED)
{
   if (_5s_cancel)
     {
        ecore_timer_del(_5s_timeout);
        _5s_timeout = NULL;
     }

   eina_list_free(data);
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

        drag_data = malloc(len + eina_list_count(*items) * (FILESEP_LEN + 1) + 1);
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
_dnd_create_icon(void *data, Evas_Object *win, Evas_Coord *xoff, Evas_Coord *yoff)
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
        if (xoff) *xoff = xm - (w/2);
        if (yoff) *yoff = ym - (h/2);
        icon = elm_icon_add(win);
        elm_image_file_set(icon, f, g);
        evas_object_size_hint_align_set(icon, EVAS_HINT_FILL, EVAS_HINT_FILL);
        evas_object_size_hint_weight_set(icon, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        if (xoff && yoff) evas_object_move(icon, *xoff, *yoff);
        evas_object_resize(icon, w, h);
     }

   return icon;
}

static Eina_List *
_dnd_icons_get(void *data)
{
   Eina_List *l;
   Eina_List *icons = NULL;
   Evas_Coord xm, ym;

   evas_pointer_canvas_xy_get(evas_object_evas_get(data), &xm, &ym);
   Eina_List *items = eina_list_clone(elm_gengrid_selected_items_get(data));
   Elm_Object_Item *gli = elm_gengrid_at_xy_item_get(data, xm, ym, NULL, NULL);
   if (gli)
     {
        void *p = eina_list_search_sorted(items, _entry_cmp, gli);
        if (!p)
          items = eina_list_append(items, gli);
     }

   EINA_LIST_FOREACH(items, l, gli)
     {
        Evas_Object *o = elm_object_item_part_content_get(gli, "elm.swallow.icon");

        if (o)
          {
             int x, y, w, h;
             const char *f, *g;
             elm_image_file_get(o, &f, &g);
             Evas_Object *ic = elm_icon_add(data);
             elm_image_file_set(ic, f, g);
             evas_object_geometry_get(o, &x, &y, &w, &h);
             evas_object_size_hint_align_set(ic, EVAS_HINT_FILL, EVAS_HINT_FILL);
             evas_object_size_hint_weight_set(ic, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
             evas_object_move(ic, x, y);
             evas_object_resize(ic, w, h);
             evas_object_show(ic);
             icons =  eina_list_append(icons, ic);
          }
     }

   eina_list_free(items);
   return icons;
}

static const char *
_dnd_get_drag_data(Evas_Object *obj, Elm_Object_Item *it, Eina_List **items)
{
   const char *drag_data = NULL;

   *items = eina_list_clone(elm_gengrid_selected_items_get(obj));
   if (it)
     {  
        void *p = eina_list_search_sorted(*items, _entry_cmp, it);
        if (!p)
          *items = eina_list_append(*items, it);
     }
   drag_data = _dnd_drag_data_build(items);

   return drag_data;
}

static Elm_Object_Item *
_dnd_item_get(Evas_Object *obj, Evas_Coord x, Evas_Coord y, int *xposret, int *yposret)
{
   Elm_Object_Item *item;
   item = elm_gengrid_at_xy_item_get(obj, x, y, xposret, yposret);
   return item;
}

static Eina_Bool
_dnd_item_data_get(Evas_Object *obj, Elm_Object_Item *it, Elm_Drag_User_Info *info)
{
   info->format = ELM_SEL_FORMAT_TARGETS;
   info->createicon = _dnd_create_icon;
   info->createdata = it;
   info->dragstart = _dnd_drag_start;
   info->icons = _dnd_icons_get(obj);
   info->dragdone = _dnd_drag_done;
   info->data = _dnd_get_drag_data(obj, it, (Eina_List **) &info->donecbdata);
   info->acceptdata = info->donecbdata;
   if (info->data)
     return EINA_TRUE;
   else
     return EINA_FALSE;
}

static void
_main_del(void *data, Evas *e EINA_UNUSED, Evas_Object *o EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;
   Ecore_Event_Handler *handler;

   _todo_items_free(tb);
   _dir_items_free(tb);
   _grid_items_free(tb);
   EINA_LIST_FREE(tb->handlers, handler)
      ecore_event_handler_del(handler);

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
   free(tb);
}

static Eina_Bool
_ephoto_thumb_populate_start(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Ephoto_Thumb_Browser *tb = data;

   evas_object_smart_callback_call(tb->main, "changed,directory", NULL);

   _todo_items_free(tb);
   _dir_items_free(tb);
   _grid_items_free(tb);
   elm_gengrid_clear(tb->grid);
   elm_genlist_clear(tb->fsel);
   tb->totimages = 0;
   tb->totsize = 0;

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ephoto_thumb_populate_end(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
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
        if (!tb->nolabel)
          {
             tb->nolabel = elm_label_add(tb->table);
             elm_label_line_wrap_set(tb->nolabel, ELM_WRAP_WORD);
             elm_object_text_set(tb->nolabel, "There are no images in this directory");
             evas_object_size_hint_weight_set(tb->nolabel, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
             evas_object_size_hint_align_set(tb->nolabel, EVAS_HINT_FILL, EVAS_HINT_FILL);
             elm_table_pack(tb->table, tb->nolabel, 0, 0, 4, 1);
             evas_object_show(tb->nolabel);
             elm_object_text_set(tb->infolabel, "<b>Total</b> 0 images        <b>Size:</b> 0 bytes");
          }
     }
   else
     {
        if (tb->nolabel)
          {
             elm_table_unpack(tb->table, tb->nolabel);
             evas_object_del(tb->nolabel);
             tb->nolabel = NULL;
          }
        char isize[PATH_MAX];
        char image_info[PATH_MAX];

        if (tb->totsize < 1024.0) snprintf(isize, sizeof(isize), "%'.0f bytes", tb->totsize);
        else
          {
             tb->totsize /= 1024.0;
             if (tb->totsize < 1024) snprintf(isize, sizeof(isize), "%'.0f KB", tb->totsize);
             else
               {
                  tb->totsize /= 1024.0;
                  if (tb->totsize < 1024) snprintf(isize, sizeof(isize), "%'.1f MB", tb->totsize);
                  else
                    {
                       tb->totsize /= 1024.0;
                       if (tb->totsize < 1024) snprintf(isize, sizeof(isize), "%'.1f GB", tb->totsize);
                       else
                         {
                            tb->totsize /= 1024.0;
                            snprintf(isize, sizeof(isize), "%'.1f TB", tb->totsize);
                         }
                    }
               }
          }
        snprintf(image_info, PATH_MAX, "<b>Total:</b> %d images        <b>Size:</b> %s", 
                 tb->totimages, isize);
        elm_object_text_set(tb->infolabel, image_info);
     }
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ephoto_thumb_populate_error(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ephoto_thumb_entry_create(void *data, int type EINA_UNUSED, void *event)
{
   Ephoto_Thumb_Browser *tb = data;
   Ephoto_Event_Entry_Create *ev = event;
   Ephoto_Entry *e;

   e = ev->entry;
   tb->todo_items = eina_list_append(tb->todo_items, e);

   if (!e->is_dir)
     {
        Eina_File *f;
        tb->totimages += 1;
        f = eina_file_open(e->path, EINA_FALSE);
        tb->totsize += (double)eina_file_size_get(f);
        eina_file_close(f);
     }

   if (!tb->animator.todo_items)
     tb->animator.todo_items = ecore_animator_add(_todo_items_process, tb);

   return ECORE_CALLBACK_PASS_ON;
}

Evas_Object *
ephoto_thumb_browser_add(Ephoto *ephoto, Evas_Object *parent)
{
   Evas_Object *box = elm_box_add(parent);
   Evas_Object *icon, *min, *max, *hbox, *botbox, *but, *sep, *ic;
   Evas_Coord w, h;
   Ephoto_Thumb_Browser *tb;

   EINA_SAFETY_ON_NULL_RETURN_VAL(box, NULL);

   tb = calloc(1, sizeof(Ephoto_Thumb_Browser));
   EINA_SAFETY_ON_NULL_GOTO(tb, error);

   _ephoto_thumb_file_class.item_style = "thumb";
   _ephoto_thumb_file_class.func.text_get = _ephoto_thumb_item_text_get;
   _ephoto_thumb_file_class.func.content_get = _ephoto_thumb_file_icon_get;
   _ephoto_thumb_file_class.func.state_get = NULL;
   _ephoto_thumb_file_class.func.del = _ephoto_thumb_item_del;

   _ephoto_dir_class.item_style = "default";
   _ephoto_dir_class.func.text_get = _ephoto_dir_item_text_get;
   _ephoto_dir_class.func.content_get = _ephoto_dir_item_icon_get;
   _ephoto_dir_class.func.state_get = NULL;
   _ephoto_dir_class.func.del = _ephoto_dir_item_del;

   tb->ephoto = ephoto;
   tb->main = box;
   elm_box_horizontal_set(tb->main, EINA_TRUE);
   evas_object_size_hint_weight_set(tb->main, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->main, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_event_callback_add(tb->main, EVAS_CALLBACK_DEL, _main_del, tb);
   evas_object_event_callback_add
     (tb->main, EVAS_CALLBACK_KEY_DOWN, _key_down, tb);
   evas_object_data_set(tb->main, "thumb_browser", tb);

   tb->leftbox = elm_box_add(tb->main);
   evas_object_size_hint_weight_set(tb->leftbox, 0.0, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->leftbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(tb->main, tb->leftbox);
   evas_object_show(tb->leftbox);

   tb->bar = elm_toolbar_add(tb->leftbox);
   elm_toolbar_horizontal_set(tb->bar, EINA_TRUE);
   elm_toolbar_homogeneous_set(tb->bar, EINA_TRUE);
   elm_toolbar_shrink_mode_set(tb->bar, ELM_TOOLBAR_SHRINK_NONE);
   elm_toolbar_select_mode_set(tb->bar, ELM_OBJECT_SELECT_MODE_NONE);
   evas_object_size_hint_weight_set(tb->bar, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(tb->bar, EVAS_HINT_FILL, EVAS_HINT_FILL);   

   icon = elm_toolbar_item_append(tb->bar, "zoom-in", "Zoom In", _zoom_in, tb);
   max = elm_object_item_widget_get(icon);
   icon = elm_toolbar_item_append(tb->bar, "zoom-out", "Zoom Out", _zoom_out, tb);
   min = elm_object_item_widget_get(icon);
   evas_object_data_set(max, "min", min);
   evas_object_data_set(min, "max", max);
   elm_toolbar_item_append(tb->bar, "stock_media-play", "Slideshow", _slideshow, tb);
   elm_toolbar_item_append(tb->bar, "emblem-system", "Settings", _settings, tb);
   elm_toolbar_item_append(tb->bar, "help-about", "About", _about, tb);

   elm_box_pack_end(tb->leftbox, tb->bar);
   evas_object_show(tb->bar);

   evas_object_size_hint_min_get(tb->bar, &w, 0);
   evas_object_size_hint_min_set(tb->leftbox, w, 0);

   sep = elm_separator_add(tb->leftbox);
   elm_separator_horizontal_set(sep, EINA_TRUE);
   evas_object_size_hint_min_set(sep, 0, 20);
   elm_box_pack_end(tb->leftbox, sep);
   evas_object_show(sep);

   hbox = elm_box_add(tb->leftbox);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(tb->leftbox, hbox);
   evas_object_show(hbox);

   ic = elm_icon_add(hbox);
   elm_icon_standard_set(ic, "go-up");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   but = elm_button_add(hbox);
   elm_object_part_content_set(but, "icon", ic);
   elm_object_text_set(but, "Up");
   evas_object_size_hint_weight_set(but, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(but, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_callback_add(but, "clicked", _ephoto_dir_go_up, tb);
   elm_box_pack_end(hbox, but);
   evas_object_show(but);

   ic = elm_icon_add(hbox);
   elm_icon_standard_set(ic, "go-home");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   but = elm_button_add(hbox);
   elm_object_part_content_set(but, "icon", ic);
   elm_object_text_set(but, "Home");
   evas_object_size_hint_weight_set(but, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(but, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_callback_add(but, "clicked", _ephoto_dir_go_home, tb);
   elm_box_pack_end(hbox, but);
   evas_object_show(but);

   tb->fsel = elm_genlist_add(tb->leftbox);
   evas_object_size_hint_weight_set(tb->fsel, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->fsel, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(tb->leftbox, tb->fsel);
   evas_object_smart_callback_add
     (tb->fsel, "clicked,double", _ephoto_dir_selected, tb);
   evas_object_show(tb->fsel);

   ic = elm_icon_add(hbox);
   elm_icon_standard_set(ic, "go-previous");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   but = elm_button_add(hbox);
   elm_object_part_content_set(but, "icon", ic);
   elm_object_tooltip_text_set(but, "Hide the file selector");
   evas_object_size_hint_weight_set(but, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(but, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_callback_add(but, "clicked", _ephoto_dir_hide_folders, tb);
   elm_box_pack_end(tb->leftbox, but);
   evas_object_show(but);
   evas_object_size_hint_min_get(but, 0, &h);
   tb->ephoto->bottom_bar_size = h;

   tb->table = elm_table_add(tb->main);
   evas_object_size_hint_weight_set(tb->table, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(tb->main, tb->table);
   evas_object_show(tb->table);

   tb->grid = elm_gengrid_add(tb->table);
   evas_object_size_hint_weight_set
     (tb->grid, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->grid, EVAS_HINT_FILL, EVAS_HINT_FILL);

   elm_gengrid_align_set(tb->grid, 0.5, 0.0);
   elm_gengrid_highlight_mode_set(tb->grid, EINA_FALSE);
   elm_scroller_bounce_set(tb->grid, EINA_FALSE, EINA_TRUE);
   evas_object_size_hint_align_set
     (tb->grid, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_weight_set
     (tb->grid, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);

   evas_object_smart_callback_add
     (tb->grid, "selected", _ephoto_thumb_selected, tb);
   elm_drag_item_container_add(tb->grid, ANIM_TIME, DRAG_TIMEOUT, _dnd_item_get, _dnd_item_data_get);

   _zoom_set(tb, tb->ephoto->config->thumb_size);

   evas_object_show(tb->grid);
   elm_table_pack(tb->table, tb->grid, 0, 0, 4, 1);

   botbox = evas_object_rectangle_add(evas_object_evas_get(tb->table));
   evas_object_color_set(botbox, 0, 0, 0, 0);
   evas_object_size_hint_min_set(botbox, 0, h);
   evas_object_size_hint_weight_set(botbox, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_fill_set(botbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(tb->table, botbox, 0, 1, 4, 1);
   evas_object_show(botbox);

   tb->infolabel = elm_label_add(tb->table);
   elm_label_line_wrap_set(tb->infolabel, ELM_WRAP_WORD);
   elm_object_text_set(tb->infolabel, "Info Label");
   evas_object_size_hint_weight_set(tb->infolabel, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(tb->infolabel, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_aspect_set(tb->infolabel, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_table_pack(tb->table, tb->infolabel, 0, 1, 4, 1);
   evas_object_show(tb->infolabel);

   tb->handlers = eina_list_append
      (tb->handlers, ecore_event_handler_add
       (EPHOTO_EVENT_POPULATE_START, _ephoto_thumb_populate_start, tb));

   tb->handlers = eina_list_append
      (tb->handlers, ecore_event_handler_add
       (EPHOTO_EVENT_POPULATE_END, _ephoto_thumb_populate_end, tb));

   tb->handlers = eina_list_append
      (tb->handlers, ecore_event_handler_add
       (EPHOTO_EVENT_POPULATE_ERROR, _ephoto_thumb_populate_error, tb));

   tb->handlers = eina_list_append
      (tb->handlers, ecore_event_handler_add
       (EPHOTO_EVENT_ENTRY_CREATE, _ephoto_thumb_entry_create, tb));

   return tb->main;

 error:
   evas_object_del(tb->main);
   return NULL;
}
