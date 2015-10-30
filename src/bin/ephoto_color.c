#include "ephoto.h"

typedef struct _Ephoto_Color Ephoto_Color;
struct _Ephoto_Color
{
   Evas_Object *main;
   Evas_Object *parent;
   Evas_Object *image;
   Evas_Object *frame;
   Evas_Object *bslider;
   Evas_Object *gslider;
   Evas_Object *rslider;
   int blue;
   int green;
   int red;
   int w, h;
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
     return (color * (255 / alpha));
   else
     return color;
}

static int
_demul_color_alpha(int color, int alpha)
{
   if (alpha > 0 && alpha < 255)
     return ((color * alpha) / 255);
   else
     return color;
}

unsigned int *
_ephoto_color_adjust_red(Ephoto_Color *eco, int red, unsigned int *image_data)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   int x, y, w, h;
   int a, r, g, b, rr;

   im_data = malloc(sizeof(unsigned int) * eco->w * eco->h);
   if (image_data)
     memcpy(im_data, image_data, sizeof(unsigned int) * eco->w * eco->h);
   else
     memcpy(im_data, eco->original_im_data, sizeof(unsigned int) * eco->w * eco->h);

   eco->red = red;
   im_data_new = malloc(sizeof(unsigned int) * eco->w * eco->h);

   for (y = 0; y < eco->h; y++)
     {
        p1 = im_data + (y * eco->w);
        p2 = im_data_new + (y * eco->w);
        for (x = 0; x < eco->w; x++)
          {
             b = (int)((*p1) & 0xff);
             g = (int)((*p1 >> 8) & 0xff);
             r = (int)((*p1 >> 16) & 0xff);
             a = (int)((*p1 >> 24) & 0xff);
             b = _mul_color_alpha(b, a);
             g = _mul_color_alpha(g, a);
             r = _mul_color_alpha(r, a);
             rr = (int)r+eco->red;
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
   ephoto_single_browser_image_data_update(eco->main, eco->image, EINA_FALSE, im_data_new, eco->w, eco->h);
   free(im_data);
   return im_data_new;
}

unsigned int *
_ephoto_color_adjust_green(Ephoto_Color *eco, int green, unsigned int *image_data)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   int x, y, w, h;
   int a, r, g, b, gg;

   im_data = malloc(sizeof(unsigned int) * eco->w * eco->h);
   if (image_data)
     memcpy(im_data, image_data, sizeof(unsigned int) * eco->w * eco->h);
   else
     memcpy(im_data, eco->original_im_data, sizeof(unsigned int) * eco->w * eco->h);

   eco->green = green;
   im_data_new = malloc(sizeof(unsigned int) * eco->w * eco->h);

   for (y = 0; y < eco->h; y++)
     {
        p1 = im_data + (y * eco->w);
        p2 = im_data_new + (y * eco->w);
        for (x = 0; x < eco->w; x++)
          {
             b = (int)((*p1) & 0xff);
             g = (int)((*p1 >> 8) & 0xff);
             r = (int)((*p1 >> 16) & 0xff);
             a = (int)((*p1 >> 24) & 0xff);
             b = _mul_color_alpha(b, a);
             g = _mul_color_alpha(g, a);
             r = _mul_color_alpha(r, a);
             gg = (int)g+eco->green;
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
   ephoto_single_browser_image_data_update(eco->main, eco->image, EINA_FALSE, im_data_new, eco->w, eco->h);
   free(im_data);
   return im_data_new;
}

unsigned int *
_ephoto_color_adjust_blue(Ephoto_Color *eco, int blue, unsigned int *image_data)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   int x, y, w, h;
   int a, r, g, b, bb;

   im_data = malloc(sizeof(unsigned int) * eco->w * eco->h);
   if (image_data)
     memcpy(im_data, image_data, sizeof(unsigned int) * eco->w * eco->h);
   else
     memcpy(im_data, eco->original_im_data, sizeof(unsigned int) * eco->w * eco->h);

   eco->blue = blue;
   im_data_new = malloc(sizeof(unsigned int) * eco->w * eco->h);

   for (y = 0; y < eco->h; y++)
     {
        p1 = im_data + (y * eco->w);
        p2 = im_data_new + (y * eco->w);
        for (x = 0; x < eco->w; x++)
          {
             b = (int)((*p1) & 0xff);
             g = (int)((*p1 >> 8) & 0xff);
             r = (int)((*p1 >> 16) & 0xff);
             a = (int)((*p1 >> 24) & 0xff);
             b = _mul_color_alpha(b, a);
             g = _mul_color_alpha(g, a);
             r = _mul_color_alpha(r, a);
             bb = (int)b+eco->blue;
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
   ephoto_single_browser_image_data_update(eco->main, eco->image, EINA_FALSE, im_data_new, eco->w, eco->h);
   free(im_data);
   return im_data_new;
}

static void
_red_slider_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Ephoto_Color *eco = data;
   int red;
   unsigned int *image_data, *image_data_two, *image_data_three;

   red = elm_slider_value_get(obj);
   image_data = _ephoto_color_adjust_red(eco, red, NULL);
   image_data_two = _ephoto_color_adjust_green(eco, eco->green, image_data);
   image_data_three = _ephoto_color_adjust_blue(eco, eco->blue, image_data_two);
}


static void
_green_slider_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Ephoto_Color *eco = data;
   int green;
   unsigned int *image_data, *image_data_two, *image_data_three;

   green = elm_slider_value_get(obj);
   image_data = _ephoto_color_adjust_green(eco, green, NULL);
   image_data_two = _ephoto_color_adjust_red(eco, eco->red, image_data);
   image_data_three = _ephoto_color_adjust_blue(eco, eco->blue, image_data_two);
}

static void
_blue_slider_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Ephoto_Color *eco = data;
   int blue;
   unsigned int *image_data, *image_data_two, *image_data_three;

   blue = elm_slider_value_get(obj);
   image_data = _ephoto_color_adjust_blue(eco, blue, NULL);
   image_data_two = _ephoto_color_adjust_red(eco, eco->red, image_data);
   image_data_three = _ephoto_color_adjust_green(eco, eco->green, image_data_two);
}

static void
_color_reset(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Color *eco = data;

   elm_slider_value_set(eco->rslider, 0);
   elm_slider_value_set(eco->gslider, 0);
   elm_slider_value_set(eco->bslider, 0);
   eco->red = 0;
   eco->green = 0;
   eco->blue = 0;
   _red_slider_changed(eco, eco->rslider, NULL);
}

static void
_color_apply(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Color *eco = data;
   unsigned int *image_data;
   int w, h;

   image_data = evas_object_image_data_get(elm_image_object_get(eco->image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(eco->image), &w, &h);
   ephoto_single_browser_image_data_update(eco->main, eco->image, EINA_TRUE, image_data, w, h);
   evas_object_del(eco->frame);
}

static void
_color_cancel(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Color *eco = data;

   elm_slider_value_set(eco->rslider, 0);
   elm_slider_value_set(eco->gslider, 0);
   elm_slider_value_set(eco->bslider, 0);
   eco->red = 0;
   eco->green = 0;
   eco->blue = 0;
   _red_slider_changed(eco, eco->rslider, NULL);
   ephoto_single_browser_cancel_editing(eco->main, eco->image);
   evas_object_del(eco->frame);
}

static void
_frame_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Color *eco = data;
   free(eco->original_im_data);
   free(eco);
}

void ephoto_color_add(Evas_Object *main, Evas_Object *parent, Evas_Object *image)
{
   Evas_Object *win, *box, *slider, *ic, *button;
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
   im_data = evas_object_image_data_get(elm_image_object_get(eco->image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(eco->image), &eco->w, &eco->h);
   eco->original_im_data = malloc(sizeof(unsigned int) * eco->w * eco->h);
   memcpy(eco->original_im_data, im_data, sizeof(unsigned int) * eco->w * eco->h);

   eco->frame = elm_frame_add(parent);
   elm_object_text_set(eco->frame, _("Adjust Color Levels"));
   evas_object_size_hint_weight_set(eco->frame, 0.3, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(eco->frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(parent, eco->frame);
   evas_object_data_set(eco->frame, "eco", eco);
   evas_object_event_callback_add(eco->frame, EVAS_CALLBACK_DEL, _frame_del, eco);
   evas_object_show(eco->frame);

   box = elm_box_add(eco->frame);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_content_set(eco->frame, box);
   evas_object_show(box);

   slider = elm_slider_add(box);
   elm_object_text_set(slider, _("Red"));
   elm_slider_min_max_set(slider, -100, 100);
   elm_slider_step_set(slider, 1);
   elm_slider_value_set(slider, 0);
   elm_slider_unit_format_set(slider, "%1.0f");
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_smart_callback_add(slider, "delay,changed", _red_slider_changed, eco);
   elm_box_pack_end(box, slider);
   evas_object_show(slider);
   eco->rslider = slider;

   slider = elm_slider_add(box);
   elm_object_text_set(slider, _("Green"));
   elm_slider_min_max_set(slider, -100, 100);
   elm_slider_step_set(slider, 1);
   elm_slider_value_set(slider, 0);
   elm_slider_unit_format_set(slider, "%1.0f");
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_smart_callback_add(slider, "delay,changed", _green_slider_changed, eco);
   elm_box_pack_end(box, slider);
   evas_object_show(slider);
   eco->gslider = slider;

   slider = elm_slider_add(box);
   elm_object_text_set(slider, _("Blue"));
   elm_slider_min_max_set(slider, -100, 100);
   elm_slider_step_set(slider, 1);
   elm_slider_value_set(slider, 0);
   elm_slider_unit_format_set(slider, "%1.0f");
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_smart_callback_add(slider, "delay,changed", _blue_slider_changed, eco);
   elm_box_pack_end(box, slider);
   evas_object_show(slider);
   eco->bslider = slider;

   ic = elm_icon_add(box);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "edit-undo");

   button = elm_button_add(box);
   elm_object_text_set(button, _("Reset"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _color_reset, eco);
   evas_object_size_hint_weight_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, button);
   evas_object_show(button);

   ic = elm_icon_add(box);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "document-save");

   button = elm_button_add(box);
   elm_object_text_set(button, _("Apply"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _color_apply, eco);
   evas_object_size_hint_weight_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, button);
   evas_object_show(button);

   ic = elm_icon_add(box);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "window-close");

   button = elm_button_add(box);
   elm_object_text_set(button, _("Cancel"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _color_cancel, eco);
   evas_object_size_hint_weight_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, button);
   evas_object_show(button);

   return;

   error:
      return;
}
