#include "ephoto.h"

#define ZOOM_STEP 0.2

typedef struct _Ephoto_Single_Browser Ephoto_Single_Browser;
typedef struct _Ephoto_Viewer Ephoto_Viewer;

struct _Ephoto_Single_Browser
{
   Ephoto *ephoto;
   Evas_Object *main;
   Evas_Object *bar;
   Evas_Object *table;
   Evas_Object *panel;
   Evas_Object *viewer;
   const char *pending_path;
   Ephoto_Entry *entry;
   Ephoto_Orient orient;
   Eina_List *handlers;
};

struct _Ephoto_Viewer
{
   Evas_Object *scroller;
   Evas_Object *table;
   Evas_Object *image;
   double zoom;
   Eina_Bool fit:1;
};

static void _zoom_set(Ephoto_Single_Browser *sb, double zoom);
static void _zoom_in(Ephoto_Single_Browser *sb);
static void _zoom_out(Ephoto_Single_Browser *sb);

static void
_viewer_del(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Viewer *v = data;
   free(v);
}

static Evas_Object *
_viewer_add(Evas_Object *parent, const char *path)
{
   Ephoto_Viewer *v = calloc(1, sizeof(Ephoto_Viewer));
   Evas_Object *obj;
   int err;

   EINA_SAFETY_ON_NULL_RETURN_VAL(v, NULL);

   Evas_Coord w, h;
   const char *group = NULL;
   const char *ext = strrchr(path, '.');
   if (ext)
     {
        ext++;
        if ((strcasecmp(ext, "edj") == 0))
          {
             if (edje_file_group_exists(path, "e/desktop/background"))
               group = "e/desktop/background";
             else
               {
                  Eina_List *g = edje_file_collection_list(path);
                  group = eina_list_data_get(g);
                  edje_file_collection_list_free(g);
               }
          }
       }
   obj = v->scroller = elm_scroller_add(parent);
   EINA_SAFETY_ON_NULL_GOTO(obj, error);  
   evas_object_size_hint_weight_set(obj, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(obj, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_data_set(obj, "viewer", v);
   evas_object_event_callback_add(obj, EVAS_CALLBACK_DEL, _viewer_del, v);
   evas_object_show(obj);

   v->table = elm_table_add(v->scroller);
   evas_object_size_hint_weight_set(v->table, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(v->table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_content_set(v->scroller, v->table);
   evas_object_show(v->table);

   v->image = elm_image_add(v->table);
   elm_image_preload_disabled_set(v->image, EINA_FALSE);
   elm_image_file_set(v->image, path, group);
   err = evas_object_image_load_error_get(elm_image_object_get(v->image));
   if (err != EVAS_LOAD_ERROR_NONE) goto load_error;
   elm_image_object_size_get(v->image, &w, &h);
   evas_object_size_hint_min_set(v->image, w, h);
   evas_object_size_hint_max_set(v->image, w, h);
   elm_table_pack(v->table, v->image, 0, 0, 1, 1);
   evas_object_show(v->image);

   return obj;

 load_error:
   ERR("could not load image '%s': %s", path, evas_load_error_str(err));
   evas_object_del(obj);
 error:
   free(v);
   return NULL;
}

static void
_viewer_zoom_apply(Ephoto_Viewer *v, double zoom)
{
   v->zoom = zoom;

   Evas_Coord w, h;
   evas_object_image_size_get(elm_image_object_get(v->image), &w, &h);
   w *= zoom;
   h *= zoom;
   evas_object_size_hint_min_set(v->image, w, h);
   evas_object_size_hint_max_set(v->image, w, h);
}

static void
_viewer_zoom_fit_apply(Ephoto_Viewer *v)
{
   Evas_Coord cw, ch, iw, ih;
   double zx, zy, zoom;

   evas_object_geometry_get(v->scroller, NULL, NULL, &cw, &ch);
   evas_object_image_size_get(elm_image_object_get(v->image), &iw, &ih);

   if ((cw <= 0) || (ch <= 0)) return; /* object still not resized */
   EINA_SAFETY_ON_TRUE_RETURN(iw <= 0);
   EINA_SAFETY_ON_TRUE_RETURN(ih <= 0);

   zx = (double)cw / (double)iw;
   zy = (double)ch / (double)ih;

   zoom = (zx < zy) ? zx : zy;
   _viewer_zoom_apply(v, zoom);
}

static void
_viewer_resized(void *data, Evas *e __UNUSED__, Evas_Object *obj __UNUSED__, void *event_info __UNUSED__)
{
   _viewer_zoom_fit_apply(data);
}

static void
_viewer_zoom_set(Evas_Object *obj, double zoom)
{
   Ephoto_Viewer *v = evas_object_data_get(obj, "viewer");
   EINA_SAFETY_ON_NULL_RETURN(v);
   _viewer_zoom_apply(v, zoom);

   if (v->fit)
     {
        evas_object_event_callback_del_full
          (v->scroller, EVAS_CALLBACK_RESIZE, _viewer_resized, v);
        v->fit = EINA_FALSE;
     }
}

static double
_viewer_zoom_get(Evas_Object *obj)
{
   Ephoto_Viewer *v = evas_object_data_get(obj, "viewer");
   EINA_SAFETY_ON_NULL_RETURN_VAL(v, 0.0);
   return v->zoom;
}

static void
_viewer_zoom_fit(Evas_Object *obj)
{
   Ephoto_Viewer *v = evas_object_data_get(obj, "viewer");
   EINA_SAFETY_ON_NULL_RETURN(v);

   if (v->fit) return;
   v->fit = EINA_TRUE;

   evas_object_event_callback_add
     (v->scroller, EVAS_CALLBACK_RESIZE, _viewer_resized, v);

   _viewer_zoom_fit_apply(v);
}

static void
_orient_apply(Ephoto_Single_Browser *sb)
{
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");
   int w, h; 
   EINA_SAFETY_ON_NULL_RETURN(v);

   elm_table_unpack(v->table, v->image);
   switch (sb->orient)
     {
      case EPHOTO_ORIENT_0:
         break;
      case EPHOTO_ORIENT_90:
         elm_image_orient_set(v->image, ELM_IMAGE_ROTATE_90);
         break;
      case EPHOTO_ORIENT_180:
         elm_image_orient_set(v->image, ELM_IMAGE_ROTATE_180);
         break;
      case EPHOTO_ORIENT_270:
         elm_image_orient_set(v->image, ELM_IMAGE_ROTATE_270);
         break;
      case EPHOTO_ORIENT_FLIP_HORIZ:
         elm_image_orient_set(v->image, ELM_IMAGE_FLIP_HORIZONTAL);
         break;
      case EPHOTO_ORIENT_FLIP_VERT:
         elm_image_orient_set(v->image, ELM_IMAGE_FLIP_VERTICAL);
         break;
      case EPHOTO_ORIENT_FLIP_HORIZ_90:
         elm_image_orient_set(v->image, ELM_IMAGE_FLIP_TRANSPOSE);
         break;
      case EPHOTO_ORIENT_FLIP_VERT_90:
         elm_image_orient_set(v->image, ELM_IMAGE_FLIP_TRANSVERSE);
         break;
      default:
         return;
     }
   elm_image_object_size_get(v->image, &w, &h);
   evas_object_size_hint_min_set(v->image, w, h);
   evas_object_size_hint_max_set(v->image, w, h);
   elm_table_pack(v->table, v->image, 0, 0, 1, 1);
   if (v->fit)
     _viewer_zoom_fit_apply(v);
   else
     _viewer_zoom_set(sb->viewer, _viewer_zoom_get(sb->viewer));
   DBG("orient: %d", sb->orient);
}

static void
_rotate_counterclock(Ephoto_Single_Browser *sb)
{
   sb->orient = EPHOTO_ORIENT_270;
   _orient_apply(sb);
}

static void
_rotate_clock(Ephoto_Single_Browser *sb)
{
   sb->orient = EPHOTO_ORIENT_90;
   _orient_apply(sb);
}

static void
_flip_horiz(Ephoto_Single_Browser *sb)
{

   sb->orient = EPHOTO_ORIENT_FLIP_HORIZ;
   _orient_apply(sb);
}

static void
_flip_vert(Ephoto_Single_Browser *sb)
{
   sb->orient = EPHOTO_ORIENT_FLIP_VERT;
   _orient_apply(sb);
}

static void
_mouse_wheel(void *data, Evas *e __UNUSED__, Evas_Object *o __UNUSED__, void *event_info)
{
   Ephoto_Single_Browser *sb = data;
   Evas_Event_Mouse_Wheel *ev = event_info;
   if (!evas_key_modifier_is_set(ev->modifiers, "Control")) return;

   if (ev->z > 0) _zoom_in(sb);
   else _zoom_out(sb);
}

static Ephoto_Entry *
_first_entry_find(Ephoto_Single_Browser *sb)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(sb->ephoto, NULL);

   return eina_list_nth(sb->ephoto->entries, 0);
}

static Ephoto_Entry *
_last_entry_find(Ephoto_Single_Browser *sb)
{
   EINA_SAFETY_ON_NULL_RETURN_VAL(sb->ephoto, NULL);
   return eina_list_last_data_get(sb->ephoto->entries);
}


static void
_ephoto_single_browser_recalc(Ephoto_Single_Browser *sb)
{
   if (sb->viewer)
     {
        evas_object_del(sb->viewer);
        sb->viewer = NULL;
     }

   if (sb->entry)
     {
        const char *bname = ecore_file_file_get(sb->entry->path);

        elm_table_clear(sb->table, EINA_FALSE);

        sb->viewer = _viewer_add(sb->main, sb->entry->path);
        elm_table_pack(sb->table, sb->viewer, 0, 0, 4, 1);
        evas_object_show(sb->viewer);
        evas_object_event_callback_add
          (sb->viewer, EVAS_CALLBACK_MOUSE_WHEEL, _mouse_wheel, sb);
        ephoto_title_set(sb->ephoto, bname);

        elm_table_pack(sb->table, sb->panel, 0, 0, 1, 1);
     }

   elm_object_focus_set(sb->main, EINA_TRUE);
}

static void
_zoom_set(Ephoto_Single_Browser *sb, double zoom)
{
   DBG("zoom %f", zoom);
   if (zoom <= 0.0) return;
   _viewer_zoom_set(sb->viewer, zoom);
}

static void
_zoom_fit(Ephoto_Single_Browser *sb)
{
   if (sb->viewer) _viewer_zoom_fit(sb->viewer);
}

static void
_zoom_in(Ephoto_Single_Browser *sb)
{
   double change = (1.0 + ZOOM_STEP);
   _viewer_zoom_set(sb->viewer, _viewer_zoom_get(sb->viewer) * change);
}

static void
_zoom_out(Ephoto_Single_Browser *sb)
{
   double change = (1.0 - ZOOM_STEP);
   _viewer_zoom_set(sb->viewer, _viewer_zoom_get(sb->viewer) * change);
}

static void
_zoom_in_cb(void *data, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Single_Browser *sb = data;
   _zoom_in(sb);
}

static void
_zoom_out_cb(void *data, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Single_Browser *sb = data;
   _zoom_out(sb);
}

static void
_zoom_1_cb(void *data, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Single_Browser *sb = data;
   _zoom_set(sb, 1.0);
}

static void
_zoom_fit_cb(void *data, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Single_Browser *sb = data;
   _zoom_fit(sb);
}

static void
_next_entry(Ephoto_Single_Browser *sb)
{
   Ephoto_Entry *entry = NULL;
   Eina_List *node;
   EINA_SAFETY_ON_NULL_RETURN(sb->entry);

   node = eina_list_data_find_list(sb->ephoto->entries, sb->entry);
   if (!node) return;
   if ((node = node->next))
     entry = node->data;
   if (!entry)
     entry = _first_entry_find(sb);
   if (entry)
     {
        DBG("next is '%s'", entry->path);
        ephoto_single_browser_entry_set(sb->main, entry);
     }
}

static void
_prev_entry(Ephoto_Single_Browser *sb)
{
   Eina_List *node;
   Ephoto_Entry *entry = NULL;
   EINA_SAFETY_ON_NULL_RETURN(sb->entry);

   node = eina_list_data_find_list(sb->ephoto->entries, sb->entry);
   if (!node) return;
   if ((node = node->prev))
     entry = node->data;
   if (!entry)
     entry = _last_entry_find(sb);
   if (entry)
     {
        DBG("prev is '%s'", entry->path);
        ephoto_single_browser_entry_set(sb->main, entry);
     }
}

static void
_first_entry(Ephoto_Single_Browser *sb)
{
   Ephoto_Entry *entry = _first_entry_find(sb);
   if (!entry) return;
   DBG("first is '%s'", entry->path);
   ephoto_single_browser_entry_set(sb->main, entry);
}

static void
_last_entry(Ephoto_Single_Browser *sb)
{
   Ephoto_Entry *entry = _last_entry_find(sb);
   if (!entry) return;
   DBG("last is '%s'", entry->path);
   ephoto_single_browser_entry_set(sb->main, entry);
}

static void
_go_first(void *data, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Single_Browser *sb = data;
   _first_entry(sb);
}

static void
_go_prev(void *data, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Single_Browser *sb = data;
   _prev_entry(sb);
}

static void
_go_next(void *data, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Single_Browser *sb = data;
   _next_entry(sb);
}

static void
_go_last(void *data, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Single_Browser *sb = data;
   _last_entry(sb);
}

static void
_go_rotate_counterclock(void *data, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Single_Browser *sb = data;
   _rotate_counterclock(sb);
}

static void
_go_rotate_clock(void *data, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Single_Browser *sb = data;
   _rotate_clock(sb);
}

static void
_go_flip_horiz(void *data, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Single_Browser *sb = data;
   _flip_horiz(sb);
}

static void
_go_flip_vert(void *data, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Single_Browser *sb = data;
   _flip_vert(sb);
}

static void
_slideshow(void *data, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Single_Browser *sb = data;
   if (sb->entry)
     evas_object_smart_callback_call(sb->main, "slideshow", sb->entry);
}

static void
_back(void *data, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Single_Browser *sb = data;
   evas_object_smart_callback_call(sb->main, "back", sb->entry);
}

static void
_settings(void *data, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->ephoto)
     ephoto_config_window(sb->ephoto);
}

static void
_about(void *data, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->ephoto)
     ephoto_about_window(sb->ephoto);
}

static void
_key_down(void *data, Evas *e __UNUSED__, Evas_Object *o __UNUSED__, void *event_info)
{
   Ephoto_Single_Browser *sb = data;
   Evas_Event_Key_Down *ev = event_info;
   Eina_Bool ctrl = evas_key_modifier_is_set(ev->modifiers, "Control");
   Eina_Bool shift = evas_key_modifier_is_set(ev->modifiers, "Shift");
   const char *k = ev->keyname;

   DBG("key pressed '%s'", k);
   if (ctrl)
     {
        if ((!strcmp(k, "plus")) || (!strcmp(k, "equal")))
          _zoom_in(sb);
        else if (!strcmp(k, "minus"))
          _zoom_out(sb);
        else if (!strcmp(k, "0"))
          {
             if (shift) _zoom_fit(sb);
             else _zoom_set(sb, 1.0);
          }

        return;
     }

   if (!strcmp(k, "Escape"))
     evas_object_smart_callback_call(sb->main, "back", sb->entry);
   else if (!strcmp(k, "Left"))
     _prev_entry(sb);
   else if (!strcmp(k, "Right"))
     _next_entry(sb);
   else if (!strcmp(k, "Home"))
     _first_entry(sb);
   else if (!strcmp(k, "End"))
     _last_entry(sb);
   else if (!strcmp(k, "bracketleft"))
     {
        if (!shift) _rotate_counterclock(sb);
        else        _flip_horiz(sb);
     }
   else if (!strcmp(k, "bracketright"))
     {
        if (!shift) _rotate_clock(sb);
        else        _flip_vert(sb);
     }
   else if (!strcmp(k, "F5"))
     {
        if (sb->entry)
          evas_object_smart_callback_call(sb->main, "slideshow", sb->entry);
     }
   else if (!strcmp(k, "F11"))
     {
        Evas_Object *win = sb->ephoto->win;
        elm_win_fullscreen_set(win, !elm_win_fullscreen_get(win));
     }
}

static void
_entry_free(void *data, const Ephoto_Entry *entry __UNUSED__)
{
   Ephoto_Single_Browser *sb = data;
   sb->entry = NULL;
}

static void
_main_del(void *data, Evas *e __UNUSED__, Evas_Object *o __UNUSED__, void *event_info __UNUSED__)
{
   Ephoto_Single_Browser *sb = data;
   Ecore_Event_Handler *handler;

   if (elm_panel_hidden_get(sb->panel))
     sb->ephoto->config->single_browser_panel = 1;
   else
     sb->ephoto->config->single_browser_panel = 0;

   EINA_LIST_FREE(sb->handlers, handler)
      ecore_event_handler_del(handler);
   if (sb->entry)
     ephoto_entry_free_listener_del(sb->entry, _entry_free, sb);
   if (sb->pending_path)
     eina_stringshare_del(sb->pending_path);
   free(sb);
}

static Eina_Bool
_ephoto_single_populate_end(void *data __UNUSED__, int type __UNUSED__, void *event __UNUSED__)
{
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ephoto_single_entry_create(void *data, int type __UNUSED__, void *event __UNUSED__)
{
   Ephoto_Single_Browser *sb = data;
   Ephoto_Event_Entry_Create *ev = event;
   Ephoto_Entry *e;

   e = ev->entry;
   if (!sb->entry && sb->pending_path && e->path == sb->pending_path)
     {
        DBG("Adding entry %p for path %s", e, sb->pending_path);

        eina_stringshare_del(sb->pending_path);
        sb->pending_path = NULL;
        ephoto_single_browser_entry_set(sb->ephoto->single_browser, e);
     }

   return ECORE_CALLBACK_PASS_ON;
}

Evas_Object *
ephoto_single_browser_add(Ephoto *ephoto, Evas_Object *parent)
{
   Evas_Object *box = elm_box_add(parent);
   Elm_Object_Item *icon;
   Ephoto_Single_Browser *sb;

   EINA_SAFETY_ON_NULL_RETURN_VAL(box, NULL);

   sb = calloc(1, sizeof(Ephoto_Single_Browser));
   EINA_SAFETY_ON_NULL_GOTO(sb, error);

   sb->ephoto = ephoto;
   sb->main = box;

   evas_object_size_hint_weight_set(sb->main, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(sb->main, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_event_callback_add(sb->main, EVAS_CALLBACK_DEL, _main_del, sb);
   evas_object_event_callback_add
     (sb->main, EVAS_CALLBACK_KEY_DOWN, _key_down, sb);
   evas_object_data_set(sb->main, "single_browser", sb);

   sb->table = elm_table_add(sb->main);
   evas_object_size_hint_weight_set(sb->table, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(sb->table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(sb->main, sb->table);
   evas_object_show(sb->table);

   sb->panel = elm_panel_add(sb->table);
   EINA_SAFETY_ON_NULL_GOTO(sb->panel, error);
   elm_panel_orient_set(sb->panel, ELM_PANEL_ORIENT_LEFT);
   evas_object_size_hint_weight_set(sb->panel, 0.0, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(sb->panel, EVAS_HINT_FILL, EVAS_HINT_FILL);
   if (sb->ephoto->config->single_browser_panel)
     elm_panel_hidden_set(sb->panel, EINA_TRUE);
   else
     elm_panel_hidden_set(sb->panel, EINA_FALSE);
   elm_table_pack(sb->table, sb->panel, 0, 0, 1, 1);
   evas_object_show(sb->panel);
   
   sb->bar = elm_toolbar_add(sb->panel);
   EINA_SAFETY_ON_NULL_GOTO(sb->bar, error);
   elm_toolbar_horizontal_set(sb->bar, EINA_FALSE);
   elm_toolbar_shrink_mode_set(sb->bar, ELM_TOOLBAR_SHRINK_SCROLL);
   elm_toolbar_select_mode_set(sb->bar, ELM_OBJECT_SELECT_MODE_NONE);
   evas_object_size_hint_weight_set(sb->bar, 0.0, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(sb->bar, EVAS_HINT_FILL, EVAS_HINT_FILL);

   icon = elm_toolbar_item_append(sb->bar, "stock_home", "Back", _back, sb);
   elm_toolbar_item_priority_set(icon, 150);

   icon = elm_toolbar_item_append(sb->bar, "stock_media-play", "Slideshow", _slideshow, sb);
   elm_toolbar_item_priority_set(icon, 150);
   
   elm_toolbar_item_separator_set(elm_toolbar_item_append(sb->bar, NULL, NULL, NULL, NULL), EINA_TRUE);

   icon = elm_toolbar_item_append(sb->bar, "zoom-in", "Zoom In", _zoom_in_cb, sb);
   elm_toolbar_item_priority_set(icon, 100);

   icon = elm_toolbar_item_append(sb->bar, "zoom-out", "Zoom Out", _zoom_out_cb, sb);
   elm_toolbar_item_priority_set(icon, 100);

   icon = elm_toolbar_item_append(sb->bar, "zoom-fit", "Zoom Fit", _zoom_fit_cb, sb);
   elm_toolbar_item_priority_set(icon, 80);

   icon = elm_toolbar_item_append(sb->bar, "zoom-original", "Zoom 1:1", _zoom_1_cb, sb);
   elm_toolbar_item_priority_set(icon, 80);
   
   elm_toolbar_item_separator_set(elm_toolbar_item_append(sb->bar, NULL, NULL, NULL, NULL), EINA_TRUE);

   icon = elm_toolbar_item_append(sb->bar, "go-first", "First", _go_first, sb);
   elm_toolbar_item_priority_set(icon, 60);

   icon = elm_toolbar_item_append(sb->bar, "go-previous", "Previous", _go_prev, sb);
   elm_toolbar_item_priority_set(icon, 70);

   icon = elm_toolbar_item_append(sb->bar, "go-next", "Next", _go_next, sb);
   elm_toolbar_item_priority_set(icon, 70);

   icon = elm_toolbar_item_append(sb->bar, "go-last", "Last", _go_last, sb);
   elm_toolbar_item_priority_set(icon, 60);

   elm_toolbar_item_separator_set(elm_toolbar_item_append(sb->bar, NULL, NULL, NULL, NULL), EINA_TRUE);

   icon = elm_toolbar_item_append(sb->bar, "object-rotate-left", "Rotate Left", _go_rotate_counterclock, sb);
   elm_toolbar_item_priority_set(icon, 50);

   icon = elm_toolbar_item_append(sb->bar, "object-rotate-right", "Rotate Right", _go_rotate_clock, sb);
   elm_toolbar_item_priority_set(icon, 40);

   icon = elm_toolbar_item_append(sb->bar, "object-flip-horizontal", "Flip Horizontal", _go_flip_horiz, sb);
   elm_toolbar_item_priority_set(icon, 30);

   icon = elm_toolbar_item_append(sb->bar, "object-flip-vertical", "Flip Vertical", _go_flip_vert, sb);
   elm_toolbar_item_priority_set(icon, 20);

   elm_toolbar_item_separator_set(elm_toolbar_item_append(sb->bar, NULL, NULL, NULL, NULL), EINA_TRUE);

   icon = elm_toolbar_item_append(sb->bar, "emblem-system", "Settings", _settings, sb);
   elm_toolbar_item_priority_set(icon, 10);

   icon = elm_toolbar_item_append(sb->bar, "stock_about", "About", _about, sb);
   elm_toolbar_item_priority_set(icon, 0);

   elm_object_content_set(sb->panel, sb->bar);
   evas_object_show(sb->bar);

   sb->handlers = eina_list_append
      (sb->handlers, ecore_event_handler_add
       (EPHOTO_EVENT_POPULATE_END, _ephoto_single_populate_end, sb));

   sb->handlers = eina_list_append
      (sb->handlers, ecore_event_handler_add
       (EPHOTO_EVENT_ENTRY_CREATE, _ephoto_single_entry_create, sb));

   return sb->main;

 error:
   evas_object_del(sb->main);
   return NULL;
}

void
ephoto_single_browser_entry_set(Evas_Object *obj, Ephoto_Entry *entry)
{
   Ephoto_Single_Browser *sb = evas_object_data_get(obj, "single_browser");
   EINA_SAFETY_ON_NULL_RETURN(sb);

   DBG("entry %p, was %p", entry, sb->entry);

   if (sb->entry)
     ephoto_entry_free_listener_del(sb->entry, _entry_free, sb);

   sb->entry = entry;

   if (entry)
     ephoto_entry_free_listener_add(entry, _entry_free, sb);
   _ephoto_single_browser_recalc(sb);
   _zoom_fit(sb);
}

void
ephoto_single_browser_path_pending_set(Evas_Object *obj, const char *path)
{
   Ephoto_Single_Browser *sb = evas_object_data_get(obj, "single_browser");
   EINA_SAFETY_ON_NULL_RETURN(sb);

   DBG("Setting pending path '%s'", path);
   sb->pending_path = eina_stringshare_add(path);
}
