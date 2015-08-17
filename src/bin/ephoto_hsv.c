#include "ephoto.h"

typedef struct _Ephoto_HSV Ephoto_HSV;
struct _Ephoto_HSV
{
   Evas_Object *main;
   Evas_Object *parent;
   Evas_Object *image;
   Evas_Object *frame;
   Evas_Object *hslider;
   Evas_Object *sslider;
   Evas_Object *vslider;
   double hue;
   double saturation;
   double value;
   int w, h;
   unsigned int *original_im_data;
};

unsigned int *
_ephoto_hsv_adjust_hue(Ephoto_HSV *ehsv, double hue, unsigned int *image_data)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   int x, y, w, h;
   int a, r, g, b, nb, ng, nr;
   float hh, s, v;

   im_data = malloc(sizeof(unsigned int) * ehsv->w * ehsv->h);
   if (image_data)
     memcpy(im_data, image_data, sizeof(unsigned int) * ehsv->w * ehsv->h);
   else
     memcpy(im_data, ehsv->original_im_data, sizeof(unsigned int) * ehsv->w * ehsv->h);

   im_data_new = malloc(sizeof(unsigned int) * ehsv->w * ehsv->h);

   for (y = 0; y < ehsv->h; y++)
     {
        p1 = im_data + (y * ehsv->w);
        p2 = im_data_new + (y * ehsv->w);
        for (x = 0; x < ehsv->w; x++)
          {
             b = (int)((*p1) & 0xff);
             g = (int)((*p1 >> 8) & 0xff);
             r = (int)((*p1 >> 16) & 0xff);
             a = (int)((*p1 >> 24) & 0xff);
             if (a > 0 && a < 255)
               {
                  b = b * (255 / a);
                  g = g * (255 / a);
                  r = r * (255 / a);
               }
             evas_color_rgb_to_hsv(r, g, b, &hh, &s, &v);
             hh += hue;
             if (hh < 0)
               hh += 360;
             if (hh > 360)
               hh -= 360;
             evas_color_hsv_to_rgb(hh, s, v, &nr, &ng, &nb);
             if (nb < 0) nb = 0;
             if (nb > 255) nb = 255;
             if (ng < 0) ng = 0;
             if (ng > 255) ng = 255;
             if (nr < 0) nr = 0;
             if (nr > 255) nr = 255;
             if (a > 0 && a < 255)
               {
                  nb = (nb * a) / 255;
                  ng = (ng * a) / 255;
                  nr = (nr * a) / 255;
               }
             *p2 = (a << 24) | (nr << 16) | (ng << 8) | nb;
             p2++;
             p1++;
          }
     }
   ehsv->hue = hue;
   ephoto_single_browser_image_data_update(ehsv->main, ehsv->image, EINA_FALSE, im_data_new, ehsv->w, ehsv->h);
   free(im_data);
   return im_data_new;
}

unsigned int *
_ephoto_hsv_adjust_saturation(Ephoto_HSV *ehsv, double saturation, unsigned int *image_data)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   int x, y, w, h;
   int a, r, g, b, nb, ng, nr;
   float hh, s, v;
   
   im_data = malloc(sizeof(unsigned int) * ehsv->w * ehsv->h);
   if (image_data)
     memcpy(im_data, image_data, sizeof(unsigned int) * ehsv->w * ehsv->h);
   else
     memcpy(im_data, ehsv->original_im_data, sizeof(unsigned int) * ehsv->w * ehsv->h);

   im_data_new = malloc(sizeof(unsigned int) * ehsv->w * ehsv->h);

   for (y = 0; y < ehsv->h; y++)
     {
        p1 = im_data + (y * ehsv->w);
        p2 = im_data_new + (y * ehsv->w);
        for (x = 0; x < ehsv->w; x++)
          {
             b = (int)((*p1) & 0xff);
             g = (int)((*p1 >> 8) & 0xff);
             r = (int)((*p1 >> 16) & 0xff);
             a = (int)((*p1 >> 24) & 0xff);
             if (a > 0 && a < 255)
               {
                  b = b * (255 / a);
                  g = g * (255 / a);
                  r = r * (255 / a);
               }
             evas_color_rgb_to_hsv(r, g, b, &hh, &s, &v);
             s += s*((float)saturation / 100);
             if (s < 0)
               s = 0;
             if (s > 1)
               s = 1;
             evas_color_hsv_to_rgb(hh, s, v, &nr, &ng, &nb);
             if (nb < 0) nb = 0;
             if (nb > 255) nb = 255;
             if (ng < 0) ng = 0;
             if (ng > 255) ng = 255;
             if (nr < 0) nr = 0;
             if (nr > 255) nr = 255;
             if (a > 0 && a < 255)
               {
                  nb = (nb * a) / 255;
                  ng = (ng * a) / 255;
                  nr = (nr * a) / 255;
               }
             *p2 = (a << 24) | (nr << 16) | (ng << 8) | nb;
             p2++;
             p1++;
          }
     }
   ehsv->saturation = saturation;
   ephoto_single_browser_image_data_update(ehsv->main, ehsv->image, EINA_FALSE, im_data_new, ehsv->w, ehsv->h);
   free(im_data);
   return im_data_new;
}

unsigned int *
_ephoto_hsv_adjust_value(Ephoto_HSV *ehsv, double value, unsigned int *image_data)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   int x, y, w, h;
   int a, r, g, b, nb, ng, nr;
   float hh, s, v;

   im_data = malloc(sizeof(unsigned int) * ehsv->w * ehsv->h);
   if (image_data)
     memcpy(im_data, image_data, sizeof(unsigned int) * ehsv->w * ehsv->h);
   else
     memcpy(im_data, ehsv->original_im_data, sizeof(unsigned int) * ehsv->w * ehsv->h);

   im_data_new = malloc(sizeof(unsigned int) * ehsv->w * ehsv->h);

   for (y = 0; y < ehsv->h; y++)
     {
        p1 = im_data + (y * ehsv->w);
        p2 = im_data_new + (y * ehsv->w);
        for (x = 0; x < ehsv->w; x++)
          {
             b = (int)((*p1) & 0xff);
             g = (int)((*p1 >> 8) & 0xff);
             r = (int)((*p1 >> 16) & 0xff);
             a = (int)((*p1 >> 24) & 0xff);
             if (a > 0 && a < 255)
               {
                  b = b * (255 / a);
                  g = g * (255 / a);
                  r = r * (255 / a);
               }
             evas_color_rgb_to_hsv(r, g, b, &hh, &s, &v);
             v += (v*((float)value/100));
             if (v < 0)
               v = 0;
             if (v > 1)
               v = 1;
             evas_color_hsv_to_rgb(hh, s, v, &nr, &ng, &nb);
             if (nb < 0) nb = 0;
             if (nb > 255) nb = 255;
             if (ng < 0) ng = 0;
             if (ng > 255) ng = 255;
             if (nr < 0) nr = 0;
             if (nr > 255) nr = 255;
             if (a > 0 && a < 255)
               {
                  nb = (nb * a) / 255;
                  ng = (ng * a) / 255;
                  nr = (nr * a) / 255;
               }
             *p2 = (a << 24) | (nr << 16) | (ng << 8) | nb;
             p2++;
             p1++;
          }
     }
   ehsv->value = value;
   ephoto_single_browser_image_data_update(ehsv->main, ehsv->image, EINA_FALSE, im_data_new, ehsv->w, ehsv->h);
   free(im_data);
   return im_data_new;
}

static void
_hue_slider_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Ephoto_HSV *ehsv = data;
   double hue;
   unsigned int *image_data, *image_data_two, *image_data_three;

   hue = elm_slider_value_get(obj);
   image_data = _ephoto_hsv_adjust_hue(ehsv, hue, NULL);
   image_data_two = _ephoto_hsv_adjust_saturation(ehsv, ehsv->saturation, image_data);
   image_data_three = _ephoto_hsv_adjust_value(ehsv, ehsv->value, image_data_two);
}


static void
_saturation_slider_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Ephoto_HSV *ehsv = data;
   double saturation;
   unsigned int *image_data, *image_data_two, *image_data_three;

   saturation = elm_slider_value_get(obj);
   image_data = _ephoto_hsv_adjust_saturation(ehsv, saturation, NULL);
   image_data_two = _ephoto_hsv_adjust_hue(ehsv, ehsv->hue, image_data);
   image_data_three = _ephoto_hsv_adjust_value(ehsv, ehsv->value, image_data_two);
}

static void
_value_slider_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Ephoto_HSV *ehsv = data;
   double value;
   unsigned int *image_data, *image_data_two, *image_data_three;

   value = elm_slider_value_get(obj);
   image_data = _ephoto_hsv_adjust_value(ehsv, value, NULL);
   image_data_two = _ephoto_hsv_adjust_hue(ehsv, ehsv->hue, image_data);
   image_data_three = _ephoto_hsv_adjust_saturation(ehsv, ehsv->saturation, image_data_two);
}

static void
_hsv_reset(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_HSV *ehsv = data;
   elm_slider_value_set(ehsv->hslider, 0);
   elm_slider_value_set(ehsv->sslider, 0);
   elm_slider_value_set(ehsv->vslider, 0);
   ehsv->hue = 0;
   ehsv->saturation = 0;
   ehsv->value = 0;
   _hue_slider_changed(ehsv, ehsv->hslider, NULL);
}

static void
_hsv_apply(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_HSV *ehsv = data;
   unsigned int *image_data;
   int w, h;

   image_data = evas_object_image_data_get(elm_image_object_get(ehsv->image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(ehsv->image), &w, &h);
   ephoto_single_browser_image_data_update(ehsv->main, ehsv->image, EINA_TRUE, image_data, w, h);
   evas_object_del(ehsv->frame);
}

static void
_hsv_cancel(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_HSV *ehsv = data;
   ephoto_single_browser_cancel_editing(ehsv->main);
   evas_object_del(ehsv->frame);
}

static void
_frame_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_HSV *ehsv = data;
   free(ehsv->original_im_data);
   free(ehsv);
}

void ephoto_hsv_add(Evas_Object *main, Evas_Object *parent, Evas_Object *image)
{
   Evas_Object *win, *box, *slider, *ic, *button;
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
   im_data = evas_object_image_data_get(elm_image_object_get(ehsv->image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(ehsv->image), &ehsv->w, &ehsv->h);
   ehsv->original_im_data = malloc(sizeof(unsigned int) * ehsv->w * ehsv->h);
   memcpy(ehsv->original_im_data, im_data, sizeof(unsigned int) * ehsv->w * ehsv->h);

   ehsv->frame = elm_frame_add(parent);
   elm_object_text_set(ehsv->frame, "Hue/Saturation/Value");
   evas_object_size_hint_weight_set(ehsv->frame, 0.3, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(ehsv->frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(parent, ehsv->frame);
   evas_object_data_set(ehsv->frame, "ehsv", ehsv);
   evas_object_event_callback_add(ehsv->frame, EVAS_CALLBACK_DEL, _frame_del, ehsv);
   evas_object_show(ehsv->frame);

   box = elm_box_add(ehsv->frame);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_content_set(ehsv->frame, box);
   evas_object_show(box);

   slider = elm_slider_add(box);
   elm_object_text_set(slider, _("Hue"));
   elm_slider_min_max_set(slider, -180, 180);
   elm_slider_step_set(slider, 1);
   elm_slider_value_set(slider, 0);
   elm_slider_unit_format_set(slider, "%1.0f");
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_smart_callback_add(slider, "delay,changed", _hue_slider_changed, ehsv);
   elm_box_pack_end(box, slider);
   evas_object_show(slider);
   ehsv->hslider = slider;

   slider = elm_slider_add(box);
   elm_object_text_set(slider, _("Saturation"));
   elm_slider_min_max_set(slider, -100, 100);
   elm_slider_step_set(slider, 1.20);
   elm_slider_value_set(slider, 0);
   elm_slider_unit_format_set(slider, "%1.2f");
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_smart_callback_add(slider, "delay,changed", _saturation_slider_changed, ehsv);
   elm_box_pack_end(box, slider);
   evas_object_show(slider);
   ehsv->sslider = slider;

   slider = elm_slider_add(box);
   elm_object_text_set(slider, _("Value"));
   elm_slider_min_max_set(slider, -100, 100);
   elm_slider_step_set(slider, 1.20);
   elm_slider_value_set(slider, 0);
   elm_slider_unit_format_set(slider, "%1.2f");
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_smart_callback_add(slider, "delay,changed", _value_slider_changed, ehsv);
   elm_box_pack_end(box, slider);
   evas_object_show(slider);
   ehsv->vslider = slider;

   ic = elm_icon_add(box);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "edit-undo");

   button = elm_button_add(box);
   elm_object_text_set(button, _("Reset"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _hsv_reset, ehsv);
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
   evas_object_smart_callback_add(button, "clicked", _hsv_apply, ehsv);
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
   evas_object_smart_callback_add(button, "clicked", _hsv_cancel, ehsv);
   evas_object_size_hint_weight_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, button);
   evas_object_show(button);

   return;

   error:
      return;
}
