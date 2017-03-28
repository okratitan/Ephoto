#include "ephoto.h"

typedef struct _Ephoto_BCG Ephoto_BCG;
struct _Ephoto_BCG
{
   Evas_Object *main;
   Evas_Object *parent;
   Evas_Object *image;
   Evas_Object *editor;
   Evas_Object *bslider;
   Evas_Object *cslider;
   Evas_Object *gslider;
   Eina_List *handlers;
   int contrast;
   int brightness;
   double gamma;
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
_ephoto_bcg_adjust_brightness(Ephoto_BCG *ebcg, int brightness,
    unsigned int *image_data)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   Evas_Coord x, y;
   int a, r, g, b, bb, gg, rr;

   im_data = malloc(sizeof(unsigned int) * ebcg->w * ebcg->h);
   if (image_data)
      memcpy(im_data, image_data, sizeof(unsigned int) * ebcg->w * ebcg->h);
   else
      memcpy(im_data, ebcg->original_im_data,
	  sizeof(unsigned int) * ebcg->w * ebcg->h);

   ebcg->brightness = brightness;
   im_data_new = malloc(sizeof(unsigned int) * ebcg->w * ebcg->h);

   for (y = 0; y < ebcg->h; y++)
     {
	p1 = im_data + (y * ebcg->w);
	p2 = im_data_new + (y * ebcg->w);
	for (x = 0; x < ebcg->w; x++)
	  {
	     b = (int) ((*p1) & 0xff);
	     g = (int) ((*p1 >> 8) & 0xff);
	     r = (int) ((*p1 >> 16) & 0xff);
	     a = (int) ((*p1 >> 24) & 0xff);
	     b = _mul_color_alpha(b, a);
	     g = _mul_color_alpha(g, a);
	     r = _mul_color_alpha(r, a);
	     bb = (int) b + ebcg->brightness;
	     gg = (int) g + ebcg->brightness;
	     rr = (int) r + ebcg->brightness;
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
   ephoto_single_browser_image_data_update(ebcg->main, ebcg->image,
       im_data_new, ebcg->w, ebcg->h);
   free(im_data);
   return im_data_new;
}

unsigned int *
_ephoto_bcg_adjust_contrast(Ephoto_BCG *ebcg, int contrast,
    unsigned int *image_data)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   Evas_Coord x, y;
   int top, bottom, a, r, g, b, bb, gg, rr;
   float factor;

   im_data = malloc(sizeof(unsigned int) * ebcg->w * ebcg->h);
   if (image_data)
      memcpy(im_data, image_data, sizeof(unsigned int) * ebcg->w * ebcg->h);
   else
      memcpy(im_data, ebcg->original_im_data,
	  sizeof(unsigned int) * ebcg->w * ebcg->h);

   ebcg->contrast = contrast;
   top = ((255 + (contrast)) * 259);
   bottom = ((259 - (contrast)) * 255);
   factor = (float) top / (float) bottom;
   im_data_new = malloc(sizeof(unsigned int) * ebcg->w * ebcg->h);

   for (y = 0; y < ebcg->h; y++)
     {
	p1 = im_data + (y * ebcg->w);
	p2 = im_data_new + (y * ebcg->w);
	for (x = 0; x < ebcg->w; x++)
	  {
	     b = (int) ((*p1) & 0xff);
	     g = (int) ((*p1 >> 8) & 0xff);
	     r = (int) ((*p1 >> 16) & 0xff);
	     a = (int) ((*p1 >> 24) & 0xff);
	     b = _mul_color_alpha(b, a);
	     g = _mul_color_alpha(g, a);
	     r = _mul_color_alpha(r, a);
	     bb = (int) ((factor * (b - 128)) + 128);
	     gg = (int) ((factor * (g - 128)) + 128);
	     rr = (int) ((factor * (r - 128)) + 128);
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
   ephoto_single_browser_image_data_update(ebcg->main, ebcg->image,
       im_data_new, ebcg->w, ebcg->h);
   free(im_data);
   return im_data_new;
}

unsigned int *
_ephoto_bcg_adjust_gamma(Ephoto_BCG *ebcg, double gamma,
    unsigned int *image_data)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   Evas_Coord x, y;
   int a, r, g, b, bb, gg, rr;

   im_data = malloc(sizeof(unsigned int) * ebcg->w * ebcg->h);
   if (image_data)
      memcpy(im_data, image_data, sizeof(unsigned int) * ebcg->w * ebcg->h);
   else
      memcpy(im_data, ebcg->original_im_data,
	  sizeof(unsigned int) * ebcg->w * ebcg->h);

   ebcg->gamma = 1 / gamma;
   im_data_new = malloc(sizeof(unsigned int) * ebcg->w * ebcg->h);

   for (y = 0; y < ebcg->h; y++)
     {
	p1 = im_data + (y * ebcg->w);
	p2 = im_data_new + (y * ebcg->w);
	for (x = 0; x < ebcg->w; x++)
	  {
	     b = (int) ((*p1) & 0xff);
	     g = (int) ((*p1 >> 8) & 0xff);
	     r = (int) ((*p1 >> 16) & 0xff);
	     a = (int) ((*p1 >> 24) & 0xff);
	     b = _mul_color_alpha(b, a);
	     g = _mul_color_alpha(g, a);
	     r = _mul_color_alpha(r, a);
	     bb = (int) (pow(((double) b / 255), ebcg->gamma) * 255);
	     gg = (int) (pow(((double) g / 255), ebcg->gamma) * 255);
	     rr = (int) (pow(((double) r / 255), ebcg->gamma) * 255);
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
   ephoto_single_browser_image_data_update(ebcg->main, ebcg->image,
       im_data_new, ebcg->w, ebcg->h);
   free(im_data);
   return im_data_new;
}

static void
_brightness_slider_changed(void *data, Evas_Object *obj,
    void *event_info EINA_UNUSED)
{
   Ephoto_BCG *ebcg = data;
   int brightness;
   unsigned int *image_data, *image_data_two;

   brightness = elm_slider_value_get(obj);
   image_data = _ephoto_bcg_adjust_brightness(ebcg, brightness, NULL);
   image_data_two =
       _ephoto_bcg_adjust_contrast(ebcg, ebcg->contrast, image_data);
   _ephoto_bcg_adjust_gamma(ebcg, ebcg->gamma, image_data_two);
}

static void
_contrast_slider_changed(void *data, Evas_Object *obj,
    void *event_info EINA_UNUSED)
{
   Ephoto_BCG *ebcg = data;
   int contrast;
   unsigned int *image_data, *image_data_two;

   contrast = elm_slider_value_get(obj);
   image_data = _ephoto_bcg_adjust_contrast(ebcg, contrast, NULL);
   image_data_two =
       _ephoto_bcg_adjust_brightness(ebcg, ebcg->brightness, image_data);
   _ephoto_bcg_adjust_gamma(ebcg, ebcg->gamma, image_data_two);
}

static void
_gamma_slider_changed(void *data, Evas_Object *obj,
    void *event_info EINA_UNUSED)
{
   Ephoto_BCG *ebcg = data;
   double gamma;
   unsigned int *image_data, *image_data_two;

   gamma = elm_slider_value_get(obj);
   image_data = _ephoto_bcg_adjust_gamma(ebcg, gamma, NULL);
   image_data_two =
       _ephoto_bcg_adjust_brightness(ebcg, ebcg->brightness, image_data);
   _ephoto_bcg_adjust_contrast(ebcg, ebcg->contrast, image_data_two);
}

static Eina_Bool
_bcg_reset(void *data, int type EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_BCG *ebcg = data;

   elm_slider_value_set(ebcg->bslider, 0);
   elm_slider_value_set(ebcg->cslider, 0);
   elm_slider_value_set(ebcg->gslider, 1);
   ebcg->brightness = 0;
   ebcg->contrast = 0;
   ebcg->gamma = 1;
   _brightness_slider_changed(ebcg, ebcg->bslider, NULL);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_bcg_apply(void *data, int type EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_BCG *ebcg = data;
   unsigned int *image_data;
   Evas_Coord w, h;

   if (elm_slider_value_get(ebcg->bslider) == 0 &&
       elm_slider_value_get(ebcg->cslider) == 0 &&
       elm_slider_value_get(ebcg->gslider) == 1)
     {
        ephoto_single_browser_cancel_editing(ebcg->main);
     }
   else
     {
        image_data =
            evas_object_image_data_get(ebcg->image, EINA_FALSE);
        evas_object_image_size_get(ebcg->image, &w, &h);
        ephoto_single_browser_image_data_done(ebcg->main, image_data, w, h);
     }
   ephoto_editor_del(ebcg->editor);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_bcg_cancel(void *data, int type EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_BCG *ebcg = data;

   elm_slider_value_set(ebcg->bslider, 0);
   elm_slider_value_set(ebcg->cslider, 0);
   elm_slider_value_set(ebcg->gslider, 1);
   ebcg->brightness = 0;
   ebcg->contrast = 0;
   ebcg->gamma = 1;
   _brightness_slider_changed(ebcg, ebcg->bslider, NULL);
   ephoto_single_browser_cancel_editing(ebcg->main);
   ephoto_editor_del(ebcg->editor);

   return ECORE_CALLBACK_PASS_ON;
}

static void
_editor_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_BCG *ebcg = data;
   Ecore_Event_Handler *handler;

   EINA_LIST_FREE(ebcg->handlers, handler)
     ecore_event_handler_del(handler);
   free(ebcg->original_im_data);
   free(ebcg);
}

void
ephoto_bcg_add(Ephoto *ephoto, Evas_Object *main, Evas_Object *parent, Evas_Object *image)
{
   Evas_Object *slider;
   Ephoto_BCG *ebcg;
   unsigned int *im_data;

   EINA_SAFETY_ON_NULL_GOTO(image, error);

   ebcg = calloc(1, sizeof(Ephoto_BCG));
   EINA_SAFETY_ON_NULL_GOTO(ebcg, error);

   ebcg->brightness = 0;
   ebcg->contrast = 0;
   ebcg->gamma = 1;
   ebcg->main = main;
   ebcg->parent = parent;
   ebcg->image = image;
   im_data =
       evas_object_image_data_get(ebcg->image,
       EINA_FALSE);
   evas_object_image_size_get(ebcg->image, &ebcg->w,
       &ebcg->h);
   ebcg->original_im_data = malloc(sizeof(unsigned int) * ebcg->w * ebcg->h);
   memcpy(ebcg->original_im_data, im_data,
       sizeof(unsigned int) * ebcg->w * ebcg->h);

   ebcg->editor = ephoto_editor_add(ephoto, _("Brightness/Contrast/Gamma"),
       "ebcg", ebcg);
   evas_object_event_callback_add(ebcg->editor, EVAS_CALLBACK_DEL, _editor_del,
       ebcg);

   slider = elm_slider_add(ebcg->editor);
   elm_object_text_set(slider, _("Gamma"));
   elm_slider_min_max_set(slider, -0.1, 3);
   elm_slider_step_set(slider, .1);
   elm_slider_value_set(slider, 1);
   elm_slider_unit_format_set(slider, "%1.2f");
   elm_slider_indicator_format_set(slider, "%1.2f");
   EPHOTO_WEIGHT(slider, EVAS_HINT_EXPAND, 0.0);
   EPHOTO_ALIGN(slider, EVAS_HINT_FILL, 0.5);
   evas_object_smart_callback_add(slider, "delay,changed",
       _gamma_slider_changed, ebcg);
   elm_box_pack_start(ebcg->editor, slider);
   evas_object_show(slider);
   ebcg->gslider = slider;

   slider = elm_slider_add(ebcg->editor);
   elm_object_text_set(slider, _("Contrast"));
   elm_slider_min_max_set(slider, -100, 100);
   elm_slider_step_set(slider, 1);
   elm_slider_value_set(slider, 0);
   elm_slider_unit_format_set(slider, "%1.0f");
   elm_slider_indicator_format_set(slider, "%1.0f");
   EPHOTO_WEIGHT(slider, EVAS_HINT_EXPAND, 0.0);
   EPHOTO_ALIGN(slider, EVAS_HINT_FILL, 0.5);
   evas_object_smart_callback_add(slider, "delay,changed",
       _contrast_slider_changed, ebcg);
   elm_box_pack_start(ebcg->editor, slider);
   evas_object_show(slider);
   ebcg->cslider = slider;

   slider = elm_slider_add(ebcg->editor);
   elm_object_text_set(slider, _("Brightness"));
   elm_slider_min_max_set(slider, -100, 100);
   elm_slider_step_set(slider, 1);
   elm_slider_value_set(slider, 0);
   elm_slider_unit_format_set(slider, "%1.0f");
   elm_slider_indicator_format_set(slider, "%1.0f");
   EPHOTO_WEIGHT(slider, EVAS_HINT_EXPAND, 0.0);
   EPHOTO_ALIGN(slider, EVAS_HINT_FILL, 0.5);
   evas_object_smart_callback_add(slider, "delay,changed",
       _brightness_slider_changed, ebcg);
   elm_box_pack_start(ebcg->editor, slider);
   evas_object_show(slider);
   ebcg->bslider = slider;

   ebcg->handlers =
       eina_list_append(ebcg->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_EDITOR_RESET,
           _bcg_reset, ebcg));
   ebcg->handlers =
       eina_list_append(ebcg->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_EDITOR_APPLY,
           _bcg_apply, ebcg));
   ebcg->handlers =
       eina_list_append(ebcg->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_EDITOR_CANCEL,
           _bcg_cancel, ebcg));

   return;

  error:
   return;
}
