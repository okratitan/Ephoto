#include "ephoto.h"

typedef struct _Ephoto_BCG Ephoto_BCG;
struct _Ephoto_BCG
{
   Evas_Object *main;
   Evas_Object *parent;
   Evas_Object *image;
   Evas_Object *frame;
   Evas_Object *bslider;
   Evas_Object *cslider;
   Evas_Object *gslider;
   int contrast;
   int brightness;
   double gamma;
   int w, h;
   unsigned int *original_im_data;
};

unsigned int *
_ephoto_bcg_adjust_brightness(Ephoto_BCG *ebcg, int brightness, unsigned int *image_data)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   int x, y, w, h;
   int a, r, g, b, nb, ng, nr;

   im_data = malloc(sizeof(unsigned int) * ebcg->w * ebcg->h);
   if (image_data)
     memcpy(im_data, image_data, sizeof(unsigned int) * ebcg->w * ebcg->h);
   else
     memcpy(im_data, ebcg->original_im_data, sizeof(unsigned int) * ebcg->w * ebcg->h);

   ebcg->brightness = brightness;
   im_data_new = malloc(sizeof(unsigned int) * ebcg->w * ebcg->h);

   for (y = 0; y < ebcg->h; y++)
     {
        p1 = im_data + (y * ebcg->w);
        p2 = im_data_new + (y * ebcg->w);
        for (x = 0; x < ebcg->w; x++)
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
             nb = (int)b+ebcg->brightness;
             ng = (int)g+ebcg->brightness;
             nr = (int)r+ebcg->brightness;
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
   ephoto_single_browser_image_data_update(ebcg->main, ebcg->image, EINA_FALSE, im_data_new, ebcg->w, ebcg->h);
   free(im_data);
   return im_data_new;
}

unsigned int *
_ephoto_bcg_adjust_contrast(Ephoto_BCG *ebcg, int contrast, unsigned int *image_data)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   int x, y, w, h, top, bottom;
   int a, r, g, b, nb, ng, nr;
   float factor;

   im_data = malloc(sizeof(unsigned int) * ebcg->w * ebcg->h);
   if (image_data)
     memcpy(im_data, image_data, sizeof(unsigned int) * ebcg->w * ebcg->h);
   else
     memcpy(im_data, ebcg->original_im_data, sizeof(unsigned int) * ebcg->w * ebcg->h); 

   ebcg->contrast = contrast;
   top = ((255+(contrast))*259);
   bottom = ((259-(contrast))*255);
   factor = (float)top / (float)bottom; 
   im_data_new = malloc(sizeof(unsigned int) * ebcg->w * ebcg->h);
   
   for (y = 0; y < ebcg->h; y++)
     {
        p1 = im_data + (y * ebcg->w);
        p2 = im_data_new + (y * ebcg->w);
        for (x = 0; x < ebcg->w; x++)
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
             nb = (int)((factor * (b - 128)) + 128);
             ng = (int)((factor * (g - 128)) + 128);
             nr = (int)((factor * (r - 128)) + 128);
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
   ephoto_single_browser_image_data_update(ebcg->main, ebcg->image, EINA_FALSE, im_data_new, ebcg->w, ebcg->h);
   free(im_data);
   return im_data_new;
}

unsigned int *
_ephoto_bcg_adjust_gamma(Ephoto_BCG *ebcg, double gamma, unsigned int *image_data)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   int x, y, w, h;
   int a, r, g, b, nb, ng, nr;

   im_data = malloc(sizeof(unsigned int) * ebcg->w * ebcg->h);
   if (image_data)
     memcpy(im_data, image_data, sizeof(unsigned int) * ebcg->w * ebcg->h);
   else
     memcpy(im_data, ebcg->original_im_data, sizeof(unsigned int) * ebcg->w * ebcg->h); 

   ebcg->gamma = 1/gamma;
   im_data_new = malloc(sizeof(unsigned int) * ebcg->w * ebcg->h);
   
   for (y = 0; y < ebcg->h; y++)
     {
        p1 = im_data + (y * ebcg->w);
        p2 = im_data_new + (y * ebcg->w);
        for (x = 0; x < ebcg->w; x++)
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
             nb = (int)(pow(((double)b/255), ebcg->gamma) * 255);
             ng = (int)(pow(((double)g/255), ebcg->gamma) * 255);
             nr = (int)(pow(((double)r/255), ebcg->gamma) * 255);
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
   ephoto_single_browser_image_data_update(ebcg->main, ebcg->image, EINA_FALSE, im_data_new, ebcg->w, ebcg->h);  
   free(im_data);
   return im_data_new;
}

static void
_brightness_slider_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Ephoto_BCG *ebcg = data;
   int brightness;
   unsigned int *image_data, *image_data_two, *image_data_three;

   brightness = elm_slider_value_get(obj);
   image_data = _ephoto_bcg_adjust_brightness(ebcg, brightness, NULL);
   image_data_two = _ephoto_bcg_adjust_contrast(ebcg, ebcg->contrast, image_data);
   image_data_three = _ephoto_bcg_adjust_gamma(ebcg, ebcg->gamma, image_data_two);
}


static void
_contrast_slider_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Ephoto_BCG *ebcg = data;
   int contrast;
   unsigned int *image_data, *image_data_two, *image_data_three;

   contrast = elm_slider_value_get(obj);
   image_data = _ephoto_bcg_adjust_contrast(ebcg, contrast, NULL);
   image_data_two = _ephoto_bcg_adjust_brightness(ebcg, ebcg->brightness, image_data);
   image_data_three = _ephoto_bcg_adjust_gamma(ebcg, ebcg->gamma, image_data_two);
}

static void
_gamma_slider_changed(void *data, Evas_Object *obj, void *event_info EINA_UNUSED)
{
   Ephoto_BCG *ebcg = data;
   double gamma;
   unsigned int *image_data, *image_data_two, *image_data_three;

   gamma = elm_slider_value_get(obj);
   image_data = _ephoto_bcg_adjust_gamma(ebcg, gamma, NULL);
   image_data_two = _ephoto_bcg_adjust_brightness(ebcg, ebcg->brightness, image_data);
   image_data_three = _ephoto_bcg_adjust_contrast(ebcg, ebcg->contrast, image_data_two);
}

static void
_bcg_reset(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_BCG *ebcg = data;
   elm_slider_value_set(ebcg->bslider, 0);
   elm_slider_value_set(ebcg->cslider, 0);
   elm_slider_value_set(ebcg->gslider, 1);
   ebcg->brightness = 0;
   ebcg->contrast = 0;
   ebcg->gamma = 1;
   _brightness_slider_changed(ebcg, ebcg->bslider, NULL);
}

static void
_bcg_apply(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_BCG *ebcg = data;
   unsigned int *image_data;
   int w, h;

   image_data = evas_object_image_data_get(elm_image_object_get(ebcg->image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(ebcg->image), &w, &h);
   ephoto_single_browser_image_data_update(ebcg->main, ebcg->image, EINA_TRUE, image_data, w, h);
   evas_object_del(ebcg->frame);
}

static void
_bcg_cancel(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_BCG *ebcg = data;
   ephoto_single_browser_cancel_editing(ebcg->main, ebcg->image);
   evas_object_del(ebcg->frame);
}

static void
_frame_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_BCG *ebcg = data;
   free(ebcg->original_im_data);
   free(ebcg);
}

void ephoto_bcg_add(Evas_Object *main, Evas_Object *parent, Evas_Object *image)
{
   Evas_Object *win, *box, *slider, *ic, *button;
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
   im_data = evas_object_image_data_get(elm_image_object_get(ebcg->image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(ebcg->image), &ebcg->w, &ebcg->h);
   ebcg->original_im_data = malloc(sizeof(unsigned int) * ebcg->w * ebcg->h);
   memcpy(ebcg->original_im_data, im_data, sizeof(unsigned int) * ebcg->w * ebcg->h);

   ebcg->frame = elm_frame_add(parent);
   elm_object_text_set(ebcg->frame, "Brightness/Contrast/Gamma");
   evas_object_size_hint_weight_set(ebcg->frame, 0.3, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(ebcg->frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(parent, ebcg->frame);
   evas_object_data_set(ebcg->frame, "ebcg", ebcg);
   evas_object_event_callback_add(ebcg->frame, EVAS_CALLBACK_DEL, _frame_del, ebcg);
   evas_object_show(ebcg->frame);

   box = elm_box_add(ebcg->frame);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_object_content_set(ebcg->frame, box);
   evas_object_show(box);

   slider = elm_slider_add(box);
   elm_object_text_set(slider, _("Brightness"));
   elm_slider_min_max_set(slider, -100, 100);
   elm_slider_step_set(slider, 1);
   elm_slider_value_set(slider, 0);
   elm_slider_unit_format_set(slider, "%1.0f");
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_smart_callback_add(slider, "delay,changed", _brightness_slider_changed, ebcg);
   elm_box_pack_end(box, slider);
   evas_object_show(slider);
   ebcg->bslider = slider;

   slider = elm_slider_add(box);
   elm_object_text_set(slider, _("Contrast"));
   elm_slider_min_max_set(slider, -100, 100);
   elm_slider_step_set(slider, 1);
   elm_slider_value_set(slider, 0);
   elm_slider_unit_format_set(slider, "%1.0f");
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_smart_callback_add(slider, "delay,changed", _contrast_slider_changed, ebcg);
   elm_box_pack_end(box, slider);
   evas_object_show(slider);
   ebcg->cslider = slider;

   slider = elm_slider_add(box);
   elm_object_text_set(slider, _("Gamma"));
   elm_slider_min_max_set(slider, 0.1, 3);
   elm_slider_step_set(slider, .1);
   elm_slider_value_set(slider, 1);
   elm_slider_unit_format_set(slider, "%1.1f");
   evas_object_size_hint_weight_set(slider, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(slider, EVAS_HINT_FILL, 0.5);
   evas_object_smart_callback_add(slider, "delay,changed", _gamma_slider_changed, ebcg);
   elm_box_pack_end(box, slider);
   evas_object_show(slider);
   ebcg->gslider = slider;

   ic = elm_icon_add(box);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "edit-undo");

   button = elm_button_add(box);
   elm_object_text_set(button, _("Reset"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _bcg_reset, ebcg);
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
   evas_object_smart_callback_add(button, "clicked", _bcg_apply, ebcg);
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
   evas_object_smart_callback_add(button, "clicked", _bcg_cancel, ebcg);
   evas_object_size_hint_weight_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, button);
   evas_object_show(button);

   return;

   error:
      return;
}
