#include "ephoto.h"

typedef struct _Ephoto_Slideshow Ephoto_Slideshow;

struct _Ephoto_Slideshow
{
   Ephoto *ephoto;
   Evas_Object *slideshow;
   Evas_Object *event;
   Evas_Object *notify;
   Eina_List *entries;
   Evas_Object *pause;
   Evas_Object *pause_after;
   Evas_Object *fullscreen;
   Evas_Object *fullscreen_after;
   Ephoto_Entry *entry;
   Eina_Bool playing;
};

static Evas_Object *_add_icon(Evas_Object *parent, const char *icon,
    const char *label, Evas_Object *before);

static void
_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Slideshow *ss = data;
   Elm_Object_Item *slideshow_item;
   Ephoto_Entry *entry;

   elm_layout_content_unset(ss->ephoto->layout, "ephoto.swallow.controls");
   evas_object_del(ss->notify);
   elm_layout_content_set(ss->ephoto->layout, "ephoto.swallow.controls",
       ss->ephoto->statusbar);

   slideshow_item = elm_slideshow_item_current_get(ss->slideshow);
   if (slideshow_item)
      entry = elm_object_item_data_get(slideshow_item);
   else
      entry = ss->entry;
   if (ss->event)
     {
	evas_object_del(ss->event);
	ss->event = NULL;
     }
   evas_object_smart_callback_call(ss->slideshow, "back", entry);
   elm_slideshow_clear(ss->slideshow);
   ss->playing = 0;
   evas_object_freeze_events_set(ss->slideshow, EINA_TRUE);
}

static void
_entry_free(void *data, const Ephoto_Entry *entry EINA_UNUSED)
{
   Ephoto_Slideshow *ss = data;

   ss->entry = NULL;
}

static void
_slideshow_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Slideshow *ss = data;

   if (ss->entry)
      ephoto_entry_free_listener_del(ss->entry, _entry_free, ss);
   free(ss);
}

static void
_back(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Slideshow *ss = data;
   Elm_Object_Item *slideshow_item;
   Ephoto_Entry *entry;

   elm_layout_content_unset(ss->ephoto->layout, "ephoto.swallow.controls");
   evas_object_del(ss->notify);
   elm_layout_content_set(ss->ephoto->layout, "ephoto.swallow.controls",
       ss->ephoto->statusbar);

   slideshow_item = elm_slideshow_item_current_get(ss->slideshow);
   if (slideshow_item)
      entry = elm_object_item_data_get(slideshow_item);
   else
      entry = ss->entry;
   if (ss->event)
     {
	evas_object_del(ss->event);
	ss->event = NULL;
     }
   evas_object_smart_callback_call(ss->slideshow, "back", entry);
   elm_slideshow_clear(ss->slideshow);
   ss->playing = 0;
   evas_object_freeze_events_set(ss->slideshow, EINA_TRUE);
}

static void
_first(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   elm_slideshow_item_show(elm_slideshow_item_nth_get(data, 0));
}

static void
_next(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   elm_slideshow_next(data);
}

static void
_pause(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Slideshow *ss = data;

   evas_object_del(ss->pause);

   if (ss->playing)
     {
	elm_slideshow_timeout_set(ss->slideshow, 0.0);
	ss->pause =
            _add_icon(ss->notify, "media-playback-start", _("Play"),
                ss->pause_after);
        evas_object_smart_callback_add(ss->pause, "clicked", _pause, ss);
	ss->playing = 0;
     }
   else
     {
	elm_slideshow_timeout_set(ss->slideshow,
	    ss->ephoto->config->slideshow_timeout);
        ss->pause =
            _add_icon(ss->notify, "media-playback-pause", _("Pause"),
                ss->pause_after);
        evas_object_smart_callback_add(ss->pause, "clicked", _pause, ss);
	ss->playing = 1;
     }
}

static void
_previous(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   elm_slideshow_previous(data);
}

static void
_last(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   unsigned int count = elm_slideshow_count_get(data);

   elm_slideshow_item_show(elm_slideshow_item_nth_get(data, count - 1));
}

static void
_fullscreen(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Slideshow *ss = data;

   evas_object_del(ss->fullscreen);

   if (elm_win_fullscreen_get(ss->ephoto->win))
     {
	ss->fullscreen =
            _add_icon(ss->notify, "view-fullscreen", _("Fullscreen"),
            ss->fullscreen_after);
        evas_object_smart_callback_add(ss->fullscreen, "clicked",
            _fullscreen, ss);
	elm_win_fullscreen_set(ss->ephoto->win, EINA_FALSE);
     }
   else
     {
	ss->fullscreen =
            _add_icon(ss->notify, "view-restore", _("Normal"),
            ss->fullscreen_after);
        evas_object_smart_callback_add(ss->fullscreen, "clicked",
            _fullscreen, ss);
	elm_win_fullscreen_set(ss->ephoto->win, EINA_TRUE);
     }
}

static void
_settings(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Slideshow *ss = data;

   ephoto_config_main(ss->ephoto);
}

static void
_key_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto_Slideshow *ss = data;
   Evas_Event_Key_Down *ev = event_info;
   const char *k = ev->keyname;

   if (!strcmp(k, "Escape") || !strcmp(k, "F5"))
     {
	_back(ss, NULL, NULL);
     }
   else if (!strcmp(k, "F1"))
     {
        _settings(ss, NULL, NULL);
     }
   else if (!strcmp(k, "F11"))
     {
	Evas_Object *win = ss->ephoto->win;

	elm_win_fullscreen_set(win, !elm_win_fullscreen_get(win));
     }
   else if (!strcmp(k, "space"))
     {
	_pause(ss, NULL, NULL);
     }
   else if (!strcmp(k, "Home"))
     {
	_first(ss->slideshow, NULL, NULL);
     }
   else if (!strcmp(k, "End"))
     {
	_last(ss->slideshow, NULL, NULL);
     }
   else if (!strcmp(k, "Left"))
     {
	_previous(ss->slideshow, NULL, NULL);
     }
   else if (!strcmp(k, "Right"))
     {
	_next(ss->slideshow, NULL, NULL);
     }
}

static void
_main_focused(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Ephoto_Slideshow *ss = data;

   if (ss->ephoto->state == EPHOTO_STATE_SLIDESHOW)
     {
	if (ss->event)
	   elm_object_focus_set(ss->event, EINA_TRUE);
     }
}

static Evas_Object *
_add_icon(Evas_Object *parent, const char *icon, const char *label, Evas_Object *before)
{
   Evas_Object *ic, *but;
   int ret;

   ic = elm_icon_add(parent);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   ret = elm_icon_standard_set(ic, icon);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   but = elm_button_add(parent);
   if (!ret)
     elm_object_text_set(but, label);
   else
     {
        elm_object_part_content_set(but, "icon", ic);
        elm_object_tooltip_text_set(but, label);
        elm_object_tooltip_orient_set(but, ELM_TOOLTIP_ORIENT_TOP);
     }
   if (before)
     elm_box_pack_before(parent, but, before);
   else
     elm_box_pack_end(parent, but);
   evas_object_show(but);

   return but;
}

void
ephoto_slideshow_show_controls(Ephoto *ephoto)
{
   Ephoto_Slideshow *ss = evas_object_data_get(ephoto->slideshow, "slideshow");
   Evas_Object *but;

   elm_layout_content_unset(ephoto->layout, "ephoto.swallow.controls");
   evas_object_hide(ephoto->statusbar);

   ss->notify = elm_box_add(ephoto->win);
   elm_box_horizontal_set(ss->notify, EINA_TRUE);
   evas_object_size_hint_weight_set(ss->notify, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_fill_set(ss->notify, EVAS_HINT_FILL, EVAS_HINT_FILL);

   but = _add_icon(ss->notify, "window-close", _("Back"), NULL);
   evas_object_smart_callback_add(but, "clicked", _back, ss);
   but = _add_icon(ss->notify, "go-first", _("First"), NULL);
   evas_object_smart_callback_add(but, "clicked", _first, ss->slideshow);
   but = _add_icon(ss->notify, "go-previous", _("Previous"), NULL);
   evas_object_smart_callback_add(but, "clicked", _previous, ss->slideshow);
   ss->pause =
       _add_icon(ss->notify, "media-playback-start", _("Play"), NULL);
   evas_object_smart_callback_add(ss->pause, "clicked", _pause, ss);
   ss->pause_after =
       _add_icon(ss->notify, "go-next", _("Next"), NULL);
   evas_object_smart_callback_add(ss->pause_after, "clicked", _next,
       ss->slideshow);
   but = _add_icon(ss->notify, "go-last", _("Last"), NULL);
   evas_object_smart_callback_add(but, "clicked", _last, ss->slideshow);
   ss->fullscreen =
       _add_icon(ss->notify, "view-fullscreen", _("Fullscreen"), NULL);
   evas_object_smart_callback_add(ss->fullscreen, "clicked", _fullscreen, ss);
   ss->fullscreen_after =
       _add_icon(ss->notify, "preferences-system", _("Settings"), NULL);
   evas_object_smart_callback_add(ss->fullscreen_after, "clicked", _settings, ss);

   elm_layout_content_set(ephoto->layout, "ephoto.swallow.controls", ss->notify);
}

Evas_Object *
ephoto_slideshow_add(Ephoto *ephoto, Evas_Object *parent)
{
   Evas_Object *slideshow = elm_slideshow_add(parent);
   Ephoto_Slideshow *ss;

   EINA_SAFETY_ON_NULL_RETURN_VAL(slideshow, NULL);

   ss = calloc(1, sizeof(Ephoto_Slideshow));
   EINA_SAFETY_ON_NULL_GOTO(ss, error);
   ss->ephoto = ephoto;
   ss->slideshow = slideshow;
   ss->playing = 0;
   ss->event = NULL;

   evas_object_event_callback_add(slideshow, EVAS_CALLBACK_DEL, _slideshow_del,
       ss);
   evas_object_event_callback_add(slideshow, EVAS_CALLBACK_MOUSE_DOWN,
       _mouse_down, ss);
   evas_object_event_callback_add(ss->ephoto->win, EVAS_CALLBACK_FOCUS_IN,
       _main_focused, ss);
   evas_object_data_set(slideshow, "slideshow", ss);
   elm_object_tree_focus_allow_set(slideshow, EINA_FALSE);
   elm_slideshow_loop_set(slideshow, EINA_TRUE);
   elm_slideshow_layout_set(slideshow, "fullscreen");
   evas_object_size_hint_weight_set(slideshow, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(slideshow, EVAS_HINT_FILL, EVAS_HINT_FILL);

   evas_object_freeze_events_set(ss->slideshow, EINA_TRUE);

   return ss->slideshow;

  error:
   evas_object_del(slideshow);
   return NULL;
}

static void
_image_shown(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Ephoto_Entry *entry = data;

   ephoto_title_set(entry->ephoto, entry->basename);
}

static Evas_Object *
_slideshow_item_get(void *data, Evas_Object *obj)
{
   Ephoto_Entry *entry = data;

   const char *group = NULL;
   const char *ext = strrchr(entry->path, '.');

   if (ext)
     {
	ext++;
	if ((strcasecmp(ext, "edj") == 0))
	  {
	     if (edje_file_group_exists(entry->path, "e/desktop/background"))
		group = "e/desktop/background";
	     else
	       {
		  Eina_List *g = edje_file_collection_list(entry->path);

		  group = eina_list_data_get(g);
		  edje_file_collection_list_free(g);
	       }
	  }
     }
   Evas_Object *image = elm_image_add(obj);

   elm_image_file_set(image, entry->path, group);
   elm_object_style_set(image, "shadow");
   evas_object_event_callback_add(image, EVAS_CALLBACK_SHOW, _image_shown,
       entry);

   return image;
}

static const Elm_Slideshow_Item_Class _item_cls =
    { {_slideshow_item_get, NULL} };

void
ephoto_slideshow_entries_set(Evas_Object *obj, Eina_List *entries)
{
   Ephoto_Slideshow *ss = evas_object_data_get(obj, "slideshow");

   if (entries)
     ss->entries = entries;
}

void
ephoto_slideshow_entry_set(Evas_Object *obj, Ephoto_Entry *entry)
{
   Ephoto_Slideshow *ss = evas_object_data_get(obj, "slideshow");
   Ephoto_Entry *itr;
   const Eina_List *l;

   if (ss->entry)
      ephoto_entry_free_listener_del(ss->entry, _entry_free, ss);

   ss->entry = entry;

   if (entry)
      ephoto_entry_free_listener_add(entry, _entry_free, ss);

   elm_slideshow_loop_set(ss->slideshow, EINA_TRUE);
   elm_slideshow_transition_set(ss->slideshow,
       ss->ephoto->config->slideshow_transition);
   elm_slideshow_timeout_set(ss->slideshow,
       ss->ephoto->config->slideshow_timeout);
   elm_slideshow_clear(ss->slideshow);
   if (!entry)
      return;

   evas_object_freeze_events_set(ss->slideshow, EINA_FALSE);

   if (ss->pause)
     {
	evas_object_del(ss->pause);
	ss->pause =
           _add_icon(ss->notify, "media-playback-pause", _("Pause"),
           ss->pause_after);
        evas_object_smart_callback_add(ss->pause, "clicked", _pause, ss);
	ss->playing = 1;
     }
   if (ss->fullscreen)
     {
	evas_object_del(ss->fullscreen);
	if (elm_win_fullscreen_get(ss->ephoto->win))
	  {
             ss->fullscreen =
                 _add_icon(ss->notify, "view-restore", _("Normal"),
                 ss->fullscreen_after);
             evas_object_smart_callback_add(ss->fullscreen, "clicked",
                 _fullscreen, ss);
	  }
        else
	  {
             ss->fullscreen =
                 _add_icon(ss->notify, "view-fullscreen", _("Fullscreen"),
                 ss->fullscreen_after);
             evas_object_smart_callback_add(ss->fullscreen, "clicked",
                 _fullscreen, ss);
	  }
     }

   EINA_LIST_FOREACH(ss->entries, l, itr)
     {
        Elm_Object_Item *slideshow_item;

        slideshow_item = elm_slideshow_item_add(ss->slideshow,
            &_item_cls, itr);
        if (itr == entry)
	  elm_slideshow_item_show(slideshow_item);
     }
   if (ss->event)
     {
	evas_object_del(ss->event);
	ss->event = NULL;
     }
   ss->event = evas_object_rectangle_add(ss->ephoto->win);
   evas_object_smart_member_add(ss->event, ss->ephoto->win);
   evas_object_color_set(ss->event, 0, 0, 0, 0);
   evas_object_repeat_events_set(ss->event, EINA_TRUE);
   evas_object_show(ss->event);
   evas_object_event_callback_add(ss->event, EVAS_CALLBACK_KEY_DOWN, _key_down,
       ss);
   evas_object_raise(ss->event);
   elm_object_focus_set(ss->event, EINA_TRUE);
}
