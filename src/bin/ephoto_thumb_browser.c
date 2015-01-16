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
   Evas_Object *bar;
   Evas_Object *entry;
   Evas_Object *grid;
   Evas_Object *panel;
   Evas_Object *nolabel;
   Eio_File *ls;
   Eina_List *todo_items;
   Eina_List *grid_items;
   Eina_List *handlers;
   struct {
      Elm_Object_Item *zoom_in;
      Elm_Object_Item *zoom_out;
      Elm_Object_Item *view_single;
      Elm_Object_Item *slideshow;
   } action;
   struct {
      Ecore_Animator *todo_items;
   } animator;
   Eina_Bool main_deleted : 1;
};

static void
_todo_items_free(Ephoto_Thumb_Browser *tb)
{
   eina_list_free(tb->todo_items);
   tb->todo_items = NULL;
}

static void
_grid_items_free(Ephoto_Thumb_Browser *tb)
{
   eina_list_free(tb->grid_items);
   tb->grid_items = NULL;
}

static char *
_ephoto_thumb_item_text_get(void *data, Evas_Object *obj __UNUSED__, const char *part __UNUSED__)
{
   Ephoto_Entry *e = data;
   return strdup(e->label);
}

static Evas_Object *
_ephoto_thumb_file_icon_get(void *data, Evas_Object *obj, const char *part __UNUSED__)
{
   Ephoto_Entry *e = data;
   return ephoto_thumb_add(e->ephoto, obj, e->path);
}

static void
_ephoto_thumb_item_del(void *data __UNUSED__, Evas_Object *obj __UNUSED__)
{
   /* FIXME: the entry is already freed when changing directories
    * One solution is to take care of this cleaning when manually removing
    * some grid items
   Ephoto_Entry *e = data;
   e->item = NULL;
   */
}

static Elm_Gengrid_Item_Class _ephoto_thumb_file_class;

static int
_entry_cmp(const void *pa, const void *pb)
{
   const Elm_Object_Item *ia = pa;
   const Ephoto_Entry *a, *b = pb;

   a = elm_object_item_data_get(ia);

   return strcoll(a->basename, b->basename);
}

static void
_entry_item_add(Ephoto_Thumb_Browser *tb, Ephoto_Entry *e)
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
     _entry_item_add(tb, entry);

   return EINA_FALSE;
}

static void
_ephoto_thumb_selected(void *data, Evas_Object *o __UNUSED__, void *event_info)
{
   Ephoto_Thumb_Browser *tb = data;
   Elm_Object_Item *it = event_info;
   Ephoto_Entry *e = elm_object_item_data_get(it);

   elm_gengrid_item_selected_set(it, EINA_FALSE);

   evas_object_smart_callback_call(tb->main, "view", e);
}

static void
_changed_dir(void *data, Evas_Object *o __UNUSED__, void *event_info)
{
   Ephoto_Thumb_Browser *tb = data;
   const char *path = event_info;
   if (path)
     ephoto_directory_set(tb->ephoto, path);
   else
     elm_fileselector_path_set(tb->entry, tb->ephoto->config->directory); 
}

static void
_changed_dir_text(void *data, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Thumb_Browser *tb = data;
   const char *path = elm_fileselector_path_get(tb->entry);
   if (ecore_file_is_dir(path))
     ephoto_directory_set(tb->ephoto, path);
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
_zoom_in(void *data, Evas_Object *o, void *event_info __UNUSED__)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Object *min = evas_object_data_get(o, "min");
   int zoom = tb->ephoto->config->thumb_size + ZOOM_STEP;
   _zoom_set(tb, zoom);
   if (zoom >= ZOOM_MAX) elm_object_disabled_set(o, EINA_TRUE);
   if (zoom > ZOOM_MIN) elm_object_disabled_set(min, EINA_FALSE);
}

static void
_zoom_out(void *data, Evas_Object *o, void *event_info __UNUSED__)
{
   Ephoto_Thumb_Browser *tb = data;
   Evas_Object *max = evas_object_data_get(o, "max");
   int zoom = tb->ephoto->config->thumb_size - ZOOM_STEP;
   _zoom_set(tb, zoom);
   if (zoom <= ZOOM_MIN) elm_object_disabled_set(o, EINA_TRUE);
   if (zoom < ZOOM_MAX) elm_object_disabled_set(max, EINA_FALSE);
}

static void
_view_single(void *data, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Thumb_Browser *tb = data;
   Elm_Object_Item *it = elm_gengrid_selected_item_get(tb->grid);
   Ephoto_Entry *entry;

   if (it) entry = elm_object_item_data_get(it);
   else entry = eina_list_nth(tb->ephoto->entries, 0);

   if (!entry) return;
   evas_object_smart_callback_call(tb->main, "view", entry);
}

static void
_slideshow(void *data, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
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
_settings(void *data, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Thumb_Browser *tb = data;

   if (tb->ephoto)
     ephoto_config_window(tb->ephoto);
}

static void
_about(void *data, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Thumb_Browser *tb = data;

   if (tb->ephoto)
     ephoto_about_window(tb->ephoto);
}

static void
_key_down(void *data, Evas *e __UNUSED__, Evas_Object *o __UNUSED__, void *event_info)
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
_main_del(void *data, Evas *e __UNUSED__, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Thumb_Browser *tb = data;
   Ecore_Event_Handler *handler;

   if (elm_panel_hidden_get(tb->panel))
     tb->ephoto->config->thumb_browser_panel = 1;
   else
     tb->ephoto->config->thumb_browser_panel = 0;

   _todo_items_free(tb);
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
_ephoto_thumb_populate_start(void *data, int type __UNUSED__, void *event __UNUSED__)
{
   Ephoto_Thumb_Browser *tb = data;

   evas_object_smart_callback_call(tb->main, "changed,directory", NULL);

   _todo_items_free(tb);
   _grid_items_free(tb);
   elm_gengrid_clear(tb->grid);
   elm_fileselector_path_set(tb->entry, tb->ephoto->config->directory);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ephoto_thumb_populate_end(void *data, int type __UNUSED__, void *event __UNUSED__)
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
        elm_table_unpack(tb->table, tb->panel);
        tb->nolabel = elm_label_add(tb->table);
        elm_label_line_wrap_set(tb->nolabel, ELM_WRAP_WORD);
        elm_object_text_set(tb->nolabel, "There are no images in this directory");
        evas_object_size_hint_weight_set(tb->nolabel, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
        evas_object_size_hint_align_set(tb->nolabel, EVAS_HINT_FILL, EVAS_HINT_FILL);
        elm_table_pack(tb->table, tb->nolabel, 0, 0, 4, 1);
        evas_object_show(tb->nolabel);
        elm_table_pack(tb->table, tb->panel, 0, 0, 1, 1);
     }
   else
     {
        if (tb->nolabel)
          {
             elm_table_unpack(tb->table, tb->nolabel);
             evas_object_del(tb->nolabel);
             tb->nolabel = NULL;
          }
     }

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ephoto_thumb_populate_error(void *data __UNUSED__, int type __UNUSED__, void *event __UNUSED__)
{
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ephoto_thumb_entry_create(void *data, int type __UNUSED__, void *event)
{
   Ephoto_Thumb_Browser *tb = data;
   Ephoto_Event_Entry_Create *ev = event;
   Ephoto_Entry *e;

   e = ev->entry;
   tb->todo_items = eina_list_append(tb->todo_items, e);

   if (!tb->animator.todo_items)
     tb->animator.todo_items = ecore_animator_add(_todo_items_process, tb);

   return ECORE_CALLBACK_PASS_ON;
}

Evas_Object *
ephoto_thumb_browser_add(Ephoto *ephoto, Evas_Object *parent)
{
   Evas_Object *box = elm_box_add(parent);
   Elm_Object_Item *icon;
   Evas_Object *min, *max, *ic;
   Ephoto_Thumb_Browser *tb;

   EINA_SAFETY_ON_NULL_RETURN_VAL(box, NULL);

   tb = calloc(1, sizeof(Ephoto_Thumb_Browser));
   EINA_SAFETY_ON_NULL_GOTO(tb, error);

   _ephoto_thumb_file_class.item_style = "thumb";
   _ephoto_thumb_file_class.func.text_get = _ephoto_thumb_item_text_get;
   _ephoto_thumb_file_class.func.content_get = _ephoto_thumb_file_icon_get;
   _ephoto_thumb_file_class.func.state_get = NULL;
   _ephoto_thumb_file_class.func.del = _ephoto_thumb_item_del;

   tb->ephoto = ephoto;
   tb->main = box;

   evas_object_size_hint_weight_set(tb->main, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->main, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_event_callback_add(tb->main, EVAS_CALLBACK_DEL, _main_del, tb);
   evas_object_event_callback_add
     (tb->main, EVAS_CALLBACK_KEY_DOWN, _key_down, tb);
   evas_object_data_set(tb->main, "thumb_browser", tb);

   ic = elm_icon_add(tb->main);
   elm_icon_standard_set(ic, "folder");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);

   tb->entry = elm_fileselector_entry_add(tb->main);
   EINA_SAFETY_ON_NULL_GOTO(tb->entry, error);
   evas_object_size_hint_weight_set(tb->entry, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(tb->entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_text_set(tb->entry, "Choose");
   elm_object_part_content_set(tb->entry, "button icon", ic);
   elm_fileselector_folder_only_set(tb->entry, EINA_TRUE);
   elm_fileselector_entry_inwin_mode_set(tb->entry, EINA_TRUE);
   elm_fileselector_expandable_set(tb->entry, EINA_FALSE);
   evas_object_smart_callback_add
     (tb->entry, "file,chosen", _changed_dir, tb);
   evas_object_smart_callback_add
     (tb->entry, "activated", _changed_dir_text, tb);
   elm_box_pack_end(tb->main, tb->entry);
   evas_object_show(tb->entry);

   tb->table = elm_table_add(tb->main);
   evas_object_size_hint_weight_set(tb->table, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(tb->main, tb->table);
   evas_object_show(tb->table);

   tb->grid = elm_gengrid_add(tb->table);
   EINA_SAFETY_ON_NULL_GOTO(tb->grid, error);
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
   
   tb->panel = elm_panel_add(tb->table);
   EINA_SAFETY_ON_NULL_GOTO(tb->panel, error);
   elm_panel_orient_set(tb->panel, ELM_PANEL_ORIENT_LEFT);
   evas_object_size_hint_weight_set(tb->panel, 0.0, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->panel, EVAS_HINT_FILL, EVAS_HINT_FILL);
   if (tb->ephoto->config->thumb_browser_panel)
     elm_panel_hidden_set(tb->panel, EINA_TRUE);
   else
     elm_panel_hidden_set(tb->panel, EINA_FALSE);
   elm_table_pack(tb->table, tb->panel, 0, 0, 1, 1);
   evas_object_show(tb->panel);
   
   tb->bar = elm_toolbar_add(tb->panel);
   EINA_SAFETY_ON_NULL_GOTO(tb->bar, error);
   elm_toolbar_horizontal_set(tb->bar, EINA_FALSE);
   elm_toolbar_shrink_mode_set(tb->bar, ELM_TOOLBAR_SHRINK_SCROLL);
   elm_toolbar_select_mode_set(tb->bar, ELM_OBJECT_SELECT_MODE_NONE);
   evas_object_size_hint_weight_set(tb->bar, 0.0, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(tb->bar, EVAS_HINT_FILL, EVAS_HINT_FILL);

   elm_toolbar_item_append(tb->bar, "image", "View Single", _view_single, tb);
   elm_toolbar_item_append(tb->bar, "stock_media-play", "Slideshow", _slideshow, tb);
   elm_toolbar_item_separator_set(elm_toolbar_item_append(tb->bar, NULL, NULL, NULL, NULL), EINA_TRUE);
   icon = elm_toolbar_item_append(tb->bar, "zoom-in", "Zoom In", _zoom_in, tb);
   max = elm_object_item_widget_get(icon);
   icon = elm_toolbar_item_append(tb->bar, "zoom-out", "Zoom Out", _zoom_out, tb);
   min = elm_object_item_widget_get(icon);
   evas_object_data_set(max, "min", min);
   evas_object_data_set(min, "max", max);
   elm_toolbar_item_separator_set(elm_toolbar_item_append(tb->bar, NULL, NULL, NULL, NULL), EINA_TRUE);
   elm_toolbar_item_append(tb->bar, "emblem-system", "Settings", _settings, tb);
   elm_toolbar_item_append(tb->bar, "stock_about", "About", _about, tb);

   elm_object_content_set(tb->panel, tb->bar);
   evas_object_show(tb->bar);

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
