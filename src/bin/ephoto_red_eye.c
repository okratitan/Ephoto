#include "ephoto.h"

typedef struct _Ephoto_Reye Ephoto_Reye;
struct _Ephoto_Reye
{
   Evas_Object *main;
   Evas_Object *parent;
   Evas_Object *image;
   Evas_Object *editor;
   Evas_Object *rslider;
   Eina_List *handlers;
   int rad;
   Evas_Coord w, h;
   unsigned int *original_im_data;
   unsigned int *edited_im_data;
};

static int
_normalize_color(int color)
{
   if (color < 0)
      return 0;
   else if (color > 255)
      return 255;
   else
      return color;
}

static int
_mul_color_alpha(int color, int alpha)
{
   if (alpha > 0 && alpha < 255)
      return color * (255 / alpha);
   else
      return color;
}

static int
_demul_color_alpha(int color, int alpha)
{
   if (alpha > 0 && alpha < 255)
      return (color * alpha) / 255;
   else
      return color;
}

static void
_reye_clicked(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Ephoto_Reye *er = data;
   unsigned int *im_data, *p1;
   Evas_Coord x, y, imx, imy, imw, imh;
   Evas_Coord xpos, ypos, xadj, yadj, nx, ny;
   Evas_Coord xx, yy, nnx, nny;
   int a, r, g, b;
   double scalex, scaley;

   evas_pointer_canvas_xy_get(evas_object_evas_get(er->image), &xpos, &ypos);
   evas_object_geometry_get(er->image, &imx, &imy, &imw, &imh);

   xadj = xpos-imx;
   yadj = ypos-imy;

   if (xadj < 0) xadj = 0;
   if (yadj < 0) yadj = 0;

   scalex = (double) (xadj) / (double) imw;
   scaley = (double) (yadj) / (double) imh;

   nx = er->w * scalex;
   ny = er->h * scaley;

   if (nx < 0) nx = 0;
   if (ny < 0) ny = 0;

   im_data = malloc(sizeof(unsigned int) * er->w * er->h);
   if (er->edited_im_data)
     memcpy(im_data, er->edited_im_data,
          sizeof(unsigned int) * er->w * er->h);
   else
     memcpy(im_data, er->original_im_data,
          sizeof(unsigned int) * er->w * er->h);

   for (yy = -er->rad; yy <= er->rad; yy++)
     {
        for (xx = -er->rad; xx <= er->rad; xx++)
           {
              if ((xx * xx) + (yy * yy) <= (er->rad * er->rad))
                {
                   nnx = nx + xx;
                   nny = ny + yy;

                   p1 = im_data + (nny * er->w) + nnx;
                   b = (int) ((*p1) & 0xff);
                   g = (int) ((*p1 >> 8) & 0xff);
                   r = (int) ((*p1 >> 16) & 0xff);
                   a = (int) ((*p1 >> 24) & 0xff);
                   b = _mul_color_alpha(b, a);
                   g = _mul_color_alpha(g, a);
                   r = _mul_color_alpha(r, a);
                   r = (int) ((g+b)/2);
                   b = _normalize_color(b);
                   g = _normalize_color(g);
                   r = _normalize_color(r);
                   b = _demul_color_alpha(b, a);
                   g = _demul_color_alpha(g, a);
                   r = _demul_color_alpha(r, a);
                   *p1 = (a << 24) | (r << 16) | (g << 8) | b;
               }
          }
     }
   er->edited_im_data = im_data;
   ephoto_single_browser_image_data_update(er->main, er->image,
       im_data, er->w, er->h);
}

static void
_radius_slider_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Ephoto_Reye *er = data;

   er->rad = elm_slider_value_get(obj);
}

static Eina_Bool
_reye_reset(void *data, int type EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Reye *er = data;

   elm_slider_value_set(er->rslider, 15);
   er->rad = 15;
   if (er->edited_im_data)
     {
        free(er->edited_im_data);
        er->edited_im_data = NULL;
     }
   ephoto_single_browser_image_data_update(er->main, er->image,
       er->original_im_data, er->w, er->h);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_reye_apply(void *data, int type EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Reye *er = data;
   unsigned int *image_data;
   Evas_Coord w, h;

   image_data =
       evas_object_image_data_get(elm_image_object_get(er->image),
           EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(er->image), &w, &h);
   ephoto_single_browser_image_data_done(er->main, image_data, w, h);
   ephoto_editor_del(er->editor);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_reye_cancel(void *data, int type EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Reye *er = data;

   elm_slider_value_set(er->rslider, 15);
   er->rad = 15;

   ephoto_single_browser_cancel_editing(er->main);
   if (er->edited_im_data)
     {
        free(er->edited_im_data);
        er->edited_im_data = NULL;
     }
   ephoto_editor_del(er->editor);

   return ECORE_CALLBACK_PASS_ON;
}

static void
_editor_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Reye *er = data;
   Ecore_Event_Handler *handler;

   evas_object_event_callback_del(er->image, EVAS_CALLBACK_MOUSE_UP, _reye_clicked);
   EINA_LIST_FREE(er->handlers, handler)
     ecore_event_handler_del(handler);
   free(er->original_im_data);
   free(er);
}

void
ephoto_red_eye_add(Evas_Object *main, Evas_Object *parent, Evas_Object *image)
{
   Evas_Object *slider, *label;
   Ephoto_Reye *er;
   unsigned int *im_data;

   EINA_SAFETY_ON_NULL_GOTO(image, error);

   er = calloc(1, sizeof(Ephoto_Reye));
   EINA_SAFETY_ON_NULL_GOTO(er, error);

   er->rad = 15;
   er->main = main;
   er->parent = parent;
   er->image = image;
   im_data =
       evas_object_image_data_get(elm_image_object_get(er->image),
           EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(er->image), &er->w,
       &er->h);
   er->original_im_data = malloc(sizeof(unsigned int) * er->w * er->h);
   memcpy(er->original_im_data, im_data,
       sizeof(unsigned int) * er->w * er->h);

   evas_object_event_callback_add(er->image, EVAS_CALLBACK_MOUSE_UP,
       _reye_clicked, er);

   er->editor = ephoto_editor_add(parent, _("Red Eye Removal"),
       "ereye", er);
   evas_object_event_callback_add(er->editor, EVAS_CALLBACK_DEL, _editor_del,
       er);

   slider = elm_slider_add(er->editor);
   elm_object_text_set(slider, _("Radius"));
   elm_slider_min_max_set(slider, 5, 50);
   elm_slider_step_set(slider, 1);
   elm_slider_value_set(slider, 15);
   elm_slider_unit_format_set(slider, "%1.0f");
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_smart_callback_add(slider, "delay,changed",
       _radius_slider_changed, er);
   elm_box_pack_start(er->editor, slider);
   evas_object_show(slider);
   er->rslider = slider;

   label = elm_label_add(er->editor);
   elm_object_text_set(label, _("<b>Click on an eye</b>"));
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, 0.5);
   elm_box_pack_start(er->editor, label);
   evas_object_show(label);

   er->handlers =
       eina_list_append(er->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_EDITOR_RESET,
           _reye_reset, er));
   er->handlers =
       eina_list_append(er->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_EDITOR_APPLY,
           _reye_apply, er));
   er->handlers =
       eina_list_append(er->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_EDITOR_CANCEL,
           _reye_cancel, er));

   return;

  error:
   return;
}
