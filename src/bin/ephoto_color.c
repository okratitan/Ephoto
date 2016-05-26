#include "ephoto.h"

typedef struct _Ephoto_Color Ephoto_Color;
struct _Ephoto_Color
{
   Evas_Object *main;
   Evas_Object *parent;
   Evas_Object *image;
   Evas_Object *editor;
   Evas_Object *bslider;
   Evas_Object *gslider;
   Evas_Object *rslider;
   Eina_List *handlers;
   int blue;
   int green;
   int red;
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
_ephoto_color_adjust_red(Ephoto_Color *eco, int red, unsigned int *image_data)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   Evas_Coord x, y;
   int a, r, g, b, rr;

   im_data = malloc(sizeof(unsigned int) * eco->w * eco->h);
   if (image_data)
      memcpy(im_data, image_data, sizeof(unsigned int) * eco->w * eco->h);
   else
      memcpy(im_data, eco->original_im_data,
	  sizeof(unsigned int) * eco->w * eco->h);

   eco->red = red;
   im_data_new = malloc(sizeof(unsigned int) * eco->w * eco->h);

   for (y = 0; y < eco->h; y++)
     {
	p1 = im_data + (y * eco->w);
	p2 = im_data_new + (y * eco->w);
	for (x = 0; x < eco->w; x++)
	  {
	     b = (int) ((*p1) & 0xff);
	     g = (int) ((*p1 >> 8) & 0xff);
	     r = (int) ((*p1 >> 16) & 0xff);
	     a = (int) ((*p1 >> 24) & 0xff);
	     b = _mul_color_alpha(b, a);
	     g = _mul_color_alpha(g, a);
	     r = _mul_color_alpha(r, a);
	     rr = (int) r + eco->red;
	     b = _normalize_color(b);
	     g = _normalize_color(g);
	     rr = _normalize_color(rr);
	     b = _demul_color_alpha(b, a);
	     g = _demul_color_alpha(g, a);
	     rr = _demul_color_alpha(rr, a);
	     *p2 = (a << 24) | (rr << 16) | (g << 8) | b;
	     p2++;
	     p1++;
	  }
     }
   ephoto_single_browser_image_data_update(eco->main, eco->image,
       im_data_new, eco->w, eco->h);
   free(im_data);
   return im_data_new;
}

unsigned int *
_ephoto_color_adjust_green(Ephoto_Color *eco, int green,
    unsigned int *image_data)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   Evas_Coord x, y;
   int a, r, g, b, gg;

   im_data = malloc(sizeof(unsigned int) * eco->w * eco->h);
   if (image_data)
      memcpy(im_data, image_data, sizeof(unsigned int) * eco->w * eco->h);
   else
      memcpy(im_data, eco->original_im_data,
	  sizeof(unsigned int) * eco->w * eco->h);

   eco->green = green;
   im_data_new = malloc(sizeof(unsigned int) * eco->w * eco->h);

   for (y = 0; y < eco->h; y++)
     {
	p1 = im_data + (y * eco->w);
	p2 = im_data_new + (y * eco->w);
	for (x = 0; x < eco->w; x++)
	  {
	     b = (int) ((*p1) & 0xff);
	     g = (int) ((*p1 >> 8) & 0xff);
	     r = (int) ((*p1 >> 16) & 0xff);
	     a = (int) ((*p1 >> 24) & 0xff);
	     b = _mul_color_alpha(b, a);
	     g = _mul_color_alpha(g, a);
	     r = _mul_color_alpha(r, a);
	     gg = (int) g + eco->green;
	     b = _normalize_color(b);
	     gg = _normalize_color(gg);
	     r = _normalize_color(r);
	     b = _demul_color_alpha(b, a);
	     gg = _demul_color_alpha(gg, a);
	     r = _demul_color_alpha(r, a);
	     *p2 = (a << 24) | (r << 16) | (gg << 8) | b;
	     p2++;
	     p1++;
	  }
     }
   ephoto_single_browser_image_data_update(eco->main, eco->image,
       im_data_new, eco->w, eco->h);
   free(im_data);
   return im_data_new;
}

unsigned int *
_ephoto_color_adjust_blue(Ephoto_Color *eco, int blue,
    unsigned int *image_data)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   Evas_Coord x, y;
   int a, r, g, b, bb;

   im_data = malloc(sizeof(unsigned int) * eco->w * eco->h);
   if (image_data)
      memcpy(im_data, image_data, sizeof(unsigned int) * eco->w * eco->h);
   else
      memcpy(im_data, eco->original_im_data,
	  sizeof(unsigned int) * eco->w * eco->h);

   eco->blue = blue;
   im_data_new = malloc(sizeof(unsigned int) * eco->w * eco->h);

   for (y = 0; y < eco->h; y++)
     {
	p1 = im_data + (y * eco->w);
	p2 = im_data_new + (y * eco->w);
	for (x = 0; x < eco->w; x++)
	  {
	     b = (int) ((*p1) & 0xff);
	     g = (int) ((*p1 >> 8) & 0xff);
	     r = (int) ((*p1 >> 16) & 0xff);
	     a = (int) ((*p1 >> 24) & 0xff);
	     b = _mul_color_alpha(b, a);
	     g = _mul_color_alpha(g, a);
	     r = _mul_color_alpha(r, a);
	     bb = (int) b + eco->blue;
	     bb = _normalize_color(bb);
	     g = _normalize_color(g);
	     r = _normalize_color(r);
	     bb = _demul_color_alpha(bb, a);
	     g = _demul_color_alpha(g, a);
	     r = _demul_color_alpha(r, a);
	     *p2 = (a << 24) | (r << 16) | (g << 8) | bb;
	     p2++;
	     p1++;
	  }
     }
   ephoto_single_browser_image_data_update(eco->main, eco->image,
       im_data_new, eco->w, eco->h);
   free(im_data);
   return im_data_new;
}

static void
_red_slider_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Ephoto_Color *eco = data;
   int red;
   unsigned int *image_data, *image_data_two;

   red = elm_slider_value_get(obj);
   image_data = _ephoto_color_adjust_red(eco, red, NULL);
   image_data_two = _ephoto_color_adjust_green(eco, eco->green, image_data);
   _ephoto_color_adjust_blue(eco, eco->blue, image_data_two);
}

static void
_green_slider_changed(void *data, Evas_Object *obj,
    void *event_info EINA_UNUSED)
{
   Ephoto_Color *eco = data;
   int green;
   unsigned int *image_data, *image_data_two;

   green = elm_slider_value_get(obj);
   image_data = _ephoto_color_adjust_green(eco, green, NULL);
   image_data_two = _ephoto_color_adjust_red(eco, eco->red, image_data);
   _ephoto_color_adjust_blue(eco, eco->blue, image_data_two);
}

static void
_blue_slider_changed(void *data, Evas_Object *obj,
    void *event_info EINA_UNUSED)
{
   Ephoto_Color *eco = data;
   int blue;
   unsigned int *image_data, *image_data_two;

   blue = elm_slider_value_get(obj);
   image_data = _ephoto_color_adjust_blue(eco, blue, NULL);
   image_data_two = _ephoto_color_adjust_red(eco, eco->red, image_data);
   _ephoto_color_adjust_green(eco, eco->green, image_data_two);
}

static Eina_Bool
_color_reset(void *data, int type EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Color *eco = data;

   elm_slider_value_set(eco->rslider, 0);
   elm_slider_value_set(eco->gslider, 0);
   elm_slider_value_set(eco->bslider, 0);
   eco->red = 0;
   eco->green = 0;
   eco->blue = 0;
   _red_slider_changed(eco, eco->rslider, NULL);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_color_apply(void *data, int type EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Color *eco = data;
   unsigned int *image_data;
   Evas_Coord w, h;

   image_data =
       evas_object_image_data_get(elm_image_object_get(eco->image),
           EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(eco->image), &w, &h);
   ephoto_single_browser_image_data_done(eco->main, image_data, w, h);
   ephoto_editor_del(eco->editor);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_color_cancel(void *data, int type EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Color *eco = data;

   elm_slider_value_set(eco->rslider, 0);
   elm_slider_value_set(eco->gslider, 0);
   elm_slider_value_set(eco->bslider, 0);
   eco->red = 0;
   eco->green = 0;
   eco->blue = 0;
   _red_slider_changed(eco, eco->rslider, NULL);
   ephoto_single_browser_cancel_editing(eco->main);
   ephoto_editor_del(eco->editor);

   return ECORE_CALLBACK_PASS_ON;
}

static void
_editor_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Color *eco = data;
   Ecore_Event_Handler *handler;

   EINA_LIST_FREE(eco->handlers, handler)
     ecore_event_handler_del(handler);
   free(eco->original_im_data);
   free(eco);
}

void
ephoto_color_add(Ephoto *ephoto, Evas_Object *main, Evas_Object *parent, Evas_Object *image)
{
   Evas_Object *slider;
   Ephoto_Color *eco;
   unsigned int *im_data;

   EINA_SAFETY_ON_NULL_GOTO(image, error);

   eco = calloc(1, sizeof(Ephoto_Color));
   EINA_SAFETY_ON_NULL_GOTO(eco, error);

   eco->red = 0;
   eco->green = 0;
   eco->blue = 0;
   eco->main = main;
   eco->parent = parent;
   eco->image = image;
   im_data =
       evas_object_image_data_get(elm_image_object_get(eco->image),
           EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(eco->image), &eco->w,
       &eco->h);
   eco->original_im_data = malloc(sizeof(unsigned int) * eco->w * eco->h);
   memcpy(eco->original_im_data, im_data,
       sizeof(unsigned int) * eco->w * eco->h);

   eco->editor = ephoto_editor_add(ephoto, _("Adjust Color Levels"), 
       "eco", eco);
   evas_object_event_callback_add(eco->editor, EVAS_CALLBACK_DEL, _editor_del,
       eco);

   slider = elm_slider_add(eco->editor);
   elm_object_text_set(slider, _("Blue"));
   elm_slider_min_max_set(slider, -100, 100);
   elm_slider_step_set(slider, 1);
   elm_slider_value_set(slider, 0);
   elm_slider_unit_format_set(slider, "%1.0f");
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_smart_callback_add(slider, "delay,changed",
       _blue_slider_changed, eco);
   elm_box_pack_start(eco->editor, slider);
   evas_object_show(slider);
   eco->bslider = slider;

   slider = elm_slider_add(eco->editor);
   elm_object_text_set(slider, _("Green"));
   elm_slider_min_max_set(slider, -100, 100);
   elm_slider_step_set(slider, 1);
   elm_slider_value_set(slider, 0);
   elm_slider_unit_format_set(slider, "%1.0f");
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_smart_callback_add(slider, "delay,changed",
       _green_slider_changed, eco);
   elm_box_pack_start(eco->editor, slider);
   evas_object_show(slider);
   eco->gslider = slider;

   slider = elm_slider_add(eco->editor);
   elm_object_text_set(slider, _("Red"));
   elm_slider_min_max_set(slider, -100, 100);
   elm_slider_step_set(slider, 1);
   elm_slider_value_set(slider, 0);
   elm_slider_unit_format_set(slider, "%1.0f");
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_smart_callback_add(slider, "delay,changed",
       _red_slider_changed, eco);
   elm_box_pack_start(eco->editor, slider);
   evas_object_show(slider);
   eco->rslider = slider;

   eco->handlers =
       eina_list_append(eco->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_EDITOR_RESET,
           _color_reset, eco));
   eco->handlers =
       eina_list_append(eco->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_EDITOR_APPLY,
           _color_apply, eco));
   eco->handlers =
       eina_list_append(eco->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_EDITOR_CANCEL,
           _color_cancel, eco));

   return;

  error:
   return;
}
