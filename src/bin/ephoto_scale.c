#include "ephoto.h"

typedef struct _Ephoto_Scale Ephoto_Scale;
struct _Ephoto_Scale
{
   Evas_Object *main;
   Evas_Object *parent;
   Evas_Object *image;
   Evas_Object *editor;
   Evas_Object *scalew;
   Evas_Object *scaleh;
   Evas_Object *aspect;
   Eina_Stringshare *tmp_file;
   Eina_List *handlers;
   double aspectw;
   double aspecth;
   Evas_Coord w, h;
   unsigned int *original_im_data;
};

static void _scale_width_changed(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED);

static void
_scale_height_changed(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Scale *es = data;
   double nw;

   if (!elm_check_state_get(es->aspect))
     return;
   evas_object_smart_callback_del(es->scalew, "changed",
       _scale_width_changed);
   nw = round(es->aspectw * elm_spinner_value_get(es->scaleh));
   if (nw <= 1)
     {
        double nh;
        evas_object_smart_callback_del(es->scaleh, "changed",
            _scale_height_changed);
        nh = round(es->aspecth * 1);
        elm_spinner_value_set(es->scaleh, nh);
        evas_object_smart_callback_add(es->scaleh, "changed",
            _scale_height_changed, es);
        nw = 1;
     }
   elm_spinner_value_set(es->scalew, nw);
   evas_object_smart_callback_add(es->scalew, "changed",
       _scale_width_changed, es);
}

static void
_scale_width_changed(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Scale *es = data;
   double nh;

   if (!elm_check_state_get(es->aspect))
     return;
   evas_object_smart_callback_del(es->scaleh, "changed",
       _scale_height_changed);
   nh = round(es->aspecth * elm_spinner_value_get(es->scalew));
   if (nh <= 1)
     {
        double nw;

        evas_object_smart_callback_del(es->scalew, "changed",
            _scale_width_changed);
        nw = round(es->aspectw * 1);
        elm_spinner_value_set(es->scalew, nw);
        evas_object_smart_callback_add(es->scalew, "changed",
            _scale_width_changed, es);
        nh = 1;
     }
   elm_spinner_value_set(es->scaleh, nh);
   evas_object_smart_callback_add(es->scaleh, "changed",
       _scale_height_changed, es);
}

static Eina_Bool
_es_reset(void *data, int type EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Scale *es = data;

   elm_spinner_value_set(es->scalew, es->w);
   elm_spinner_value_set(es->scaleh, es->h);
   es->aspectw = (double)es->w / (double)es->h;
   es->aspecth = (double)es->h / (double)es->w;
   
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_es_apply(void *data, int type EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Scale *es = data;
   const unsigned int *image_data;
   unsigned int *im_data;
   const char *key;
   Evas_Object *im;
   int w, h, alpha;
   Ecore_Evas *ee;
   Evas *eecanvas;

   w = round(elm_spinner_value_get(es->scalew));
   h = round(elm_spinner_value_get(es->scaleh));
   evas_object_image_file_get(es->image,
       NULL, &key);

   ee = ecore_evas_buffer_new(1, 1);
   eecanvas = ecore_evas_get(ee);
   evas_image_cache_set(eecanvas, 0);
   evas_font_cache_set(eecanvas, 0);
   alpha = 1;
   
   im = evas_object_image_add(eecanvas);
   evas_object_image_load_size_set(im, w, h);
   evas_object_image_load_orientation_set(im, EINA_TRUE);
   evas_object_image_file_set(im, es->tmp_file, key);
   alpha = evas_object_image_alpha_get(im);
   evas_object_image_fill_set(im, 0, 0, w, h);
   evas_object_move(im, 0, 0);
   evas_object_resize(im, w, h);
   
   ecore_evas_alpha_set(ee, alpha);
   ecore_evas_resize(ee, w, h);
   
   evas_object_show(im);

   image_data = ecore_evas_buffer_pixels_get(ee);
   im_data = malloc(sizeof(unsigned int) * w * h);
   memcpy(im_data, image_data, sizeof(unsigned int) * w * h);

   ephoto_single_browser_image_data_done(es->main, im_data, w, h);
   ephoto_editor_del(es->editor);

   evas_object_del(im);
   ecore_evas_free(ee);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_es_cancel(void *data, int type EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Scale *es = data;

   ephoto_single_browser_cancel_editing(es->main);
   ephoto_editor_del(es->editor);

   return ECORE_CALLBACK_PASS_ON;
}

static void
_editor_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Scale *es = data;
   Ecore_Event_Handler *handler;

   EINA_LIST_FREE(es->handlers, handler)
     ecore_event_handler_del(handler);
   ecore_file_unlink(es->tmp_file);
   eina_stringshare_del(es->tmp_file);
   free(es->original_im_data);
   free(es);
}

void
ephoto_scale_add(Ephoto *ephoto, Evas_Object *main, Evas_Object *parent,
    Evas_Object *image, const char *file)
{
   Ephoto_Scale *es;
   unsigned int *im_data;
   char buf[PATH_MAX];

   EINA_SAFETY_ON_NULL_GOTO(image, error);

   es = calloc(1, sizeof(Ephoto_Scale));
   EINA_SAFETY_ON_NULL_GOTO(es, error);

   es->main = main;
   es->parent = parent;
   es->image = image;

   snprintf(buf, PATH_MAX, "%s/.config/ephoto/temp.%s", getenv("HOME"),
       strrchr(file, '.')+1);
   es->tmp_file = eina_stringshare_add(buf);
   if (ecore_file_exists(es->tmp_file))
     ecore_file_unlink(es->tmp_file);
   evas_object_image_save(es->image, es->tmp_file,
       NULL, NULL);

   im_data =
       evas_object_image_data_get(es->image,
       EINA_FALSE);
   evas_object_image_size_get(es->image, &es->w,
       &es->h);
   es->original_im_data = malloc(sizeof(unsigned int) * es->w * es->h);
   memcpy(es->original_im_data, im_data,
       sizeof(unsigned int) * es->w * es->h);
   es->aspectw = (double)es->w / (double)es->h;
   es->aspecth = (double)es->h / (double)es->w;

   es->editor = ephoto_editor_add(ephoto, _("Scale Image"),
       "es", es);
   evas_object_event_callback_add(es->editor, EVAS_CALLBACK_DEL, _editor_del,
       es);

   es->aspect = elm_check_add(es->editor);
   elm_object_text_set(es->aspect, _("Keep Aspect"));
   evas_object_size_hint_align_set(es->aspect, 0.5, EVAS_HINT_FILL);
   elm_check_state_set(es->aspect, EINA_TRUE);
   elm_box_pack_start(es->editor, es->aspect);
   evas_object_show(es->aspect);

   es->scaleh = elm_spinner_add(es->editor);
   elm_spinner_editable_set(es->scaleh, EINA_TRUE);
   elm_spinner_label_format_set(es->scaleh, "Height: %1.0f");
   elm_spinner_step_set(es->scaleh, 1);
   elm_spinner_wrap_set(es->scaleh, EINA_FALSE);
   elm_spinner_min_max_set(es->scaleh, 1, 99999);
   elm_spinner_value_set(es->scaleh, es->h);
   evas_object_size_hint_align_set(es->scaleh, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(es->scaleh, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   evas_object_smart_callback_add(es->scaleh, "changed",
                                  _scale_height_changed, es);
   elm_box_pack_start(es->editor, es->scaleh);
   evas_object_show(es->scaleh);

   es->scalew = elm_spinner_add(es->editor);
   elm_spinner_editable_set(es->scalew, EINA_TRUE);
   elm_spinner_label_format_set(es->scalew, "Width: %1.0f");
   elm_spinner_step_set(es->scalew, 1);
   elm_spinner_wrap_set(es->scalew, EINA_FALSE);
   elm_spinner_min_max_set(es->scalew, 1, 99999);
   elm_spinner_value_set(es->scalew, es->w);
   evas_object_size_hint_align_set(es->scalew, EVAS_HINT_FILL, 0.5);
   evas_object_size_hint_weight_set(es->scalew, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   evas_object_smart_callback_add(es->scalew, "changed",
                                  _scale_width_changed, es);
   elm_box_pack_start(es->editor, es->scalew);
   evas_object_show(es->scalew);

   es->handlers =
       eina_list_append(es->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_EDITOR_RESET,
           _es_reset, es));
   es->handlers =
       eina_list_append(es->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_EDITOR_APPLY,
           _es_apply, es));
   es->handlers =
       eina_list_append(es->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_EDITOR_CANCEL,
           _es_cancel, es));

   return;

  error:
   return;
}

