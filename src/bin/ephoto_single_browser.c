#include "ephoto.h"

#define ZOOM_STEP 0.2

static Ecore_Timer *_1s_hold = NULL;

typedef struct _Ephoto_Single_Browser Ephoto_Single_Browser;
typedef struct _Ephoto_Viewer Ephoto_Viewer;

struct _Ephoto_Single_Browser
{
   Ephoto *ephoto;
   Evas_Object *main;
   Evas_Object *mhbox;
   Evas_Object *table;
   Evas_Object *viewer;
   Evas_Object *nolabel;
   Evas_Object *event;
   Elm_Object_Item *save;
   const char *pending_path;
   Ephoto_Entry *entry;
   Ephoto_Orient orient;
   Eina_List *handlers;
   Eina_List *entries;
   Eina_Bool editing:1;
   Eina_Bool cropping:1;
   unsigned int *edited_image_data;
   Evas_Coord ew;
   Evas_Coord eh;
};

struct _Ephoto_Viewer
{
   Eina_List *handlers;
   Ecore_File_Monitor *monitor;
   Evas_Object *scroller;
   Evas_Object *table;
   Evas_Object *image;
   double zoom;
   Eina_Bool fit:1;
   Eina_Bool zoom_first:1;
};

/*Common Callbacks*/
static const char *_ephoto_get_edje_group(const char  *path);
static char *_ephoto_get_file_size(const char *path);
static void _ephoto_update_bottom_bar(Ephoto_Single_Browser *sb);

/*Main Callbacks*/
static void _ephoto_main_edit_menu(Ephoto_Single_Browser *sb);
static void _ephoto_main_key_down(void *data, Evas *e EINA_UNUSED,
    Evas_Object *obj EINA_UNUSED,void *event_info EINA_UNUSED);
static void _ephoto_main_focused(void *data, Evas *e EINA_UNUSED,
    Evas_Object *obj EINA_UNUSED,void *event_info EINA_UNUSED);
static void _ephoto_show_settings(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED);
static void _ephoto_main_back(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED);
static void _ephoto_main_del(void *data, Evas *e EINA_UNUSED,
    Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED);

/*Common*/
static const char *
_ephoto_get_edje_group(const char  *path)
{
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
   return group;
}

static void
_ephoto_update_bottom_bar(Ephoto_Single_Browser *sb)
{
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");
   char image_info[PATH_MAX], *tmp;
   Evas_Coord w, h;

   if (sb->editing)
     return;

   evas_object_image_size_get(elm_image_object_get(v->image),
            &w, &h);
   tmp = _ephoto_get_file_size(sb->entry->path);
   snprintf(image_info, PATH_MAX,
       "<b>%s:</b> %s        <b>%s:</b> %dx%d        <b>%s:</b> %s",
       _("Type"), efreet_mime_type_get(sb->entry->path),
       _("Resolution"), w, h, _("File Size"), tmp);
   free(tmp);

   elm_object_text_set(sb->ephoto->infolabel, image_info);
}

static char *
_ephoto_get_file_size(const char *path)
{
   char isize[PATH_MAX];
   Eina_File *f = eina_file_open(path, EINA_FALSE);
   size_t size = eina_file_size_get(f);

   eina_file_close(f);
   double dsize = (double) size;

   if (dsize < 1024.0)
      snprintf(isize, sizeof(isize), "%'.0f%s", dsize, ngettext("B", "B",
	      dsize));
   else
     {
	dsize /= 1024.0;
	if (dsize < 1024)
	   snprintf(isize, sizeof(isize), "%'.0f%s", dsize,
	       ngettext("KB", "KB", dsize));
	else
	  {
	     dsize /= 1024.0;
	     if (dsize < 1024)
		snprintf(isize, sizeof(isize), "%'.1f%s", dsize,
		    ngettext("MB", "MB", dsize));
	     else
	       {
		  dsize /= 1024.0;
		  if (dsize < 1024)
		     snprintf(isize, sizeof(isize), "%'.1f%s", dsize,
			 ngettext("GB", "GB", dsize));
		  else
		    {
		       dsize /= 1024.0;
		       snprintf(isize, sizeof(isize), "%'.1f%s", dsize,
			   ngettext("TB", "TB", dsize));
		    }
	       }
	  }
     }
   return strdup(isize);
}

static void
_menu_dismissed_cb(void *data, Evas_Object *obj,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   evas_object_del(obj);
   elm_object_focus_set(sb->event, EINA_TRUE);
}

/*Image Viewer Callbacks*/
static Evas_Object *
_image_create_icon(void *data, Evas_Object *parent, Evas_Coord *xoff,
    Evas_Coord *yoff)
{
   Evas_Object *ic;
   Evas_Object *io = data;
   const char *f, *g;
   Evas_Coord x, y, w, h, xm, ym;

   elm_image_file_get(io, &f, &g);
   ic = elm_image_add(parent);
   elm_image_file_set(ic, f, g);
   evas_object_geometry_get(io, &x, &y, &w, &h);
   evas_object_move(ic, x, y);
   evas_object_resize(ic, 60, 60);
   evas_object_show(ic);

   evas_pointer_canvas_xy_get(evas_object_evas_get(io), &xm, &ym);
   if (xoff)
      *xoff = xm - 30;
   if (yoff)
      *yoff = ym - 30;

   return ic;
}

static Eina_Bool
_1s_hold_time(void *data)
{
   const char *f;
   char dd[PATH_MAX];
   Evas_Object *io = data;

   elm_image_file_get(io, &f, NULL);
   snprintf(dd, sizeof(dd), "file://%s", f);

   elm_drag_start(io, ELM_SEL_FORMAT_IMAGE, dd, ELM_XDND_ACTION_COPY,
       _image_create_icon, io, NULL, NULL, NULL, NULL, NULL, NULL);
   _1s_hold = NULL;

   return ECORE_CALLBACK_CANCEL;
}

static void
_image_mouse_down_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED,
    Evas_Object *obj, void *event_info)
{
   Evas_Object *io = obj;
   Evas_Event_Mouse_Down *ev = event_info;

   if (ev->flags != EVAS_BUTTON_NONE)
     {
        ecore_timer_del(_1s_hold);
        _1s_hold = NULL;
     }
   else if (ev->button == 1)
     {
        _1s_hold = ecore_timer_add(0.5, _1s_hold_time, io);
     }
}

static void
_image_mouse_up_cb(void *data, Evas *e EINA_UNUSED,
    Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Ephoto_Single_Browser *sb = data;
   Evas_Event_Mouse_Up *ev = event_info;

   if ((ev->button == 1) && (ev->flags == EVAS_BUTTON_DOUBLE_CLICK))
     {
        elm_win_fullscreen_set(sb->ephoto->win,
            !elm_win_fullscreen_get(sb->ephoto->win));
     }
   ecore_timer_del(_1s_hold);
   _1s_hold = NULL;
}

static void
_scroller_mouse_up_cb(void *data, Evas *e EINA_UNUSED,
    Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Ephoto_Single_Browser *sb = data;
   Evas_Event_Mouse_Up *ev = event_info;

   if (sb->editing)
     return;
   if (ev->button == 3)
     {
        _ephoto_main_edit_menu(sb);
        _ephoto_update_bottom_bar(sb);
     }
}

static void
_viewer_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Viewer *v = data;
   if (v->monitor)
     ecore_file_monitor_del(v->monitor);
   free(v);
}

static void
_monitor_cb(void *data, Ecore_File_Monitor *em EINA_UNUSED,
    Ecore_File_Event event, const char *path EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

   if (event == ECORE_FILE_EVENT_MODIFIED)
     {
        if (!ecore_file_exists(sb->entry->path))
          ephoto_entry_free(sb->ephoto, sb->entry);
        else
          {
             Evas_Object *tmp;
             Evas_Coord w, h;
             const char *group = _ephoto_get_edje_group(sb->entry->path);

             tmp = evas_object_image_add(evas_object_evas_get(v->table));
             evas_object_image_file_set(tmp, sb->entry->path, group);
             evas_object_image_size_get(tmp, &w, &h);
             evas_object_del(tmp);

             if (w > 0 && h > 0)
               {
                  evas_object_hide(v->image);
                  elm_image_file_set(v->image, sb->entry->path, group);
                  evas_object_show(v->image);
               }
          }
     }
   return;
}

static void
_viewer_zoom_apply(Ephoto_Viewer *v, double zoom)
{
   v->zoom = zoom;

   Evas_Coord w, h;
   Evas_Object *image;

   image = v->image;
   evas_object_image_size_get(elm_image_object_get(image), &w, &h);
   w *= zoom;
   h *= zoom;
   evas_object_size_hint_min_set(v->image, w, h);
   evas_object_size_hint_max_set(v->image, w, h);
}

static void
_viewer_zoom_fit_apply(Ephoto_Viewer *v)
{
   Evas_Coord cw, ch, iw, ih;
   Evas_Object *image;
   double zx, zy, zoom;

   image = v->image;
   evas_object_geometry_get(v->scroller, NULL, NULL, &cw, &ch);
   evas_object_image_size_get(elm_image_object_get(image), &iw, &ih);

   if ((cw <= 0) || (ch <= 0))
      return;
   EINA_SAFETY_ON_TRUE_RETURN(iw <= 0);
   EINA_SAFETY_ON_TRUE_RETURN(ih <= 0);

   zx = (double) (cw-15) / (double) iw;
   zy = (double) (ch-15) / (double) ih;

   zoom = (zx < zy) ? zx : zy;
   _viewer_zoom_apply(v, zoom);
}

static void
_viewer_resized(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Viewer *v = data;

   if (v->zoom_first)
     {
	Evas_Coord cw, ch, iw, ih;
	Evas_Object *image;

	image = v->image;
	evas_object_geometry_get(v->scroller, NULL, NULL, &cw, &ch);
	evas_object_image_size_get(elm_image_object_get(image), &iw, &ih);

	if ((cw <= 0) || (ch <= 0))
	   return;
	EINA_SAFETY_ON_TRUE_RETURN(iw <= 0);
	EINA_SAFETY_ON_TRUE_RETURN(ih <= 0);
	if (iw < cw && ih < ch)
	   _viewer_zoom_apply(v, 1);
	else
	   _viewer_zoom_fit_apply(v);
     }
   else
      _viewer_zoom_fit_apply(v);
}

static void
_viewer_zoom_set(Evas_Object *obj, double zoom)
{
   Ephoto_Viewer *v = evas_object_data_get(obj, "viewer");

   _viewer_zoom_apply(v, zoom);

   if (v->fit)
     {
	evas_object_event_callback_del_full(v->scroller, EVAS_CALLBACK_RESIZE,
	    _viewer_resized, v);
	v->fit = EINA_FALSE;
     }
}

static double
_viewer_zoom_get(Evas_Object *obj)
{
   Ephoto_Viewer *v = evas_object_data_get(obj, "viewer");

   return v->zoom;
}

static void
_viewer_zoom_fit(Evas_Object *obj)
{
   Ephoto_Viewer *v = evas_object_data_get(obj, "viewer");

   if (v->fit)
      return;
   v->fit = EINA_TRUE;

   evas_object_event_callback_add(v->scroller, EVAS_CALLBACK_RESIZE,
       _viewer_resized, v);

   _viewer_zoom_fit_apply(v);
}

static void
_orient_apply(Ephoto_Single_Browser *sb)
{
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");
   Evas_Coord w, h;

   switch (sb->orient)
     {
       case EPHOTO_ORIENT_0:
	  elm_image_orient_set(v->image, ELM_IMAGE_ORIENT_NONE);
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
   elm_table_unpack(v->table, v->image);
   elm_object_content_unset(v->scroller);
   elm_image_object_size_get(v->image, &w, &h);
   sb->edited_image_data =
       evas_object_image_data_get(elm_image_object_get(v->image), EINA_FALSE);
   sb->ew = w;
   sb->eh = h;
   evas_object_size_hint_min_set(v->image, w, h);
   evas_object_size_hint_max_set(v->image, w, h);
   elm_table_pack(v->table, v->image, 0, 0, 1, 1);
   elm_object_content_set(v->scroller, v->table);
   
   if (v->fit)
      _viewer_zoom_fit_apply(v);
   else
      _viewer_zoom_set(sb->viewer, _viewer_zoom_get(sb->viewer));
}

static void
_rotate_counterclock(Ephoto_Single_Browser *sb)
{
   switch (sb->orient)
     {
       case EPHOTO_ORIENT_0:
	  sb->orient = EPHOTO_ORIENT_270;
	  break;

       case EPHOTO_ORIENT_90:
	  sb->orient = EPHOTO_ORIENT_0;
	  break;

       case EPHOTO_ORIENT_180:
	  sb->orient = EPHOTO_ORIENT_90;
	  break;

       case EPHOTO_ORIENT_270:
	  sb->orient = EPHOTO_ORIENT_180;
	  break;

       case EPHOTO_ORIENT_FLIP_HORIZ:
	  sb->orient = EPHOTO_ORIENT_FLIP_VERT_90;
	  break;

       case EPHOTO_ORIENT_FLIP_VERT_90:
	  sb->orient = EPHOTO_ORIENT_FLIP_VERT;
	  break;

       case EPHOTO_ORIENT_FLIP_VERT:
	  sb->orient = EPHOTO_ORIENT_FLIP_HORIZ_90;
	  break;

       case EPHOTO_ORIENT_FLIP_HORIZ_90:
	  sb->orient = EPHOTO_ORIENT_FLIP_HORIZ;
	  break;

       default:
	  sb->orient = EPHOTO_ORIENT_0;
	  break;
     }
   _orient_apply(sb);
}

static void
_rotate_clock(Ephoto_Single_Browser *sb)
{
   switch (sb->orient)
     {
       case EPHOTO_ORIENT_0:
	  sb->orient = EPHOTO_ORIENT_90;
	  break;

       case EPHOTO_ORIENT_90:
	  sb->orient = EPHOTO_ORIENT_180;
	  break;

       case EPHOTO_ORIENT_180:
	  sb->orient = EPHOTO_ORIENT_270;
	  break;

       case EPHOTO_ORIENT_270:
	  sb->orient = EPHOTO_ORIENT_0;
	  break;

       case EPHOTO_ORIENT_FLIP_HORIZ:
	  sb->orient = EPHOTO_ORIENT_FLIP_HORIZ_90;
	  break;

       case EPHOTO_ORIENT_FLIP_VERT_90:
	  sb->orient = EPHOTO_ORIENT_FLIP_HORIZ;
	  break;

       case EPHOTO_ORIENT_FLIP_VERT:
	  sb->orient = EPHOTO_ORIENT_FLIP_VERT_90;
	  break;

       case EPHOTO_ORIENT_FLIP_HORIZ_90:
	  sb->orient = EPHOTO_ORIENT_FLIP_VERT;
	  break;

       default:
	  sb->orient = EPHOTO_ORIENT_0;
	  break;
     }
   _orient_apply(sb);
}

static void
_flip_horiz(Ephoto_Single_Browser *sb)
{
   switch (sb->orient)
     {
       case EPHOTO_ORIENT_0:
	  sb->orient = EPHOTO_ORIENT_FLIP_HORIZ;
	  break;

       case EPHOTO_ORIENT_90:
	  sb->orient = EPHOTO_ORIENT_FLIP_VERT_90;
	  break;

       case EPHOTO_ORIENT_180:
	  sb->orient = EPHOTO_ORIENT_FLIP_VERT;
	  break;

       case EPHOTO_ORIENT_270:
	  sb->orient = EPHOTO_ORIENT_FLIP_HORIZ_90;
	  break;

       case EPHOTO_ORIENT_FLIP_HORIZ:
	  sb->orient = EPHOTO_ORIENT_0;
	  break;

       case EPHOTO_ORIENT_FLIP_VERT_90:
	  sb->orient = EPHOTO_ORIENT_90;
	  break;

       case EPHOTO_ORIENT_FLIP_VERT:
	  sb->orient = EPHOTO_ORIENT_180;
	  break;

       case EPHOTO_ORIENT_FLIP_HORIZ_90:
	  sb->orient = EPHOTO_ORIENT_270;
	  break;

       default:
	  sb->orient = EPHOTO_ORIENT_0;
	  break;
     }
   _orient_apply(sb);
}

static void
_flip_vert(Ephoto_Single_Browser *sb)
{
   switch (sb->orient)
     {
       case EPHOTO_ORIENT_0:
	  sb->orient = EPHOTO_ORIENT_FLIP_VERT;
	  break;

       case EPHOTO_ORIENT_90:
	  sb->orient = EPHOTO_ORIENT_FLIP_HORIZ_90;
	  break;

       case EPHOTO_ORIENT_180:
	  sb->orient = EPHOTO_ORIENT_FLIP_HORIZ;
	  break;

       case EPHOTO_ORIENT_270:
	  sb->orient = EPHOTO_ORIENT_FLIP_VERT_90;
	  break;

       case EPHOTO_ORIENT_FLIP_HORIZ:
	  sb->orient = EPHOTO_ORIENT_180;
	  break;

       case EPHOTO_ORIENT_FLIP_VERT_90:
	  sb->orient = EPHOTO_ORIENT_270;
	  break;

       case EPHOTO_ORIENT_FLIP_VERT:
	  sb->orient = EPHOTO_ORIENT_0;
	  break;

       case EPHOTO_ORIENT_FLIP_HORIZ_90:
	  sb->orient = EPHOTO_ORIENT_90;
	  break;

       default:
	  sb->orient = EPHOTO_ORIENT_0;
	  break;
     }
   _orient_apply(sb);
}

static Ephoto_Entry *
_first_entry_find(Ephoto_Single_Browser *sb)
{
   return eina_list_nth(sb->entries, 0);
}

static Ephoto_Entry *
_last_entry_find(Ephoto_Single_Browser *sb)
{
   return eina_list_last_data_get(sb->entries);
}

static void
_zoom_set(Ephoto_Single_Browser *sb, double zoom)
{
   if (zoom <= 0.0)
      return;
   _viewer_zoom_set(sb->viewer, zoom);
}

static void
_zoom_fit(Ephoto_Single_Browser *sb)
{
   if (sb->viewer)
      _viewer_zoom_fit(sb->viewer);
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
_zoom_in_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

   v->zoom_first = EINA_FALSE;
   _zoom_in(sb);
}

static void
_zoom_out_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

   v->zoom_first = EINA_FALSE;
   _zoom_out(sb);
}

static void
_zoom_1_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

   v->zoom_first = EINA_FALSE;
   _zoom_set(sb, 1.0);
}

static void
_zoom_fit_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");
   Eina_Bool first_click;

   if (v->zoom_first)
      first_click = EINA_TRUE;
   else
      first_click = EINA_FALSE;
   v->zoom_first = EINA_FALSE;
   if (first_click)
      v->fit = EINA_FALSE;
   _zoom_fit(sb);
}

static void
_mouse_wheel(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto_Single_Browser *sb = data;
   Evas_Event_Mouse_Wheel *ev = event_info;

   if (!evas_key_modifier_is_set(ev->modifiers, "Control"))
      return;

   if (ev->z > 0)
      _zoom_out(sb);
   else
      _zoom_in(sb);
}

static void
_next_entry(Ephoto_Single_Browser *sb)
{
   Ephoto_Entry *entry = NULL;
   Eina_List *node;

   node = eina_list_data_find_list(sb->entries, sb->entry);
   if (!node)
      return;
   if ((node = node->next))
      entry = node->data;
   if (!entry)
      entry = _first_entry_find(sb);
   if (entry)
     {
	ephoto_single_browser_entry_set(sb->main, entry);
     }
}

static void
_prev_entry(Ephoto_Single_Browser *sb)
{
   Eina_List *node;
   Ephoto_Entry *entry = NULL;
   node = eina_list_data_find_list(sb->entries, sb->entry);
   if (!node)
      return;
   if ((node = node->prev))
      entry = node->data;
   if (!entry)
      entry = _last_entry_find(sb);
   if (entry)
     {
	ephoto_single_browser_entry_set(sb->main, entry);
     }
}

static void
_first_entry(Ephoto_Single_Browser *sb)
{
   Ephoto_Entry *entry = _first_entry_find(sb);

   if (!entry)
      return;

   ephoto_single_browser_entry_set(sb->main, entry);
}

static void
_last_entry(Ephoto_Single_Browser *sb)
{
   Ephoto_Entry *entry = _last_entry_find(sb);

   if (!entry)
      return;

   ephoto_single_browser_entry_set(sb->main, entry);
}

static void
_reset_yes(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto_Single_Browser *sb = evas_object_data_get(popup, "single_browser");

   sb->orient = EPHOTO_ORIENT_0;
   ephoto_single_browser_entry_set(sb->main, sb->entry);
   evas_object_del(popup);
   if (sb->event)
     {
        elm_object_focus_set(sb->event, EINA_TRUE);
        evas_object_freeze_events_set(sb->event, EINA_FALSE);
     }
}

static void
_reset_no(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto_Single_Browser *sb = evas_object_data_get(popup, "single_browser");

   evas_object_del(popup);
   if (sb->event)
     {
        elm_object_focus_set(sb->event, EINA_TRUE);
        evas_object_freeze_events_set(sb->event, EINA_FALSE);
     }
}

static void
_reset_image(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Evas_Object *popup, *box, *label, *ic, *button;

   if (sb->event)
     evas_object_freeze_events_set(sb->event, EINA_TRUE);

   popup = elm_popup_add(sb->ephoto->win);
   elm_object_part_text_set(popup, "title,text", _("Reset Image"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);

   label = elm_label_add(box);
   elm_object_text_set(label,
       _("Are you sure you want to reset your changes?"));
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, label);
   evas_object_show(label);

   ic = elm_icon_add(popup);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "document-save");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Yes"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _reset_yes, popup);
   elm_object_part_content_set(popup, "button1", button);
   evas_object_show(button);

   ic = elm_icon_add(popup);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "window-close");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("No"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _reset_no, popup);
   elm_object_part_content_set(popup, "button2", button);
   evas_object_show(button);

   evas_object_data_set(popup, "single_browser", sb);
   elm_object_part_content_set(popup, "default", box);
   evas_object_show(popup);
}

static void
_save_image(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

   ephoto_file_save_image(sb->ephoto, sb->entry, v->image);
}

static void
_save_image_as(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

   ephoto_file_save_image_as(sb->ephoto, sb->entry, v->image);
}

static void
_upload_image(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   
   ephoto_file_upload_image(sb->ephoto, sb->entry);
}

static void
_delete_image(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Eina_List *files = NULL;

   files = eina_list_append(files, sb->entry->path);
   ephoto_file_delete(sb->ephoto, files, EINA_FILE_REG);
}

static void
_rename_image(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   
   ephoto_file_rename(sb->ephoto, sb->entry->path);
}

static void
_go_first(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   _first_entry(sb);
}

static void
_go_prev(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   _prev_entry(sb);
}

static void
_go_next(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   _next_entry(sb);
}

static void
_go_last(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   _last_entry(sb);
}

static void
_go_rotate_counterclock(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   _rotate_counterclock(sb);
}

static void
_go_rotate_clock(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   _rotate_clock(sb);
}

static void
_go_flip_horiz(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   _flip_horiz(sb);
}

static void
_go_flip_vert(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   _flip_vert(sb);
}

static void
_crop_image(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->viewer)
     {
	sb->editing = EINA_TRUE;
	sb->cropping = EINA_TRUE;
	Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

	elm_table_unpack(v->table, v->image);
	ephoto_cropper_add(sb->ephoto, sb->main, sb->mhbox, v->table, v->image);
     }
}

static void
_go_bcg(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->viewer)
     {
	sb->editing = EINA_TRUE;
        Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

	ephoto_bcg_add(sb->ephoto, sb->main, sb->mhbox, v->image);
     }
}

static void
_go_hsv(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->viewer)
     {
	sb->editing = EINA_TRUE;
	Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

	ephoto_hsv_add(sb->ephoto, sb->main, sb->mhbox, v->image);
     }
}

static void
_go_color(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->viewer)
     {
	sb->editing = EINA_TRUE;
	Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

	ephoto_color_add(sb->ephoto, sb->main, sb->mhbox, v->image);
     }
}

static void
_go_reye(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->viewer)
     {
        sb->editing = EINA_TRUE;
        Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

        ephoto_red_eye_add(sb->ephoto, sb->main, sb->mhbox, v->image);
     }
}

static void
_go_auto_eq(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->viewer)
     {
	sb->editing = EINA_TRUE;
	Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

	ephoto_filter_histogram_eq(sb->main, v->image);
     }
}

static void
_go_blur(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->viewer)
     {
	sb->editing = EINA_TRUE;
	Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

	ephoto_filter_blur(sb->main, v->image);
     }
}

static void
_go_sharpen(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->viewer)
     {
	sb->editing = EINA_TRUE;
	Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

	ephoto_filter_sharpen(sb->main, v->image);
     }
}

static void
_go_black_and_white(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->viewer)
     {
	sb->editing = EINA_TRUE;
        Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

	ephoto_filter_black_and_white(sb->main, v->image);
     }
}

static void
_go_old_photo(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->viewer)
     {
	sb->editing = EINA_TRUE;
	Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

	ephoto_filter_old_photo(sb->main, v->image);
     }
}

static void
_image_changed(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");
   Evas_Coord w, h, sw, sh;
   Edje_Message_Int_Set *msg;

   if (sb->ephoto->state != EPHOTO_STATE_SINGLE)
     return;

   elm_scroller_region_get(v->scroller, 0, 0, &w, &h);
   evas_object_geometry_get(v->scroller, 0, 0, &sw, &sh);

   sw -= w;
   sh -= h;
   msg = alloca(sizeof(Edje_Message_Int_Set) + (2 * sizeof(int)));
   msg->count = 2;
   msg->val[0] = sw;
   msg->val[1] = sh;
   edje_object_message_send(elm_layout_edje_get(sb->ephoto->layout),
       EDJE_MESSAGE_INT_SET, 1, msg);
}

/*Image Viewer Function*/
static Evas_Object *
_viewer_add(Evas_Object *parent, const char *path, Ephoto_Single_Browser *sb)
{
   Ephoto_Viewer *v = calloc(1, sizeof(Ephoto_Viewer));
   int err;

   v->zoom_first = EINA_TRUE;
   Evas_Coord w, h;
   const char *group = _ephoto_get_edje_group(path);

   v->scroller = elm_scroller_add(parent);
   evas_object_size_hint_weight_set(v->scroller, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(v->scroller,
       EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_data_set(v->scroller, "viewer", v);
   evas_object_event_callback_add(v->scroller, EVAS_CALLBACK_MOUSE_UP,
       _scroller_mouse_up_cb, sb);
   evas_object_event_callback_add(v->scroller, EVAS_CALLBACK_DEL, _viewer_del,
       v);
   evas_object_show(v->scroller);

   v->table = elm_table_add(v->scroller);
   evas_object_size_hint_weight_set(v->table, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(v->table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_content_set(v->scroller, v->table);
   evas_object_show(v->table);

   v->image = elm_image_add(v->table);
   elm_object_style_set(v->image, "ephoto");
   elm_image_preload_disabled_set(v->image, EINA_TRUE);
   elm_image_file_set(v->image, path, group);
   err = evas_object_image_load_error_get(elm_image_object_get(v->image));
   if (err != EVAS_LOAD_ERROR_NONE)
     goto error;
   evas_object_image_size_get(elm_image_object_get(v->image), &w, &h);
   elm_drop_target_add(v->image, ELM_SEL_FORMAT_IMAGE, NULL, NULL, NULL, NULL,
       NULL, NULL, NULL, NULL);
   evas_object_size_hint_min_set(v->image, w, h);
   evas_object_size_hint_max_set(v->image, w, h);
   evas_object_event_callback_add(v->image, EVAS_CALLBACK_MOUSE_DOWN,
       _image_mouse_down_cb, sb);
   evas_object_event_callback_add(v->image, EVAS_CALLBACK_MOUSE_UP,
       _image_mouse_up_cb, sb);
   evas_object_event_callback_add(v->image, EVAS_CALLBACK_RESIZE, _image_changed, sb);
   elm_table_pack(v->table, v->image, 0, 0, 1, 1);
   evas_object_show(v->image);
   if (elm_image_animated_available_get(v->image))
     {
        elm_image_animated_set(v->image, EINA_TRUE);
        elm_image_animated_play_set(v->image, EINA_TRUE);
     }


   v->monitor = ecore_file_monitor_add(path, _monitor_cb, sb);
   return v->scroller;

  error:
   evas_object_event_callback_del(v->scroller, EVAS_CALLBACK_DEL, _viewer_del);
   evas_object_data_del(v->scroller, "viewer");
   free(v);
   return NULL;
}

/*Single Browser Populating Functions*/
static void
_entry_free(void *data, const Ephoto_Entry *entry)
{
   Ephoto_Single_Browser *sb = data;

   if (entry == sb->entry)
     {
        if (eina_list_count(sb->entries) <= 1)
          evas_object_smart_callback_call(sb->main, "back", NULL);
        else
          _next_entry(sb);
     }
}

static Eina_Bool
_ephoto_single_populate_end(void *data EINA_UNUSED, int type EINA_UNUSED,
    void *event EINA_UNUSED)
{
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ephoto_single_entry_create(void *data, int type EINA_UNUSED,
    void *event EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Ephoto_Event_Entry_Create *ev = event;
   Ephoto_Entry *e;

   e = ev->entry;
   if (sb->pending_path && !strcmp(e->path, sb->pending_path))
     {
	eina_stringshare_del(sb->pending_path);
	sb->pending_path = NULL;
	ephoto_single_browser_entry_set(sb->ephoto->single_browser, e);
     }

   return ECORE_CALLBACK_PASS_ON;
}

static void
_ephoto_single_browser_recalc(Ephoto_Single_Browser *sb)
{
   if (sb->viewer)
     {
	evas_object_del(sb->viewer);
	sb->viewer = NULL;
     }
   if (sb->nolabel)
     {
	evas_object_del(sb->nolabel);
	sb->nolabel = NULL;
     }
   if (sb->entry)
     {
	const char *bname = ecore_file_file_get(sb->entry->path);

	sb->viewer = _viewer_add(sb->main, sb->entry->path, sb);
	if (sb->viewer)
	  {
	     elm_box_pack_start(sb->mhbox, sb->viewer);
	     evas_object_show(sb->viewer);
	     evas_object_event_callback_add(sb->viewer,
		 EVAS_CALLBACK_MOUSE_WHEEL, _mouse_wheel, sb);
             _ephoto_update_bottom_bar(sb);
	     ephoto_title_set(sb->ephoto, bname);

             if (!_ephoto_file_image_can_save(strrchr(bname, '.')+1))
               elm_object_item_disabled_set(sb->save, EINA_TRUE);
             else
               elm_object_item_disabled_set(sb->save, EINA_FALSE);
	  }
        else
	  {
	     sb->nolabel = elm_label_add(sb->mhbox);
	     elm_label_line_wrap_set(sb->nolabel, ELM_WRAP_WORD);
	     elm_object_text_set(sb->nolabel,
		 _("This image does not exist or is corrupted!"));
	     evas_object_size_hint_weight_set(sb->nolabel, EVAS_HINT_EXPAND,
		 EVAS_HINT_EXPAND);
	     evas_object_size_hint_align_set(sb->nolabel, EVAS_HINT_FILL,
		 EVAS_HINT_FILL);
	     elm_box_pack_start(sb->mhbox, sb->nolabel);
	     evas_object_show(sb->nolabel);
	     ephoto_title_set(sb->ephoto, _("Bad Image"));
	  }
     }
}

/*Ephoto Main Callbacks*/
static void
_add_edit_menu_items(Ephoto_Single_Browser *sb, Evas_Object *menu)
{
   Evas_Object *menu_it, *menu_itt;

   menu_it =
       elm_menu_item_add(menu, NULL, "system-file-manager", _("File"), NULL, NULL);
   elm_menu_item_add(menu, menu_it, "edit-undo", _("Reset"), _reset_image, sb);
   elm_menu_item_add(menu, menu_it, "document-save", _("Save"), _save_image, sb);
   elm_menu_item_add(menu, menu_it, "document-save-as", _("Save As"),
       _save_image_as, sb);
   elm_menu_item_add(menu, menu_it, "document-send", _("Upload"), _upload_image,
       sb);
   elm_menu_item_add(menu, menu_it, "edit", _("Rename"),
            _rename_image, sb);
   elm_menu_item_add(menu, menu_it, "edit-delete", _("Delete"),
            _delete_image, sb);
   menu_it =
       elm_menu_item_add(menu, NULL, "document-properties", _("Edit"), NULL, NULL);

   menu_itt =
       elm_menu_item_add(menu, menu_it, "document-properties", _("Transform"),
       NULL, NULL);
   elm_menu_item_add(menu, menu_itt, "edit-cut", _("Crop"), _crop_image, sb);
   elm_menu_item_separator_add(menu, menu_itt);
   elm_menu_item_add(menu, menu_itt, "object-rotate-left", _("Rotate Left"),
       _go_rotate_counterclock, sb);
   elm_menu_item_add(menu, menu_itt, "object-rotate-right", _("Rotate Right"),
       _go_rotate_clock, sb);
   elm_menu_item_add(menu, menu_itt, "object-flip-horizontal",
       _("Flip Horizontal"), _go_flip_horiz, sb);
   elm_menu_item_add(menu, menu_itt, "object-flip-vertical", _("Flip Vertical"),
       _go_flip_vert, sb);

   menu_itt =
       elm_menu_item_add(menu, menu_it, "document-properties", _("Color"), NULL,
       NULL);
   elm_menu_item_add(menu, menu_itt, "insert-image", _("Auto Equalize"),
       _go_auto_eq, sb);
   elm_menu_item_separator_add(menu, menu_itt);
   elm_menu_item_add(menu, menu_itt, "insert-image",
       _("Brightness/Contrast/Gamma"), _go_bcg, sb);
   elm_menu_item_add(menu, menu_itt, "insert-image", _("Hue/Saturation/Value"),
       _go_hsv, sb);
   elm_menu_item_add(menu, menu_itt, "insert-image", _("Red Eye Removal"),
       _go_reye, sb);
   elm_menu_item_add(menu, menu_itt, "insert-image", _("Color Levels"),
       _go_color, sb);

   menu_itt =
       elm_menu_item_add(menu, menu_it, "document-properties", _("Filters"), NULL,
       NULL);
   elm_menu_item_add(menu, menu_itt, "insert-image", _("Blur"), _go_blur, sb);
   elm_menu_item_add(menu, menu_itt, "insert-image", _("Sharpen"), _go_sharpen,
       sb);
   elm_menu_item_separator_add(menu, menu_itt);
   elm_menu_item_add(menu, menu_itt, "insert-image", _("Black and White"),
       _go_black_and_white, sb);
   elm_menu_item_add(menu, menu_itt, "insert-image", _("Old Photo"),
       _go_old_photo, sb);

   menu_it =
       elm_menu_item_add(menu, NULL, "document-properties", _("View"), NULL,
       NULL);  
   menu_itt =
       elm_menu_item_add(menu, menu_it, "go-next", _("Go"), NULL, NULL);
   elm_menu_item_add(menu, menu_itt, "go-first", _("First"), _go_first, sb);
   elm_menu_item_add(menu, menu_itt, "go-previous", _("Previous"), _go_prev, sb);
   elm_menu_item_add(menu, menu_itt, "go-next", _("Next"), _go_next, sb);
   elm_menu_item_add(menu, menu_itt, "go-last", _("Last"), _go_last, sb);
   menu_itt =
       elm_menu_item_add(menu, menu_it, "zoom-in", _("Zoom"), NULL, NULL);
   elm_menu_item_add(menu, menu_itt, "zoom-in", _("Zoom In"), _zoom_in_cb,
       sb);
   elm_menu_item_add(menu, menu_itt, "zoom-out", _("Zoom Out"), _zoom_out_cb,
       sb);
   elm_menu_item_add(menu, menu_itt, "zoom-fit-best", _("Zoom Fit"),
       _zoom_fit_cb, sb);
   elm_menu_item_add(menu, menu_itt, "zoom-original", _("Zoom 1:1"),
       _zoom_1_cb, sb);
}

static void
_ephoto_main_edit_menu(Ephoto_Single_Browser *sb)
{
   Evas_Object *menu;
   Evas_Coord x, y;

   evas_pointer_canvas_xy_get(evas_object_evas_get(sb->main), &x, &y);
   menu = elm_menu_add(sb->main);
   elm_menu_move(menu, x, y);

   _add_edit_menu_items(sb, menu);
   
   evas_object_smart_callback_add(menu, "dismissed", _menu_dismissed_cb,
       sb);
   evas_object_show(menu);
}

static void
_ephoto_main_key_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto_Single_Browser *sb = data;
   Evas_Event_Key_Down *ev = event_info;
   Eina_Bool ctrl = evas_key_modifier_is_set(ev->modifiers, "Control");
   Eina_Bool shift = evas_key_modifier_is_set(ev->modifiers, "Shift");
   const char *k = ev->keyname;

   if (ctrl)
     {
	if ((!strcmp(k, "plus")) || (!strcmp(k, "equal")))
	   _zoom_in(sb);
	else if (!strcmp(k, "minus"))
	   _zoom_out(sb);
	else if (!strcmp(k, "0"))
	  {
	     if (shift)
		_zoom_fit(sb);
	     else
		_zoom_set(sb, 1.0);
	  }
        else if (!strcmp(k, "f") && !sb->editing)
          {
             if (shift)
               ephoto_show_folders(sb->ephoto, EINA_TRUE);
          }
        else if (!strcmp(k, "l") && !sb->editing)
          {
             if (!shift)
               _rotate_counterclock(sb);
             else
               _flip_horiz(sb);
          }
        else if (!strcmp(k, "r") && !sb->editing)
          {
             if (!shift)
               _rotate_clock(sb);
             else
               _flip_vert(sb);
          }
        else if (!strcmp(k, "Delete"))
          {
             _delete_image(sb, NULL, NULL);
          }
        else if (!strcmp(k, "s"))
          {
             if (!shift)
               _save_image_as(sb, NULL, NULL);
             else
               _save_image(sb, NULL, NULL);
          }
        else if (!strcmp(k, "u"))
          {
             _reset_image(sb, NULL, NULL);
          }
	return;
     }

   if (!strcmp(k, "Escape") && !sb->editing)
     {
	if (sb->event)
	  {
	     evas_object_del(sb->event);
	     sb->event = NULL;
	  }
	evas_object_smart_callback_call(sb->main, "back", sb->entry);
     }
   else if (!strcmp(k, "Left") && !sb->editing)
      _prev_entry(sb);
   else if (!strcmp(k, "Right") && !sb->editing)
      _next_entry(sb);
   else if (!strcmp(k, "Home") && !sb->editing)
      _first_entry(sb);
   else if (!strcmp(k, "End") && !sb->editing)
      _last_entry(sb);
   else if (!strcmp(k, "F1"))
     {
        _ephoto_show_settings(sb, NULL, NULL);
     }
   else if (!strcmp(k, "F2"))
     {
        _rename_image(sb, NULL, NULL);
     }
   else if (!strcmp(k, "F5") && !sb->editing)
     {
	if (sb->entry)
	  {
	     if (sb->event)
	       {
		  evas_object_del(sb->event);
		  sb->event = NULL;
	       }
       evas_object_smart_callback_call(sb->main, "slideshow", sb->entry);
	  }
     }
   else if (!strcmp(k, "F11"))
     {
	Evas_Object *win = sb->ephoto->win;

	elm_win_fullscreen_set(win, !elm_win_fullscreen_get(win));
     }
}

void
ephoto_single_browser_slideshow(Evas_Object *obj)
{
   Ephoto_Single_Browser *sb = evas_object_data_get(obj, "single_browser");

   if (sb->entry)
     {
        if (sb->event)
          {
             evas_object_del(sb->event);
             sb->event = NULL;
          }
        evas_object_smart_callback_call(sb->main, "slideshow", sb->entry);
     }
}

static void
_ephoto_show_settings(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   ephoto_config_main(sb->ephoto);
}

static void
_ephoto_main_back(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->event)
     {
        evas_object_del(sb->event);
        sb->event = NULL;
     }
   evas_object_smart_callback_call(sb->main, "back", sb->entry);
}

static void
_ephoto_main_focused(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->ephoto->state == EPHOTO_STATE_SINGLE)
     {
	if (sb->event)
          {
             elm_object_focus_set(sb->event, EINA_TRUE);
             evas_object_raise(sb->event);
          }
     }
}

static void
_ephoto_main_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   char tmp_path[PATH_MAX];
   Ephoto_Single_Browser *sb = data;
   Ecore_Event_Handler *handler;
   Eina_Iterator *tmps;
   Eina_File_Direct_Info *info;

   EINA_LIST_FREE(sb->handlers, handler) ecore_event_handler_del(handler);
   if (sb->entry)
      ephoto_entry_free_listener_del(sb->entry, _entry_free, sb);
   if (sb->pending_path)
      eina_stringshare_del(sb->pending_path);
   snprintf(tmp_path, PATH_MAX, "%s/.config/ephoto/", getenv("HOME"));
   tmps = eina_file_stat_ls(tmp_path);
   EINA_ITERATOR_FOREACH(tmps, info)
   {
      const char *bname = info->path + info->name_start;

      if (!strncmp(bname, "tmp", 3))
	 ecore_file_unlink(info->path);
   }
   free(sb);
}

/*Ephoto Single Browser Public Functions*/
void
ephoto_single_browser_adjust_offsets(Ephoto *ephoto)
{
   Ephoto_Single_Browser *sb = evas_object_data_get(ephoto->single_browser, 
       "single_browser");
   Edje_Message_Int_Set *msg;

   msg = alloca(sizeof(Edje_Message_Int_Set) + (2 * sizeof(int)));
   msg->count = 2;
   msg->val[0] = 0;
   msg->val[1] = 0;
   edje_object_message_send(elm_layout_edje_get(sb->ephoto->layout),
       EDJE_MESSAGE_INT_SET, 1, msg);
   
   _image_changed(sb, NULL, NULL, NULL);
}

void
ephoto_single_browser_entries_set(Evas_Object *obj, Eina_List *entries)
{
   Ephoto_Single_Browser *sb = evas_object_data_get(obj, "single_browser");

   if (entries)
     sb->entries = entries;
}

void
ephoto_single_browser_entry_set(Evas_Object *obj, Ephoto_Entry *entry)
{
   Ephoto_Single_Browser *sb = evas_object_data_get(obj, "single_browser");

   if (sb->entry)
      ephoto_entry_free_listener_del(sb->entry, _entry_free, sb);

   sb->entry = entry;

   if (entry)
     ephoto_entry_free_listener_add(entry, _entry_free, sb);

   _ephoto_single_browser_recalc(sb);
   if (sb->edited_image_data)
     {
	sb->edited_image_data = NULL;
	sb->ew = 0;
	sb->eh = 0;
     }
   if (sb->viewer)
      _zoom_fit(sb);
   if (sb->event)
     {
	evas_object_del(sb->event);
	sb->event = NULL;
     }
   sb->event = evas_object_rectangle_add(sb->ephoto->win);
   evas_object_smart_member_add(sb->event, sb->ephoto->win);
   evas_object_color_set(sb->event, 0, 0, 0, 0);
   evas_object_repeat_events_set(sb->event, EINA_TRUE);
   evas_object_show(sb->event);
   evas_object_event_callback_add(sb->event, EVAS_CALLBACK_KEY_DOWN, _ephoto_main_key_down,
       sb);
   evas_object_raise(sb->event);
   elm_object_focus_set(sb->event, EINA_TRUE);
}

void
ephoto_single_browser_focus_set(Ephoto *ephoto)
{
   Ephoto_Single_Browser *sb = evas_object_data_get
       (ephoto->single_browser, "single_browser");

   if (sb->event)
     {
        evas_object_raise(sb->event);
        elm_object_focus_set(sb->event, EINA_TRUE);
     }
}

void
ephoto_single_browser_path_pending_set(Evas_Object *obj, const char *path)
{
   Ephoto_Single_Browser *sb = evas_object_data_get(obj, "single_browser");

   ephoto_single_browser_path_pending_unset(obj);
   sb->pending_path = eina_stringshare_add(path);
}

void
ephoto_single_browser_path_pending_unset(Evas_Object *obj)
{
   Ephoto_Single_Browser *sb = evas_object_data_get(obj, "single_browser");

   if (sb->pending_path)
     {
        eina_stringshare_del(sb->pending_path);
        sb->pending_path = NULL;
     }
}

void
ephoto_single_browser_path_created(Evas_Object *obj, Ephoto_Entry *entry)
{
   Ephoto_Single_Browser *sb = evas_object_data_get(obj, "single_browser");

   if (sb->pending_path && !strcmp(entry->path, sb->pending_path))
     {
        eina_stringshare_del(sb->pending_path);
        sb->pending_path = NULL;
        ephoto_single_browser_entry_set(sb->ephoto->single_browser, entry);
     }
}

void
ephoto_single_browser_image_data_update(Evas_Object *main, Evas_Object *image,
    unsigned int *image_data, Evas_Coord w, Evas_Coord h)
{
   Ephoto_Single_Browser *sb = evas_object_data_get(main, "single_browser");

   if (sb->editing)
     {
	evas_object_image_data_set(elm_image_object_get(image), image_data);
	evas_object_image_data_update_add(elm_image_object_get(image), 0, 0, w,
	    h);
     }
}

void
ephoto_single_browser_image_data_done(Evas_Object *main,
    unsigned int *image_data, Evas_Coord w, Evas_Coord h)
{
   Ephoto_Single_Browser *sb = evas_object_data_get(main, "single_browser");

   if (sb->editing)
     {
        _ephoto_single_browser_recalc(sb);
        Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");
        if (sb->cropping)
             sb->cropping = EINA_FALSE;
        evas_object_image_size_set(elm_image_object_get(v->image), w, h);
        evas_object_image_data_set(elm_image_object_get(v->image), image_data);
        evas_object_image_data_update_add(elm_image_object_get(v->image), 0, 0, w,
            h);

        _ephoto_update_bottom_bar(sb);
        sb->edited_image_data = image_data;
        sb->ew = w;
        sb->eh = h;
        sb->editing = EINA_FALSE;
        _zoom_fit(sb);
     }
}

void
ephoto_single_browser_cancel_editing(Evas_Object *main)
{
   Ephoto_Single_Browser *sb = evas_object_data_get(main, "single_browser");

   if (sb->editing)
     {
        _ephoto_single_browser_recalc(sb);
        Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");
	if (sb->cropping)
	   sb->cropping = EINA_FALSE;
	if (sb->edited_image_data)
	  {
	     evas_object_image_size_set(elm_image_object_get(v->image), sb->ew,
		 sb->eh);
	     evas_object_image_data_set(elm_image_object_get(v->image),
		 sb->edited_image_data);
	     evas_object_image_data_update_add(elm_image_object_get(v->image), 0,
		 0, sb->ew, sb->eh);
	  }
	sb->editing = EINA_FALSE;
	_zoom_fit(sb);
     }
}

void
ephoto_single_browser_show_controls(Ephoto *ephoto)
{
   Ephoto_Single_Browser *sb = evas_object_data_get(ephoto->single_browser,
       "single_browser");
   Evas_Object *but, *ic;
   int ret;

   ic = elm_icon_add(ephoto->controls_left);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   ret = elm_icon_standard_set(ic, "view-list-icons");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   if (!ret)
     ret = elm_image_file_set(ic, PACKAGE_DATA_DIR "/images/grid.png", NULL);

   but = elm_button_add(ephoto->controls_left);
   if (!ret)
     elm_object_text_set(but, _("Thumbnails"));
   elm_object_part_content_set(but, "icon", ic);
   elm_object_tooltip_text_set(but, _("View Thumbnails"));
   elm_object_tooltip_orient_set(but, ELM_TOOLTIP_ORIENT_TOP);
   evas_object_smart_callback_add(but, "clicked", _ephoto_main_back, sb);
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
      evas_object_smart_callback_add(but, "clicked", _zoom_in_cb, sb);
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
   evas_object_smart_callback_add(but, "clicked", _zoom_out_cb, sb);
   elm_box_pack_end(ephoto->controls_left, but);
   evas_object_show(but);

   ic = elm_icon_add(ephoto->controls_right);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   ret = elm_icon_standard_set(ic, "go-previous");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   
   but = elm_button_add(ephoto->controls_right);
   if (!ret)
     elm_object_text_set(but, _("Previous"));
   elm_object_part_content_set(but, "icon", ic);
   elm_object_tooltip_text_set(but, _("Previous"));
   elm_object_tooltip_orient_set(but, ELM_TOOLTIP_ORIENT_TOP);
   evas_object_smart_callback_add(but, "clicked", _go_prev, sb);
   elm_box_pack_end(ephoto->controls_right, but);
   evas_object_show(but);

   ic = elm_icon_add(ephoto->controls_right);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   ret = elm_icon_standard_set(ic, "go-next");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   
   but = elm_button_add(ephoto->controls_right);
   if (!ret)
     elm_object_text_set(but, _("Next"));
   elm_object_part_content_set(but, "icon", ic);
   elm_object_tooltip_text_set(but, _("Next"));
   elm_object_tooltip_orient_set(but, ELM_TOOLTIP_ORIENT_TOP);
   evas_object_smart_callback_add(but, "clicked", _go_next, sb);
   elm_box_pack_end(ephoto->controls_right, but);
   evas_object_show(but);
}

Evas_Object *
ephoto_single_browser_add(Ephoto *ephoto, Evas_Object *parent)
{
   Evas_Object *box = elm_box_add(parent);
   Ephoto_Single_Browser *sb;

   EINA_SAFETY_ON_NULL_RETURN_VAL(box, NULL);

   sb = calloc(1, sizeof(Ephoto_Single_Browser));
   EINA_SAFETY_ON_NULL_GOTO(sb, error);

   sb->ephoto = ephoto;
   sb->editing = EINA_FALSE;
   sb->cropping = EINA_FALSE;
   sb->main = box;

   elm_box_horizontal_set(sb->main, EINA_FALSE);
   elm_object_tree_focus_allow_set(sb->main, EINA_FALSE);
   evas_object_event_callback_add(sb->main, EVAS_CALLBACK_DEL, _ephoto_main_del, sb);
   evas_object_event_callback_add(sb->main, EVAS_CALLBACK_KEY_DOWN, _ephoto_main_key_down,
       sb);
   evas_object_event_callback_add(sb->ephoto->win, EVAS_CALLBACK_FOCUS_IN,
       _ephoto_main_focused, sb);
   evas_object_size_hint_weight_set(sb->main, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(sb->main, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_data_set(sb->main, "single_browser", sb);

   sb->mhbox = elm_box_add(sb->main);
   elm_box_horizontal_set(sb->mhbox, EINA_TRUE);
   evas_object_size_hint_weight_set(sb->mhbox, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(sb->mhbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(sb->main, sb->mhbox);
   evas_object_show(sb->mhbox);


   sb->handlers =
       eina_list_append(sb->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_POPULATE_END,
	   _ephoto_single_populate_end, sb));

   sb->handlers =
       eina_list_append(sb->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_ENTRY_CREATE,
	   _ephoto_single_entry_create, sb));

   sb->orient = EPHOTO_ORIENT_0;

   return sb->main;

  error:
   evas_object_del(sb->main);
   return NULL;
}

