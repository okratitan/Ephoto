#include "ephoto.h"

static void
_editor_reset(void *data EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   ecore_event_add(EPHOTO_EVENT_EDITOR_RESET, NULL, NULL, NULL);   
}

static void
_editor_apply(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;

   ecore_event_add(EPHOTO_EVENT_EDITOR_APPLY, NULL, NULL, NULL);
   edje_object_signal_emit(elm_layout_edje_get(ephoto->layout),
       "ephoto,editor,hide", "ephoto");
   ephoto->editor_blocking = EINA_FALSE;
}

static void
_editor_cancel(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;

   ecore_event_add(EPHOTO_EVENT_EDITOR_CANCEL, NULL, NULL, NULL);
   edje_object_signal_emit(elm_layout_edje_get(ephoto->layout),
       "ephoto,editor,hide", "ephoto"); 
   ephoto->editor_blocking = EINA_FALSE;
}

Evas_Object *
ephoto_editor_add(Ephoto *ephoto, const char *title, const char *data_name,
    void *data)
{
   Evas_Object *frame, *box, *ic, *button;

   ephoto->editor_blocking = EINA_TRUE;
   edje_object_signal_emit(elm_layout_edje_get(ephoto->layout),
       "ephoto,controls,hide", "ephoto");
   edje_object_signal_emit(elm_layout_edje_get(ephoto->layout),
       "ephoto,folders,hide", "ephoto");

   frame = elm_frame_add(ephoto->layout);
   elm_object_text_set(frame, title);
   evas_object_size_hint_weight_set(frame, 0.3, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(frame, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_data_set(frame, data_name, data);
   elm_layout_content_set(ephoto->layout, "ephoto.swallow.editor", frame);
   evas_object_show(frame);

   box = elm_box_add(frame);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_data_set(box, data_name, data);
   evas_object_data_set(box, "frame", frame);
   elm_object_content_set(frame, box);
   evas_object_show(box);

   ic = elm_icon_add(box);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(), 20*elm_config_scale_get());
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "edit-undo");

   button = elm_button_add(box);
   elm_object_text_set(button, _("Reset"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _editor_reset, box);
   evas_object_size_hint_weight_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_min_set(button, 220*elm_config_scale_get(), 30*elm_config_scale_get());
   elm_box_pack_end(box, button);
   evas_object_show(button);

   ic = elm_icon_add(box);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(), 20*elm_config_scale_get());
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "document-save");

   button = elm_button_add(box);
   elm_object_text_set(button, _("Apply"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _editor_apply, ephoto);
   evas_object_size_hint_weight_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_min_set(button, 220*elm_config_scale_get(), 30*elm_config_scale_get());
   elm_box_pack_end(box, button);
   evas_object_show(button);

   ic = elm_icon_add(box);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(), 20*elm_config_scale_get());
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "window-close");

   button = elm_button_add(box);
   elm_object_text_set(button, _("Cancel"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _editor_cancel, ephoto);
   evas_object_size_hint_weight_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(button, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_min_set(button, 220*elm_config_scale_get(), 30*elm_config_scale_get());
   elm_box_pack_end(box, button);
   evas_object_show(button);   

   edje_object_signal_emit(elm_layout_edje_get(ephoto->layout),
       "ephoto,editor,show", "ephoto");

   return box;
}

void
ephoto_editor_del(Evas_Object *obj)
{
   Evas_Object *frame = evas_object_data_get(obj, "frame");

   if (frame)
     evas_object_del(frame);
}

