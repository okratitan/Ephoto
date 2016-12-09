#include "ephoto.h"

#define ZOOM_STEP 0.2

static Ecore_Timer *_1s_hold = NULL;

typedef struct _Ephoto_Single_Browser Ephoto_Single_Browser;
typedef struct _Ephoto_Viewer Ephoto_Viewer;
typedef struct _Ephoto_History Ephoto_History;

struct _Ephoto_Single_Browser
{
   Ephoto *ephoto;
   Evas_Object *main;
   Evas_Object *mhbox;
   Evas_Object *table;
   Evas_Object *viewer;
   Evas_Object *nolabel;
   Evas_Object *event;
   Evas_Object *edit_main;
   Elm_Object_Item *save;
   const char *pending_path;
   Ephoto_Entry *entry;
   Ephoto_Orient orient;
   Eina_List *handlers;
   Eina_List *entries;
   Eina_List *history;
   unsigned int history_pos;
   Eina_Bool editing:1;
   Eina_Bool cropping:1;
};

struct _Ephoto_Viewer
{
   Eina_List *handlers;
   Eio_Monitor *monitor;
   Eina_List *monitor_handlers;
   Evas_Object *scroller;
   Evas_Object *table;
   Evas_Object *image;
   double zoom;
   Eina_Bool fit:1;
   Eina_Bool zoom_first:1;
   double duration;
   int frame_count;
   int cur_frame;
   Ecore_Timer *anim_timer;
};

struct _Ephoto_History
{
   unsigned int *im_data;
   Evas_Coord w;
   Evas_Coord h;
   Ephoto_Orient orient;
};

/*Common Callbacks*/
static const char *_ephoto_get_edje_group(const char  *path);
static char *_ephoto_get_file_size(const char *path);
static void _ephoto_update_bottom_bar(Ephoto_Single_Browser *sb);

/*Main Callbacks*/
static void _ephoto_main_edit_menu(Ephoto_Single_Browser *sb);
static void _ephoto_main_key_down(void *data, Evas *e EINA_UNUSED,
    Evas_Object *obj EINA_UNUSED,void *event_info EINA_UNUSED);
static void _ephoto_show_settings(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED);
static void _ephoto_main_back(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED);
static void _ephoto_main_del(void *data, Evas *e EINA_UNUSED,
    Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED);
static void _next_entry(Ephoto_Single_Browser *sb);
static void _orient_apply(Ephoto_Single_Browser *sb);

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

   evas_object_image_size_get(v->image,
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
        if (!elm_win_fullscreen_get(sb->ephoto->win))
          {
             elm_win_fullscreen_set(sb->ephoto->win, EINA_TRUE);
             elm_box_pack_end(sb->ephoto->statusbar, sb->ephoto->exit);
             evas_object_show(sb->ephoto->exit);
          }
        else
          {
             elm_win_fullscreen_set(sb->ephoto->win, EINA_FALSE);
             elm_box_unpack(sb->ephoto->statusbar, sb->ephoto->exit);
             evas_object_hide(sb->ephoto->exit);
          }
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
   Ecore_Event_Handler *handler;
   if (v->monitor)
     {
        eio_monitor_del(v->monitor);
        EINA_LIST_FREE(v->monitor_handlers, handler)
          ecore_event_handler_del(handler);
     }
   if (v->anim_timer)
     ecore_timer_del(v->anim_timer);
   free(v);
}

static Eina_Bool
_monitor_cb(void *data, int type,
    void *event EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

   if (eina_list_count(sb->history))
     return ECORE_CALLBACK_PASS_ON;
   if (type == EIO_MONITOR_FILE_MODIFIED)
     {
        if (!ecore_file_exists(sb->entry->path))
          {
             if (eina_list_count(sb->entries) > 1)
               _next_entry(sb);
             else
               _ephoto_main_back(sb, NULL, NULL);
             ephoto_entry_free(sb->ephoto, sb->entry);
          }
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
                  evas_object_image_file_set(v->image, sb->entry->path, group);
                  evas_object_show(v->image);
               }
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

static void
_viewer_zoom_apply(Ephoto_Viewer *v, double zoom)
{
   v->zoom = zoom;

   Evas_Coord w, h;
   Evas_Object *image;

   image = v->image;
   evas_object_image_size_get(image, &w, &h);
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
   evas_object_image_size_get(image, &iw, &ih);

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
	evas_object_image_size_get(image, &iw, &ih);

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
_orient_set(Ephoto_Single_Browser *sb, Ephoto_Orient orient)
{
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

   switch (orient)
     {
       case EPHOTO_ORIENT_0:
          evas_object_image_orient_set(v->image, EVAS_IMAGE_ORIENT_NONE);
          break;

       case EPHOTO_ORIENT_90:
          evas_object_image_orient_set(v->image, EVAS_IMAGE_ORIENT_90);
          break;

       case EPHOTO_ORIENT_180:
          evas_object_image_orient_set(v->image, EVAS_IMAGE_ORIENT_180);
          break;

       case EPHOTO_ORIENT_270:
          evas_object_image_orient_set(v->image, EVAS_IMAGE_ORIENT_270);
          break;

       case EPHOTO_ORIENT_FLIP_HORIZ:
          evas_object_image_orient_set(v->image, EVAS_IMAGE_FLIP_HORIZONTAL);
          break;

       case EPHOTO_ORIENT_FLIP_VERT:
          evas_object_image_orient_set(v->image, EVAS_IMAGE_FLIP_VERTICAL);
          break;

       case EPHOTO_ORIENT_FLIP_HORIZ_90:
          evas_object_image_orient_set(v->image, EVAS_IMAGE_FLIP_TRANSPOSE);
          break;

       case EPHOTO_ORIENT_FLIP_VERT_90:
          evas_object_image_orient_set(v->image, EVAS_IMAGE_FLIP_TRANSVERSE);
          break;

       default:
          return;
     }
}

static void
_orient_apply(Ephoto_Single_Browser *sb)
{
   Ephoto_History *eh = NULL;
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");
   Evas_Coord w, h;
   Eina_List *l;
   char buf[PATH_MAX];

   _orient_set(sb, sb->orient);
   elm_table_unpack(v->table, v->image);
   elm_object_content_unset(v->scroller);
   evas_object_image_size_get(v->image, &w, &h);
   evas_object_image_data_update_add(v->image, 0, 0, w, h);
   if (sb->history_pos < (eina_list_count(sb->history)-1))
     {
        int count;

        count = sb->history_pos + 1;
        l = eina_list_nth_list(sb->history, count);
        while (l)
          {
             eh = eina_list_data_get(l);
             sb->history = eina_list_remove_list(sb->history, l);
             free(eh->im_data);
             free(eh);
             eh = NULL;
             l = eina_list_nth_list(sb->history, count);
          }
     }
   eh = calloc(1, sizeof(Ephoto_History));
   eh->im_data = malloc(sizeof(unsigned int) * w * h);
   eh->im_data = memcpy(eh->im_data, evas_object_image_data_get(v->image, EINA_FALSE),
       sizeof(unsigned int) * w * h);
   eh->w = w;
   eh->h = h;
   eh->orient = sb->orient;
   sb->history = eina_list_append(sb->history, eh);
   sb->history_pos = eina_list_count(sb->history) - 1;
   snprintf(buf, PATH_MAX, "%s [%s]", sb->entry->basename, _("MODIFIED"));
   ephoto_title_set(sb->ephoto, buf);
   _ephoto_update_bottom_bar(sb);
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
_undo_image(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");
   Ephoto_History *eh;

   if (!v)
     return;

   if (sb->history && sb->history_pos > 0)
     {
        sb->history_pos--;
        eh = eina_list_nth(sb->history, sb->history_pos);
        elm_table_unpack(v->table, v->image);
        elm_object_content_unset(v->scroller);
        _orient_set(sb, eh->orient);
        evas_object_image_size_set(v->image, eh->w, eh->h);
        evas_object_image_data_set(v->image, eh->im_data);
        evas_object_image_data_update_add(v->image, 0, 0, eh->w, eh->h);
        evas_object_size_hint_min_set(v->image, eh->w, eh->h);
        evas_object_size_hint_max_set(v->image, eh->w, eh->h);
        elm_table_pack(v->table, v->image, 0, 0, 1, 1);
        if (sb->orient != eh->orient)
          sb->orient = eh->orient;
        if (v->fit)
          _viewer_zoom_fit_apply(v);
        else
          _viewer_zoom_set(sb->viewer, _viewer_zoom_get(sb->viewer));
     }
}

static void
_redo_image(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");
   Ephoto_History *eh;

   if (!v)
     return;

   if (sb->history && sb->history_pos < (eina_list_count(sb->history)-1))
     {
        sb->history_pos++;
        eh = eina_list_nth(sb->history, sb->history_pos);
        elm_table_unpack(v->table, v->image);
        elm_object_content_unset(v->scroller);
        _orient_set(sb, eh->orient);
        evas_object_image_size_set(v->image, eh->w, eh->h);
        evas_object_image_data_set(v->image, eh->im_data);
        evas_object_image_data_update_add(v->image, 0, 0, eh->w, eh->h);
        evas_object_size_hint_min_set(v->image, eh->w, eh->h);
        evas_object_size_hint_max_set(v->image, eh->w, eh->h);
        elm_table_pack(v->table, v->image, 0, 0, 1, 1);
        if (sb->orient != eh->orient)
          sb->orient = eh->orient;
        if (v->fit)
          _viewer_zoom_fit_apply(v);
        else
          _viewer_zoom_set(sb->viewer, _viewer_zoom_get(sb->viewer));
     }
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
     }
}

static void
_reset_image(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Evas_Object *popup, *box, *label, *ic, *button;

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
_close_editor(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   edje_object_signal_emit(elm_layout_edje_get(sb->ephoto->layout),
       "ephoto,editor,hide", "ephoto");
   evas_object_del(sb->edit_main);
   sb->edit_main = NULL;
   sb->ephoto->editor_blocking = EINA_FALSE;
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
_scale_image(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->viewer)
     {
        sb->editing = EINA_TRUE;
        Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

        ephoto_scale_add(sb->ephoto, sb->main, sb->mhbox, v->image, sb->entry->path);
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

	ephoto_filter_histogram_eq(sb->ephoto, v->image);
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

	ephoto_filter_blur(sb->ephoto, v->image);
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

	ephoto_filter_sharpen(sb->ephoto, v->image);
     }
}

static void
_go_dither(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->viewer)
     {
        sb->editing = EINA_TRUE;
        Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

        ephoto_filter_dither(sb->ephoto, v->image);
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

	ephoto_filter_black_and_white(sb->ephoto, v->image);
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

	ephoto_filter_old_photo(sb->ephoto, v->image);
     }
}

static void
_go_posterize(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->viewer)
     {
        sb->editing = EINA_TRUE;
        Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

        ephoto_filter_posterize(sb->ephoto, v->image);
     }
}

static void
_go_painting(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->viewer)
     {
        sb->editing = EINA_TRUE;
        Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

        ephoto_filter_painting(sb->ephoto, v->image);
     }
}

static void
_go_invert(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->viewer)
     {
        sb->editing = EINA_TRUE;
        Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

        ephoto_filter_invert(sb->ephoto, v->image);
     }
}

static void
_go_sketch(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->viewer)
     {
        sb->editing = EINA_TRUE;
        Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

        ephoto_filter_sketch(sb->ephoto, v->image);
     }
}

static void
_go_edge(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->viewer)
     {
        sb->editing = EINA_TRUE;
        Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

        ephoto_filter_edge(sb->ephoto, v->image);
     }
}

static void
_go_emboss(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->viewer)
     {
        sb->editing = EINA_TRUE;
        Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

        ephoto_filter_emboss(sb->ephoto, v->image);
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
static Eina_Bool
_animate_cb(void *data)
{
   Ephoto_Viewer *v = data;

   v->cur_frame++;
   if (v->cur_frame > v->frame_count)
     v->cur_frame = 1;
   evas_object_image_animated_frame_set(v->image, v->cur_frame);
   v->duration = evas_object_image_animated_frame_duration_get(v->image,
       v->cur_frame, 0);
   if (v->duration > 0)
     ecore_timer_interval_set(v->anim_timer, v->duration);

   return ECORE_CALLBACK_RENEW;
}

static Evas_Object *
_viewer_add(Evas_Object *parent, const char *path, Ephoto_Single_Browser *sb)
{
   Ephoto_Viewer *v;
   int err;
   Evas_Coord w, h;
   const char *group;

   EINA_SAFETY_ON_NULL_RETURN_VAL(path, NULL);

   group = _ephoto_get_edje_group(path);

   v = calloc(1, sizeof(Ephoto_Viewer));
   v->zoom_first = EINA_TRUE;
   v->cur_frame = 0;
   v->anim_timer = NULL;
   v->duration = 0;
   v->frame_count = 0;

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

   v->image = evas_object_image_add(evas_object_evas_get(v->table));
   evas_object_image_load_orientation_set(v->image, EINA_TRUE);
   evas_object_image_filled_set(v->image, EINA_TRUE);
   evas_object_image_smooth_scale_set(v->image, sb->ephoto->config->smooth);
   evas_object_image_preload(v->image, EINA_FALSE);
   evas_object_image_file_set(v->image, path, group);
   err = evas_object_image_load_error_get(v->image);
   if (err != EVAS_LOAD_ERROR_NONE)
     goto error;
   evas_object_image_size_get(v->image, &w, &h);
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
   if (evas_object_image_animated_get(v->image))
     {
        v->frame_count = evas_object_image_animated_frame_count_get(v->image);
        v->cur_frame = 1;
        evas_object_image_animated_frame_set(v->image, v->cur_frame);
        v->duration = evas_object_image_animated_frame_duration_get(v->image,
            v->cur_frame, 0);
        v->anim_timer = ecore_timer_add(v->duration, _animate_cb, v);
     }

   v->monitor = eio_monitor_add(path);
   v->monitor_handlers =
       eina_list_append(v->monitor_handlers,
           ecore_event_handler_add(EIO_MONITOR_FILE_MODIFIED,
               _monitor_cb, sb));
   v->monitor_handlers =
       eina_list_append(v->monitor_handlers,
           ecore_event_handler_add(EIO_MONITOR_FILE_DELETED,
               _monitor_cb, sb));
   return v->scroller;

  error:
   evas_object_event_callback_del(v->scroller, EVAS_CALLBACK_DEL, _viewer_del);
   evas_object_data_del(v->scroller, "viewer");
   free(v);
   return NULL;
}

/*Single Browser Populating Functions*/

static Eina_Bool
_ephoto_single_populate_end(void *data, int type EINA_UNUSED,
    void *event EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (!sb->entry && sb->ephoto->state == EPHOTO_STATE_SINGLE)
     ephoto_single_browser_entry_set(sb->main,
                 _first_entry_find(sb));

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
	ephoto_single_browser_entry_set(sb->main, e);
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

static void
_edit_function_item_add(Evas_Object *parent, const char *icon, const char *label,
    Evas_Smart_Cb callback, void *data)
{
   Evas_Object *button, *ic;

   ic = elm_icon_add(parent);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, icon);

   button = elm_button_add(parent);
   elm_object_tooltip_text_set(button, label);
   elm_object_tooltip_orient_set(button, ELM_TOOLTIP_ORIENT_LEFT);
   elm_object_tooltip_window_mode_set(button, EINA_TRUE);
   elm_object_part_content_set(button, "icon", ic);
   evas_object_size_hint_min_set(button, 30*elm_config_scale_get(),
       30*elm_config_scale_get());
   evas_object_smart_callback_add(button, "clicked", callback, data);
   evas_object_size_hint_weight_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(parent, button);
   evas_object_show(button);
}

char *
_item_text_get(void *data, Evas_Object *obj EINA_UNUSED, const char *part EINA_UNUSED)
{
   Evas_Object *ic = data;
   char *txt = evas_object_data_get(ic, "label");

   if (txt)
     return strdup(txt);
   else
     return NULL;
}

char *
_header_text_get(void *data, Evas_Object *obj EINA_UNUSED, const char *part EINA_UNUSED)
{
   char *txt = data;

   if (txt)
     return strdup(txt);
   else
     return NULL;
}

Evas_Object *
_item_content_get(void *data, Evas_Object *obj EINA_UNUSED, const char *part)
{
   Evas_Object *ic = data;

   if (!strcmp(part, "elm.swallow.end"))
     return NULL;

   return ic;
}

static void
_edit_item_add(Evas_Object *parent, Elm_Object_Item *par, const char *icon, const char *label,
    Evas_Smart_Cb callback, void *data)
{
   Evas_Object *ic;
   Elm_Genlist_Item_Class *itc = elm_genlist_item_class_new();

   itc = elm_genlist_item_class_new();
   itc->item_style = "default";
   itc->func.text_get = _item_text_get;
   itc->func.content_get = _item_content_get;
   itc->func.state_get = NULL;
   itc->func.del = NULL;

   ic = elm_icon_add(parent);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, icon);
   evas_object_data_set(ic, "label", label);

   elm_genlist_item_append(parent, itc, ic, par,
       ELM_GENLIST_ITEM_NONE, callback, data);
}

static void
_editor_menu(void *data, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Evas_Object *frame, *box, *vbox, *sep, *list;
   Elm_Genlist_Item_Class *itc = elm_genlist_item_class_new();
   Elm_Object_Item *par;

   if (sb->edit_main)
     return;

   itc = elm_genlist_item_class_new();
   itc->item_style = "group_index";
   itc->func.text_get = _header_text_get;
   itc->func.content_get = NULL;
   itc->func.state_get = NULL;
   itc->func.del = NULL;

   sb->ephoto->editor_blocking = EINA_TRUE;
   edje_object_signal_emit(elm_layout_edje_get(sb->ephoto->layout),
       "ephoto,controls,hide", "ephoto");
   edje_object_signal_emit(elm_layout_edje_get(sb->ephoto->layout),
       "ephoto,folders,hide", "ephoto");

   frame = elm_frame_add(sb->ephoto->layout);
   elm_object_text_set(frame, _("Edit"));
   evas_object_size_hint_weight_set(frame, 0.3, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_layout_content_set(sb->ephoto->layout, "ephoto.swallow.editor", frame);
   evas_object_show(frame);

   box = elm_box_add(frame);
   elm_box_horizontal_set(box, EINA_TRUE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_data_set(box, "frame", frame);
   elm_object_content_set(frame, box);
   evas_object_show(box);

   vbox = elm_box_add(box);
   elm_box_horizontal_set(vbox, EINA_FALSE);
   evas_object_size_hint_weight_set(vbox, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(vbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, vbox);
   evas_object_show(vbox);

   sep = elm_separator_add(box);
   elm_separator_horizontal_set(sep, EINA_FALSE);
   evas_object_size_hint_weight_set(sep, 0.0, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(sep, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, sep);
   evas_object_show(sep);

   list = elm_genlist_add(vbox);
   elm_genlist_select_mode_set(list, ELM_OBJECT_SELECT_MODE_ALWAYS);
   evas_object_size_hint_weight_set(list, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(list, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(vbox, list);
   evas_object_show(list);

   par = elm_genlist_item_append(list, itc, _("Transform"), NULL, ELM_GENLIST_ITEM_GROUP,
       NULL, NULL);
   elm_genlist_item_select_mode_set(par, ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);
   _edit_item_add(list, par, "edit-cut", _("Crop"), _crop_image, sb);
   _edit_item_add(list, par, "zoom-in", _("Scale"), _scale_image, sb);
   _edit_item_add(list, par, "object-rotate-left", _("Rotate Left"),
       _go_rotate_counterclock, sb);
   _edit_item_add(list, par, "object-rotate-right", _("Rotate Right"),
       _go_rotate_clock, sb);
   _edit_item_add(list, par, "object-flip-horizontal", _("Flip Horizontal"),
       _go_flip_horiz, sb);
   _edit_item_add(list, par, "object-flip-vertical", _("Flip Vertical"),
       _go_flip_vert, sb);
   par = elm_genlist_item_append(list, itc, _("Color"), NULL, ELM_GENLIST_ITEM_GROUP,
       NULL, NULL);
   elm_genlist_item_select_mode_set(par, ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);
   _edit_item_add(list, par, "insert-image", _("Auto Equalize"),
       _go_auto_eq, sb);
   _edit_item_add(list, par, "insert-image", _("Brightness/Contrast/Gamma"),
       _go_bcg, sb);
   _edit_item_add(list, par, "insert-image", _("Hue/Saturation/Value"),
       _go_hsv, sb);
   _edit_item_add(list, par, "insert-image", _("Color Levels"),
       _go_color, sb);
   _edit_item_add(list, par, "insert-image", _("Red Eye Removal"),
       _go_reye, sb);
   par = elm_genlist_item_append(list, itc, _("Filters"), NULL, ELM_GENLIST_ITEM_GROUP,
       NULL, NULL);
   elm_genlist_item_select_mode_set(par, ELM_OBJECT_SELECT_MODE_DISPLAY_ONLY);
   _edit_item_add(list, par, "insert-image", _("Black and White"),
       _go_black_and_white, sb);
   _edit_item_add(list, par, "insert-image", _("Blur"),
       _go_blur, sb);
   _edit_item_add(list, par, "insert-image", _("Dither"),
       _go_dither, sb);
   _edit_item_add(list, par, "insert-image", _("Edge Detect"),
       _go_edge, sb);
   _edit_item_add(list, par, "insert-image", _("Emboss"),
       _go_emboss, sb);
   _edit_item_add(list, par, "insert-image", _("Invert Colors"),
       _go_invert, sb);
   _edit_item_add(list, par, "insert-image", _("Old Photo"),
       _go_old_photo, sb);
   _edit_item_add(list, par, "insert-image", _("Painting"),
       _go_painting, sb);
   _edit_item_add(list, par, "insert-image", _("Posterize"),
       _go_posterize, sb);
   _edit_item_add(list, par, "insert-image", _("Sharpen"),
       _go_sharpen, sb);
   _edit_item_add(list, par, "insert-image", _("Sketch"),
       _go_sketch, sb);

   vbox = elm_box_add(box);
   elm_box_horizontal_set(vbox, EINA_FALSE);
   elm_box_homogeneous_set(vbox, EINA_TRUE);
   evas_object_size_hint_weight_set(vbox, 0.0, EVAS_HINT_EXPAND);
   evas_object_size_hint_min_set(vbox, 30, 30);
   evas_object_size_hint_align_set(vbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, vbox);
   evas_object_show(vbox);

   _edit_function_item_add(vbox, "document-save", _("Save"), _save_image, sb);
   _edit_function_item_add(vbox, "document-save-as", _("Save As"),
       _save_image_as, sb);
   _edit_function_item_add(vbox, "document-send", _("Upload"), _upload_image,
       sb);
   _edit_function_item_add(vbox, "edit-undo", _("Undo"), _undo_image, sb);
   _edit_function_item_add(vbox, "edit-redo", _("Redo"), _redo_image, sb);
   _edit_function_item_add(vbox, "edit-clear", _("Reset"), _reset_image, sb);
   _edit_function_item_add(vbox, "window-close", _("Close"), _close_editor, sb);

   edje_object_signal_emit(elm_layout_edje_get(sb->ephoto->layout),
       "ephoto,editor,show", "ephoto");
   sb->edit_main = frame;
}

static Eina_Bool
_show_edit_main(void *data, int type EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->edit_main)
     {
        sb->edit_main = NULL;
     }
   _editor_menu(sb, NULL, NULL);

   return ECORE_CALLBACK_PASS_ON;
}

/*Ephoto Main Callbacks*/
static void
_add_edit_menu_items(Ephoto_Single_Browser *sb, Evas_Object *menu)
{
   Elm_Object_Item *menu_it;

   menu_it = elm_menu_item_add(menu, NULL, "insert-image", _("File"), NULL, NULL);
   elm_menu_item_add(menu, menu_it, "insert-image", _("Edit"), _editor_menu, sb);
   elm_menu_item_add(menu, menu_it, "edit-clear", _("Reset"), _reset_image, sb);
   elm_menu_item_add(menu, menu_it, "document-save", _("Save"), _save_image, sb);
   elm_menu_item_add(menu, menu_it, "document-save-as", _("Save As"),
       _save_image_as, sb);
   elm_menu_item_add(menu, menu_it, "edit", _("Rename"),
            _rename_image, sb);
   elm_menu_item_add(menu, menu_it, "edit-delete", _("Delete"),
            _delete_image, sb);
   elm_menu_item_add(menu, menu_it, "document-send", _("Upload"), _upload_image,
       sb);
   elm_menu_item_separator_add(menu, NULL);
   elm_menu_item_add(menu, NULL, "zoom-fit", _("Zoom Fit"), _zoom_fit_cb,
       sb);
   elm_menu_item_add(menu, NULL, "zoom-original", _("Zoom 1:1"), _zoom_1_cb,
       sb);
   elm_menu_item_add(menu, NULL, "object-rotate-left", _("Rotate Left"), _go_rotate_counterclock,
       sb);
   elm_menu_item_add(menu, NULL, "object-rotate-right", _("Rotate Right"), _go_rotate_clock,
       sb);
}

static void
_ephoto_main_edit_menu(Ephoto_Single_Browser *sb)
{
   Evas_Object *menu;
   Evas_Coord x, y;

   evas_pointer_canvas_xy_get(evas_object_evas_get(sb->main), &x, &y);
   menu = elm_menu_add(sb->ephoto->win);
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
        else if (!strcmp(k, "z"))
          {
             if (!shift)
               _undo_image(sb, NULL, NULL);
             else
               _redo_image(sb, NULL, NULL);
          }
        else if (!strcmp(k, "y"))
          {
             _redo_image(sb, NULL, NULL);
          }
	return;
     }

   if (!strcmp(k, "Escape") && !sb->editing && !sb->edit_main)
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
   else if (!strcmp(k, "space") && !sb->editing)
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
	if (!elm_win_fullscreen_get(sb->ephoto->win))
          {
             elm_win_fullscreen_set(sb->ephoto->win, EINA_TRUE);
             elm_box_pack_end(sb->ephoto->statusbar, sb->ephoto->exit);
             evas_object_show(sb->ephoto->exit);
          }
        else
          {
             elm_win_fullscreen_set(sb->ephoto->win, EINA_FALSE);
             elm_box_unpack(sb->ephoto->statusbar, sb->ephoto->exit);
             evas_object_hide(sb->ephoto->exit);
          }
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
_ephoto_main_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   char tmp_path[PATH_MAX];
   Ephoto_Single_Browser *sb = data;
   Ecore_Event_Handler *handler;
   Eina_Iterator *tmps;
   Eina_File_Direct_Info *info;
   Ephoto_History *eh;

   EINA_LIST_FREE(sb->handlers, handler)
     ecore_event_handler_del(handler);
   if (sb->pending_path)
     eina_stringshare_del(sb->pending_path);
   if (sb->edit_main)
     evas_object_del(sb->edit_main);
   snprintf(tmp_path, PATH_MAX, "%s/.config/ephoto/", getenv("HOME"));
   tmps = eina_file_stat_ls(tmp_path);
   EINA_ITERATOR_FOREACH(tmps, info)
   {
      const char *bname = info->path + info->name_start;

      if (!strncmp(bname, "tmp", 3))
	 ecore_file_unlink(info->path);
   }
   if (sb->history)
     {
        EINA_LIST_FREE(sb->history, eh)
          {
             free(eh->im_data);
             free(eh);
             eh = NULL;
          }
        sb->history = NULL;
        sb->history_pos = 0;
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
   Ephoto_Viewer *v = NULL;

   if (sb->viewer)
     v = evas_object_data_get(sb->viewer, "viewer");
   if (entries)
     {
        sb->entries = entries;
        if (sb->ephoto->state == EPHOTO_STATE_SINGLE)
          {
             if (v)
               {
                  const char *image;
                  char *dir;

                  elm_image_file_get(v->image, &image, NULL);
                  dir = ecore_file_dir_get(image);
                  if (strcmp(sb->ephoto->config->directory, dir))
                    ephoto_single_browser_entry_set(sb->main,
                        _first_entry_find(sb));
                  free(dir);
               }
             else
               ephoto_single_browser_entry_set(sb->main,
                        _first_entry_find(sb));
          }
     }
   else
     ephoto_single_browser_entry_set(sb->main, NULL);
}

void
ephoto_single_browser_entry_set(Evas_Object *obj, Ephoto_Entry *entry)
{
   Ephoto_Single_Browser *sb = evas_object_data_get(obj, "single_browser");
   Ephoto_Viewer *v;
   Ephoto_History *eh;
   Evas_Coord w, h;

   if (!entry)
     sb->entry = NULL;
   else if (entry)
     sb->entry = entry;
   _ephoto_single_browser_recalc(sb);
   if (sb->history)
     {
        EINA_LIST_FREE(sb->history, eh)
          {
             free(eh->im_data);
             free(eh);
             eh = NULL;
          }
        sb->history = NULL;
        sb->history_pos = 0;
     }
   if (sb->viewer)
     {
        v = evas_object_data_get(sb->viewer, "viewer");
        evas_object_image_size_get(v->image, &w, &h);
        eh = calloc(1, sizeof(Ephoto_History));
        eh->im_data = malloc(sizeof(unsigned int) * w * h);
        memcpy(eh->im_data, evas_object_image_data_get(v->image, EINA_FALSE),
            sizeof(unsigned int) * w * h);
        eh->w = w;
        eh->h = h;
        eh->orient = sb->orient;
        sb->history = eina_list_append(sb->history, eh);
        _zoom_fit(sb);
     }
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
	evas_object_image_data_set(image, image_data);
	evas_object_image_data_update_add(image, 0, 0, w,
	    h);
     }
}

void
ephoto_single_browser_image_data_done(Evas_Object *main,
    unsigned int *image_data, Evas_Coord w, Evas_Coord h)
{
   Ephoto_Single_Browser *sb = evas_object_data_get(main, "single_browser");
   Ephoto_History *eh;
   Eina_List *l;
   char buf[PATH_MAX];

   if (!image_data)
     return;
   if (sb->editing)
     {
        _ephoto_single_browser_recalc(sb);
        Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");
        if (sb->cropping)
             sb->cropping = EINA_FALSE;
        evas_object_image_size_set(v->image, w, h);
        evas_object_image_data_set(v->image, image_data);
        evas_object_image_data_update_add(v->image, 0, 0, w,
            h);
        if (sb->history_pos < (eina_list_count(sb->history)-1))
          {
             int count;

             count = sb->history_pos + 1;
             l = eina_list_nth_list(sb->history, count);
             while (l)
               {
                  eh = eina_list_data_get(l);
                  sb->history = eina_list_remove_list(sb->history, l);
                  free(eh->im_data);
                  free(eh);
                  eh = NULL;
                  l = eina_list_nth_list(sb->history, count);
               }
          }
        eh = calloc(1, sizeof(Ephoto_History));
        eh->im_data = malloc(sizeof(unsigned int) * w * h);
        memcpy(eh->im_data, image_data,
            sizeof(unsigned int) * w * h);
        eh->w = w;
        eh->h = h;
        eh->orient = sb->orient;
        sb->history = eina_list_append(sb->history, eh);
        sb->history_pos = eina_list_count(sb->history) - 1;
        snprintf(buf, PATH_MAX, "%s [%s]", sb->entry->basename, _("MODIFIED"));
        ephoto_title_set(sb->ephoto, buf);
        _ephoto_update_bottom_bar(sb);
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
	if (sb->history_pos)
	  {
             Ephoto_History *eh = eina_list_nth(sb->history, sb->history_pos);
	     evas_object_image_size_set(v->image, eh->w,
		 eh->h);
	     evas_object_image_data_set(v->image,
		 eh->im_data);
	     evas_object_image_data_update_add(v->image, 0,
		 0, eh->w, eh->h);
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

   ic = elm_icon_add(ephoto->controls_left);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   ret = elm_icon_standard_set(ic, "go-previous");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);

   but = elm_button_add(ephoto->controls_left);
   if (!ret)
     elm_object_text_set(but, _("Previous"));
   elm_object_part_content_set(but, "icon", ic);
   elm_object_tooltip_text_set(but, _("Previous"));
   elm_object_tooltip_orient_set(but, ELM_TOOLTIP_ORIENT_TOP);
   evas_object_smart_callback_add(but, "clicked", _go_prev, sb);
   elm_box_pack_end(ephoto->controls_left, but);
   evas_object_show(but);

   ic = elm_icon_add(ephoto->controls_left);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   ret = elm_icon_standard_set(ic, "go-next");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);

   but = elm_button_add(ephoto->controls_left);
   if (!ret)
     elm_object_text_set(but, _("Next"));
   elm_object_part_content_set(but, "icon", ic);
   elm_object_tooltip_text_set(but, _("Next"));
   elm_object_tooltip_orient_set(but, ELM_TOOLTIP_ORIENT_TOP);
   evas_object_smart_callback_add(but, "clicked", _go_next, sb);
   elm_box_pack_end(ephoto->controls_left, but);
   evas_object_show(but);

   ic = elm_icon_add(ephoto->controls_right);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   ret = elm_icon_standard_set(ic, "insert-image");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);

   but = elm_button_add(ephoto->controls_right);
   if (!ret)
     elm_object_text_set(but, _("Edit"));
   elm_object_part_content_set(but, "icon", ic);
   elm_object_tooltip_text_set(but, _("Edit"));
   elm_object_tooltip_orient_set(but, ELM_TOOLTIP_ORIENT_TOP);
   evas_object_smart_callback_add(but, "clicked", _editor_menu, sb);
   elm_box_pack_end(ephoto->controls_right, but);
   evas_object_show(but);

   ic = elm_icon_add(ephoto->controls_right);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   ret = elm_icon_standard_set(ic, "document-save-as");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);

   but = elm_button_add(ephoto->controls_right);
   if (!ret)
     elm_object_text_set(but, _("Save As"));
   elm_object_part_content_set(but, "icon", ic);
   elm_object_tooltip_text_set(but, _("Save As"));
   elm_object_tooltip_orient_set(but, ELM_TOOLTIP_ORIENT_TOP);
   evas_object_smart_callback_add(but, "clicked", _save_image_as, sb);
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
   sb->history = NULL;
   sb->history_pos = 0;
   sb->edit_main = NULL;

   elm_box_horizontal_set(sb->main, EINA_FALSE);
   evas_object_event_callback_add(sb->main, EVAS_CALLBACK_DEL, _ephoto_main_del, sb);
   evas_object_event_callback_add(sb->main, EVAS_CALLBACK_KEY_DOWN, _ephoto_main_key_down,
       sb);
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

   sb->handlers =
       eina_list_append(sb->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_EDITOR_BACK,
           _show_edit_main, sb));

   sb->orient = EPHOTO_ORIENT_0;

   return sb->main;

  error:
   evas_object_del(sb->main);
   return NULL;
}

