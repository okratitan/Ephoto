#include "ephoto.h"

int EPHOTO_EVENT_ENTRY_CREATE = 0;
int EPHOTO_EVENT_POPULATE_START = 0;
int EPHOTO_EVENT_POPULATE_END = 0;
int EPHOTO_EVENT_POPULATE_ERROR = 0;
int EPHOTO_EVENT_EDITOR_RESET = 0;
int EPHOTO_EVENT_EDITOR_APPLY = 0;
int EPHOTO_EVENT_EDITOR_CANCEL = 0;

typedef struct _Ephoto_Entry_Free_Listener Ephoto_Entry_Free_Listener;
struct _Ephoto_Entry_Free_Listener
{
   void (*cb) (void *data, const Ephoto_Entry *dead);
   const void *data;
};

typedef struct _Ephoto_Dir_Data Ephoto_Dir_Data;
struct _Ephoto_Dir_Data
{
   Ephoto *ephoto;
   Elm_Object_Item *expanded;
   Eina_Bool dirs_only;
   Eina_Bool thumbs_only;
};

static void
_ephoto_state_set(Ephoto *ephoto, Ephoto_State state)
{
   ephoto->prev_state = ephoto->state;
   ephoto->state = state;
}

static void
_ephoto_thumb_browser_show(Ephoto *ephoto, Ephoto_Entry *entry)
{
   ephoto_single_browser_entry_set(ephoto->single_browser, NULL);
   ephoto_slideshow_entry_set(ephoto->slideshow, NULL);
   elm_naviframe_item_promote(ephoto->tb);
   elm_object_focus_set(ephoto->thumb_browser, EINA_TRUE);
   _ephoto_state_set(ephoto, EPHOTO_STATE_THUMB);
   ephoto_title_set(ephoto, ephoto->config->directory);
   ephoto_thumb_browser_update_info_label(ephoto);
   elm_box_clear(ephoto->controls_left);
   elm_box_clear(ephoto->controls_right);
   ephoto->blocking = EINA_FALSE;
   ephoto->menu_blocking = EINA_FALSE;
   ephoto->hover_blocking = EINA_FALSE;
   ephoto->editor_blocking = EINA_FALSE;
   ephoto_thumb_browser_show_controls(ephoto);

   if ((entry) && (entry->item))
     elm_gengrid_item_bring_in(entry->item, ELM_GENGRID_ITEM_SCROLLTO_IN);
}

static void
_ephoto_single_browser_show(Ephoto *ephoto, Ephoto_Entry *entry)
{
   if (ephoto->selentries)
     ephoto_single_browser_entries_set(ephoto->single_browser,
         ephoto->selentries);
   else if (ephoto->searchentries)
     ephoto_single_browser_entries_set(ephoto->single_browser,
         ephoto->searchentries);
   else
     ephoto_single_browser_entries_set(ephoto->single_browser,
         ephoto->entries);

   ephoto_single_browser_entry_set(ephoto->single_browser, entry);
   elm_naviframe_item_simple_promote(ephoto->pager, ephoto->single_browser);
   elm_object_focus_set(ephoto->single_browser, EINA_TRUE);
   _ephoto_state_set(ephoto, EPHOTO_STATE_SINGLE);

   elm_box_clear(ephoto->controls_left);
   elm_box_clear(ephoto->controls_right);
   ephoto->blocking = EINA_FALSE;
   ephoto->menu_blocking = EINA_FALSE;
   ephoto->hover_blocking = EINA_FALSE;
   ephoto->editor_blocking = EINA_FALSE;
   ephoto_single_browser_show_controls(ephoto);
   ephoto_single_browser_adjust_offsets(ephoto);
}

static void
_ephoto_slideshow_show(Ephoto *ephoto, Ephoto_Entry *entry)
{
   ephoto_slideshow_show_controls(ephoto);

   if (ephoto->selentries)
     ephoto_slideshow_entries_set(ephoto->slideshow, ephoto->selentries);
   else if (ephoto->searchentries)
     ephoto_slideshow_entries_set(ephoto->slideshow, ephoto->searchentries);
   else
     ephoto_slideshow_entries_set(ephoto->slideshow, ephoto->entries);
   ephoto_slideshow_entry_set(ephoto->slideshow, entry);
   elm_naviframe_item_simple_promote(ephoto->pager, ephoto->slideshow);
   elm_object_focus_set(ephoto->slideshow, EINA_TRUE);
   _ephoto_state_set(ephoto, EPHOTO_STATE_SLIDESHOW);
   edje_object_signal_emit(elm_layout_edje_get(ephoto->layout),
       "ephoto,controls,hide", "ephoto");
   edje_object_signal_emit(elm_layout_edje_get(ephoto->layout),
       "ephoto,folders,hide", "ephoto");
   ephoto->folders_toggle = EINA_FALSE;
   ephoto->blocking = EINA_FALSE;
   ephoto->menu_blocking = EINA_FALSE;
   ephoto->hover_blocking = EINA_FALSE;
   ephoto->editor_blocking = EINA_FALSE;
}

static void
_ephoto_single_browser_back(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto *ephoto = data;
   Ephoto_Entry *entry = event_info;

   ephoto->selentries = NULL;
   _ephoto_thumb_browser_show(ephoto, entry);
}

static void
_ephoto_slideshow_back(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto *ephoto = data;
   Ephoto_Entry *entry = event_info;

   switch (ephoto->prev_state)
     {
       case EPHOTO_STATE_SINGLE:
	  _ephoto_single_browser_show(ephoto, entry);
	  break;

       case EPHOTO_STATE_THUMB:
          ephoto->selentries = NULL;
	  _ephoto_thumb_browser_show(ephoto, entry);
	  break;

       default:
          ephoto->selentries = NULL;
	  _ephoto_thumb_browser_show(ephoto, entry);
     }
}

static void
_ephoto_thumb_browser_view(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto *ephoto = data;
   Ephoto_Entry *entry = event_info;

   _ephoto_single_browser_show(ephoto, entry);
}

static void
_ephoto_thumb_browser_changed_directory(void *data,
    Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;

   ephoto->selentries = NULL;
   ephoto_single_browser_entry_set(ephoto->single_browser, NULL);
   ephoto_slideshow_entry_set(ephoto->slideshow, NULL);
}

static void
_ephoto_thumb_browser_slideshow(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto *ephoto = data;
   Ephoto_Entry *entry = event_info;

   _ephoto_slideshow_show(ephoto, entry);
}

static void
_ephoto_single_browser_slideshow(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info)
{
   Ephoto *ephoto = data;
   Ephoto_Entry *entry = event_info;

   _ephoto_slideshow_show(ephoto, entry);
}

static void
_win_free(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;
   Ecore_Event_Handler *handler;

   if (ephoto->file_idler)
     ecore_idler_del(ephoto->file_idler);
   if (ephoto->file_idler_pos)
     eina_list_free(ephoto->file_idler_pos);
   if (ephoto->upload_handlers)
     EINA_LIST_FREE(ephoto->upload_handlers, handler)
       ecore_event_handler_del(handler);
   if (ephoto->url_up)
     ecore_con_url_free(ephoto->url_up);
   if (ephoto->url_ret)
     free(ephoto->url_ret);
   if (ephoto->upload_error)
     free(ephoto->upload_error);
   if (ephoto->top_directory)
     eina_stringshare_del(ephoto->top_directory);
   if (ephoto->timer.thumb_regen)
     ecore_timer_del(ephoto->timer.thumb_regen);
   if (ephoto->monitor)
     ecore_file_monitor_del(ephoto->monitor);
   if (ephoto->overlay_timer)
     ecore_timer_del(ephoto->overlay_timer);
   ephoto_config_save(ephoto);
   free(ephoto);
}

static void
_mouse_up_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;
   Evas_Object *but = evas_object_data_get(ephoto->layout, "folder_button");
   Evas_Coord x, y, w, h, bx, by, bw, bh, cx, cy;

   evas_pointer_canvas_xy_get(evas_object_evas_get(ephoto->dir_browser), &cx, &cy);
   evas_object_geometry_get(ephoto->dir_browser, &x, &y, &w, &h);
   evas_object_geometry_get(but, &bx, &by, &bw, &bh);

   if (cx >= x && cx <= x+w)
     {
        if (cy >= y && cy <= y+h)
          return;
     }
   if (cx >= bx && cx <= bx+bw)
     {
        if (cy >= by && cy <= by+bh)
          return;
     }
   edje_object_signal_emit(elm_layout_edje_get(ephoto->layout),
       "ephoto,folders,hide", "ephoto");
   ephoto->folders_toggle = EINA_FALSE;
   elm_object_tooltip_text_set(but, _("Show Folders"));   
}

static void
_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;
   Evas_Coord w, h;

   evas_object_geometry_get(ephoto->win, 0, 0, &w, &h);
   if (w && h)
     {
	ephoto->config->window_width = w;
	ephoto->config->window_height = h;
     }
}

static Eina_Bool
_timer_cb(void *data)
{
   Ephoto *ephoto = data;
   Edje_Object *edje = elm_layout_edje_get(ephoto->layout);

   if (ephoto->blocking || ephoto->menu_blocking ||
       ephoto->right_blocking || ephoto->hover_blocking ||
       ephoto->editor_blocking)
     return ECORE_CALLBACK_PASS_ON;

   edje_object_signal_emit(edje, "ephoto,controls,hide", "ephoto");
   if (ephoto->folders_toggle)
     edje_object_signal_emit(edje, "ephoto,folders,hide", "ephoto");
   ecore_timer_del(ephoto->overlay_timer);
   ephoto->overlay_timer = NULL;

   return ECORE_CALLBACK_CANCEL;
}

static void
_mouse_move_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;
   Edje_Object *edje = elm_layout_edje_get(ephoto->layout);

   if (ephoto->blocking || ephoto->menu_blocking ||
       ephoto->right_blocking || ephoto->hover_blocking ||
       ephoto->editor_blocking)
     return;

   if (ephoto->overlay_timer)
     ecore_timer_del(ephoto->overlay_timer);
   ephoto->overlay_timer = NULL;
   edje_object_signal_emit(edje, "ephoto,controls,show", "ephoto");
   if (ephoto->folders_toggle)
     edje_object_signal_emit(edje, "ephoto,folders,show", "ephoto");
   ephoto->overlay_timer = ecore_timer_add(3.0, _timer_cb, ephoto);
}

static void
_mouse_out_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;
   ephoto->blocking = EINA_FALSE;
}

static void
_mouse_in_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;
   ephoto->blocking = EINA_TRUE;
}

static void
_folder_icon_clicked(void *data, Evas_Object *obj,
    void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;

   if (!ephoto->folders_toggle)
     {
        edje_object_signal_emit(elm_layout_edje_get(ephoto->layout),
            "ephoto,folders,show", "ephoto");
        ephoto->folders_toggle = EINA_TRUE;
        elm_object_tooltip_text_set(obj, _("Hide Folders"));
     }
   else
     {
        edje_object_signal_emit(elm_layout_edje_get(ephoto->layout),
            "ephoto,folders,hide", "ephoto");
        ephoto->folders_toggle = EINA_FALSE;
        elm_object_tooltip_text_set(obj, _("Show Folders"));
     }
}

/*Ephoto Thumb Browser Main Callbacks*/
static void
_slideshow_icon_clicked(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;

   if (ephoto->state == EPHOTO_STATE_THUMB)
     ephoto_thumb_browser_slideshow(ephoto->thumb_browser);
   else if (ephoto->state == EPHOTO_STATE_SINGLE)
     ephoto_single_browser_slideshow(ephoto->single_browser);
}


static void
_settings_icon_clicked(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;

   ephoto_config_main(ephoto);
}

/*Toggle determines whether to toggle folder visibility, or just force visible*/
void
ephoto_show_folders(Ephoto *ephoto, Eina_Bool toggle)
{
   Evas_Object *but = evas_object_data_get(ephoto->layout, "folder_button");
   if (!ephoto->folders_toggle || !toggle)
     {
        _mouse_move_cb(ephoto, NULL, NULL, NULL);
        edje_object_signal_emit(elm_layout_edje_get(ephoto->layout),
            "ephoto,folders,show", "ephoto");
        ephoto->folders_toggle = EINA_TRUE;
        elm_object_tooltip_text_set(but, _("Hide Folders"));
     }
   else if (ephoto->folders_toggle && toggle)
     {
        edje_object_signal_emit(elm_layout_edje_get(ephoto->layout),
            "ephoto,folders,hide", "ephoto");
        ephoto->folders_toggle = EINA_FALSE;
        elm_object_tooltip_text_set(but, _("Show Folders"));
     }
}

Evas_Object *
ephoto_window_add(const char *path)
{
   Ephoto *ephoto = calloc(1, sizeof(Ephoto));
   Evas_Object *ic, *but;
   char buf[PATH_MAX];

   EINA_SAFETY_ON_NULL_RETURN_VAL(ephoto, NULL);

   EPHOTO_EVENT_ENTRY_CREATE = ecore_event_type_new();
   EPHOTO_EVENT_POPULATE_START = ecore_event_type_new();
   EPHOTO_EVENT_POPULATE_END = ecore_event_type_new();
   EPHOTO_EVENT_POPULATE_ERROR = ecore_event_type_new();
   EPHOTO_EVENT_EDITOR_RESET = ecore_event_type_new();
   EPHOTO_EVENT_EDITOR_APPLY = ecore_event_type_new();
   EPHOTO_EVENT_EDITOR_CANCEL = ecore_event_type_new();

   ephoto->selentries = NULL;
   ephoto->blocking = EINA_FALSE;
   ephoto->menu_blocking = EINA_FALSE;
   ephoto->hover_blocking = EINA_FALSE;
   ephoto->folders_toggle = EINA_FALSE;
   ephoto->editor_blocking = EINA_FALSE;
   ephoto->win = elm_win_util_standard_add("ephoto", "Ephoto");
   if (!ephoto->win)
     {
	free(ephoto);
	return NULL;
     }

   evas_object_event_callback_add(ephoto->win, EVAS_CALLBACK_FREE,
       _win_free, ephoto);
   evas_object_event_callback_add(ephoto->win, EVAS_CALLBACK_RESIZE,
       _resize_cb, ephoto);
   elm_win_autodel_set(ephoto->win, EINA_TRUE);

   if (!ephoto_config_init(ephoto))
     {
	evas_object_del(ephoto->win);
	return NULL;
     }

   if ((ephoto->config->thumb_gen_size != 128) &&
       (ephoto->config->thumb_gen_size != 256) &&
       (ephoto->config->thumb_gen_size != 512))
      ephoto_thumb_size_set(ephoto, ephoto->config->thumb_size);

   ephoto->layout = elm_layout_add(ephoto->win);
   elm_layout_file_set(ephoto->layout, PACKAGE_DATA_DIR "/themes/ephoto.edj",
       "ephoto,main,layout");
   evas_object_size_hint_weight_set(ephoto->layout, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(ephoto->layout, EVAS_HINT_FILL,
       EVAS_HINT_FILL);
   evas_object_event_callback_add(ephoto->layout, EVAS_CALLBACK_MOUSE_MOVE,
       _mouse_move_cb, ephoto);
   evas_object_event_callback_add(ephoto->layout, EVAS_CALLBACK_MOUSE_UP,
       _mouse_up_cb, ephoto);
   elm_win_resize_object_add(ephoto->win, ephoto->layout);
   evas_object_show(ephoto->layout);

   ephoto->pager = elm_naviframe_add(ephoto->win);
   elm_object_focus_allow_set(ephoto->pager, EINA_FALSE);
   elm_naviframe_prev_btn_auto_pushed_set(ephoto->pager, EINA_FALSE);
   evas_object_size_hint_weight_set(ephoto->pager, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_fill_set(ephoto->pager, EVAS_HINT_FILL,
       EVAS_HINT_FILL);
   elm_layout_content_set(ephoto->layout, "ephoto.swallow.main", ephoto->pager);
   evas_object_show(ephoto->pager);

   ephoto->thumb_browser = ephoto_thumb_browser_add(ephoto, ephoto->layout);
   if (!ephoto->thumb_browser)
     {
	evas_object_del(ephoto->win);
	return NULL;
     }
   ephoto->tb =
       elm_naviframe_item_push(ephoto->pager, NULL, NULL, NULL,
       ephoto->thumb_browser, "overlap");
   elm_naviframe_item_title_enabled_set(ephoto->tb, EINA_FALSE, EINA_FALSE);
   evas_object_smart_callback_add(ephoto->thumb_browser, "view",
       _ephoto_thumb_browser_view, ephoto);
   evas_object_smart_callback_add(ephoto->thumb_browser, "changed,directory",
       _ephoto_thumb_browser_changed_directory, ephoto);
   evas_object_smart_callback_add(ephoto->thumb_browser, "slideshow",
       _ephoto_thumb_browser_slideshow, ephoto);
   
   ephoto->single_browser = ephoto_single_browser_add(ephoto, ephoto->layout);
   if (!ephoto->single_browser)
     {
	evas_object_del(ephoto->win);
	return NULL;
     }
   ephoto->sb =
       elm_naviframe_item_insert_after(ephoto->pager, ephoto->tb, NULL, NULL,
       NULL, ephoto->single_browser, "overlap");
   elm_naviframe_item_title_enabled_set(ephoto->sb, EINA_FALSE, EINA_FALSE);
   evas_object_smart_callback_add(ephoto->single_browser, "back",
       _ephoto_single_browser_back, ephoto);
   evas_object_smart_callback_add(ephoto->single_browser, "slideshow",
       _ephoto_single_browser_slideshow, ephoto);
   ephoto->slideshow = ephoto_slideshow_add(ephoto, ephoto->layout);
   if (!ephoto->slideshow)
     {
	evas_object_del(ephoto->win);
	return NULL;
     }
   ephoto->sl =
       elm_naviframe_item_insert_after(ephoto->pager, ephoto->sb, NULL, NULL,
       NULL, ephoto->slideshow, "overlap");
   elm_naviframe_item_title_enabled_set(ephoto->sl, EINA_FALSE, EINA_FALSE);
   evas_object_smart_callback_add(ephoto->slideshow, "back",
       _ephoto_slideshow_back, ephoto);

   ephoto->dir_browser = ephoto_directory_browser_add(ephoto, ephoto->layout);
   elm_layout_content_set(ephoto->layout, "ephoto.swallow.folders",
       ephoto->dir_browser);
   evas_object_event_callback_add(ephoto->dir_browser, EVAS_CALLBACK_MOUSE_IN,
       _mouse_in_cb, ephoto);
   evas_object_event_callback_add(ephoto->dir_browser, EVAS_CALLBACK_MOUSE_OUT,
       _mouse_out_cb, ephoto);
   evas_object_show(ephoto->dir_browser);

   ephoto->statusbar = elm_box_add(ephoto->layout);
   elm_object_tree_focus_allow_set(ephoto->statusbar, EINA_FALSE);
   elm_box_horizontal_set(ephoto->statusbar, EINA_TRUE);
   evas_object_size_hint_weight_set(ephoto->statusbar,
       EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(ephoto->statusbar, EVAS_HINT_FILL,
       EVAS_HINT_FILL);
   elm_layout_content_set(ephoto->layout, "ephoto.swallow.controls",
       ephoto->statusbar);
   evas_object_event_callback_add(ephoto->statusbar, EVAS_CALLBACK_MOUSE_IN,
       _mouse_in_cb, ephoto);
   evas_object_event_callback_add(ephoto->statusbar, EVAS_CALLBACK_MOUSE_OUT,
       _mouse_out_cb, ephoto);
   evas_object_show(ephoto->statusbar);

   ic = elm_icon_add(ephoto->statusbar);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   elm_icon_standard_set(ic, "folder");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   but = elm_button_add(ephoto->statusbar);
   elm_object_part_content_set(but, "icon", ic);
   evas_object_smart_callback_add(but, "clicked", _folder_icon_clicked, ephoto);
   elm_object_tooltip_text_set(but, _("Show Folders"));
   elm_object_tooltip_orient_set(but, ELM_TOOLTIP_ORIENT_RIGHT);
   elm_box_pack_end(ephoto->statusbar, but);
   evas_object_show(but);
   evas_object_data_set(ephoto->layout, "folder_button", but);

   ephoto->controls_left = elm_box_add(ephoto->statusbar);
   elm_box_horizontal_set(ephoto->controls_left, EINA_TRUE);
   evas_object_size_hint_weight_set(ephoto->controls_left,
       0.0, 0.0);
   evas_object_size_hint_align_set(ephoto->controls_left, EVAS_HINT_FILL,
       EVAS_HINT_FILL);
   elm_box_pack_end(ephoto->statusbar, ephoto->controls_left);
   evas_object_show(ephoto->controls_left);

   ephoto->infolabel = elm_label_add(ephoto->statusbar);
   elm_object_style_set(ephoto->infolabel, "info");
   elm_label_line_wrap_set(ephoto->infolabel, ELM_WRAP_MIXED);
   elm_object_text_set(ephoto->infolabel, _("Information"));
   evas_object_size_hint_weight_set(ephoto->infolabel, 
       EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(ephoto->infolabel, EVAS_HINT_FILL,
       EVAS_HINT_FILL);
   evas_object_size_hint_aspect_set(ephoto->infolabel, EVAS_ASPECT_CONTROL_HORIZONTAL,
       1, 1);
   elm_box_pack_end(ephoto->statusbar, ephoto->infolabel);
   evas_object_show(ephoto->infolabel);

   ephoto->controls_right = elm_box_add(ephoto->statusbar);
   elm_box_horizontal_set(ephoto->controls_right, EINA_TRUE);
   evas_object_size_hint_weight_set(ephoto->controls_right,
       0.0, 0.0);
   evas_object_size_hint_align_set(ephoto->controls_right, EVAS_HINT_FILL,
       EVAS_HINT_FILL);
   elm_box_pack_end(ephoto->statusbar, ephoto->controls_right);
   evas_object_show(ephoto->controls_right);

   ic = elm_icon_add(ephoto->statusbar);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   elm_icon_standard_set(ic, "media-playback-start");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   but = elm_button_add(ephoto->statusbar);
   elm_object_part_content_set(but, "icon", ic);
   evas_object_smart_callback_add(but, "clicked", _slideshow_icon_clicked, ephoto);
   elm_object_tooltip_text_set(but, _("Slideshow"));
   elm_object_tooltip_orient_set(but, ELM_TOOLTIP_ORIENT_TOP);
   elm_box_pack_end(ephoto->statusbar, but);
   evas_object_show(but);

   ic = elm_icon_add(ephoto->statusbar);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   elm_icon_standard_set(ic, "preferences-system");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   but = elm_button_add(ephoto->statusbar);
   elm_object_part_content_set(but, "icon", ic);
   evas_object_smart_callback_add(but, "clicked", _settings_icon_clicked, ephoto);
   elm_object_tooltip_text_set(but, _("Settings"));
   elm_object_tooltip_orient_set(but, ELM_TOOLTIP_ORIENT_LEFT);
   elm_box_pack_end(ephoto->statusbar, but);
   evas_object_show(but);

   if ((!path) || (!ecore_file_exists(path)))
     {
	if (ephoto->config->open)
	  {
	     if (!strcmp(ephoto->config->open, "Last"))
		path = ephoto->config->directory;
	     else
		path = ephoto->config->open;
	     if ((path) && (!ecore_file_exists(path)))
		path = NULL;
	  }
        else if (!ephoto->config->open || path)
	  {
	     if (getcwd(buf, sizeof(buf)))
		path = buf;
	     else
		path = getenv("HOME");
	  }
     }

   if (ecore_file_is_dir(path))
     {
        eina_stringshare_replace(&ephoto->config->directory,
            ecore_file_realpath(path));
	_ephoto_thumb_browser_show(ephoto, NULL);
     }
   else
     {
	char *dir = ecore_file_dir_get(path);

        eina_stringshare_replace(&ephoto->config->directory,
            ecore_file_realpath(dir));
	free(dir);
	ephoto_single_browser_path_pending_set(ephoto->single_browser, path);

	elm_naviframe_item_simple_promote(ephoto->pager,
	    ephoto->single_browser);
	ephoto->state = EPHOTO_STATE_SINGLE;
     }
   ephoto_directory_browser_top_dir_set(ephoto, ephoto->config->directory);
   ephoto_directory_browser_initialize_structure(ephoto);
   evas_object_resize(ephoto->win, ephoto->config->window_width,
       ephoto->config->window_height);
   evas_object_show(ephoto->win);

   ephoto->overlay_timer = ecore_timer_add(5.0, _timer_cb, ephoto);

   return ephoto->win;
}

void
ephoto_title_set(Ephoto *ephoto, const char *title)
{
   char buf[1024] = "Ephoto";

   if (title)
     {
	snprintf(buf, sizeof(buf), "Ephoto - %s", title);
	elm_win_title_set(ephoto->win, buf);
     }
   else
      elm_win_title_set(ephoto->win, "Ephoto");
}

int
ephoto_entries_cmp(const void *pa, const void *pb)
{
   const Ephoto_Entry *a = pa, *b = pb;

   return strcoll(a->basename, b->basename);
}

static void
_ephoto_populate_main(void *data, Eio_File *handler EINA_UNUSED,
    const Eina_File_Direct_Info *info)
{
   Ephoto_Dir_Data *ed = data;
   Ephoto_Entry *e;
   Ephoto_Event_Entry_Create *ev;

   if (ephoto_entry_exists(ed->ephoto, info->path))
     return;
   e = ephoto_entry_new(ed->ephoto, info->path, info->path + info->name_start,
       info->type);

   if (e->is_dir)
     e->parent = ed->expanded;
   else
     {
	if (!ed->ephoto->entries)
	   ed->ephoto->entries = eina_list_append(ed->ephoto->entries, e);
	else
	  {
	     int near_cmp;
	     Eina_List *near_node =
		 eina_list_search_sorted_near_list(ed->ephoto->entries,
		 ephoto_entries_cmp, e, &near_cmp);

	     if (near_cmp < 0)
		ed->ephoto->entries =
		    eina_list_append_relative_list(ed->ephoto->entries, e,
		    near_node);
	     else
		ed->ephoto->entries =
		    eina_list_prepend_relative_list(ed->ephoto->entries, e,
		    near_node);
	  }
	e->parent = NULL;
     }
   ev = calloc(1, sizeof(Ephoto_Event_Entry_Create));
   ev->entry = e;

   ecore_event_add(EPHOTO_EVENT_ENTRY_CREATE, ev, NULL, NULL);
}

static Eina_Bool
_ephoto_populate_filter(void *data, Eio_File *handler EINA_UNUSED,
    const Eina_File_Direct_Info *info)
{
   Ephoto_Dir_Data *ed = data;
   const char *bname = info->path + info->name_start;

   if (bname[0] == '.')
      return EINA_FALSE;
   if (info->type == EINA_FILE_DIR && !ed->thumbs_only)
     {
        return EINA_TRUE;
     }
   if (!ed->dirs_only)
     return _ephoto_eina_file_direct_info_image_useful(info);
   else
     return EINA_FALSE;
}

static void
_ephoto_populate_end(void *data, Eio_File *handler EINA_UNUSED)
{
   Ephoto_Dir_Data *ed = data;

   ed->ephoto->ls = NULL;

   ephoto_single_browser_entries_set(ed->ephoto->single_browser,
       ed->ephoto->entries);
   ecore_event_add(EPHOTO_EVENT_POPULATE_END, NULL, NULL, NULL);
   free(ed);
}

static void
_ephoto_populate_error(void *data, Eio_File *handler, int error EINA_UNUSED)
{
   Ephoto_Dir_Data *ed = data;

   ecore_event_add(EPHOTO_EVENT_POPULATE_ERROR, NULL, NULL, NULL);
   _ephoto_populate_end(ed->ephoto, handler);
}

static void
_ephoto_populate_entries(Ephoto_Dir_Data *ed)
{
   Ephoto_Entry *entry;

   if (ed->thumbs_only)
     EINA_LIST_FREE(ed->ephoto->entries, entry)
       ephoto_entry_free(entry->ephoto, entry);
   else if (!ed->dirs_only)
     ephoto_entries_free(ed->ephoto);

   ed->ephoto->ls =
       eio_file_stat_ls(ed->ephoto->config->directory, _ephoto_populate_filter,
           _ephoto_populate_main, _ephoto_populate_end, _ephoto_populate_error,
           ed);

   ecore_event_add(EPHOTO_EVENT_POPULATE_START, NULL, NULL, NULL);
}

static void
_ephoto_change_dir(void *data)
{
   Ephoto_Dir_Data *ed = data;

   ed->ephoto->job.change_dir = NULL;
   _ephoto_populate_entries(ed);
}

static void
_monitor_cb(void *data, Ecore_File_Monitor *em EINA_UNUSED,
    Ecore_File_Event event, const char *path)
{
   Ephoto *ephoto = data;
   char file[PATH_MAX], dir[PATH_MAX];

   snprintf(file, PATH_MAX, "%s", path);
   snprintf(dir, PATH_MAX, "%s", ecore_file_dir_get(file));

   if (strcmp(ephoto->config->directory, dir))
     return;

   if (evas_object_image_extension_can_load_get(path))
     {
        if (event == ECORE_FILE_EVENT_CREATED_FILE)
          {
             Ephoto_Entry *entry;
             char buf[PATH_MAX];

             if (ephoto_entry_exists(ephoto, path))
               return;
             snprintf(buf, PATH_MAX, "%s", path);
             entry = ephoto_entry_new(ephoto, path, basename(buf),
                 EINA_FILE_REG);
             ephoto_single_browser_path_created(ephoto->single_browser, entry);
             if (!ephoto->entries)
               {
                  ephoto->entries = eina_list_append(ephoto->entries, entry);
               }
             else
               {
                  int near_cmp;
                  Eina_List *near_node =
                      eina_list_search_sorted_near_list(ephoto->entries,
                      ephoto_entries_cmp, entry, &near_cmp);

                  if (near_cmp < 0)
                     ephoto->entries =
                         eina_list_append_relative_list(ephoto->entries, entry,
                         near_node);
                  else
                     ephoto->entries =
                         eina_list_prepend_relative_list(ephoto->entries, entry,
                         near_node);
               }
             ephoto_thumb_browser_insert(ephoto, entry);
             return;
          }
        else if (event == ECORE_FILE_EVENT_DELETED_FILE)
          {
             Eina_List *l;
             Ephoto_Entry *entry;

             EINA_LIST_FOREACH(ephoto->entries, l, entry)
               {
                  if (!strcmp(entry->path, path))
                    {
                       ephoto_thumb_browser_remove(ephoto, entry);
                       ephoto_entry_free(ephoto, entry);
                       break;
                    }
               }
             return;
          }
        else if (event == ECORE_FILE_EVENT_MODIFIED)
          {
             Eina_List *l;
             Ephoto_Entry *entry;
             int found = 0;

             EINA_LIST_FOREACH(ephoto->entries, l, entry)
               {
                  if (!strcmp(entry->path, path))
                    {
                       if (!ecore_file_exists(entry->path))
                         {
                            elm_object_item_del(entry->item);
                            ephoto_entry_free(ephoto, entry);
                         }
                       else
                         {
                            if (!entry->item)
                              ephoto_thumb_browser_insert(ephoto, entry);
                            else
                              ephoto_thumb_browser_update(ephoto, entry);
                         }
                       found = 1;
                       break;
                    }
               }
             if (!found)
               {
                  char buf[PATH_MAX];

                  if (ephoto_entry_exists(ephoto, path))
                    return;
                  snprintf(buf, PATH_MAX, "%s", path);
                  entry = ephoto_entry_new(ephoto, path, basename(buf),
                      EINA_FILE_REG);
                  ephoto_single_browser_path_created(ephoto->single_browser, entry);
                  if (!ephoto->entries)
                    {
                       ephoto->entries = eina_list_append(ephoto->entries, entry);
                    }
                  else
                    {
                       int near_cmp;
                       Eina_List *near_node =
                           eina_list_search_sorted_near_list(ephoto->entries,
                           ephoto_entries_cmp, entry, &near_cmp);

                       if (near_cmp < 0)
                          ephoto->entries =
                              eina_list_append_relative_list(ephoto->entries, entry,
                              near_node);
                       else
                          ephoto->entries =
                              eina_list_prepend_relative_list(ephoto->entries, entry,
                              near_node);
                    }
                  ephoto_thumb_browser_insert(ephoto, entry);
                  return;
               }
          }
     }
}

void
ephoto_directory_set(Ephoto *ephoto, const char *path, Evas_Object *expanded,
    Eina_Bool dirs_only, Eina_Bool thumbs_only)
{
   Ephoto_Dir_Data *ed;

   ed = malloc(sizeof(Ephoto_Dir_Data));
   ed->ephoto = ephoto;
   ed->expanded = expanded;
   ed->dirs_only = dirs_only;
   ed->thumbs_only = thumbs_only;

   if (!ecore_file_can_read(path))
     return;

   ephoto_title_set(ephoto, NULL);
   eina_stringshare_replace(&ephoto->config->directory,
       ecore_file_realpath(path));

   if (ed->ephoto->job.change_dir)
      ecore_job_del(ed->ephoto->job.change_dir);
   ed->ephoto->job.change_dir = ecore_job_add(_ephoto_change_dir, ed);
   if (ephoto->monitor)
     ecore_file_monitor_del(ephoto->monitor);
   ephoto->monitor = ecore_file_monitor_add(path, _monitor_cb, ephoto);
}

static Eina_Bool
_thumb_gen_size_changed_timer_cb(void *data)
{
   Ephoto *ephoto = data;
   const Eina_List *l;
   Evas_Object *o;

   if (ephoto->config->thumb_gen_size == ephoto->thumb_gen_size)
      goto end;

   ephoto->config->thumb_gen_size = ephoto->thumb_gen_size;

   EINA_LIST_FOREACH(ephoto->thumbs, l, o)
     {
        e_thumb_icon_size_set(o, ephoto->thumb_gen_size,
		   ephoto->thumb_gen_size);
	e_thumb_icon_rethumb(o);
     }
  end:
   ephoto->timer.thumb_regen = NULL;
   return EINA_FALSE;
}

void
ephoto_thumb_size_set(Ephoto *ephoto, int size)
{
   if (ephoto->config->thumb_size != size)
     {
	ephoto->config->thumb_size = size;
     }

   if (size <= 128)
      ephoto->thumb_gen_size = 128;
   else if (size <= 256)
      ephoto->thumb_gen_size = 256;
   else
      ephoto->thumb_gen_size = 512;

   if (ephoto->timer.thumb_regen)
      ecore_timer_del(ephoto->timer.thumb_regen);
   ephoto->timer.thumb_regen =
       ecore_timer_add(0.1, _thumb_gen_size_changed_timer_cb, ephoto);
}

static void
_thumb_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj,
    void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;

   e_thumb_icon_end(obj);
   ephoto->thumbs = eina_list_remove(ephoto->thumbs, obj);
}

Evas_Object *
ephoto_thumb_add(Ephoto *ephoto, Evas_Object *parent, const char *path)
{
   Evas_Object *o;

   if (path)
     {
	const char *ext = strrchr(path, '.');

	if (ext)
	  {
	     ext++;
	     if ((strcasecmp(ext, "edj") == 0))
               {
		  o = elm_icon_add(parent);
               }
	     else
               {
	          o = e_thumb_icon_add(parent); 
                  e_thumb_icon_file_set(o, path, NULL);
                  e_thumb_icon_size_set(o, ephoto->thumb_gen_size,
                     ephoto->thumb_gen_size);
                  e_thumb_icon_begin(o);
               }
	  }
        else
          {
	     o = e_thumb_icon_add(parent);
	     e_thumb_icon_file_set(o, path, NULL);
             e_thumb_icon_size_set(o, ephoto->thumb_gen_size,
                 ephoto->thumb_gen_size);
             e_thumb_icon_begin(o);
          }
     }
   else
      o = e_thumb_icon_add(parent);
   if (!o)
      return NULL;

   ephoto->thumbs = eina_list_append(ephoto->thumbs, o);
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, _thumb_del, ephoto);

   return o;
}

void
ephoto_thumb_path_set(Evas_Object *obj, const char *path)
{
   const char *group = NULL;
   const char *ext = strrchr(path, '.');

   if (ext)
     {
	ext++;
        if ((strcasecmp(ext, "edj") == 0))
	  {
	     if (edje_file_group_exists(path, "e/desktop/background"))
		group = "e/desktop/background";
	     else
	       {
		  Eina_List *g = edje_file_collection_list(path);

		  group = eina_list_data_get(g);
		  edje_file_collection_list_free(g);
	       }
	     elm_image_file_set(obj, path, group);
	     return;
	  }
     }
   e_thumb_icon_file_set(obj, path, group);
   e_thumb_icon_begin(obj);
}

Ephoto_Entry *
ephoto_entry_new(Ephoto *ephoto, const char *path, const char *label,
    Eina_File_Type type)
{
   Ephoto_Entry *entry;

   entry = calloc(1, sizeof(Ephoto_Entry));
   entry->ephoto = ephoto;
   entry->path = eina_stringshare_add(path);
   entry->basename = ecore_file_file_get(entry->path);
   entry->label = eina_stringshare_add(label);
   if (type == EINA_FILE_DIR)
      entry->is_dir = EINA_TRUE;
   else
      entry->is_dir = EINA_FALSE;
   return entry;
}

Eina_Bool 
ephoto_entry_exists(Ephoto *ephoto, const char *path)
{
   Ephoto_Entry *entry;
   Eina_List *l;

   EINA_LIST_FOREACH(ephoto->entries, l, entry)
     {
        if (!strcmp(entry->path, path))
          return EINA_TRUE;
     }
   return EINA_FALSE;
}

void
ephoto_entry_free(Ephoto *ephoto, Ephoto_Entry *entry)
{
   Ephoto_Entry_Free_Listener *fl;
   Eina_List *node;
   
   EINA_LIST_FREE(entry->free_listeners, fl)
     {
        fl->cb((void *) fl->data, entry);
        free(fl);
     }
   if (!entry->is_dir)
     {
        node = eina_list_data_find_list(ephoto->entries, entry);
        ephoto->entries = eina_list_remove_list(ephoto->entries, node);
        if (ephoto->selentries)
          {
             node = eina_list_data_find_list(ephoto->selentries, entry);
             ephoto->selentries = eina_list_remove_list(ephoto->selentries,
                 node);
          }
     }
   eina_stringshare_del(entry->path);
   eina_stringshare_del(entry->label);
   if (entry->monitor)
     ecore_file_monitor_del(entry->monitor);
   free(entry);
}

void
ephoto_entry_free_listener_add(Ephoto_Entry *entry, void (*cb) (void *data,
	const Ephoto_Entry *entry), const void *data)
{
   Ephoto_Entry_Free_Listener *fl;

   fl = malloc(sizeof(Ephoto_Entry_Free_Listener));
   fl->cb = cb;
   fl->data = data;
   entry->free_listeners = eina_list_append(entry->free_listeners, fl);
}

void
ephoto_entry_free_listener_del(Ephoto_Entry *entry, void (*cb) (void *data,
	const Ephoto_Entry *entry), const void *data)
{
   Eina_List *l;
   Ephoto_Entry_Free_Listener *fl;

   EINA_LIST_FOREACH(entry->free_listeners, l, fl)
     {
        if ((fl->cb == cb) && (fl->data == data))
	  {
	     entry->free_listeners =
	         eina_list_remove_list(entry->free_listeners, l);
	     break;
          }
     }
}

void
ephoto_entries_free(Ephoto *ephoto)
{
   Ephoto_Entry *entry;

   EINA_LIST_FREE(ephoto->entries, entry) ephoto_entry_free(ephoto, entry);
}
