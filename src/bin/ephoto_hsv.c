#include "ephoto.h"

typedef struct _Ephoto_HSV Ephoto_HSV;
struct _Ephoto_HSV
{
   Evas_Object *main;
   Evas_Object *parent;
   Evas_Object *image;
   Evas_Object *editor;
   Evas_Object *hslider;
   Evas_Object *sslider;
   Evas_Object *vslider;
   Eina_List *handlers;
   double hue;
   double saturation;
   double value;
   Evas_Coord w, h;
   unsigned int *original_im_data;
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

unsigned int *
_ephoto_hsv_adjust_hue(Ephoto_HSV *ehsv, double hue, unsigned int *image_data)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   Evas_Coord x, y;
   int a, r, g, b, bb, gg, rr;
   float hh, s, v;

   im_data = malloc(sizeof(unsigned int) * ehsv->w * ehsv->h);
   if (image_data)
      memcpy(im_data, image_data, sizeof(unsigned int) * ehsv->w * ehsv->h);
   else
      memcpy(im_data, ehsv->original_im_data,
	  sizeof(unsigned int) * ehsv->w * ehsv->h);

   im_data_new = malloc(sizeof(unsigned int) * ehsv->w * ehsv->h);

   for (y = 0; y < ehsv->h; y++)
     {
	p1 = im_data + (y * ehsv->w);
	p2 = im_data_new + (y * ehsv->w);
	for (x = 0; x < ehsv->w; x++)
	  {
	     b = (int) ((*p1) & 0xff);
	     g = (int) ((*p1 >> 8) & 0xff);
	     r = (int) ((*p1 >> 16) & 0xff);
	     a = (int) ((*p1 >> 24) & 0xff);
	     b = _mul_color_alpha(b, a);
	     g = _mul_color_alpha(g, a);
	     r = _mul_color_alpha(r, a);
	     evas_color_rgb_to_hsv(r, g, b, &hh, &s, &v);
	     hh += hue;
	     if (hh < 0)
		hh += 360;
	     if (hh > 360)
		hh -= 360;
	     evas_color_hsv_to_rgb(hh, s, v, &rr, &gg, &bb);
	     bb = _normalize_color(bb);
	     gg = _normalize_color(gg);
	     rr = _normalize_color(rr);
	     bb = _demul_color_alpha(bb, a);
	     gg = _demul_color_alpha(gg, a);
	     rr = _demul_color_alpha(rr, a);
	     *p2 = (a << 24) | (rr << 16) | (gg << 8) | bb;
	     p2++;
	     p1++;
	  }
     }
   ehsv->hue = hue;
   ephoto_single_browser_image_data_update(ehsv->main, ehsv->image,
       im_data_new, ehsv->w, ehsv->h);
   free(im_data);
   return im_data_new;
}

unsigned int *
_ephoto_hsv_adjust_saturation(Ephoto_HSV *ehsv, double saturation,
    unsigned int *image_data)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   Evas_Coord x, y;
   int a, r, g, b, bb, gg, rr;
   float hh, s, v;

   im_data = malloc(sizeof(unsigned int) * ehsv->w * ehsv->h);
   if (image_data)
      memcpy(im_data, image_data, sizeof(unsigned int) * ehsv->w * ehsv->h);
   else
      memcpy(im_data, ehsv->original_im_data,
	  sizeof(unsigned int) * ehsv->w * ehsv->h);

   im_data_new = malloc(sizeof(unsigned int) * ehsv->w * ehsv->h);

   for (y = 0; y < ehsv->h; y++)
     {
	p1 = im_data + (y * ehsv->w);
	p2 = im_data_new + (y * ehsv->w);
	for (x = 0; x < ehsv->w; x++)
	  {
	     b = (int) ((*p1) & 0xff);
	     g = (int) ((*p1 >> 8) & 0xff);
	     r = (int) ((*p1 >> 16) & 0xff);
	     a = (int) ((*p1 >> 24) & 0xff);
	     b = _mul_color_alpha(b, a);
	     g = _mul_color_alpha(g, a);
	     r = _mul_color_alpha(r, a);
	     evas_color_rgb_to_hsv(r, g, b, &hh, &s, &v);
	     s += s * ((float) saturation / 100);
	     if (s < 0)
		s = 0;
	     if (s > 1)
		s = 1;
	     evas_color_hsv_to_rgb(hh, s, v, &rr, &gg, &bb);
	     bb = _normalize_color(bb);
	     gg = _normalize_color(gg);
	     rr = _normalize_color(rr);
	     bb = _demul_color_alpha(bb, a);
	     gg = _demul_color_alpha(gg, a);
	     rr = _demul_color_alpha(rr, a);
	     *p2 = (a << 24) | (rr << 16) | (gg << 8) | bb;
	     p2++;
	     p1++;
	  }
     }
   ehsv->saturation = saturation;
   ephoto_single_browser_image_data_update(ehsv->main, ehsv->image,
       im_data_new, ehsv->w, ehsv->h);
   free(im_data);
   return im_data_new;
}

unsigned int *
_ephoto_hsv_adjust_value(Ephoto_HSV *ehsv, double value,
    unsigned int *image_data)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   Evas_Coord x, y;
   int a, r, g, b, bb, gg, rr;
   float hh, s, v;

   im_data = malloc(sizeof(unsigned int) * ehsv->w * ehsv->h);
   if (image_data)
      memcpy(im_data, image_data, sizeof(unsigned int) * ehsv->w * ehsv->h);
   else
      memcpy(im_data, ehsv->original_im_data,
	  sizeof(unsigned int) * ehsv->w * ehsv->h);

   im_data_new = malloc(sizeof(unsigned int) * ehsv->w * ehsv->h);

   for (y = 0; y < ehsv->h; y++)
     {
	p1 = im_data + (y * ehsv->w);
	p2 = im_data_new + (y * ehsv->w);
	for (x = 0; x < ehsv->w; x++)
	  {
	     b = (int) ((*p1) & 0xff);
	     g = (int) ((*p1 >> 8) & 0xff);
	     r = (int) ((*p1 >> 16) & 0xff);
	     a = (int) ((*p1 >> 24) & 0xff);
	     b = _mul_color_alpha(b, a);
	     g = _mul_color_alpha(g, a);
	     r = _mul_color_alpha(r, a);
	     evas_color_rgb_to_hsv(r, g, b, &hh, &s, &v);
	     v += (v * ((float) value / 100));
	     if (v < 0)
		v = 0;
	     if (v > 1)
		v = 1;
	     evas_color_hsv_to_rgb(hh, s, v, &rr, &gg, &bb);
	     bb = _normalize_color(bb);
	     gg = _normalize_color(gg);
	     rr = _normalize_color(rr);
	     bb = _demul_color_alpha(bb, a);
	     gg = _demul_color_alpha(gg, a);
	     rr = _demul_color_alpha(rr, a);
	     *p2 = (a << 24) | (rr << 16) | (gg << 8) | bb;
	     p2++;
	     p1++;
	  }
     }
   ehsv->value = value;
   ephoto_single_browser_image_data_update(ehsv->main, ehsv->image,
       im_data_new, ehsv->w, ehsv->h);
   free(im_data);
   return im_data_new;
}

static void
_hue_slider_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Ephoto_HSV *ehsv = data;
   double hue;
   unsigned int *image_data, *image_data_two;

   hue = elm_slider_value_get(obj);
   image_data = _ephoto_hsv_adjust_hue(ehsv, hue, NULL);
   image_data_two =
       _ephoto_hsv_adjust_saturation(ehsv, ehsv->saturation, image_data);
   _ephoto_hsv_adjust_value(ehsv, ehsv->value, image_data_two);
}

static void
_saturation_slider_changed(void *data, Evas_Object *obj,
    void *event_info EINA_UNUSED)
{
   Ephoto_HSV *ehsv = data;
   double saturation;
   unsigned int *image_data, *image_data_two;

   saturation = elm_slider_value_get(obj);
   image_data = _ephoto_hsv_adjust_saturation(ehsv, saturation, NULL);
   image_data_two = _ephoto_hsv_adjust_hue(ehsv, ehsv->hue, image_data);
   _ephoto_hsv_adjust_value(ehsv, ehsv->value, image_data_two);
}

static void
_value_slider_changed(void *data, Evas_Object *obj,
    void *event_info EINA_UNUSED)
{
   Ephoto_HSV *ehsv = data;
   double value;
   unsigned int *image_data, *image_data_two;

   value = elm_slider_value_get(obj);
   image_data = _ephoto_hsv_adjust_value(ehsv, value, NULL);
   image_data_two = _ephoto_hsv_adjust_hue(ehsv, ehsv->hue, image_data);
   _ephoto_hsv_adjust_saturation(ehsv, ehsv->saturation, image_data_two);
}

static Eina_Bool
_hsv_reset(void *data, int type EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_HSV *ehsv = data;

   elm_slider_value_set(ehsv->hslider, 0);
   elm_slider_value_set(ehsv->sslider, 0);
   elm_slider_value_set(ehsv->vslider, 0);
   ehsv->hue = 0;
   ehsv->saturation = 0;
   ehsv->value = 0;
   _hue_slider_changed(ehsv, ehsv->hslider, NULL);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_hsv_apply(void *data, int type EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_HSV *ehsv = data;
   unsigned int *image_data;
   Evas_Coord w, h;

   image_data =
       evas_object_image_data_get(ehsv->image,
       EINA_FALSE);
   evas_object_image_size_get(ehsv->image, &w, &h);
   ephoto_single_browser_image_data_done(ehsv->main, image_data, w, h);
   ephoto_editor_del(ehsv->editor);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_hsv_cancel(void *data, int type EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_HSV *ehsv = data;

   elm_slider_value_set(ehsv->hslider, 0);
   elm_slider_value_set(ehsv->sslider, 0);
   elm_slider_value_set(ehsv->vslider, 0);
   ehsv->hue = 0;
   ehsv->saturation = 0;
   ehsv->value = 0;
   _hue_slider_changed(ehsv, ehsv->hslider, NULL);
   ephoto_single_browser_cancel_editing(ehsv->main);
   ephoto_editor_del(ehsv->editor);

   return ECORE_CALLBACK_PASS_ON;
}

static void
_editor_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_HSV *ehsv = data;
   Ecore_Event_Handler *handler;

   EINA_LIST_FREE(ehsv->handlers, handler)
     ecore_event_handler_del(handler);
   free(ehsv->original_im_data);
   free(ehsv);
}

void
ephoto_hsv_add(Ephoto *ephoto, Evas_Object *main, Evas_Object *parent, Evas_Object *image)
{
   Evas_Object *slider;
   Ephoto_HSV *ehsv;
   unsigned int *im_data;

   EINA_SAFETY_ON_NULL_GOTO(image, error);

   ehsv = calloc(1, sizeof(Ephoto_HSV));
   EINA_SAFETY_ON_NULL_GOTO(ehsv, error);

   ehsv->hue = 0;
   ehsv->saturation = 0;
   ehsv->value = 0;
   ehsv->main = main;
   ehsv->parent = parent;
   ehsv->image = image;
   im_data =
       evas_object_image_data_get(ehsv->image,
       EINA_FALSE);
   evas_object_image_size_get(ehsv->image, &ehsv->w,
       &ehsv->h);
   ehsv->original_im_data = malloc(sizeof(unsigned int) * ehsv->w * ehsv->h);
   memcpy(ehsv->original_im_data, im_data,
       sizeof(unsigned int) * ehsv->w * ehsv->h);

   ehsv->editor = ephoto_editor_add(ephoto, _("Hue/Saturation/Value"), 
       "ehsv", ehsv);
   evas_object_event_callback_add(ehsv->editor, EVAS_CALLBACK_DEL, _editor_del,
       ehsv);

   slider = elm_slider_add(ehsv->editor);
   elm_object_text_set(slider, _("Value"));
   elm_slider_min_max_set(slider, -100, 100);
   elm_slider_step_set(slider, 1.20);
   elm_slider_value_set(slider, 0);
   elm_slider_unit_format_set(slider, "%1.2f");
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_smart_callback_add(slider, "delay,changed", _value_slider_changed,
       ehsv);
   elm_box_pack_start(ehsv->editor, slider);
   evas_object_show(slider);
   ehsv->vslider = slider;

   slider = elm_slider_add(ehsv->editor);
   elm_object_text_set(slider, _("Saturation"));
   elm_slider_min_max_set(slider, -100, 100);
   elm_slider_step_set(slider, 1.20);
   elm_slider_value_set(slider, 0);
   elm_slider_unit_format_set(slider, "%1.2f");
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_smart_callback_add(slider, "delay,changed",
       _saturation_slider_changed, ehsv);
   elm_box_pack_start(ehsv->editor, slider);
   evas_object_show(slider);
   ehsv->sslider = slider;

   slider = elm_slider_add(ehsv->editor);
   elm_object_text_set(slider, _("Hue"));
   elm_slider_min_max_set(slider, -180, 180);
   elm_slider_step_set(slider, 1);
   elm_slider_value_set(slider, 0);
   elm_slider_unit_format_set(slider, "%1.0f");
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_smart_callback_add(slider, "delay,changed",
       _hue_slider_changed, ehsv);
   elm_box_pack_start(ehsv->editor, slider);
   evas_object_show(slider);
   ehsv->hslider = slider;

   ehsv->handlers =
       eina_list_append(ehsv->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_EDITOR_RESET,
           _hsv_reset, ehsv));
   ehsv->handlers =
       eina_list_append(ehsv->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_EDITOR_APPLY,
           _hsv_apply, ehsv));
   ehsv->handlers =
       eina_list_append(ehsv->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_EDITOR_CANCEL,
           _hsv_cancel, ehsv));
   return;

  error:
   return;
}
