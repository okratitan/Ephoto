#include "ephoto.h"

#define ZOOM_STEP 0.2

static Ecore_Timer *_1s_hold = NULL;

typedef struct _Ephoto_Single_Browser Ephoto_Single_Browser;
typedef struct _Ephoto_Viewer Ephoto_Viewer;

struct _Ephoto_Single_Browser
{
   Ephoto *ephoto;
   Evas_Object *main;
   Evas_Object *bar;
   Evas_Object *table;
   Evas_Object *viewer;
   Evas_Object *infolabel;
   Evas_Object *nolabel;
   Evas_Object *botbox;
   const char *pending_path;
   Ephoto_Entry *entry;
   Ephoto_Orient orient;
   Eina_List *handlers;
   Eina_Bool cropping:1;
};

struct _Ephoto_Viewer
{
   Evas_Object *scroller;
   Evas_Object *table;
   Evas_Object *image;
   double zoom;
   Eina_Bool fit:1;
   Eina_Bool cropping:1;
};

static void _zoom_set(Ephoto_Single_Browser *sb, double zoom);
static void _zoom_in(Ephoto_Single_Browser *sb);
static void _zoom_out(Ephoto_Single_Browser *sb);
static void _apply_crop(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED);
static void _cancel_crop(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED);

static void
_viewer_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Viewer *v = data;
   free(v);
}

static Evas_Object *
_image_create_icon(void *data, Evas_Object *parent, Evas_Coord *xoff, Evas_Coord *yoff)
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
   if (xoff) *xoff = xm-30;
   if (yoff) *yoff = ym-30;

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
                  _image_create_icon, io,
                  NULL, NULL, NULL, NULL, NULL, NULL);
   _1s_hold = NULL;

   return ECORE_CALLBACK_CANCEL;
}

static void
_image_mouse_down_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *io = data;

   _1s_hold = ecore_timer_add(0.5, _1s_hold_time, io);
}

static void
_image_mouse_up_cb(void *data EINA_UNUSED, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   ecore_timer_del(_1s_hold);
}

static Evas_Object *
_viewer_add(Evas_Object *parent, const char *path)
{
   Ephoto_Single_Browser *sb = evas_object_data_get(parent, "single_browser");
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

   v->cropping = sb->cropping;
   if (v->cropping)
     v->image = ephoto_cropper_add(v->table, path, group);
   else
     {
        v->image = elm_image_add(v->table);
        elm_image_preload_disabled_set(v->image, EINA_TRUE);
        elm_image_file_set(v->image, path, group);
        err = evas_object_image_load_error_get(elm_image_object_get(v->image));
        if (err != EVAS_LOAD_ERROR_NONE) goto load_error;
        evas_object_image_size_get(elm_image_object_get(v->image), &w, &h);
        elm_drop_target_add(v->image, ELM_SEL_FORMAT_IMAGE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        evas_object_size_hint_min_set(v->image, w, h);
        evas_object_size_hint_max_set(v->image, w, h);
        evas_object_event_callback_add(v->image, EVAS_CALLBACK_MOUSE_DOWN, _image_mouse_down_cb, v->image);
        evas_object_event_callback_add(v->image, EVAS_CALLBACK_MOUSE_UP, _image_mouse_up_cb, v->image);
     }
   elm_table_pack(v->table, v->image, 0, 0, 1, 1);
   evas_object_show(v->image);

   return obj;

 load_error:
   ERR("could not load image '%s': %s", path, evas_load_error_str(err));
 error:
   evas_object_event_callback_del(obj, EVAS_CALLBACK_DEL, _viewer_del);
   evas_object_data_del(obj, "viewer");
   free(v);
   return NULL;
}

static void
_viewer_zoom_apply(Ephoto_Viewer *v, double zoom)
{
   v->zoom = zoom;

   Evas_Coord w, h;
   Evas_Object *image;
   if (v->cropping)
     image = evas_object_data_get(v->image, "image");
   else
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

   if (v->cropping)
     image = evas_object_data_get(v->image, "image");
   else
     image = v->image;
   evas_object_geometry_get(v->scroller, NULL, NULL, &cw, &ch);
   evas_object_image_size_get(elm_image_object_get(image), &iw, &ih);

   if ((cw <= 0) || (ch <= 0)) return; /* object still not resized */
   EINA_SAFETY_ON_TRUE_RETURN(iw <= 0);
   EINA_SAFETY_ON_TRUE_RETURN(ih <= 0);

   zx = (double)cw/(double)iw;
   zy = (double)ch/(double)ih;

   zoom = (zx < zy) ? zx : zy;
   _viewer_zoom_apply(v, zoom);
}

static void
_viewer_resized(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
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
   evas_object_size_hint_min_set(v->image, w, h);
   evas_object_size_hint_max_set(v->image, w, h);
   elm_table_pack(v->table, v->image, 0, 0, 1, 1);
   elm_object_content_set(v->scroller, v->table);
   if (v->fit)
     _viewer_zoom_fit_apply(v);
   else
     _viewer_zoom_set(sb->viewer, _viewer_zoom_get(sb->viewer));
   DBG("orient: %d", sb->orient);
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
_mouse_wheel(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   Ephoto_Single_Browser *sb = data;
   Evas_Event_Mouse_Wheel *ev = event_info;
   if (!evas_key_modifier_is_set(ev->modifiers, "Control")) return;

   if (ev->z > 0) _zoom_out(sb);
   else _zoom_in(sb);
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

static const char *
_ephoto_get_file_size(const char *path)
{
   char isize[PATH_MAX];
   Eina_File *f = eina_file_open(path, EINA_FALSE);
   size_t size = eina_file_size_get(f);
   eina_file_close(f);
   double dsize = (double)size;
   if (dsize < 1024.0) snprintf(isize, sizeof(isize), "%'.0f bytes", dsize);
   else
     {
        dsize /= 1024.0;
        if (dsize < 1024) snprintf(isize, sizeof(isize), "%'.0f KB", dsize);
        else
          {
             dsize /= 1024.0;
             if (dsize < 1024) snprintf(isize, sizeof(isize), "%'.1f MB", dsize);
             else
               {
                  dsize /= 1024.0;
                  if (dsize < 1024) snprintf(isize, sizeof(isize), "%'.1f GB", dsize);
                  else
                    {
                       dsize /= 1024.0;
                       snprintf(isize, sizeof(isize), "%'.1f TB", dsize);
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

        sb->viewer = _viewer_add(sb->main, sb->entry->path);
        if (sb->viewer)
          {
             char image_info[PATH_MAX];
             Evas_Coord w, h;
             Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");

             elm_table_pack(sb->table, sb->viewer, 0, 1, 4, 1);
             evas_object_show(sb->viewer);
             evas_object_event_callback_add
               (sb->viewer, EVAS_CALLBACK_MOUSE_WHEEL, _mouse_wheel, sb);

             if (sb->cropping)
               {
                  Evas_Object *ic, *button;

                  sb->botbox = elm_box_add(sb->table);
                  elm_box_homogeneous_set(sb->botbox, EINA_TRUE);
                  elm_box_horizontal_set(sb->botbox, EINA_TRUE);
                  evas_object_size_hint_weight_set(sb->botbox, EVAS_HINT_EXPAND, 0.0);
                  evas_object_size_hint_align_set(sb->botbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
                  elm_table_pack(sb->table, sb->botbox, 0, 2, 4, 1);
                  evas_object_show(sb->botbox);

                  ic = elm_icon_add(sb->botbox);
                  evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
                  elm_icon_standard_set(ic, "document-save");
                  button = elm_button_add(sb->botbox);
                  elm_object_text_set(button, "Apply");
                  elm_object_part_content_set(button, "icon", ic);
                  evas_object_smart_callback_add(button, "clicked", _apply_crop, sb);
                  elm_box_pack_end(sb->botbox, button);
                  evas_object_show(button);

                  ic = elm_icon_add(sb->botbox);
                  evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
                  elm_icon_standard_set(ic, "window-close");
                  button = elm_button_add(sb->botbox);
                  elm_object_text_set(button, "Cancel");
                  elm_object_part_content_set(button, "icon", ic);
                  evas_object_smart_callback_add(button, "clicked", _cancel_crop, sb);
                  elm_box_pack_end(sb->botbox, button);
                  evas_object_show(button);
               }
             else
               {
                  evas_object_image_size_get(elm_image_object_get(v->image), &w, &h);
                  snprintf(image_info, PATH_MAX,
                           "<b>Type:</b> %s        <b>Resolution:</b> %dx%d        <b>File Size: </b>%s",
                           efreet_mime_type_get(sb->entry->path), w, h,
                           _ephoto_get_file_size(sb->entry->path));
                  sb->botbox = evas_object_rectangle_add(evas_object_evas_get(sb->table));
                  evas_object_color_set(sb->botbox, 0, 0, 0, 0);
                  evas_object_size_hint_min_set(sb->botbox, 0, sb->ephoto->bottom_bar_size);
                  evas_object_size_hint_weight_set(sb->botbox, EVAS_HINT_EXPAND, 0.0);
                  evas_object_size_hint_fill_set(sb->botbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
                  elm_table_pack(sb->table, sb->botbox, 0, 2, 4, 1);
                  evas_object_show(sb->botbox);

                  sb->infolabel = elm_label_add(sb->table);
                  elm_label_line_wrap_set(sb->infolabel, ELM_WRAP_NONE);
                  elm_object_text_set(sb->infolabel, image_info);
                  evas_object_size_hint_weight_set(sb->infolabel, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
                  evas_object_size_hint_align_set(sb->infolabel, EVAS_HINT_FILL, EVAS_HINT_FILL);
                  elm_table_pack(sb->table, sb->infolabel, 0, 2, 4, 1);
                  evas_object_show(sb->infolabel);

                  ephoto_title_set(sb->ephoto, bname);
               }
          }
        else
          {
             sb->nolabel = elm_label_add(sb->table);
             elm_label_line_wrap_set(sb->nolabel, ELM_WRAP_WORD);
             elm_object_text_set(sb->nolabel, "This image does not exist or is corrupted");
             evas_object_size_hint_weight_set(sb->nolabel, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
             evas_object_size_hint_align_set(sb->nolabel, EVAS_HINT_FILL, EVAS_HINT_FILL);
             elm_table_pack(sb->table, sb->nolabel, 0, 1, 4, 1);
             evas_object_show(sb->nolabel);
             ephoto_title_set(sb->ephoto, "Bad Image");
          }
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
   double change = (1.0+ZOOM_STEP);
   _viewer_zoom_set(sb->viewer, _viewer_zoom_get(sb->viewer) * change);
}

static void
_zoom_out(Ephoto_Single_Browser *sb)
{
   double change = (1.0-ZOOM_STEP);
   _viewer_zoom_set(sb->viewer, _viewer_zoom_get(sb->viewer) * change);
}

static void
_zoom_in_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   _zoom_in(sb);
}

static void
_zoom_out_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   _zoom_out(sb);
}

static void
_zoom_1_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   _zoom_set(sb, 1.0);
}

static void
_zoom_fit_cb(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
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
_reset_yes(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *win = data;
   Ephoto_Single_Browser *sb = evas_object_data_get(win, "single_browser");
   sb->cropping = EINA_FALSE;
   ephoto_single_browser_entry_set(sb->main, sb->entry);
   evas_object_del(win);
}

static void
_reset_no(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *win = data;
   evas_object_del(win);
}

static void
_reset_image(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Evas_Object *win, *box, *label, *hbox, *ic, *button;

   win = elm_win_inwin_add(sb->ephoto->win);
   elm_object_style_set(win, "minimal");
   evas_object_show(win);

   box = elm_box_add(win);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);

   label = elm_label_add(box);
   elm_object_text_set(label, "Are you sure you want to reset your changes?");
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, label);
   evas_object_show(label);

   hbox = elm_box_add(win);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, hbox);
   evas_object_show(hbox);

   ic = elm_icon_add(hbox);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "stock_save");

   button = elm_button_add(hbox);
   elm_object_text_set(button, "Yes");
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _reset_yes, win);
   elm_box_pack_end(hbox, button);
   evas_object_show(button);

   ic = elm_icon_add(hbox);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "stock_close");

   button = elm_button_add(hbox);
   elm_object_text_set(button, "No");
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _reset_no, win);
   elm_box_pack_end(hbox, button);
   evas_object_show(button);

   evas_object_data_set(win, "single_browser", sb);
   elm_win_inwin_content_set(win, box);
   evas_object_show(box);
}

static void
_failed_save(Ephoto_Single_Browser *sb)
{
   Evas_Object *win, *box, *label, *ic, *button;

   win = elm_win_inwin_add(sb->ephoto->win);
   elm_object_style_set(win, "minimal");
   evas_object_show(win);

   box = elm_box_add(win);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);

   label = elm_label_add(box);
   elm_object_text_set(label, "Error: Image could not be saved here!");
   evas_object_size_hint_weight_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, label);
   evas_object_show(label);

   ic = elm_icon_add(box);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "stock_close");

   button = elm_button_add(box);
   elm_object_text_set(button, "OK");
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _reset_no, win);
   elm_box_pack_end(box, button);
   evas_object_show(button);

   evas_object_data_set(win, "single_browser", sb);
   elm_win_inwin_content_set(win, box);
   evas_object_show(box);
}

static void
_save_yes(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *win = data;
   Ephoto_Single_Browser *sb = evas_object_data_get(win, "single_browser");
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");
   Eina_Bool success;
   if (ecore_file_exists(sb->entry->path))
     {
        success = ecore_file_unlink(sb->entry->path);
        if (!success)
          _failed_save(sb);
     }
   else
     {
        success = evas_object_image_save(elm_image_object_get(v->image), sb->entry->path, NULL, NULL);
        if (!success)
          _failed_save(sb);
     }
   ephoto_single_browser_entry_set(sb->main, sb->entry);
   evas_object_del(win);
}

static void
_save_no(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *win = data;
   evas_object_del(win);
}

static void
_save_image(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   if (sb->cropping)
     {
        sb->cropping = EINA_FALSE;
        ephoto_single_browser_entry_set(sb->main, sb->entry);
     }
   Evas_Object *win, *box, *label, *hbox, *ic, *button;

   win = elm_win_inwin_add(sb->ephoto->win);
   elm_object_style_set(win, "minimal");
   evas_object_show(win);

   box = elm_box_add(win);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);

   label = elm_label_add(box);
   elm_object_text_set(label, "Are you sure you want to overwrite this image?");
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, label);
   evas_object_show(label);

   hbox = elm_box_add(win);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, hbox);
   evas_object_show(hbox);

   ic = elm_icon_add(hbox);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "stock_save");

   button = elm_button_add(hbox);
   elm_object_text_set(button, "Yes");
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _save_yes, win);
   elm_box_pack_end(hbox, button);
   evas_object_show(button);

   ic = elm_icon_add(hbox);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "stock_close");

   button = elm_button_add(hbox);
   elm_object_text_set(button, "No");
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _save_no, win);
   elm_box_pack_end(hbox, button);
   evas_object_show(button);

   evas_object_data_set(win, "single_browser", sb);
   elm_win_inwin_content_set(win, box);
   evas_object_show(box);
}

static void
_save_image_as_overwrite(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *win = data;
   const char *file = evas_object_data_get(win, "file");
   Ephoto_Single_Browser *sb = evas_object_data_get(win, "single_browser");
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");
   Eina_Bool success;
   if (ecore_file_exists(file))
     {
        success = ecore_file_unlink(file);
        if (!success)
          _failed_save(sb);
     }
   else
     {
        success = evas_object_image_save(elm_image_object_get(v->image), file, NULL, NULL);
        if (!success)
          _failed_save(sb);
     }
   ephoto_single_browser_entry_set(sb->main, sb->entry);
   evas_object_del(win);
}

static void
_save_image_as_done(void *data, Evas_Object *obj EINA_UNUSED, void *event_info)
{
   const char *selected = event_info;
   Evas_Object *win = data;

   if (selected)
     {
        Ephoto_Single_Browser *sb = evas_object_data_get(win, "single_browser");
        Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");
        Eina_Bool success;

        char buf[PATH_MAX];
        if (!evas_object_image_extension_can_load_get(selected))
          snprintf(buf, PATH_MAX, "%s.jpg", selected);
        else
          snprintf(buf, PATH_MAX, "%s", selected);
        if (ecore_file_exists(buf))
          {
             Evas_Object *inwin, *box, *label, *hbox, *ic, *button;

             inwin = elm_win_inwin_add(sb->ephoto->win);
             elm_object_style_set(inwin, "minimal");
             evas_object_show(inwin);

             box = elm_box_add(inwin);
             elm_box_horizontal_set(box, EINA_FALSE);
             evas_object_size_hint_weight_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
             evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);

             label = elm_label_add(box);
             elm_object_text_set(label, "Are you sure you want to overwrite this image?");
             evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
             evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
             elm_box_pack_end(box, label);
             evas_object_show(label);

             hbox = elm_box_add(inwin);
             elm_box_horizontal_set(hbox, EINA_TRUE);
             evas_object_size_hint_weight_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
             evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
             elm_box_pack_end(box, hbox);
             evas_object_show(hbox);

             ic = elm_icon_add(hbox);
             evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
             elm_icon_standard_set(ic, "stock_save");
          
             button = elm_button_add(hbox);
             elm_object_text_set(button, "Yes");
             elm_object_part_content_set(button, "icon", ic);
             evas_object_smart_callback_add(button, "clicked", _save_image_as_overwrite, inwin);
             elm_box_pack_end(hbox, button);
             evas_object_show(button);

             ic = elm_icon_add(hbox);
             evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
             elm_icon_standard_set(ic, "stock_close");

             button = elm_button_add(hbox);
             elm_object_text_set(button, "No");
             elm_object_part_content_set(button, "icon", ic);
             evas_object_smart_callback_add(button, "clicked", _save_no, inwin);
             elm_box_pack_end(hbox, button);
             evas_object_show(button);

             evas_object_data_set(inwin, "single_browser", sb);
             evas_object_data_set(inwin, "file", strdup(buf));
             elm_win_inwin_content_set(inwin, box);
             evas_object_show(box);
          }
        else
          {
             success = evas_object_image_save(elm_image_object_get(v->image), buf, NULL, NULL);
             if (!success)
               _failed_save(sb);
          }
     }
   evas_object_del(win);
}

static void
_save_image_as(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   if (sb->cropping)
     {
        sb->cropping = EINA_FALSE;
        ephoto_single_browser_entry_set(sb->main, sb->entry);
     }
   Evas_Object *win, *fsel;

   win = elm_win_inwin_add(sb->ephoto->win);
   evas_object_show(win);

   fsel = elm_fileselector_add(win);
   elm_fileselector_is_save_set(fsel, EINA_TRUE);
   elm_fileselector_expandable_set(fsel, EINA_FALSE);
   elm_fileselector_path_set(fsel, sb->ephoto->config->directory);
   elm_fileselector_current_name_set(fsel, "Save as...");
   elm_fileselector_mime_types_filter_append(fsel, "image/*", "Image Files");
   evas_object_size_hint_weight_set(fsel, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(fsel, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_smart_callback_add(fsel, "done", _save_image_as_done, win);

   evas_object_data_set(win, "single_browser", sb);
   elm_win_inwin_content_set(win, fsel);
   evas_object_show(fsel);
}

static void
_apply_crop(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   Ephoto_Viewer *v = evas_object_data_get(sb->viewer, "viewer");
   Evas_Object *cropper = v->image;
   Evas_Object *layout = evas_object_data_get(cropper, "layout");
   Evas_Object *image = evas_object_data_get(cropper, "image");
   Evas_Object *edje = elm_layout_edje_get(layout);
   Evas_Object *crop;

   const char *path, *key, *type;;
   char tmp_path[PATH_MAX], image_info[PATH_MAX];
   int x, y, w, h, cx, cy, cw, ch, iw, ih;
   int nx, ny, nw, nh, i, j, tmpx, tmpy, ind, index;
   double scalex, scaley, scalew, scaleh;
   unsigned int *idata, *idata_new;

   elm_image_file_get(image, &path, &key);
   crop = evas_object_image_add(evas_object_evas_get(sb->main));
   evas_object_image_file_set(crop, path, key);

   evas_object_geometry_get(layout, &x, &y, &w, &h);
   edje_object_part_geometry_get(edje, "ephoto.swallow.cropper", &cx, &cy, &cw, &ch);
   evas_object_image_size_get(crop, &iw, &ih);

   idata = evas_object_image_data_get(crop, EINA_FALSE);

   scalex = (double)cx/(double)w;
   scaley = (double)cy/(double)h;
   scalew = (double)cw/(double)w;
   scaleh = (double)ch/(double)h;

   nx = iw*scalex;
   ny = ih*scaley;
   nw = iw*scalew;
   nh = ih*scaleh;

   index = 0;
   idata_new = malloc(sizeof(unsigned int)*nw*nh);

   for (i = 0; i < nh; i++)
     {
        tmpy = (i+ny)*iw;
        for (j = 0; j < nw; j++)
          {
             tmpx = j+nx;
             ind = tmpy+tmpx;
             idata_new[index] = idata[ind];
             index++;
          }
     }
   evas_object_image_size_set(crop, nw, nh);
   evas_object_image_data_set(crop, idata_new);
   evas_object_image_data_update_add(crop, 0, 0, nw, nh);

   type = strrchr(sb->entry->basename, '.');
   snprintf(tmp_path, PATH_MAX, "%s/.config/ephoto/tmp%s", getenv("HOME"), type);
   if (ecore_file_exists(tmp_path))
     ecore_file_unlink(tmp_path);
   evas_object_image_save(crop, tmp_path, NULL, NULL);
   evas_object_del(crop);

   sb->cropping = EINA_FALSE;
   evas_object_del(sb->viewer);
   sb->viewer = _viewer_add(sb->main, tmp_path);
   elm_table_pack(sb->table, sb->viewer, 0, 1, 4, 1);
   evas_object_show(sb->viewer);

   evas_object_del(sb->botbox);
   snprintf(image_info, PATH_MAX,
             "<b>Type:</b> %s        <b>Resolution:</b> %dx%d        <b>File Size: </b>%s",
             efreet_mime_type_get(tmp_path), nw, nh,
             _ephoto_get_file_size(tmp_path));
   sb->botbox = evas_object_rectangle_add(evas_object_evas_get(sb->table));
   evas_object_color_set(sb->botbox, 0, 0, 0, 0);
   evas_object_size_hint_min_set(sb->botbox, 0, sb->ephoto->bottom_bar_size);
   evas_object_size_hint_weight_set(sb->botbox, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_fill_set(sb->botbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(sb->table, sb->botbox, 0, 2, 4, 1);
   evas_object_show(sb->botbox);

   sb->infolabel = elm_label_add(sb->table);
   elm_label_line_wrap_set(sb->infolabel, ELM_WRAP_NONE);
   elm_object_text_set(sb->infolabel, image_info);
   evas_object_size_hint_weight_set(sb->infolabel, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(sb->infolabel, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(sb->table, sb->infolabel, 0, 2, 4, 1);
   evas_object_show(sb->infolabel);

   _zoom_fit(sb);
}

static void
_cancel_crop(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   sb->cropping = EINA_FALSE;
   ephoto_single_browser_entry_set(sb->main, sb->entry);
}

static void
_crop_image(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   sb->cropping = EINA_TRUE;
   ephoto_single_browser_entry_set(sb->main, sb->entry);
}

static void
_go_first(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   _first_entry(sb);
}

static void
_go_prev(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   _prev_entry(sb);
}

static void
_go_next(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   _next_entry(sb);
}

static void
_go_last(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   _last_entry(sb);
}

static void
_go_rotate_counterclock(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   _rotate_counterclock(sb);
}

static void
_go_rotate_clock(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   _rotate_clock(sb);
}

static void
_go_flip_horiz(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   _flip_horiz(sb);
}

static void
_go_flip_vert(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   _flip_vert(sb);
}

static void
_slideshow(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   if (sb->entry)
     evas_object_smart_callback_call(sb->main, "slideshow", sb->entry);
}

static void
_back(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   evas_object_smart_callback_call(sb->main, "back", sb->entry);
}

static void
_settings(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->ephoto)
     ephoto_config_window(sb->ephoto);
}

static void
_about(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;

   if (sb->ephoto)
     ephoto_about_window(sb->ephoto);
}

static void
_key_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info)
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
     {
        if (sb->cropping)
          _cancel_crop(sb, NULL, NULL);
        else
          evas_object_smart_callback_call(sb->main, "back", sb->entry);
     }
   else if (!strcmp(k, "Return"))
     {
        if (sb->cropping)
          _apply_crop(sb, NULL, NULL);
     }
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
_entry_free(void *data, const Ephoto_Entry *entry EINA_UNUSED)
{
   Ephoto_Single_Browser *sb = data;
   sb->entry = NULL;
}

static void
_main_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   char tmp_path[PATH_MAX];
   Ephoto_Single_Browser *sb = data;
   Ecore_Event_Handler *handler;
   Eina_Iterator *tmps;
   Eina_File_Direct_Info *info;

   EINA_LIST_FREE(sb->handlers, handler)
      ecore_event_handler_del(handler);
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

static Eina_Bool
_ephoto_single_populate_end(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_ephoto_single_entry_create(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
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
   Evas_Object *menu;
   Ephoto_Single_Browser *sb;

   EINA_SAFETY_ON_NULL_RETURN_VAL(box, NULL);

   sb = calloc(1, sizeof(Ephoto_Single_Browser));
   EINA_SAFETY_ON_NULL_GOTO(sb, error);

   sb->ephoto = ephoto;
   sb->cropping = EINA_FALSE;
   sb->main = box;

   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(sb->main, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(sb->main, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_event_callback_add(sb->main, EVAS_CALLBACK_DEL, _main_del, sb);
   evas_object_event_callback_add
     (sb->main, EVAS_CALLBACK_KEY_DOWN, _key_down, sb);
   evas_object_data_set(sb->main, "single_browser", sb);

   sb->bar = elm_toolbar_add(sb->ephoto->win);
   EINA_SAFETY_ON_NULL_GOTO(sb->bar, error);
   elm_toolbar_horizontal_set(sb->bar, EINA_TRUE);
   elm_toolbar_homogeneous_set(sb->bar, EINA_TRUE);
   elm_toolbar_shrink_mode_set(sb->bar, ELM_TOOLBAR_SHRINK_SCROLL);
   elm_toolbar_select_mode_set(sb->bar, ELM_OBJECT_SELECT_MODE_NONE);
   evas_object_size_hint_weight_set(sb->bar, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(sb->bar, EVAS_HINT_FILL, EVAS_HINT_FILL);

   icon = elm_toolbar_item_append(sb->bar, "go-home", "Back", _back, sb);
   icon = elm_toolbar_item_append(sb->bar, "image-x-generic", "Edit", NULL, NULL);
   elm_toolbar_item_menu_set(icon, EINA_TRUE);
   elm_toolbar_menu_parent_set(sb->bar, sb->ephoto->win);
   menu = elm_toolbar_item_menu_get(icon);

   elm_menu_item_add(menu, NULL, "edit-undo", "Reset", _reset_image, sb);
   elm_menu_item_add(menu, NULL, "document-save", "Save", _save_image, sb);
   elm_menu_item_add(menu, NULL, "document-save-as", "Save As", _save_image_as, sb);
   elm_menu_item_add(menu, NULL, "edit-cut", "Crop", _crop_image, sb);
   elm_menu_item_add(menu, NULL, "object-rotate-left", "Rotate Left", _go_rotate_counterclock, sb);
   elm_menu_item_add(menu, NULL, "object-rotate-right", "Rotate Right", _go_rotate_clock, sb);
   elm_menu_item_add(menu, NULL, "object-flip-horizontal", "Flip Horizontal", _go_flip_horiz, sb);
   elm_menu_item_add(menu, NULL, "object-flip-vertical", "Flip Vertical", _go_flip_vert, sb);

   /*FIXME: Use separators once they don't mess up homogeneous toolbar*/
   //elm_toolbar_item_separator_set(elm_toolbar_item_append(sb->bar, NULL, NULL, NULL, NULL), EINA_TRUE);

   icon = elm_toolbar_item_append(sb->bar, "go-first", "First", _go_first, sb);
   icon = elm_toolbar_item_append(sb->bar, "go-previous", "Previous", _go_prev, sb);
   icon = elm_toolbar_item_append(sb->bar, "go-next", "Next", _go_next, sb);
   icon = elm_toolbar_item_append(sb->bar, "go-last", "Last", _go_last, sb);

   //elm_toolbar_item_separator_set(elm_toolbar_item_append(sb->bar, NULL, NULL, NULL, NULL), EINA_TRUE);

   icon = elm_toolbar_item_append(sb->bar, "zoom-in", "Zoom In", _zoom_in_cb, sb);
   icon = elm_toolbar_item_append(sb->bar, "zoom-out", "Zoom Out", _zoom_out_cb, sb);
   icon = elm_toolbar_item_append(sb->bar, "zoom-fit-best", "Zoom Fit", _zoom_fit_cb, sb);
   icon = elm_toolbar_item_append(sb->bar, "zoom-original", "Zoom 1:1", _zoom_1_cb, sb);

   //elm_toolbar_item_separator_set(elm_toolbar_item_append(sb->bar, NULL, NULL, NULL, NULL), EINA_TRUE);

   icon = elm_toolbar_item_append(sb->bar, "stock_media-play", "Slideshow", _slideshow, sb);
   icon = elm_toolbar_item_append(sb->bar, "emblem-system", "Settings", _settings, sb);
   icon = elm_toolbar_item_append(sb->bar, "help-about", "About", _about, sb);

   elm_box_pack_end(sb->main, sb->bar);
   evas_object_show(sb->bar);

   sb->table = elm_table_add(sb->main);
   evas_object_size_hint_weight_set(sb->table, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(sb->table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(sb->main, sb->table);
   evas_object_show(sb->table);

   sb->handlers = eina_list_append
      (sb->handlers, ecore_event_handler_add
       (EPHOTO_EVENT_POPULATE_END, _ephoto_single_populate_end, sb));

   sb->handlers = eina_list_append
      (sb->handlers, ecore_event_handler_add
       (EPHOTO_EVENT_ENTRY_CREATE, _ephoto_single_entry_create, sb));

   sb->orient = EPHOTO_ORIENT_0;

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

   if (sb->viewer)
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
