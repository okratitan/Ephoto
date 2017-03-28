#include "ephoto.h"

typedef enum _Ephoto_Slideshow_Move Ephoto_Slideshow_Move;
typedef struct _Ephoto_Slideshow Ephoto_Slideshow;

enum _Ephoto_Slideshow_Move
{
   EPHOTO_SLIDESHOW_MOVE_LEFT_TO_RIGHT,
   EPHOTO_SLIDESHOW_MOVE_RIGHT_TO_LEFT,
   EPHOTO_SLIDESHOW_MOVE_TOP_TO_BOTTOM,
   EPHOTO_SLIDESHOW_MOVE_BOTTOM_TO_TOP,
   EPHOTO_SLIDESHOW_MOVE_LEFT_IN,
   EPHOTO_SLIDESHOW_MOVE_RIGHT_IN,
   EPHOTO_SLIDESHOW_MOVE_TOP_IN,
   EPHOTO_SLIDESHOW_MOVE_BOTTOM_IN,
   EPHOTO_SLIDESHOW_MOVE_LEFT_OUT,
   EPHOTO_SLIDESHOW_MOVE_RIGHT_OUT,
   EPHOTO_SLIDESHOW_MOVE_TOP_OUT,
   EPHOTO_SLIDESHOW_MOVE_BOTTOM_OUT,
   EPHOTO_SLIDESHOW_MOVE_LEFT_TOP_CORNER,
   EPHOTO_SLIDESHOW_MOVE_RIGHT_TOP_CORNER,
   EPHOTO_SLIDESHOW_MOVE_RIGHT_BOTTOM_CORNER,
   EPHOTO_SLIDESHOW_MOVE_LEFT_BOTTOM_CORNER,
   EPHOTO_SLIDESHOW_MOVE_LEFT_TOP_CORNER_IN,
   EPHOTO_SLIDESHOW_MOVE_RIGHT_TOP_CORNER_IN,
   EPHOTO_SLIDESHOW_MOVE_RIGHT_BOTTOM_CORNER_IN,
   EPHOTO_SLIDESHOW_MOVE_LEFT_BOTTOM_CORNER_IN,
   EPHOTO_SLIDESHOW_MOVE_LEFT_TOP_CORNER_OUT,
   EPHOTO_SLIDESHOW_MOVE_RIGHT_TOP_CORNER_OUT,
   EPHOTO_SLIDESHOW_MOVE_RIGHT_BOTTOM_CORNER_OUT,
   EPHOTO_SLIDESHOW_MOVE_LEFT_BOTTOM_CORNER_OUT
};

struct _Ephoto_Slideshow
{
   Ephoto *ephoto;
   Evas_Object *current_item;
   Evas_Object *old_item;
   Evas_Object *slideshow;
   Evas_Object *event;
   Evas_Object *notify;
   Evas_Object *notify_box;
   Eina_List *entries;
   Evas_Object *pause;
   Evas_Object *pause_after;
   Evas_Object *fullscreen;
   Evas_Object *fullscreen_after;
   Evas_Object *exit;
   Ephoto_Entry *entry;
   Eina_Bool playing;
   Eina_Bool timer_end;
   Ecore_Timer *timer;
   Ephoto_Slideshow_Move move;
   float timeout;
   int current;
};

static Evas_Object *_slideshow_item_get(Ephoto_Slideshow *ss,
    Ephoto_Entry *entry, Evas_Object *parent);
static Eina_Bool _slideshow_transition(void *data);
static void _slideshow_play(Ephoto_Slideshow *ss);
static void _slideshow_pause(Ephoto_Slideshow *ss);
static Evas_Object *_add_icon(Evas_Object *parent, const char *icon,
    const char *label, Evas_Object *before);

static void
_image_shown(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Ephoto_Entry *entry = data;

   ephoto_title_set(entry->ephoto, entry->basename);
}

static const char *
_slideshow_move_end_get(Ephoto_Slideshow *ss)
{
   switch (ss->move)
     {
        case EPHOTO_SLIDESHOW_MOVE_LEFT_TO_RIGHT:
          return "ephoto,slideshow,move,left,to,right";
        case EPHOTO_SLIDESHOW_MOVE_RIGHT_TO_LEFT:
          return "ephoto,slideshow,move,right,to,left";
        case EPHOTO_SLIDESHOW_MOVE_TOP_TO_BOTTOM:
          return "ephoto,slideshow,move,top,to,bottom";
        case EPHOTO_SLIDESHOW_MOVE_BOTTOM_TO_TOP:
          return "ephoto,slideshow,move,bottom,to,top";
        case EPHOTO_SLIDESHOW_MOVE_LEFT_IN:
          return "ephoto,slideshow,move,left,in";
        case EPHOTO_SLIDESHOW_MOVE_RIGHT_IN:
          return "ephoto,slideshow,move,right,in";
        case EPHOTO_SLIDESHOW_MOVE_TOP_IN:
          return "ephoto,slideshow,move,top,in";
        case EPHOTO_SLIDESHOW_MOVE_BOTTOM_IN:
          return "ephoto,slideshow,move,bottom,in";
        case EPHOTO_SLIDESHOW_MOVE_LEFT_OUT:
          return "ephoto,slideshow,move,left,out";
        case EPHOTO_SLIDESHOW_MOVE_RIGHT_OUT:
          return "ephoto,slideshow,move,right,out";
        case EPHOTO_SLIDESHOW_MOVE_TOP_OUT:
          return "ephoto,slideshow,move,top,out";
        case EPHOTO_SLIDESHOW_MOVE_BOTTOM_OUT:
          return "ephoto,slideshow,move,bottom,out";
        case EPHOTO_SLIDESHOW_MOVE_LEFT_TOP_CORNER:
          return "ephoto,slideshow,move,left,top,corner";
        case EPHOTO_SLIDESHOW_MOVE_RIGHT_TOP_CORNER:
          return "ephoto,slideshow,move,right,top,corner";
        case EPHOTO_SLIDESHOW_MOVE_RIGHT_BOTTOM_CORNER:
          return "ephoto,slideshow,move,right,bottom,corner";
        case EPHOTO_SLIDESHOW_MOVE_LEFT_BOTTOM_CORNER:
          return "ephoto,slideshow,move,left,bottom,corner";
        case EPHOTO_SLIDESHOW_MOVE_LEFT_TOP_CORNER_IN:
          return "ephoto,slideshow,move,left,top,corner,in";
        case EPHOTO_SLIDESHOW_MOVE_RIGHT_TOP_CORNER_IN:
          return "ephoto,slideshow,move,right,top,corner,in";
        case EPHOTO_SLIDESHOW_MOVE_RIGHT_BOTTOM_CORNER_IN:
          return "ephoto,slideshow,move,right,bottom,corner,in";
        case EPHOTO_SLIDESHOW_MOVE_LEFT_BOTTOM_CORNER_IN:
          return "ephoto,slideshow,move,left,bottom,corner,in";
        case EPHOTO_SLIDESHOW_MOVE_LEFT_TOP_CORNER_OUT:
          return "ephoto,slideshow,move,left,top,corner,out";
        case EPHOTO_SLIDESHOW_MOVE_RIGHT_TOP_CORNER_OUT:
          return "ephoto,slideshow,move,right,top,corner,out";
        case EPHOTO_SLIDESHOW_MOVE_RIGHT_BOTTOM_CORNER_OUT:
          return "ephoto,slideshow,move,right,bottom,corner,out";
        case EPHOTO_SLIDESHOW_MOVE_LEFT_BOTTOM_CORNER_OUT:
          return "ephoto,slideshow,move,left,bottom,corner,out";
        default: return "default";
     }
}

static const char *
_slideshow_move_start_get(Ephoto_Slideshow *ss)
{
   switch (ss->move)
     {
        case EPHOTO_SLIDESHOW_MOVE_LEFT_TO_RIGHT:
          return "ephoto,slideshow,default,left,to,right";
        case EPHOTO_SLIDESHOW_MOVE_RIGHT_TO_LEFT:
          return "ephoto,slideshow,default,right,to,left";
        case EPHOTO_SLIDESHOW_MOVE_TOP_TO_BOTTOM:
          return "ephoto,slideshow,default,top,to,bottom";
        case EPHOTO_SLIDESHOW_MOVE_BOTTOM_TO_TOP:
          return "ephoto,slideshow,default,bottom,to,top";
        case EPHOTO_SLIDESHOW_MOVE_LEFT_IN:
          return "ephoto,slideshow,default,left,in";
        case EPHOTO_SLIDESHOW_MOVE_RIGHT_IN:
          return "ephoto,slideshow,default,right,in";
        case EPHOTO_SLIDESHOW_MOVE_TOP_IN:
          return "ephoto,slideshow,default,top,in";
        case EPHOTO_SLIDESHOW_MOVE_BOTTOM_IN:
          return "ephoto,slideshow,default,bottom,in";
        case EPHOTO_SLIDESHOW_MOVE_LEFT_OUT:
          return "ephoto,slideshow,default,left,out";
        case EPHOTO_SLIDESHOW_MOVE_RIGHT_OUT:
          return "ephoto,slideshow,default,right,out";
        case EPHOTO_SLIDESHOW_MOVE_TOP_OUT:
          return "ephoto,slideshow,default,top,out";
        case EPHOTO_SLIDESHOW_MOVE_BOTTOM_OUT:
          return "ephoto,slideshow,default,bottom,out";
        case EPHOTO_SLIDESHOW_MOVE_LEFT_TOP_CORNER:
          return "ephoto,slideshow,default,left,top,corner";
        case EPHOTO_SLIDESHOW_MOVE_RIGHT_TOP_CORNER:
          return "ephoto,slideshow,default,right,top,corner";
        case EPHOTO_SLIDESHOW_MOVE_RIGHT_BOTTOM_CORNER:
          return "ephoto,slideshow,default,right,bottom,corner";
        case EPHOTO_SLIDESHOW_MOVE_LEFT_BOTTOM_CORNER:
          return "ephoto,slideshow,default,left,bottom,corner";
        case EPHOTO_SLIDESHOW_MOVE_LEFT_TOP_CORNER_IN:
          return "ephoto,slideshow,default,left,top,corner,in";
        case EPHOTO_SLIDESHOW_MOVE_RIGHT_TOP_CORNER_IN:
          return "ephoto,slideshow,default,right,top,corner,in";
        case EPHOTO_SLIDESHOW_MOVE_RIGHT_BOTTOM_CORNER_IN:
          return "ephoto,slideshow,default,right,bottom,corner,in";
        case EPHOTO_SLIDESHOW_MOVE_LEFT_BOTTOM_CORNER_IN:
          return "ephoto,slideshow,default,left,bottom,corner,in";
        case EPHOTO_SLIDESHOW_MOVE_LEFT_TOP_CORNER_OUT:
          return "ephoto,slideshow,default,left,top,corner,out";
        case EPHOTO_SLIDESHOW_MOVE_RIGHT_TOP_CORNER_OUT:
          return "ephoto,slideshow,default,right,top,corner,out";
        case EPHOTO_SLIDESHOW_MOVE_RIGHT_BOTTOM_CORNER_OUT:
          return "ephoto,slideshow,default,right,bottom,corner,out";
        case EPHOTO_SLIDESHOW_MOVE_LEFT_BOTTOM_CORNER_OUT:
          return "ephoto,slideshow,default,left,bottom,corner,out";
        default: return "default";
     }
}

static void
_slideshow_move_randomize(Ephoto_Slideshow *ss)
{
   int i, r = 0;
   int range = 24;
   int buckets = RAND_MAX / range;
   int limit = buckets * range;

   r = rand();
   while (r >= limit)
     {
        r = rand();
     }
   i = r / buckets;

   switch (i)
     {
        case 0:
          ss->move = EPHOTO_SLIDESHOW_MOVE_LEFT_TO_RIGHT;
          break;
        case 1:
          ss->move = EPHOTO_SLIDESHOW_MOVE_RIGHT_TO_LEFT;
          break;
        case 2:
          ss->move = EPHOTO_SLIDESHOW_MOVE_TOP_TO_BOTTOM;
          break;
        case 3:
          ss->move = EPHOTO_SLIDESHOW_MOVE_BOTTOM_TO_TOP;
          break;
        case 4:
          ss->move = EPHOTO_SLIDESHOW_MOVE_LEFT_IN;
          break;
        case 5:
          ss->move = EPHOTO_SLIDESHOW_MOVE_RIGHT_IN;
          break;
        case 6:
          ss->move = EPHOTO_SLIDESHOW_MOVE_TOP_IN;
          break;
        case 7:
          ss->move = EPHOTO_SLIDESHOW_MOVE_BOTTOM_IN;
          break;
        case 8:
          ss->move = EPHOTO_SLIDESHOW_MOVE_LEFT_OUT;
          break;
        case 9:
          ss->move = EPHOTO_SLIDESHOW_MOVE_RIGHT_OUT;
          break;
        case 10:
          ss->move = EPHOTO_SLIDESHOW_MOVE_TOP_OUT;
          break;
        case 11:
          ss->move = EPHOTO_SLIDESHOW_MOVE_BOTTOM_OUT;
          break;
        case 12:
          ss->move = EPHOTO_SLIDESHOW_MOVE_LEFT_TOP_CORNER;
          break;
        case 13:
          ss->move = EPHOTO_SLIDESHOW_MOVE_RIGHT_TOP_CORNER;
          break;
        case 14:
          ss->move = EPHOTO_SLIDESHOW_MOVE_RIGHT_BOTTOM_CORNER;
          break;
        case 15:
          ss->move = EPHOTO_SLIDESHOW_MOVE_LEFT_BOTTOM_CORNER;
          break;
        case 16:
          ss->move = EPHOTO_SLIDESHOW_MOVE_LEFT_TOP_CORNER_IN;
          break;
        case 17:
          ss->move = EPHOTO_SLIDESHOW_MOVE_RIGHT_TOP_CORNER_IN;
          break;
        case 18:
          ss->move = EPHOTO_SLIDESHOW_MOVE_RIGHT_BOTTOM_CORNER_IN;
          break;
        case 19:
          ss->move = EPHOTO_SLIDESHOW_MOVE_LEFT_BOTTOM_CORNER_IN;
          break;
        case 20:
          ss->move = EPHOTO_SLIDESHOW_MOVE_LEFT_TOP_CORNER_OUT;
          break;
        case 21:
          ss->move = EPHOTO_SLIDESHOW_MOVE_RIGHT_TOP_CORNER_OUT;
          break;
        case 22:
          ss->move = EPHOTO_SLIDESHOW_MOVE_RIGHT_BOTTOM_CORNER_OUT;
          break;
        case 23:
          ss->move = EPHOTO_SLIDESHOW_MOVE_LEFT_BOTTOM_CORNER_OUT;
          break;
        default: ss->move = EPHOTO_SLIDESHOW_MOVE_LEFT_TO_RIGHT;
     }
}

static void
_on_transition_raise(void *data, Evas_Object *obj EINA_UNUSED,
    const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Ephoto_Slideshow *ss = data;

   evas_object_raise(ss->current_item);
}

static void
_on_transition_end(void *data, Evas_Object *obj EINA_UNUSED,
    const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Ephoto_Slideshow *ss = data;

   if (ss->old_item)
     {
        elm_layout_content_unset(ss->slideshow, "ephoto.swallow.slideshow.item");
        evas_object_del(ss->old_item);
        ss->old_item = NULL;
     }
   if (ss->current_item)
     {
        elm_layout_content_unset(ss->slideshow, "ephoto.swallow.slideshow.item2");
     }
   elm_layout_content_set(ss->slideshow, "ephoto.swallow.slideshow.item",
       ss->current_item);
   evas_object_raise(ss->current_item);
   evas_object_show(ss->current_item);
   elm_layout_signal_emit(ss->slideshow, "ephoto,transition,done", "ephoto");

   if (ss->timer)
     ecore_timer_del(ss->timer);
   ss->timer = NULL;
   if (ss->playing)
     ss->timer = ecore_timer_add(ss->timeout, _slideshow_transition, ss);
}

static Evas_Object *
_slideshow_item_get(Ephoto_Slideshow *ss, Ephoto_Entry *entry, Evas_Object *parent)
{
   const char *group = NULL;
   const char *ext = strrchr(entry->path, '.');
   Evas_Coord w, h, sw, sh;
   Evas_Object *layout, *image;
   Edje_Message_Float_Set *msg;

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
   layout = elm_layout_add(parent);
   elm_layout_file_set(layout, PACKAGE_DATA_DIR "/themes/ephoto.edj",
       "ephoto,slideshow,item");
   EPHOTO_EXPAND(layout);
   EPHOTO_FILL(layout);
   evas_object_data_set(layout, "entry", entry);

   image = elm_image_add(parent);
   elm_image_preload_disabled_set(image, EINA_FALSE);
   elm_image_smooth_set(image, EINA_FALSE);
   elm_image_file_set(image, entry->path, group);
   elm_image_fill_outside_set(image, EINA_TRUE);
   EPHOTO_EXPAND(image);
   EPHOTO_FILL(image);
   evas_object_event_callback_add(image, EVAS_CALLBACK_SHOW, _image_shown,
       entry);
   evas_object_image_size_get(elm_image_object_get(image), &w, &h);
   evas_object_geometry_get(parent, 0, 0, &sw, &sh);
   if (w < sw && h < sh)
     {
        evas_object_size_hint_max_set(image, w, h);
        elm_image_fill_outside_set(image, EINA_FALSE);
     }
   elm_layout_content_set(layout, "ephoto.swallow.slideshow.item", image);

   msg = alloca(sizeof(Edje_Message_Float_Set) + (1 * sizeof(float)));
   msg->count = 1;
   msg->val[0] = (float)ss->timeout;
   edje_object_message_send(elm_layout_edje_get(layout),
       EDJE_MESSAGE_FLOAT_SET, 1, msg);

   return layout;
}

static Eina_Bool
_slideshow_transition(void *data)
{
   Ephoto_Slideshow *ss = data;
   char buf[PATH_MAX];

   if (ss->timer_end)
     ss->current += 1;
   else
     ss->timer_end = EINA_TRUE;
   if (!eina_list_nth(ss->entries, ss->current))
     ss->current = 0;
   if (ss->old_item)
     evas_object_del(ss->old_item);

   ss->old_item = ss->current_item;
   ss->current_item = _slideshow_item_get(ss, eina_list_nth(ss->entries, ss->current),
       ss->slideshow);
   elm_layout_content_set(ss->slideshow, "ephoto.swallow.slideshow.item2",
       ss->current_item);
   evas_object_show(ss->current_item);

   snprintf(buf, PATH_MAX, "ephoto,%s", ss->ephoto->config->slideshow_transition);
   elm_layout_signal_emit(ss->slideshow, buf, "ephoto");
   if (ss->ephoto->config->movess)
     {
        elm_layout_signal_emit(ss->current_item, _slideshow_move_start_get(ss), "ephoto");
        elm_layout_signal_emit(ss->current_item, _slideshow_move_end_get(ss), "ephoto");
        _slideshow_move_randomize(ss);
     }
   if (ss->timer)
     ecore_timer_del(ss->timer);
   ss->timer = NULL;

   return EINA_FALSE;
}

static void
_slideshow_play(Ephoto_Slideshow *ss)
{
   Edje_Message_Float_Set *msg;

   if (!ss->current_item)
     {
        if (!eina_list_nth(ss->entries, ss->current))
          ss->current = 0;
        ss->current_item = _slideshow_item_get(ss, eina_list_nth(ss->entries, ss->current),
            ss->slideshow);
        elm_layout_content_set(ss->slideshow, "ephoto.swallow.slideshow.item",
            ss->current_item);
        evas_object_raise(ss->current_item);
        evas_object_show(ss->current_item);
     }
   _slideshow_move_randomize(ss);

   msg = alloca(sizeof(Edje_Message_Float_Set) + (1 * sizeof(float)));
   msg->count = 1;
   msg->val[0] = (float)ss->timeout;
   edje_object_message_send(elm_layout_edje_get(ss->current_item),
       EDJE_MESSAGE_FLOAT_SET, 1, msg);

   if (ss->ephoto->config->movess)
     {
        elm_layout_signal_emit(ss->current_item, _slideshow_move_start_get(ss), "ephoto");
        elm_layout_signal_emit(ss->current_item, _slideshow_move_end_get(ss), "ephoto");
        _slideshow_move_randomize(ss);
     }

   if (ss->timer)
     ecore_timer_del(ss->timer);
   ss->timer = ecore_timer_add(ss->timeout, _slideshow_transition, ss);
}

static void
_slideshow_pause(Ephoto_Slideshow *ss)
{
   if (ss->timer)
     ss->timer = ecore_timer_del(ss->timer);
   ss->timer = NULL;
}

static void
_mouse_down(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Slideshow *ss = data;
   Ephoto_Entry *entry;

   if (elm_win_fullscreen_get(ss->ephoto->win))
     {
        elm_box_pack_end(ss->ephoto->statusbar, ss->ephoto->exit);
        evas_object_show(ss->ephoto->exit);
     }
   else
     {
        elm_box_unpack(ss->ephoto->statusbar, ss->ephoto->exit);
        evas_object_hide(ss->ephoto->exit);
     }
   if (ss->current_item)
      entry = evas_object_data_get(ss->current_item, "entry");
   else
      entry = ss->entry;
   if (ss->event)
     {
	evas_object_del(ss->event);
	ss->event = NULL;
     }
   evas_object_del(ss->notify_box);
   evas_object_del(ss->notify);
   ss->notify_box = NULL;
   ss->notify = NULL;
   evas_object_smart_callback_call(ss->slideshow, "back", entry);
   if (ss->old_item)
     {
        elm_layout_content_unset(ss->slideshow, "ephoto.swallow.slideshow.item2");
        evas_object_del(ss->old_item);
     }
   if (ss->current_item)
     {
        elm_layout_content_unset(ss->slideshow, "ephoto.swallow.slideshow.item");
        evas_object_del(ss->current_item);
     }
   ss->old_item = NULL;
   ss->current_item = NULL;
   if (ss->timer)
     ecore_timer_del(ss->timer);
   ss->timer = NULL;
   ss->current = 0;
   ss->playing = 0;
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
   Ephoto_Entry *entry;

   if (elm_win_fullscreen_get(ss->ephoto->win))
     {
        elm_box_pack_end(ss->ephoto->statusbar, ss->ephoto->exit);
        evas_object_show(ss->ephoto->exit);
     }
   else
     {
        elm_box_unpack(ss->ephoto->statusbar, ss->ephoto->exit);
        evas_object_hide(ss->ephoto->exit);
     }
   if (ss->current_item)
      entry = evas_object_data_get(ss->current_item, "entry");
   else
      entry = ss->entry;
   if (ss->event)
     {
	evas_object_del(ss->event);
	ss->event = NULL;
     }
   evas_object_del(ss->notify_box);
   evas_object_del(ss->notify);
   ss->notify_box = NULL;
   ss->notify = NULL;
   evas_object_smart_callback_call(ss->slideshow, "back", entry);
   if (ss->old_item)
     {
        elm_layout_content_unset(ss->slideshow, "ephoto.swallow.slideshow.item2");
        evas_object_del(ss->old_item);
     }
   if (ss->current_item)
     {
        elm_layout_content_unset(ss->slideshow, "ephoto.swallow.slideshow.item");
        evas_object_del(ss->current_item);
     }
   ss->old_item = NULL;
   ss->current_item = NULL;
   if (ss->timer)
     ecore_timer_del(ss->timer);
   ss->timer = NULL;
   ss->current = 0;
   ss->playing = 0;
}

static void
_first(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Slideshow *ss = data;

   if (ss->timer)
     {
        ecore_timer_del(ss->timer);
        ss->timer = NULL;
     }
   ss->current = 0;
   ss->timer_end = EINA_FALSE;
   _slideshow_transition(ss);
}

static void
_next(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Slideshow *ss = data;

   if (ss->timer)
     {
        ecore_timer_del(ss->timer);
        ss->timer = NULL;
     }
   ss->current += 1;
   ss->timer_end = EINA_FALSE;
   _slideshow_transition(ss);
}

static void
_pause(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Slideshow *ss = data;

   evas_object_del(ss->pause);

   if (ss->playing)
     {
        _slideshow_pause(ss);
	ss->pause =
            _add_icon(ss->notify_box, "media-playback-start", _("Play"),
                ss->pause_after);
        evas_object_smart_callback_add(ss->pause, "clicked", _pause, ss);
	ss->playing = 0;
     }
   else
     {
        _slideshow_play(ss);
        ss->pause =
            _add_icon(ss->notify_box, "media-playback-pause", _("Pause"),
                ss->pause_after);
        evas_object_smart_callback_add(ss->pause, "clicked", _pause, ss);
	ss->playing = 1;
     }
}

static void
_previous(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Slideshow *ss = data;

   if (ss->timer)
     {
        ecore_timer_del(ss->timer);
        ss->timer = NULL;
     }
   ss->current -= 1;
   ss->timer_end = EINA_FALSE;
   _slideshow_transition(ss);
}

static void
_last(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Slideshow *ss = data;

   if (ss->timer)
     {
        ecore_timer_del(ss->timer);
        ss->timer = NULL;
     }
   ss->current = eina_list_count(ss->entries) - 1;
   ss->timer_end = EINA_FALSE;
   _slideshow_transition(ss);
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
            _add_icon(ss->notify_box, "view-fullscreen", _("Fullscreen"),
            ss->fullscreen_after);
        evas_object_smart_callback_add(ss->fullscreen, "clicked",
            _fullscreen, ss);
        elm_win_fullscreen_set(ss->ephoto->win, EINA_FALSE);
        elm_box_unpack(ss->notify_box, ss->exit);
        evas_object_hide(ss->exit);
     }
   else
     {
	ss->fullscreen =
            _add_icon(ss->notify_box, "view-restore", _("Normal"),
            ss->fullscreen_after);
        evas_object_smart_callback_add(ss->fullscreen, "clicked",
            _fullscreen, ss);
        elm_win_fullscreen_set(ss->ephoto->win, EINA_TRUE);
        elm_box_pack_end(ss->notify_box, ss->exit);
        evas_object_show(ss->exit);
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
	_fullscreen(ss, NULL, NULL);
     }
   else if (!strcmp(k, "space"))
     {
	_pause(ss, NULL, NULL);
     }
   else if (!strcmp(k, "Home"))
     {
	_first(ss, NULL, NULL);
     }
   else if (!strcmp(k, "End"))
     {
	_last(ss, NULL, NULL);
     }
   else if (!strcmp(k, "Left"))
     {
	_previous(ss, NULL, NULL);
     }
   else if (!strcmp(k, "Right"))
     {
	_next(ss, NULL, NULL);
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
   ret = elm_icon_standard_set(ic, icon);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);

   but = elm_button_add(parent);
   if (!ret)
     elm_object_text_set(but, label);
   elm_object_part_content_set(but, "icon", ic);
   elm_object_tooltip_text_set(but, label);
   elm_object_tooltip_orient_set(but, ELM_TOOLTIP_ORIENT_TOP);

   if (before)
     elm_box_pack_before(parent, but, before);
   else
     elm_box_pack_end(parent, but);
   evas_object_show(but);

   return but;
}

static void
_mouse_in(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Slideshow *ss = data;

   if (ss->notify)
     {
        elm_notify_timeout_set(ss->notify, 0.0);
        evas_object_show(ss->notify);
     }
}

static void
_mouse_out(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Slideshow *ss = data;

   if (ss->notify)
     elm_notify_timeout_set(ss->notify, 3.0);
}

static void
_mouse_move(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Slideshow *ss = data;

   if (ss->notify)
     {
        elm_notify_timeout_set(ss->notify, 3.0);
        evas_object_show(ss->notify);
     }
}

void
ephoto_slideshow_show_controls(Ephoto *ephoto)
{
   Ephoto_Slideshow *ss = evas_object_data_get(ephoto->slideshow, "slideshow");
   Evas_Object *but;

   ss->notify = elm_notify_add(ephoto->win);
   EPHOTO_EXPAND(ss->notify);

   ss->notify_box = elm_box_add(ss->notify);
   elm_box_horizontal_set(ss->notify_box, EINA_TRUE);
   EPHOTO_WEIGHT(ss->notify, EVAS_HINT_EXPAND, 0.0);
   evas_object_event_callback_add(ss->notify_box, EVAS_CALLBACK_MOUSE_IN, _mouse_in,
       ss);
   evas_object_event_callback_add(ss->notify_box, EVAS_CALLBACK_MOUSE_OUT, _mouse_out,
       ss);
   elm_object_content_set(ss->notify, ss->notify_box);
   evas_object_show(ss->notify_box);

   if (ephoto->prev_state == EPHOTO_STATE_SINGLE)
     but = _add_icon(ss->notify_box, "view-list-icons", _("Back"), NULL);
   else
     but = _add_icon(ss->notify_box, "view-list-details", _("Back"), NULL);
   evas_object_smart_callback_add(but, "clicked", _back, ss);
   but = _add_icon(ss->notify_box, "go-first", _("First"), NULL);
   evas_object_smart_callback_add(but, "clicked", _first, ss);
   but = _add_icon(ss->notify_box, "go-previous", _("Previous"), NULL);
   evas_object_smart_callback_add(but, "clicked", _previous, ss);
   ss->pause =
       _add_icon(ss->notify_box, "media-playback-start", _("Play"), NULL);
   evas_object_smart_callback_add(ss->pause, "clicked", _pause, ss);
   ss->pause_after =
       _add_icon(ss->notify_box, "go-next", _("Next"), NULL);
   evas_object_smart_callback_add(ss->pause_after, "clicked", _next,
       ss);
   but = _add_icon(ss->notify_box, "go-last", _("Last"), NULL);
   evas_object_smart_callback_add(but, "clicked", _last, ss);
   ss->fullscreen =
       _add_icon(ss->notify_box, "view-fullscreen", _("Fullscreen"), NULL);
   evas_object_smart_callback_add(ss->fullscreen, "clicked", _fullscreen, ss);
   ss->fullscreen_after =
       _add_icon(ss->notify_box, "preferences-other", _("Settings"), NULL);
   evas_object_smart_callback_add(ss->fullscreen_after, "clicked", _settings, ss);
   ss->exit =
       _add_icon(ss->notify_box, "application-exit", _("Exit"), NULL);
   evas_object_smart_callback_add(ss->exit, "clicked", _fullscreen, ss);

   elm_box_unpack(ss->notify_box, ss->exit);
   evas_object_hide(ss->exit);

   elm_notify_align_set(ss->notify, 0.5, 1.0);
   elm_notify_timeout_set(ss->notify, 3.0);
   evas_object_show(ss->notify);
}

Evas_Object *
ephoto_slideshow_add(Ephoto *ephoto, Evas_Object *parent)
{
   Evas_Object *slideshow = elm_layout_add(parent);
   Ephoto_Slideshow *ss;

   EINA_SAFETY_ON_NULL_RETURN_VAL(slideshow, NULL);

   ss = calloc(1, sizeof(Ephoto_Slideshow));
   EINA_SAFETY_ON_NULL_GOTO(ss, error);
   ss->ephoto = ephoto;
   ss->slideshow = slideshow;
   ss->playing = 0;
   ss->current = 0;
   ss->notify = NULL;
   ss->notify_box = NULL;
   ss->old_item = NULL;
   ss->current_item = NULL;
   ss->event = NULL;
   ss->timer_end = EINA_TRUE;

   elm_layout_file_set(slideshow, PACKAGE_DATA_DIR "/themes/ephoto.edj",
       "ephoto,slideshow,base");
   evas_object_event_callback_add(slideshow, EVAS_CALLBACK_DEL, _slideshow_del,
       ss);
   evas_object_event_callback_add(slideshow, EVAS_CALLBACK_MOUSE_DOWN,
       _mouse_down, ss);
   evas_object_data_set(slideshow, "slideshow", ss);
   EPHOTO_EXPAND(slideshow);
   EPHOTO_FILL(slideshow);
   evas_object_event_callback_add(ss->slideshow, EVAS_CALLBACK_MOUSE_MOVE,
       _mouse_move, ss);
   edje_object_signal_callback_add(elm_layout_edje_get(ss->slideshow),
       "ephoto,transition,raise", "ephoto", _on_transition_raise, ss);
   edje_object_signal_callback_add(elm_layout_edje_get(ss->slideshow),
       "ephoto,transition,end", "ephoto", _on_transition_end, ss);
   return ss->slideshow;

  error:
   evas_object_del(slideshow);
   return NULL;
}

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

   if (!entry)
      return;

   if (ss->entry)
      ephoto_entry_free_listener_del(ss->entry, _entry_free, ss);

   ss->entry = entry;
   ss->current = eina_list_data_idx(ss->entries, ss->entry);

   if (entry)
      ephoto_entry_free_listener_add(entry, _entry_free, ss);

   ss->timeout = ss->ephoto->config->slideshow_timeout;
   _slideshow_play(ss);

   if (ss->pause)
     {
	evas_object_del(ss->pause);
	ss->pause =
           _add_icon(ss->notify_box, "media-playback-pause", _("Pause"),
           ss->pause_after);
        evas_object_smart_callback_add(ss->pause, "clicked", _pause, ss);
	ss->playing = 1;
     }
   if (ss->fullscreen)
     {
        evas_object_del(ss->fullscreen);
        if (!elm_win_fullscreen_get(ss->ephoto->win))
          {
             ss->fullscreen =
                 _add_icon(ss->notify_box, "view-fullscreen", _("Fullscreen"),
                 ss->fullscreen_after);
             evas_object_smart_callback_add(ss->fullscreen, "clicked",
                 _fullscreen, ss);
             elm_box_unpack(ss->notify_box, ss->exit);
             evas_object_hide(ss->exit);
          }
        else
          {
             ss->fullscreen =
                 _add_icon(ss->notify_box, "view-restore", _("Normal"),
                 ss->fullscreen_after);
             evas_object_smart_callback_add(ss->fullscreen, "clicked",
                 _fullscreen, ss);
             elm_box_pack_end(ss->notify_box, ss->exit);
             evas_object_show(ss->exit);
          }
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
