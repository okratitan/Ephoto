#include "ephoto.h"

#define ZOOM_STEP 0.2

static Ecore_Timer *_1s_hold = NULL;

typedef struct _Ephoto_Single_Browser Ephoto_Single_Browser;
typedef struct _Ephoto_Viewer Ephoto_Viewer;

struct _Ephoto_Single_Browser
{
   Ephoto *ephoto;
   Evas_Object *main;
   Evas_Object *tbox;
   Evas_Object *bar;
   Evas_Object *mhbox;
   Evas_Object *table;
   Evas_Object *viewer;
   Evas_Object *infolabel;
   Evas_Object *nolabel;
   Evas_Object *botbox;
   Evas_Object *event;
   const char *pending_path;
   Ephoto_Entry *entry;
   Ephoto_Orient orient;
   Eina_List *handlers;
   Eina_List *upload_handlers;
   Eina_List *entries;
   Eina_Bool editing:1;
   Eina_Bool cropping:1;
   unsigned int *edited_image_data;
   int ew;
   int eh;
   Ecore_Con_Url *url_up;
   char *url_ret;
   char *upload_error;
};

struct _Ephoto_Viewer
{
   Eina_List *handlers;
   Eio_Monitor *monitor;
   Evas_Object *scroller;
   Evas_Object *table;
   Evas_Object *image;
   double zoom;
   Eina_Bool fit:1;
   Eina_Bool zoom_first:1;
   Eina_Bool modified:1;
};

static void _zoom_set(Ephoto_Single_Browser *sb, double zoom);
static void _zoom_in(Ephoto_Single_Browser *sb);
static void _zoom_out(Ephoto_Single_Browser *sb);
static void _key_down(void *data, Evas *e EINA_UNUSED,
    Evas_Object *obj EINA_UNUSED, void *event_info);
static void _edit_menu(Ephoto_Single_Browser *sb);

static void
_viewer_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Viewer *v = data;
   Ecore_Event_Handler *handler;

   EINA_LIST_FREE(v->handlers, handler)
      ecore_event_handler_del(handler);
   if (v->monitor)
     eio_monitor_del(v->monitor);
   free(v);
}

static Eina_Bool
_monitor_modified(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

   if (!ecore_file_exists(sb->entry->path))
     ephoto_entry_free(sb->ephoto, sb->entry);
   else
     v->modified = EINA_TRUE;
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_monitor_closed(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

   if (v->modified == EINA_TRUE)
     {
        ephoto_single_browser_entry_set(sb->main, sb->entry);
        v->modified = EINA_FALSE;
     }
   return ECORE_CALLBACK_PASS_ON;
}

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
   else if (ev->button == 3)
     {
        _edit_menu(sb);
     }
   ecore_timer_del(_1s_hold);
   _1s_hold = NULL;
}

static Evas_Object *
_viewer_add(Evas_Object *parent, const char *path, Ephoto_Single_Browser *sb)
{
   Ephoto_Viewer *v = calloc(1, sizeof(Ephoto_Viewer));
   int err;

   v->zoom_first = EINA_TRUE;

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
   v->scroller = elm_scroller_add(parent);
   evas_object_size_hint_weight_set(v->scroller, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(v->scroller,
       EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_data_set(v->scroller, "viewer", v);
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
   elm_table_pack(v->table, v->image, 0, 0, 1, 1);
   evas_object_show(v->image);
   if (elm_image_animated_available_get(v->image))
     {
        elm_image_animated_set(v->image, EINA_TRUE);
        elm_image_animated_play_set(v->image, EINA_TRUE);
     }


   v->monitor = eio_monitor_add(path);
   v->handlers = eina_list_append(v->handlers,
       ecore_event_handler_add(EIO_MONITOR_FILE_MODIFIED,
           _monitor_modified, sb));
   v->handlers = eina_list_append(v->handlers,
       ecore_event_handler_add(EIO_MONITOR_FILE_CLOSED, _monitor_closed, sb));

   return v->scroller;

  error:
   evas_object_event_callback_del(v->scroller, EVAS_CALLBACK_DEL, _viewer_del);
   evas_object_data_del(v->scroller, "viewer");
   free(v);
   return NULL;
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
   char image_info[PATH_MAX];
   int w, h;

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
   evas_object_del(sb->botbox);
   evas_object_del(sb->infolabel);
   snprintf(image_info, PATH_MAX,
       "<b>%s:</b> %s        <b>%s:</b> %dx%d        <b>%s:</b> %s", _("Type"),
       efreet_mime_type_get(sb->entry->path), _("Resolution"), w, h,
       _("File Size"), _("N/A"));
   sb->botbox = elm_box_add(sb->table);
   evas_object_color_set(sb->botbox, 0, 0, 0, 0);
   evas_object_size_hint_min_set(sb->botbox, 0, sb->ephoto->bottom_bar_size);
   evas_object_size_hint_weight_set(sb->botbox, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_fill_set(sb->botbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(sb->table, sb->botbox, 0, 2, 4, 1);
   evas_object_show(sb->botbox);

   sb->infolabel = elm_label_add(sb->table);
   elm_label_line_wrap_set(sb->infolabel, ELM_WRAP_NONE);
   elm_object_text_set(sb->infolabel, image_info);
   evas_object_size_hint_weight_set(sb->infolabel, EVAS_HINT_EXPAND,
       EVAS_HINT_FILL);
   evas_object_size_hint_align_set(sb->infolabel, EVAS_HINT_FILL,
       EVAS_HINT_FILL);
   elm_table_pack(sb->table, sb->infolabel, 0, 2, 4, 1);
   evas_object_show(sb->infolabel);

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
_ephoto_single_browser_recalc(Ephoto_Single_Browser *sb)
{
   if (sb->viewer)
     {
	evas_object_del(sb->viewer);
	sb->viewer = NULL;
	evas_object_del(sb->botbox);
	sb->botbox = NULL;
	evas_object_del(sb->infolabel);
	sb->infolabel = NULL;
     }
   if (sb->nolabel)
     {
	evas_object_del(sb->nolabel);
	sb->nolabel = NULL;
     }
   if (sb->entry)
     {
	const char *bname = ecore_file_file_get(sb->entry->path);

	elm_table_clear(sb->table, EINA_FALSE);

	sb->viewer = _viewer_add(sb->main, sb->entry->path, sb);
	if (sb->viewer)
	  {
	     char image_info[PATH_MAX], *tmp;
	     Evas_Coord w, h;
	     Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

	     elm_table_pack(sb->table, sb->viewer, 0, 1, 4, 1);
	     evas_object_show(sb->viewer);
	     evas_object_event_callback_add(sb->viewer,
		 EVAS_CALLBACK_MOUSE_WHEEL, _mouse_wheel, sb);

	     evas_object_image_size_get(elm_image_object_get(v->image),
	         &w, &h);
	     tmp = _ephoto_get_file_size(sb->entry->path);
	     snprintf(image_info, PATH_MAX,
		 "<b>%s:</b> %s        <b>%s:</b> %dx%d        <b>%s:</b> %s",
		 _("Type"), efreet_mime_type_get(sb->entry->path),
		 _("Resolution"), w, h, _("File Size"), tmp);
	     free(tmp);
	     sb->botbox = elm_box_add(sb->table);
	     evas_object_size_hint_min_set(sb->botbox, 0,
		 sb->ephoto->bottom_bar_size);
	     evas_object_size_hint_weight_set(sb->botbox, EVAS_HINT_EXPAND,
		 0.0);
	     evas_object_size_hint_fill_set(sb->botbox, EVAS_HINT_FILL,
		 EVAS_HINT_FILL);
	     elm_table_pack(sb->table, sb->botbox, 0, 2, 4, 1);
	     evas_object_show(sb->botbox);

	     sb->infolabel = elm_label_add(sb->table);
	     elm_label_line_wrap_set(sb->infolabel, ELM_WRAP_NONE);
	     elm_object_text_set(sb->infolabel, image_info);
	     evas_object_size_hint_weight_set(sb->infolabel, EVAS_HINT_EXPAND,
		 EVAS_HINT_FILL);
	     evas_object_size_hint_align_set(sb->infolabel, EVAS_HINT_FILL,
		 EVAS_HINT_FILL);
	     elm_table_pack(sb->table, sb->infolabel, 0, 2, 4, 1);
	     evas_object_show(sb->infolabel);

	     ephoto_title_set(sb->ephoto, bname);
	  }
        else
	  {
	     sb->nolabel = elm_label_add(sb->table);
	     elm_label_line_wrap_set(sb->nolabel, ELM_WRAP_WORD);
	     elm_object_text_set(sb->nolabel,
		 _("This image does not exist or is corrupted!"));
	     evas_object_size_hint_weight_set(sb->nolabel, EVAS_HINT_EXPAND,
		 EVAS_HINT_EXPAND);
	     evas_object_size_hint_align_set(sb->nolabel, EVAS_HINT_FILL,
		 EVAS_HINT_FILL);
	     elm_table_pack(sb->table, sb->nolabel, 0, 1, 4, 1);
	     evas_object_show(sb->nolabel);
	     ephoto_title_set(sb->ephoto, _("Bad Image"));
	  }
     }
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
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "document-save");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Yes"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _reset_yes, popup);
   elm_object_part_content_set(popup, "button1", button);
   evas_object_show(button);

   ic = elm_icon_add(popup);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
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
_failed_save(Ephoto_Single_Browser *sb)
{
   Evas_Object *popup, *box, *label, *ic, *button;

   if (sb->event)
     evas_object_freeze_events_set(sb->event, EINA_TRUE);

   popup = elm_popup_add(sb->ephoto->win);
   elm_object_part_text_set(popup, "title,text", _("Save Failed"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);

   label = elm_label_add(box);
   elm_object_text_set(label, _("Error: Image could not be saved here!"));
   evas_object_size_hint_weight_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
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
   evas_object_smart_callback_add(button, "clicked", _reset_no, popup);
   elm_object_part_content_set(popup, "button1", button);
   evas_object_show(button);

   evas_object_data_set(popup, "single_browser", sb);
   elm_object_part_content_set(popup, "default", box);
   evas_object_show(popup);
}

static void
_save_yes(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto_Single_Browser *sb = evas_object_data_get(popup, "single_browser");
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");
   Eina_Bool success;

   if (ecore_file_exists(sb->entry->path))
     {
	success = ecore_file_unlink(sb->entry->path);
	if (!success)
	  {
	     _failed_save(sb);
	     ephoto_single_browser_entry_set(sb->main, sb->entry);
	     evas_object_del(popup);
             if (sb->event)
               {
                  elm_object_focus_set(sb->event, EINA_TRUE);
                  evas_object_freeze_events_set(sb->event, EINA_FALSE);
               }
	     return;
	  }
     }
   success =
       evas_object_image_save(elm_image_object_get(v->image), sb->entry->path,
       NULL, NULL);
   if (!success)
      _failed_save(sb);
   ephoto_single_browser_entry_set(sb->main, sb->entry);
   evas_object_del(popup);
   if (sb->event)
     {
        elm_object_focus_set(sb->event, EINA_TRUE);
        evas_object_freeze_events_set(sb->event, EINA_FALSE);
     }
}

static void
_save_no(void *data, Evas_Object *obj EINA_UNUSED,
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
_save_image(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Evas_Object *popup, *box, *label, *ic, *button;

   if (sb->event)
     evas_object_freeze_events_set(sb->event, EINA_TRUE);

   popup = elm_popup_add(sb->ephoto->win);
   elm_object_part_text_set(popup, "title,text", _("Save Image"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);

   label = elm_label_add(box);
   elm_object_text_set(label,
       _("Are you sure you want to overwrite this image?"));
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, label);
   evas_object_show(label);

   ic = elm_icon_add(popup);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "document-save");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Yes"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _save_yes, popup);
   elm_object_part_content_set(popup, "button1", button);
   evas_object_show(button);

   ic = elm_icon_add(popup);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "window-close");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("No"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _save_no, popup);
   elm_object_part_content_set(popup, "button2", button);
   evas_object_show(button);

   evas_object_data_set(popup, "single_browser", sb);
   elm_object_part_content_set(popup, "default", box);
   evas_object_show(popup);
}

static void
_save_image_as_overwrite(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   const char *file = evas_object_data_get(popup, "file");
   Ephoto_Single_Browser *sb = evas_object_data_get(popup, "single_browser");
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");
   Eina_Bool success;

   if (ecore_file_exists(file))
     {
	success = ecore_file_unlink(file);
	if (!success)
	  {
	     _failed_save(sb);
	     ephoto_single_browser_entry_set(sb->main, sb->entry);
	     evas_object_del(popup);
             if (sb->event)
               {
                  elm_object_focus_set(sb->event, EINA_TRUE);
                  evas_object_freeze_events_set(sb->event, EINA_FALSE);
               }
	     return;
	  }
     }
   success =
       evas_object_image_save(elm_image_object_get(v->image), file,
           NULL, NULL);
   if (!success)
      _failed_save(sb);
   else
     {
	char *dir = ecore_file_dir_get(file);

	ephoto_thumb_browser_fsel_clear(sb->ephoto);
	ephoto_directory_set(sb->ephoto, dir, NULL, EINA_FALSE, EINA_FALSE);
        ephoto_thumb_browser_top_dir_set(sb->ephoto,
            sb->ephoto->config->directory);
	free(dir);
	ephoto_single_browser_path_pending_set(sb->ephoto->single_browser,
	    file);
     }
   ephoto_single_browser_entry_set(sb->main, sb->entry);
   evas_object_del(popup);
   if (sb->event)
     {
        elm_object_focus_set(sb->event, EINA_TRUE);
        evas_object_freeze_events_set(sb->event, EINA_FALSE);
     }
}

static void
_save_image_as_done(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   const char *selected = event_info;
   Evas_Object *opopup = data;
   Ephoto_Single_Browser *sb = evas_object_data_get(opopup, "single_browser");

   if (selected)
     {
	Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");
	Eina_Bool success;

	char buf[PATH_MAX];

	if (!evas_object_image_extension_can_load_get(selected))
	   snprintf(buf, PATH_MAX, "%s.jpg", selected);
	else
	   snprintf(buf, PATH_MAX, "%s", selected);
	if (ecore_file_exists(buf))
	  {
	     Evas_Object *popup, *box, *label, *ic, *button;

             if (sb->event)
               evas_object_freeze_events_set(sb->event, EINA_TRUE);

	     popup = elm_popup_add(sb->ephoto->win);
	     elm_object_part_text_set(popup, "title,text",
	         _("Overwite Image"));
	     elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

	     box = elm_box_add(popup);
	     elm_box_horizontal_set(box, EINA_FALSE);
	     evas_object_size_hint_weight_set(box, EVAS_HINT_FILL,
		 EVAS_HINT_FILL);
	     evas_object_size_hint_align_set(box, EVAS_HINT_FILL,
		 EVAS_HINT_FILL);
	     evas_object_show(box);

	     label = elm_label_add(box);
	     elm_object_text_set(label,
		 _("Are you sure you want to overwrite this image?"));
	     evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND,
		 EVAS_HINT_EXPAND);
	     evas_object_size_hint_align_set(label, EVAS_HINT_FILL,
		 EVAS_HINT_FILL);
	     elm_box_pack_end(box, label);
	     evas_object_show(label);

	     ic = elm_icon_add(popup);
	     elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
	     evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL,
		 1, 1);
	     elm_icon_standard_set(ic, "document-save");

	     button = elm_button_add(popup);
	     elm_object_text_set(button, _("Yes"));
	     elm_object_part_content_set(button, "icon", ic);
	     evas_object_smart_callback_add(button, "clicked",
		 _save_image_as_overwrite, popup);
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
	     evas_object_smart_callback_add(button, "clicked",
	         _save_no, popup);
	     elm_object_part_content_set(popup, "button2", button);
	     evas_object_show(button);

	     evas_object_data_set(popup, "single_browser", sb);
	     evas_object_data_set(popup, "file", strdup(buf));
	     elm_object_part_content_set(popup, "default", box);
	     evas_object_show(popup);
	  }
        else
	  {
	     success =
		 evas_object_image_save(elm_image_object_get(v->image), buf,
		 NULL, NULL);
	     if (!success)
		_failed_save(sb);
	     else
	       {
		  char *dir = ecore_file_dir_get(buf);

		  ephoto_thumb_browser_fsel_clear(sb->ephoto);
		  ephoto_directory_set(sb->ephoto, dir, NULL,
                      EINA_FALSE, EINA_FALSE);
                  ephoto_thumb_browser_top_dir_set(sb->ephoto,
                      sb->ephoto->config->directory);
		  free(dir);
		  ephoto_single_browser_path_pending_set(sb->ephoto->
		      single_browser, buf);
	       }
	  }
     }
   evas_object_del(opopup);
   if (sb->event)
     {
        elm_object_focus_set(sb->event, EINA_TRUE);
        evas_object_freeze_events_set(sb->event, EINA_FALSE);
     }
}

static void
_save_image_as(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Evas_Object *popup, *fsel, *rect, *table;
   int h;

   evas_object_geometry_get(sb->ephoto->win, 0, 0, 0, &h);

   if (sb->event)
     evas_object_freeze_events_set(sb->event, EINA_TRUE);

   popup = elm_popup_add(sb->ephoto->win);
   elm_popup_scrollable_set(popup, EINA_TRUE);
   elm_object_part_text_set(popup, "title,text", _("Save Image As"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   table = elm_table_add(popup);
   evas_object_size_hint_weight_set(table, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(table, EVAS_HINT_FILL, EVAS_HINT_FILL);

   rect = elm_box_add(popup);
   evas_object_size_hint_min_set(rect, 0, h / 1.5);
   evas_object_size_hint_weight_set(rect, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(rect, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(table, rect, 0, 0, 1, 1);
   evas_object_show(rect);

   fsel = elm_fileselector_add(table);
   elm_fileselector_is_save_set(fsel, EINA_TRUE);
   elm_fileselector_expandable_set(fsel, EINA_FALSE);
   elm_fileselector_path_set(fsel, sb->ephoto->config->directory);
   elm_fileselector_current_name_set(fsel, sb->entry->basename);
   elm_fileselector_mime_types_filter_append(fsel, "image/*",
       _("Image Files"));
   evas_object_size_hint_weight_set(fsel, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(fsel, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_callback_add(fsel, "done", _save_image_as_done, popup);
   elm_table_pack(table, fsel, 0, 0, 1, 1);
   evas_object_show(fsel);

   evas_object_show(table);
   evas_object_data_set(popup, "single_browser", sb);
   elm_object_content_set(popup, table);
   evas_object_show(popup);
}

static void
_upload_image_cancel(void *data, Evas_Object *obj EINA_UNUSED,
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
_upload_entry_anchor_bt(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   char buf[PATH_MAX];
   Evas_Object *av = data;
   const char *link = evas_object_data_get(av, "link");

   elm_entry_anchor_hover_end(av);
   snprintf(buf, PATH_MAX, "xdg-open %s", link);
   ecore_exe_run(buf, NULL);
}

static void
_upload_entry_anchor(void *data, Evas_Object *obj, void *event_info)
{
   Evas_Object *av = data;
   Evas_Object *button;
   Elm_Entry_Anchor_Hover_Info *ei = event_info;

   button = elm_button_add(obj);
   elm_object_text_set(button, _("Open Link In Browser"));
   elm_object_part_content_set(ei->hover, "middle", button);
   evas_object_smart_callback_add(button, "clicked", _upload_entry_anchor_bt,
       av);
   evas_object_show(button);
}

static void
_upload_image_url_copy(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *entry = data;

   elm_entry_select_all(entry);
   elm_entry_selection_copy(entry);
   elm_entry_select_none(entry);
}

static Eina_Bool
_upload_image_complete_cb(void *data, int ev_type EINA_UNUSED, void *event)
{
   Evas_Object *ppopup = data;
   Ephoto_Single_Browser *sb = evas_object_data_get(ppopup, "single_browser");
   Ecore_Con_Event_Url_Complete *ev = event;
   Ecore_Event_Handler *handler;
   Evas_Object *popup, *box, *hbox, *label, *entry, *ic, *button;

   if (ev->url_con != sb->url_up)
      return ECORE_CALLBACK_RENEW;

   evas_object_del(ppopup);
   if (sb->event)
     {
        elm_object_focus_set(sb->event, EINA_TRUE);
        evas_object_freeze_events_set(sb->event, EINA_FALSE);
     }

   if (sb->event)
     evas_object_freeze_events_set(sb->event, EINA_TRUE);

   popup = elm_popup_add(sb->ephoto->win);
   elm_object_part_text_set(popup, "title,text", _("Image Uploaded"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);

   label = elm_label_add(box);
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, label);
   evas_object_show(label);

   hbox = elm_box_add(box);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_FILL, 0.0);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, hbox);
   evas_object_show(hbox);

   entry = elm_entry_add(hbox);
   elm_entry_anchor_hover_style_set(entry, "popout");
   elm_entry_anchor_hover_parent_set(entry, sb->main);
   elm_entry_editable_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_context_menu_disabled_set(entry, EINA_TRUE);
   elm_scroller_policy_set(entry, ELM_SCROLLER_POLICY_OFF,
       ELM_SCROLLER_POLICY_OFF);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_callback_add(entry, "anchor,hover,opened",
       _upload_entry_anchor, entry);
   elm_box_pack_end(hbox, entry);
   evas_object_show(entry);

   ic = elm_icon_add(hbox);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_HORIZONTAL, 1, 1);
   elm_icon_standard_set(ic, "edit-copy");

   button = elm_button_add(hbox);
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _upload_image_url_copy,
       entry);
   elm_box_pack_end(hbox, button);
   evas_object_show(button);

   ic = elm_icon_add(popup);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "window-close");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Ok"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _upload_image_cancel,
       popup);
   elm_object_part_content_set(popup, "button1", button);
   evas_object_show(button);

   evas_object_data_set(popup, "single_browser", sb);
   elm_object_part_content_set(popup, "default", box);
   evas_object_show(popup);

   EINA_LIST_FREE(sb->upload_handlers,
       handler) ecore_event_handler_del(handler);

   if (!sb->url_ret || ev->status != 200)
     {
	elm_object_text_set(label,
	    _("There was an error uploading your image!"));
	elm_entry_single_line_set(entry, EINA_TRUE);
	elm_object_text_set(entry, sb->upload_error);
	evas_object_show(popup);
	ecore_con_url_free(sb->url_up);
	sb->url_up = NULL;
	free(sb->upload_error);
	sb->upload_error = NULL;
	return EINA_FALSE;
     }
   else
     {
	char buf[PATH_MAX], link[PATH_MAX];

	snprintf(buf, PATH_MAX, "<a href=\"%s\"><link>%s</link</a>",
	    sb->url_ret, sb->url_ret);
	snprintf(link, PATH_MAX, "%s", sb->url_ret);
	evas_object_data_set(entry, "link", strdup(link));
	elm_object_text_set(label,
	    _("Your image was uploaded to the following link:"));
	elm_entry_single_line_set(entry, EINA_TRUE);
	elm_object_text_set(entry, buf);
	evas_object_show(popup);
	ecore_con_url_free(sb->url_up);
	sb->url_up = NULL;
	free(sb->url_ret);
	sb->url_ret = NULL;
	return ECORE_CALLBACK_RENEW;
     }
}

static Eina_Bool
_upload_image_xml_parse(void *data, Eina_Simple_XML_Type type,
    const char *content, unsigned offset EINA_UNUSED,
    unsigned length EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   char *linkf, *linkl;

   if (type == EINA_SIMPLE_XML_OPEN)
     {
	if (!strncmp("link>", content, strlen("link>")))
	  {
	     linkf = strchr(content, '>') + 1;
	     linkl = strtok(linkf, "<");
	     sb->url_ret = strdup(linkl);
	  }
     }
   return EINA_TRUE;
}

static Eina_Bool
_upload_image_cb(void *data, int ev_type EINA_UNUSED, void *event)
{
   Ephoto_Single_Browser *sb = data;
   Ecore_Con_Event_Url_Data *ev = event;
   const char *string = (const char *) ev->data;

   if (ev->url_con != sb->url_up)
      return EINA_TRUE;
   eina_simple_xml_parse(string, strlen(string) + 1, EINA_TRUE,
       _upload_image_xml_parse, sb);
   if (!sb->url_ret)
      sb->upload_error = strdup(string);

   return EINA_FALSE;
}

static void
_upload_image_confirm(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *ppopup = data;
   Evas_Object *popup, *box, *label, *pb;
   Ephoto_Single_Browser *sb = evas_object_data_get(ppopup, "single_browser");
   char buf[PATH_MAX], tmp_path[PATH_MAX];
   FILE *f;
   unsigned char *fdata;
   int fsize;

   evas_object_del(ppopup);
   if (sb->event)
     {
        elm_object_focus_set(sb->event, EINA_TRUE);
        evas_object_freeze_events_set(sb->event, EINA_FALSE);
     }

   if (sb->event)
     evas_object_freeze_events_set(sb->event, EINA_TRUE);

   popup = elm_popup_add(sb->ephoto->win);
   elm_object_part_text_set(popup, "title,text", _("Upload Image"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);

   label = elm_label_add(box);
   elm_object_text_set(label, _("Please wait while your image is uploaded."));
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

   evas_object_data_set(popup, "single_browser", sb);
   elm_object_part_content_set(popup, "default", box);
   evas_object_show(popup);

   if (sb->edited_image_data)
     {
	const char *ext = strrchr(sb->entry->path, '.');
	Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");
	Eina_Bool success;

	ext++;
	snprintf(tmp_path, PATH_MAX, "%s/.config/ephoto/tmp.%s",
	    getenv("HOME"), ext);
	success =
	    evas_object_image_save(elm_image_object_get(v->image), tmp_path,
	    NULL, NULL);
	if (!success)
	  {
	     _failed_save(sb);
	     return;
	  }
	f = fopen(tmp_path, "rb");
     }
   else
     {
	f = fopen(sb->entry->path, "rb");
     }
   fseek(f, 0, SEEK_END);
   fsize = ftell(f);
   rewind(f);
   fdata = malloc(fsize);
   fread(fdata, fsize, 1, f);
   fclose(f);

   if (sb->edited_image_data)
      ecore_file_unlink(tmp_path);

   snprintf(buf, PATH_MAX, "image=%s", fdata);

   sb->upload_handlers =
       eina_list_append(sb->upload_handlers,
       ecore_event_handler_add(ECORE_CON_EVENT_URL_DATA,
           _upload_image_cb, sb));
   sb->upload_handlers =
       eina_list_append(sb->upload_handlers,
       ecore_event_handler_add(ECORE_CON_EVENT_URL_COMPLETE,
	   _upload_image_complete_cb, popup));

   sb->url_up = ecore_con_url_new("https://api.imgur.com/3/image.xml");
   ecore_con_url_additional_header_add(sb->url_up, "Authorization",
       "Client-ID 67aecc7e6662370");
   ecore_con_url_http_version_set(sb->url_up, ECORE_CON_URL_HTTP_VERSION_1_0);
   ecore_con_url_post(sb->url_up, fdata, fsize, NULL);
}

static void
_upload_image(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Evas_Object *popup, *box, *label, *ic, *button;

   if (sb->event)
     evas_object_freeze_events_set(sb->event, EINA_TRUE);

   popup = elm_popup_add(sb->ephoto->win);
   elm_object_part_text_set(popup, "title,text", _("Upload Image"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);

   label = elm_label_add(box);
   elm_object_text_set(label,
       _
       ("Are you sure you want to upload this image publically to imgur.com?"));
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, label);
   evas_object_show(label);

   ic = elm_icon_add(popup);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "document-save");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Yes"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _upload_image_confirm,
       popup);
   elm_object_part_content_set(popup, "button1", button);
   evas_object_show(button);

   ic = elm_icon_add(popup);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "window-close");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("No"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _upload_image_cancel,
       popup);
   elm_object_part_content_set(popup, "button2", button);
   evas_object_show(button);

   evas_object_data_set(popup, "single_browser", sb);
   elm_object_part_content_set(popup, "default", box);
   evas_object_show(popup);
}

static void
_error_ok(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto_Single_Browser *sb = evas_object_data_get(popup, "single_browser");

   evas_object_del(popup);
   if (sb->event)
     {
        evas_object_freeze_events_set(sb->event, EINA_FALSE);
        elm_object_focus_set(sb->event, EINA_TRUE);
     }
}

static void
_delete_apply(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto_Single_Browser *sb = evas_object_data_get(popup, "single_browser");
   char destination[PATH_MAX];
   int ret;

   snprintf(destination, PATH_MAX, "%s/.config/ephoto/trash", getenv("HOME"));

   if (!ecore_file_exists(destination))
      ecore_file_mkpath(destination);

   if (ecore_file_exists(sb->entry->path) && ecore_file_is_dir(destination))
     {
        char dest[PATH_MAX], fp[PATH_MAX], extra[PATH_MAX];

        snprintf(fp, PATH_MAX, "%s", sb->entry->path);
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
             ret = ecore_file_mv(sb->entry->path, extra);
          }
        else
          ret = ecore_file_mv(sb->entry->path, dest);
        if (!ret)
          {
             Evas_Object *ppopup, *box, *label, *ic, *button;

             if (sb->event)
               evas_object_freeze_events_set(sb->event, EINA_TRUE);

             ppopup = elm_popup_add(sb->ephoto->win);
             elm_object_part_text_set(ppopup, "title,text", _("Error"));
             elm_popup_orient_set(ppopup, ELM_POPUP_ORIENT_CENTER);

             box = elm_box_add(ppopup);
             elm_box_horizontal_set(box, EINA_FALSE);
             evas_object_size_hint_weight_set(box,
                 EVAS_HINT_FILL, EVAS_HINT_FILL);
             evas_object_size_hint_align_set(box,
                 EVAS_HINT_FILL, EVAS_HINT_FILL);
             evas_object_show(box);

             label = elm_label_add(box);
             elm_object_text_set(label,
                 _("There was an error deleting this file"));
             evas_object_size_hint_weight_set(label,
                 EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
             evas_object_size_hint_align_set(label,
                 EVAS_HINT_FILL, EVAS_HINT_FILL);
             elm_box_pack_end(box, label);
             evas_object_show(label);

             ic = elm_icon_add(ppopup);
             elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
             evas_object_size_hint_aspect_set(ic,
                 EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
             elm_icon_standard_set(ic, "window-close");

             button = elm_button_add(ppopup);
             elm_object_text_set(button, _("Ok"));
             elm_object_part_content_set(button, "icon", ic);
             evas_object_smart_callback_add(button, "clicked",
                 _error_ok, popup);
             elm_object_part_content_set(popup, "button1", button);
             evas_object_show(button);

             evas_object_data_set(ppopup, "single_browser", sb);
             elm_object_part_content_set(ppopup, "default", box);
             evas_object_show(ppopup);
          }
     }
   evas_object_del(popup);
   if (sb->event)
     {
        elm_object_focus_set(sb->event, EINA_TRUE);
        evas_object_freeze_events_set(sb->event, EINA_FALSE);
     }
}

static void
_delete_cancel(void *data, Evas_Object *obj EINA_UNUSED,
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
_delete_image(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Evas_Object *popup, *box, *label, *ic, *button;

   if (sb->event)
     evas_object_freeze_events_set(sb->event, EINA_TRUE);

   popup = elm_popup_add(sb->ephoto->win);
   elm_object_part_text_set(popup, "title,text", _("Delete File"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);

   label = elm_label_add(box);
   elm_object_text_set(label, _("Are you sure you want to delete this file?"));
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, label);
   evas_object_show(label);

   ic = elm_icon_add(popup);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1,
       1);
   elm_icon_standard_set(ic, "document-save");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Yes"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _delete_apply,
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
   evas_object_smart_callback_add(button, "clicked", _delete_cancel,
       popup);
   elm_object_part_content_set(popup, "button2", button);
   evas_object_show(button);

   evas_object_data_set(popup, "single_browser", sb);
   elm_object_part_content_set(popup, "default", box);
   evas_object_show(popup);
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
	elm_object_disabled_set(sb->bar, EINA_TRUE);
	evas_object_freeze_events_set(sb->bar, EINA_TRUE);
	Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

	elm_table_unpack(v->table, v->image);
	ephoto_cropper_add(sb->main, sb->mhbox, v->table, v->image);
     }
}

static void
_go_bcg(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->viewer)
     {
	sb->editing = EINA_TRUE;
	elm_object_disabled_set(sb->bar, EINA_TRUE);
	evas_object_freeze_events_set(sb->bar, EINA_TRUE);
	Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

	ephoto_bcg_add(sb->main, sb->mhbox, v->image);
     }
}

static void
_go_hsv(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->viewer)
     {
	sb->editing = EINA_TRUE;
	elm_object_disabled_set(sb->bar, EINA_TRUE);
	evas_object_freeze_events_set(sb->bar, EINA_TRUE);
	Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

	ephoto_hsv_add(sb->main, sb->mhbox, v->image);
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
	elm_object_disabled_set(sb->bar, EINA_TRUE);
	evas_object_freeze_events_set(sb->bar, EINA_TRUE);
	Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

	ephoto_color_add(sb->main, sb->mhbox, v->image);
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
	elm_object_disabled_set(sb->bar, EINA_TRUE);
	evas_object_freeze_events_set(sb->bar, EINA_TRUE);
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
	elm_object_disabled_set(sb->bar, EINA_TRUE);
	evas_object_freeze_events_set(sb->bar, EINA_TRUE);
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
	elm_object_disabled_set(sb->bar, EINA_TRUE);
	evas_object_freeze_events_set(sb->bar, EINA_TRUE);
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
	elm_object_disabled_set(sb->bar, EINA_TRUE);
	evas_object_freeze_events_set(sb->bar, EINA_TRUE);
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
	elm_object_disabled_set(sb->bar, EINA_TRUE);
	evas_object_freeze_events_set(sb->bar, EINA_TRUE);
	Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

	ephoto_filter_old_photo(sb->main, v->image);
     }
}

static void
_menu_dismissed_cb(void *data, Evas_Object *obj,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   evas_object_del(obj);
   elm_object_focus_set(sb->event, EINA_TRUE);
}

static void
_edit_menu(Ephoto_Single_Browser *sb)
{
   Evas_Object *menu, *menu_it;
   int x, y;

   evas_pointer_canvas_xy_get(evas_object_evas_get(sb->main), &x, &y);
   menu = elm_menu_add(sb->main);
   elm_menu_move(menu, x, y);
   elm_menu_item_add(menu, NULL, "edit-undo", _("Reset"), _reset_image, sb);
   elm_menu_item_add(menu, NULL, "document-save", _("Save"), _save_image, sb);
   elm_menu_item_add(menu, NULL, "document-save-as", _("Save As"),
       _save_image_as, sb);
   elm_menu_item_add(menu, NULL, "document-send", _("Upload"), _upload_image,
       sb);
   elm_menu_item_add(menu, NULL, "edit-delete", _("Delete"),
            _delete_image, sb);
   elm_menu_item_separator_add(menu, NULL);

   menu_it =
       elm_menu_item_add(menu, NULL, "document-properties", _("Transform"),
       NULL, NULL);
   elm_menu_item_add(menu, menu_it, "edit-cut", _("Crop"), _crop_image, sb);
   elm_menu_item_separator_add(menu, menu_it);
   elm_menu_item_add(menu, menu_it, "object-rotate-left", _("Rotate Left"),
       _go_rotate_counterclock, sb);
   elm_menu_item_add(menu, menu_it, "object-rotate-right", _("Rotate Right"),
       _go_rotate_clock, sb);
   elm_menu_item_add(menu, menu_it, "object-flip-horizontal",
       _("Flip Horizontal"), _go_flip_horiz, sb);
   elm_menu_item_add(menu, menu_it, "object-flip-vertical", _("Flip Vertical"),
       _go_flip_vert, sb);
   elm_menu_item_separator_add(menu, NULL);

   menu_it =
       elm_menu_item_add(menu, NULL, "document-properties", _("Color"), NULL,
       NULL);
   elm_menu_item_add(menu, menu_it, "insert-image", _("Auto Equalize"),
       _go_auto_eq, sb);
   elm_menu_item_separator_add(menu, menu_it);
   elm_menu_item_add(menu, menu_it, "insert-image",
       _("Brightness/Contrast/Gamma"), _go_bcg, sb);
   elm_menu_item_add(menu, menu_it, "insert-image", _("Hue/Saturation/Value"),
       _go_hsv, sb);
   elm_menu_item_add(menu, menu_it, "insert-image", _("Color Levels"),
       _go_color, sb);

   menu_it =
       elm_menu_item_add(menu, NULL, "document-properties", _("Filters"), NULL,
       NULL);
   elm_menu_item_add(menu, menu_it, "insert-image", _("Blur"), _go_blur, sb);
   elm_menu_item_add(menu, menu_it, "insert-image", _("Sharpen"), _go_sharpen,
       sb);
   elm_menu_item_separator_add(menu, menu_it);
   elm_menu_item_add(menu, menu_it, "insert-image", _("Black and White"),
       _go_black_and_white, sb);
   elm_menu_item_add(menu, menu_it, "insert-image", _("Old Photo"),
       _go_old_photo, sb);
   evas_object_smart_callback_add(menu, "dismissed", _menu_dismissed_cb,
       sb);
   evas_object_show(menu);
}

static void
_slideshow(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

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
_back(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
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
_settings(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   ephoto_config_main(sb->ephoto);
}

static void
_key_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
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
        _settings(sb, NULL, NULL);
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

	if (!elm_win_fullscreen_get(sb->ephoto->win))
	  {
	     if (sb->ephoto->config->tool_hide)
	       {
		  evas_object_hide(sb->bar);
		  elm_box_unpack(sb->tbox, sb->bar);
		  evas_object_hide(sb->tbox);
		  elm_box_unpack(sb->main, sb->tbox);
		  evas_object_hide(sb->botbox);
		  elm_table_unpack(sb->table, sb->botbox);
		  evas_object_hide(sb->infolabel);
		  elm_table_unpack(sb->table, sb->infolabel);
	       }
	  }
        else
	  {
	     if (!evas_object_visible_get(sb->bar))
	       {
		  elm_box_pack_start(sb->main, sb->tbox);
		  evas_object_show(sb->tbox);
		  elm_box_pack_start(sb->tbox, sb->bar);
		  evas_object_show(sb->bar);
		  elm_table_pack(sb->table, sb->botbox, 0, 2, 4, 1);
		  evas_object_show(sb->botbox);
		  elm_table_pack(sb->table, sb->infolabel, 0, 2, 4, 1);
		  evas_object_show(sb->infolabel);
	       }
	  }
	elm_win_fullscreen_set(win, !elm_win_fullscreen_get(win));
     }
}

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

static void
_main_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   char tmp_path[PATH_MAX];
   Ephoto_Single_Browser *sb = data;
   Ecore_Event_Handler *handler;
   Eina_Iterator *tmps;
   Eina_File_Direct_Info *info;

   EINA_LIST_FREE(sb->handlers, handler) ecore_event_handler_del(handler);
   if (sb->upload_handlers)
     {
	EINA_LIST_FREE(sb->upload_handlers,
	    handler) ecore_event_handler_del(handler);
     }
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
   if (sb->url_up)
      ecore_con_url_free(sb->url_up);
   if (sb->url_ret)
      free(sb->url_ret);
   if (sb->upload_error)
      free(sb->upload_error);
   free(sb);
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
   if (!sb->entry && sb->pending_path && e->path == sb->pending_path)
     {
	eina_stringshare_del(sb->pending_path);
	sb->pending_path = NULL;
	ephoto_single_browser_entry_set(sb->ephoto->single_browser, e);
     }

   return ECORE_CALLBACK_PASS_ON;
}

static void
_main_focused(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->ephoto->state == EPHOTO_STATE_SINGLE)
     {
	if (sb->event)
	   elm_object_focus_set(sb->event, EINA_TRUE);
     }
}

Evas_Object *
ephoto_single_browser_add(Ephoto *ephoto, Evas_Object *parent)
{
   Evas_Object *box = elm_box_add(parent);
   Elm_Object_Item *icon;
   Evas_Object *menu, *menu_it;
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
   evas_object_event_callback_add(sb->main, EVAS_CALLBACK_DEL, _main_del, sb);
   evas_object_event_callback_add(sb->ephoto->win, EVAS_CALLBACK_FOCUS_IN,
       _main_focused, sb);
   evas_object_size_hint_weight_set(sb->main, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(sb->main, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_data_set(sb->main, "single_browser", sb);

   sb->tbox = elm_box_add(sb->main);
   evas_object_size_hint_weight_set(sb->tbox, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(sb->tbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_tree_focus_allow_set(sb->tbox, EINA_FALSE);
   elm_box_pack_end(sb->main, sb->tbox);
   evas_object_show(sb->tbox);

   sb->bar = elm_toolbar_add(sb->ephoto->win);
   elm_toolbar_horizontal_set(sb->bar, EINA_TRUE);
   elm_toolbar_homogeneous_set(sb->bar, EINA_TRUE);
   elm_toolbar_shrink_mode_set(sb->bar, ELM_TOOLBAR_SHRINK_SCROLL);
   elm_object_tree_focus_allow_set(sb->bar, EINA_FALSE);
   elm_toolbar_select_mode_set(sb->bar, ELM_OBJECT_SELECT_MODE_NONE);
   elm_toolbar_icon_order_lookup_set(sb->bar, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_weight_set(sb->bar, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(sb->bar, EVAS_HINT_FILL, EVAS_HINT_FILL);

   icon = elm_toolbar_item_append(sb->bar, "go-home", _("Back"), _back, sb);
   icon =
       elm_toolbar_item_append(sb->bar, "image-x-generic", _("Edit"), NULL,
       NULL);
   elm_toolbar_item_menu_set(icon, EINA_TRUE);
   elm_toolbar_menu_parent_set(sb->bar, sb->ephoto->win);
   menu = elm_toolbar_item_menu_get(icon);

   elm_menu_item_add(menu, NULL, "edit-undo", _("Reset"), _reset_image, sb);
   elm_menu_item_add(menu, NULL, "document-save", _("Save"), _save_image, sb);
   elm_menu_item_add(menu, NULL, "document-save-as", _("Save As"),
       _save_image_as, sb);
   elm_menu_item_add(menu, NULL, "document-send", _("Upload"), _upload_image,
       sb);
   elm_menu_item_add(menu, NULL, "edit-delete", _("Delete"),
            _delete_image, sb);
   elm_menu_item_separator_add(menu, NULL);

   menu_it =
       elm_menu_item_add(menu, NULL, "document-properties", _("Transform"),
       NULL, NULL);
   elm_menu_item_add(menu, menu_it, "edit-cut", _("Crop"), _crop_image, sb);
   elm_menu_item_separator_add(menu, menu_it);
   elm_menu_item_add(menu, menu_it, "object-rotate-left", _("Rotate Left"),
       _go_rotate_counterclock, sb);
   elm_menu_item_add(menu, menu_it, "object-rotate-right", _("Rotate Right"),
       _go_rotate_clock, sb);
   elm_menu_item_add(menu, menu_it, "object-flip-horizontal",
       _("Flip Horizontal"), _go_flip_horiz, sb);
   elm_menu_item_add(menu, menu_it, "object-flip-vertical", _("Flip Vertical"),
       _go_flip_vert, sb);
   elm_menu_item_separator_add(menu, NULL);

   menu_it =
       elm_menu_item_add(menu, NULL, "document-properties", _("Color"), NULL,
       NULL);
   elm_menu_item_add(menu, menu_it, "insert-image", _("Auto Equalize"),
       _go_auto_eq, sb);
   elm_menu_item_separator_add(menu, menu_it);
   elm_menu_item_add(menu, menu_it, "insert-image",
       _("Brightness/Contrast/Gamma"), _go_bcg, sb);
   elm_menu_item_add(menu, menu_it, "insert-image", _("Hue/Saturation/Value"),
       _go_hsv, sb);
   elm_menu_item_add(menu, menu_it, "insert-image", _("Color Levels"),
       _go_color, sb);

   menu_it =
       elm_menu_item_add(menu, NULL, "document-properties", _("Filters"), NULL,
       NULL);
   elm_menu_item_add(menu, menu_it, "insert-image", _("Blur"), _go_blur, sb);
   elm_menu_item_add(menu, menu_it, "insert-image", _("Sharpen"), _go_sharpen,
       sb);
   elm_menu_item_separator_add(menu, menu_it);
   elm_menu_item_add(menu, menu_it, "insert-image", _("Black and White"),
       _go_black_and_white, sb);
   elm_menu_item_add(menu, menu_it, "insert-image", _("Old Photo"),
       _go_old_photo, sb);

   icon =
       elm_toolbar_item_append(sb->bar, "go-first", _("First"), _go_first, sb);
   icon =
       elm_toolbar_item_append(sb->bar, "go-previous", _("Previous"), _go_prev,
       sb);
   icon = elm_toolbar_item_append(sb->bar, "go-next", _("Next"), _go_next, sb);
   icon = elm_toolbar_item_append(sb->bar, "go-last", _("Last"), _go_last, sb);

   icon =
       elm_toolbar_item_append(sb->bar, "zoom-in", _("Zoom In"), _zoom_in_cb,
       sb);
   icon =
       elm_toolbar_item_append(sb->bar, "zoom-out", _("Zoom Out"), _zoom_out_cb,
       sb);
   icon =
       elm_toolbar_item_append(sb->bar, "zoom-fit-best", _("Zoom Fit"),
       _zoom_fit_cb, sb);
   icon =
       elm_toolbar_item_append(sb->bar, "zoom-original", _("Zoom 1:1"),
       _zoom_1_cb, sb);

   icon =
       elm_toolbar_item_append(sb->bar, "media-playback-start", _("Slideshow"),
       _slideshow, sb);
   icon =
       elm_toolbar_item_append(sb->bar, "preferences-system", _("Settings"),
       _settings, sb);

   elm_box_pack_end(sb->tbox, sb->bar);
   evas_object_show(sb->bar);

   sb->mhbox = elm_box_add(sb->main);
   elm_box_horizontal_set(sb->mhbox, EINA_TRUE);
   evas_object_size_hint_weight_set(sb->mhbox, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(sb->mhbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(sb->main, sb->mhbox);
   evas_object_show(sb->mhbox);

   sb->table = elm_table_add(sb->mhbox);
   evas_object_size_hint_weight_set(sb->table, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(sb->table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(sb->mhbox, sb->table);
   evas_object_show(sb->table);

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
   if (elm_win_fullscreen_get(sb->ephoto->win))
     {
	if (sb->ephoto->config->tool_hide)
	  {
	     evas_object_hide(sb->bar);
	     elm_box_unpack(sb->tbox, sb->bar);
	     evas_object_hide(sb->tbox);
	     elm_box_unpack(sb->main, sb->tbox);
	     evas_object_hide(sb->botbox);
	     elm_table_unpack(sb->table, sb->botbox);
	     evas_object_hide(sb->infolabel);
	     elm_table_unpack(sb->table, sb->infolabel);
	  }
     }
   else
     {
	if (!evas_object_visible_get(sb->bar))
	  {
	     elm_box_pack_start(sb->main, sb->tbox);
	     evas_object_show(sb->tbox);
	     elm_box_pack_start(sb->tbox, sb->bar);
	     evas_object_show(sb->bar);
	     elm_table_pack(sb->table, sb->botbox, 0, 2, 4, 1);
	     evas_object_show(sb->botbox);
	     elm_table_pack(sb->table, sb->infolabel, 0, 2, 4, 1);
	     evas_object_show(sb->infolabel);
	  }
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
   evas_object_event_callback_add(sb->event, EVAS_CALLBACK_KEY_DOWN, _key_down,
       sb);
   evas_object_raise(sb->event);
   elm_object_focus_set(sb->event, EINA_TRUE);
}

void
ephoto_single_browser_path_pending_set(Evas_Object *obj, const char *path)
{
   Ephoto_Single_Browser *sb = evas_object_data_get(obj, "single_browser");

   sb->pending_path = eina_stringshare_add(path);
}

void
ephoto_single_browser_image_data_update(Evas_Object *main, Evas_Object *image,
    Eina_Bool finished, unsigned int *image_data, int w, int h)
{
   Ephoto_Single_Browser *sb = evas_object_data_get(main, "single_browser");

   if (sb->editing)
     {
	if (sb->cropping)
	  {
	     evas_object_image_size_set(elm_image_object_get(image), w, h);
	     sb->cropping = EINA_FALSE;
	  }
	evas_object_image_data_set(elm_image_object_get(image), image_data);
	evas_object_image_data_update_add(elm_image_object_get(image), 0, 0, w,
	    h);

	if (finished)
	  {
	     char image_info[PATH_MAX];

	     evas_object_del(sb->botbox);
	     evas_object_del(sb->infolabel);
	     snprintf(image_info, PATH_MAX,
		 "<b>%s:</b> %s        <b>%s:</b> %dx%d        <b>%s:</b> %s",
		 _("Type"), efreet_mime_type_get(sb->entry->path),
		 _("Resolution"), w, h, _("File Size"), _("N/A"));
	     sb->botbox = elm_box_add(sb->table);
	     evas_object_size_hint_min_set(sb->botbox, 0,
		 sb->ephoto->bottom_bar_size);
	     evas_object_size_hint_weight_set(sb->botbox, EVAS_HINT_EXPAND,
		 0.0);
	     evas_object_size_hint_fill_set(sb->botbox, EVAS_HINT_FILL,
		 EVAS_HINT_FILL);
	     elm_table_pack(sb->table, sb->botbox, 0, 2, 4, 1);
	     evas_object_show(sb->botbox);

	     sb->infolabel = elm_label_add(sb->table);
	     elm_label_line_wrap_set(sb->infolabel, ELM_WRAP_NONE);
	     elm_object_text_set(sb->infolabel, image_info);
	     evas_object_size_hint_weight_set(sb->infolabel, EVAS_HINT_EXPAND,
		 EVAS_HINT_FILL);
	     evas_object_size_hint_align_set(sb->infolabel, EVAS_HINT_FILL,
		 EVAS_HINT_FILL);
	     elm_table_pack(sb->table, sb->infolabel, 0, 2, 4, 1);
	     evas_object_show(sb->infolabel);

	     sb->edited_image_data = image_data;
	     sb->ew = w;
	     sb->eh = h;

	     evas_object_freeze_events_set(sb->bar, EINA_FALSE);
	     elm_object_disabled_set(sb->bar, EINA_FALSE);
	     sb->editing = EINA_FALSE;
	     _zoom_fit(sb);
	  }
     }
}

void
ephoto_single_browser_cancel_editing(Evas_Object *main, Evas_Object *image)
{
   Ephoto_Single_Browser *sb = evas_object_data_get(main, "single_browser");

   if (sb->editing)
     {
	if (sb->cropping)
	   sb->cropping = EINA_FALSE;
	if (sb->edited_image_data)
	  {
	     evas_object_image_size_set(elm_image_object_get(image), sb->ew,
		 sb->eh);
	     evas_object_image_data_set(elm_image_object_get(image),
		 sb->edited_image_data);
	     evas_object_image_data_update_add(elm_image_object_get(image), 0,
		 0, sb->ew, sb->eh);
	  }
        else
	  {
	     const char *group = NULL;
	     const char *ext = strrchr(sb->entry->path, '.');

	     if (ext)
	       {
		  ext++;
		  if ((strcasecmp(ext, "edj") == 0))
		    {
		       if (edje_file_group_exists(sb->entry->path,
			       "e/desktop/background"))
			  group = "e/desktop/background";
		       else
			 {
			    Eina_List *g =
				edje_file_collection_list(sb->entry->path);
			    group = eina_list_data_get(g);
			    edje_file_collection_list_free(g);
			 }
		       elm_image_file_set(image, sb->entry->path, group);
		    }
	       }
	  }
	evas_object_freeze_events_set(sb->bar, EINA_FALSE);
	elm_object_disabled_set(sb->bar, EINA_FALSE);
	sb->editing = EINA_FALSE;
	_zoom_fit(sb);
     }
}
