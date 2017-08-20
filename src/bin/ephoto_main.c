#include "ephoto.h"

int EPHOTO_EVENT_ENTRY_CREATE = 0;
int EPHOTO_EVENT_POPULATE_START = 0;
int EPHOTO_EVENT_POPULATE_END = 0;
int EPHOTO_EVENT_POPULATE_ERROR = 0;
int EPHOTO_EVENT_EDITOR_RESET = 0;
int EPHOTO_EVENT_EDITOR_APPLY = 0;
int EPHOTO_EVENT_EDITOR_CANCEL = 0;
int EPHOTO_EVENT_EDITOR_BACK = 0;

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
   evas_object_hide(ephoto->slideshow);
   evas_object_hide(ephoto->single_browser);
   evas_object_show(ephoto->thumb_browser);
   elm_object_focus_set(ephoto->thumb_browser, EINA_TRUE);
   _ephoto_state_set(ephoto, EPHOTO_STATE_THUMB);
   ephoto_title_set(ephoto, ephoto->config->directory);
   ephoto_thumb_browser_update_info_label(ephoto);
   evas_object_show(ephoto->statusbar);
   elm_box_clear(ephoto->controls_left);
   elm_box_clear(ephoto->controls_right);
   ephoto_thumb_browser_show_controls(ephoto);
   evas_object_freeze_events_set(ephoto->single_browser, EINA_TRUE);
   evas_object_freeze_events_set(ephoto->slideshow, EINA_TRUE);
   evas_object_freeze_events_set(ephoto->thumb_browser, EINA_FALSE);
   if ((entry) && (entry->item))
     {
        Eina_List *l;
        Elm_Object_Item *it;

        l = eina_list_clone(elm_gengrid_selected_items_get(entry->gengrid));
        if (eina_list_count(l) <= 1)
          {
             EINA_LIST_FREE(l, it)
               elm_gengrid_item_selected_set(it, EINA_FALSE);
             elm_gengrid_item_bring_in(entry->item, ELM_GENGRID_ITEM_SCROLLTO_IN);
             elm_gengrid_item_selected_set(entry->item, EINA_TRUE);
          }
     }
   ephoto_single_browser_entry_set(ephoto->single_browser, NULL);
   ephoto_slideshow_entry_set(ephoto->slideshow, NULL);
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
   evas_object_hide(ephoto->slideshow);
   evas_object_show(ephoto->single_browser);
   evas_object_hide(ephoto->thumb_browser);
   elm_object_focus_set(ephoto->single_browser, EINA_TRUE);
   _ephoto_state_set(ephoto, EPHOTO_STATE_SINGLE);
   evas_object_show(ephoto->statusbar);
   elm_box_clear(ephoto->controls_left);
   elm_box_clear(ephoto->controls_right);
   ephoto_single_browser_show_controls(ephoto);
   evas_object_freeze_events_set(ephoto->thumb_browser, EINA_TRUE);
   evas_object_freeze_events_set(ephoto->slideshow, EINA_TRUE);
   evas_object_freeze_events_set(ephoto->single_browser, EINA_FALSE);
}

static void
_ephoto_slideshow_show(Ephoto *ephoto, Ephoto_Entry *entry)
{
   _ephoto_state_set(ephoto, EPHOTO_STATE_SLIDESHOW);
   ephoto_slideshow_show_controls(ephoto);
   if (ephoto->selentries)
     ephoto_slideshow_entries_set(ephoto->slideshow, ephoto->selentries);
   else if (ephoto->searchentries)
     ephoto_slideshow_entries_set(ephoto->slideshow, ephoto->searchentries);
   else
     ephoto_slideshow_entries_set(ephoto->slideshow, ephoto->entries);
   ephoto_slideshow_entry_set(ephoto->slideshow, entry);
   evas_object_show(ephoto->slideshow);
   evas_object_hide(ephoto->single_browser);
   evas_object_hide(ephoto->thumb_browser);
   elm_object_focus_set(ephoto->slideshow, EINA_TRUE);
   elm_panes_content_left_min_size_set(ephoto->layout, 0);
   elm_panes_content_left_size_set(ephoto->layout, 0.0);
   elm_table_unpack(ephoto->main, ephoto->statusbar);
   evas_object_hide(ephoto->dir_browser);
   evas_object_hide(ephoto->statusbar);
   evas_object_freeze_events_set(ephoto->single_browser, EINA_TRUE);
   evas_object_freeze_events_set(ephoto->thumb_browser, EINA_TRUE);
   evas_object_freeze_events_set(ephoto->slideshow, EINA_FALSE);
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
   if (ephoto->folders_toggle)
     {
        elm_panes_content_left_min_size_set(ephoto->layout, 0);
        elm_panes_content_left_size_set(ephoto->layout, ephoto->config->left_size);
        evas_object_show(ephoto->dir_browser);
     }
   elm_table_pack(ephoto->main, ephoto->statusbar, 0, 2, 1, 1);
   evas_object_show(ephoto->statusbar);
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

   if (ephoto->file_thread)
     ecore_thread_cancel(ephoto->file_thread);
   if (eina_list_count(ephoto->file_pos))
      eina_list_free(ephoto->file_pos);
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
   if (ephoto->config_path)
     eina_stringshare_del(ephoto->config_path);
   if (ephoto->trash_path)
     eina_stringshare_del(ephoto->trash_path);
   if (ephoto->timer.thumb_regen)
     ecore_timer_del(ephoto->timer.thumb_regen);
   if (ephoto->monitor)
     {
        eio_monitor_del(ephoto->monitor);
        EINA_LIST_FREE(ephoto->monitor_handlers, handler)
          ecore_event_handler_del(handler);
     }
   ephoto_entries_free(ephoto);
   ephoto_config_save(ephoto);
   free(ephoto->config);
   free(ephoto);
}

static void
_resize_cb(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;
   Evas_Coord w, h;

   if (elm_win_fullscreen_get(ephoto->win))
     return;
   evas_object_geometry_get(ephoto->win, 0, 0, &w, &h);
   if (w && h)
     {
	ephoto->config->window_width = w;
	ephoto->config->window_height = h;
     }
}

static void
_folder_icon_clicked(void *data, Evas_Object *obj,
    void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;
   int ret = 0;

   if (!ephoto->folders_toggle)
     {
        elm_panes_content_left_min_size_set(ephoto->layout, 0);
        elm_panes_content_left_size_set(ephoto->layout, ephoto->config->left_size);
        ephoto->folders_toggle = EINA_TRUE;
        ret = elm_icon_standard_set(ephoto->folders_icon, "folder-open");
	if (!ret)
          elm_object_text_set(obj, _("Hide Folders"));
        elm_object_tooltip_text_set(obj, _("Hide Folders"));
     }
   else
     {
        elm_panes_content_left_min_size_set(ephoto->layout, 0);
        elm_panes_content_left_size_set(ephoto->layout, 0.0);
        ephoto->folders_toggle = EINA_FALSE;
        ret = elm_icon_standard_set(ephoto->folders_icon, "folder");
	if (!ret)
          elm_object_text_set(obj, _("Show Folders"));
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

static void
_exit_icon_clicked(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;

   evas_object_del(ephoto->win);
}

static void
_ephoto_left_pane_resized(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;

   ephoto->config->left_size = elm_panes_content_left_size_get(ephoto->layout);
}

/*Toggle determines whether to toggle folder visibility, or just force visible*/
void
ephoto_show_folders(Ephoto *ephoto, Eina_Bool toggle)
{
   int ret = 0;

   if (!ephoto->folders_toggle || !toggle)
     {
        elm_panes_content_left_min_size_set(ephoto->layout, 0);
        elm_panes_content_left_size_set(ephoto->layout, ephoto->config->left_size);
        evas_object_show(ephoto->dir_browser);
        ephoto->folders_toggle = EINA_TRUE;
        ret = elm_icon_standard_set(ephoto->folders_icon, "folder-open");
        if (!ret)
          elm_object_text_set(ephoto->folders_button, _("Hide Folders"));
	elm_object_tooltip_text_set(ephoto->folders_button, _("Hide Folders"));
     }
   else if (ephoto->folders_toggle)
     {
        elm_panes_content_left_min_size_set(ephoto->layout, 0);
        elm_panes_content_left_size_set(ephoto->layout, 0.0);
        evas_object_hide(ephoto->dir_browser);
        ephoto->folders_toggle = EINA_FALSE;
        ret = elm_icon_standard_set(ephoto->folders_icon, "folder");
        if (!ret)
          elm_object_text_set(ephoto->folders_button, _("Show Folders"));
	elm_object_tooltip_text_set(ephoto->folders_button, _("Show Folders"));
     }
}

Evas_Object *
ephoto_window_add(const char *path)
{
   Ephoto *ephoto = calloc(1, sizeof(Ephoto));
   Evas_Object *ic, *but;
   char buf[PATH_MAX], config[PATH_MAX], trash[PATH_MAX];
   int ret;

   EINA_SAFETY_ON_NULL_RETURN_VAL(ephoto, NULL);

   EPHOTO_EVENT_ENTRY_CREATE = ecore_event_type_new();
   EPHOTO_EVENT_POPULATE_START = ecore_event_type_new();
   EPHOTO_EVENT_POPULATE_END = ecore_event_type_new();
   EPHOTO_EVENT_POPULATE_ERROR = ecore_event_type_new();
   EPHOTO_EVENT_EDITOR_RESET = ecore_event_type_new();
   EPHOTO_EVENT_EDITOR_APPLY = ecore_event_type_new();
   EPHOTO_EVENT_EDITOR_CANCEL = ecore_event_type_new();
   EPHOTO_EVENT_EDITOR_BACK = ecore_event_type_new();

   ephoto->selentries = NULL;
   ephoto->folders_toggle = EINA_FALSE;
   ephoto->entries = NULL;
   ephoto->sort = EPHOTO_SORT_ALPHABETICAL_ASCENDING;
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

   ephoto->main = elm_table_add(ephoto->win);
   elm_table_homogeneous_set(ephoto->main, EINA_FALSE);
   EPHOTO_EXPAND(ephoto->main);
   EPHOTO_FILL(ephoto->main);
   elm_win_resize_object_add(ephoto->win, ephoto->main);
   evas_object_show(ephoto->main);

   ephoto->layout = elm_panes_add(ephoto->main);
   EPHOTO_EXPAND(ephoto->layout);
   EPHOTO_FILL(ephoto->layout);
   evas_object_smart_callback_add(ephoto->layout, "unpress",
      _ephoto_left_pane_resized, ephoto);
   elm_table_pack(ephoto->main, ephoto->layout, 0, 0, 1, 2);
   evas_object_show(ephoto->layout);

   ephoto->pager = elm_table_add(ephoto->layout);
   EPHOTO_EXPAND(ephoto->pager);
   EPHOTO_FILL(ephoto->pager);
   elm_object_part_content_set(ephoto->layout, "right", ephoto->pager);
   evas_object_show(ephoto->pager);

   ephoto->thumb_browser = ephoto_thumb_browser_add(ephoto, ephoto->layout);
   if (!ephoto->thumb_browser)
     {
	evas_object_del(ephoto->win);
	return NULL;
     }
   elm_table_pack(ephoto->pager, ephoto->thumb_browser, 0, 0, 1, 1);
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
   elm_table_pack(ephoto->pager, ephoto->single_browser, 0, 0, 1, 1);
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
   elm_table_pack(ephoto->pager, ephoto->slideshow, 0, 0, 1, 1);
   evas_object_smart_callback_add(ephoto->slideshow, "back",
       _ephoto_slideshow_back, ephoto);

   ephoto->dir_browser = ephoto_directory_browser_add(ephoto, ephoto->layout);
   EPHOTO_WEIGHT(ephoto->dir_browser, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   EPHOTO_FILL(ephoto->dir_browser);
   elm_panes_content_left_min_size_set(ephoto->layout, 0);
   elm_panes_content_left_size_set(ephoto->layout, ephoto->config->left_size);
   elm_object_part_content_set(ephoto->layout, "left", ephoto->dir_browser);
   evas_object_show(ephoto->dir_browser);

   ephoto->statusbar = elm_box_add(ephoto->main);
   evas_object_size_hint_min_set(ephoto->statusbar, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   elm_box_horizontal_set(ephoto->statusbar, EINA_TRUE);
   EPHOTO_WEIGHT(ephoto->statusbar, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   EPHOTO_FILL(ephoto->statusbar);
   elm_table_pack(ephoto->main, ephoto->statusbar, 0, 2, 1, 1);
   evas_object_show(ephoto->statusbar);

   ic = elm_icon_add(ephoto->statusbar);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   ret = elm_icon_standard_set(ic, "folder");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);
   ephoto->folders_icon = ic;

   but = elm_button_add(ephoto->statusbar);
   elm_object_part_content_set(but, "icon", ic);
   if (!ret)
     elm_object_text_set(but, _("Show Folders"));
   evas_object_smart_callback_add(but, "clicked", _folder_icon_clicked, ephoto);
   elm_object_tooltip_text_set(but, _("Show Folders"));
   elm_object_tooltip_orient_set(but, ELM_TOOLTIP_ORIENT_RIGHT);
   elm_box_pack_end(ephoto->statusbar, but);
   evas_object_show(but);
   ephoto->folders_button = but;

   ephoto->controls_left = elm_box_add(ephoto->statusbar);
   elm_box_horizontal_set(ephoto->controls_left, EINA_TRUE);
   EPHOTO_WEIGHT(ephoto->controls_left, EVAS_HINT_FILL, EVAS_HINT_FILL);
   EPHOTO_FILL(ephoto->controls_left);
   elm_box_pack_end(ephoto->statusbar, ephoto->controls_left);
   evas_object_show(ephoto->controls_left);

   ephoto->infolabel = elm_label_add(ephoto->statusbar);
   elm_object_style_set(ephoto->infolabel, "info");
   elm_label_line_wrap_set(ephoto->infolabel, ELM_WRAP_MIXED);
   elm_label_ellipsis_set(ephoto->infolabel, EINA_TRUE);
   elm_object_text_set(ephoto->infolabel, _("Information"));
   EPHOTO_EXPAND(ephoto->infolabel);
   EPHOTO_FILL(ephoto->infolabel);
   /*
   PLEASE SEE https://phab.enlightenment.org/T5888
   evas_object_size_hint_aspect_set(ephoto->infolabel, EVAS_ASPECT_CONTROL_HORIZONTAL,
       1, 1);
   */
   elm_box_pack_end(ephoto->statusbar, ephoto->infolabel);
   evas_object_show(ephoto->infolabel);

   ephoto->controls_right = elm_box_add(ephoto->statusbar);
   elm_box_horizontal_set(ephoto->controls_right, EINA_TRUE);
   EPHOTO_WEIGHT(ephoto->controls_right, EVAS_HINT_FILL, EVAS_HINT_FILL);
   EPHOTO_FILL(ephoto->controls_right);
   elm_box_pack_end(ephoto->statusbar, ephoto->controls_right);
   evas_object_show(ephoto->controls_right);

   ic = elm_icon_add(ephoto->statusbar);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   ret = elm_icon_standard_set(ic, "media-playback-start");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);

   but = elm_button_add(ephoto->statusbar);
   elm_object_part_content_set(but, "icon", ic);
   if (!ret)
     elm_object_text_set(but, _("Slideshow"));
   evas_object_smart_callback_add(but, "clicked",
       _slideshow_icon_clicked, ephoto);
   elm_object_tooltip_text_set(but, _("Slideshow"));
   elm_object_tooltip_orient_set(but, ELM_TOOLTIP_ORIENT_TOP);
   elm_box_pack_end(ephoto->statusbar, but);
   evas_object_show(but);

   ic = elm_icon_add(ephoto->statusbar);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   ret = elm_icon_standard_set(ic, "preferences-other");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);

   but = elm_button_add(ephoto->statusbar);
   elm_object_part_content_set(but, "icon", ic);
   if (!ret)
     elm_object_text_set(but, _("Settings"));
   evas_object_smart_callback_add(but, "clicked",
       _settings_icon_clicked, ephoto);
   elm_object_tooltip_text_set(but, _("Settings"));
   elm_object_tooltip_orient_set(but, ELM_TOOLTIP_ORIENT_TOP);
   elm_box_pack_end(ephoto->statusbar, but);
   evas_object_show(but);

   ic = elm_icon_add(ephoto->statusbar);
   evas_object_size_hint_min_set(ic, 20*elm_config_scale_get(),
       20*elm_config_scale_get());
   ret = elm_icon_standard_set(ic, "application-exit");
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_BOTH, 1, 1);

   ephoto->exit = elm_button_add(ephoto->statusbar);
   elm_object_part_content_set(ephoto->exit, "icon", ic);
   if (!ret)
     elm_object_text_set(ephoto->exit, _("Exit"));
   evas_object_smart_callback_add(ephoto->exit, "clicked",
       _exit_icon_clicked, ephoto);
   elm_object_tooltip_text_set(ephoto->exit, _("Exit"));
   elm_object_tooltip_orient_set(ephoto->exit, ELM_TOOLTIP_ORIENT_TOP);
   evas_object_hide(ephoto->exit);

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
		path = eina_environment_home_get();
	  }
     }

   if (ecore_file_is_dir(path))
     {
        char *rp = ecore_file_realpath(path);
        eina_stringshare_replace(&ephoto->config->directory, rp);
        free(rp);
	_ephoto_thumb_browser_show(ephoto, NULL);
     }
   else
     {
	char *dir = ecore_file_dir_get(path);
        char *rp = ecore_file_realpath(dir);

        eina_stringshare_replace(&ephoto->config->directory, rp);
        free(rp);
	free(dir);
        ephoto_single_browser_path_pending_set(ephoto->single_browser, path);
        evas_object_hide(ephoto->thumb_browser);
        evas_object_hide(ephoto->slideshow);
        evas_object_show(ephoto->single_browser);
        ephoto_single_browser_show_controls(ephoto);
	ephoto->state = EPHOTO_STATE_SINGLE;
     }

   snprintf(config, PATH_MAX, "%s/.config/ephoto", eina_environment_home_get());
   ephoto->config_path = eina_stringshare_add(config);
   snprintf(trash, PATH_MAX, "%s/trash", ephoto->config_path);
   ephoto->trash_path = eina_stringshare_add(trash);

   ephoto_directory_browser_top_dir_set(ephoto, ephoto->config->directory);
   ephoto_directory_browser_initialize_structure(ephoto);
   evas_object_resize(ephoto->win, ephoto->config->window_width,
       ephoto->config->window_height);
   evas_object_show(ephoto->win);

   if (!ephoto->config->folders)
     {
        evas_object_hide(ephoto->dir_browser);
        elm_panes_content_left_min_size_set(ephoto->layout, 0);
        elm_panes_content_left_size_set(ephoto->layout, 0.0);
        ephoto->folders_toggle = EINA_FALSE;
        ret = elm_icon_standard_set(ephoto->folders_icon, "folder");
	if (!ret)
          elm_object_text_set(ephoto->folders_button, _("Show Folders"));
	elm_object_tooltip_text_set(ephoto->folders_button, _("Show Folders"));
     }
   else
     {
        ephoto->folders_toggle = EINA_TRUE;
        elm_panes_content_left_min_size_set(ephoto->layout, 0);
        elm_panes_content_left_size_set(ephoto->layout, ephoto->config->left_size);
        ret = elm_icon_standard_set(ephoto->folders_icon, "folder-open");
        if (!ret)
          elm_object_text_set(ephoto->folders_button, _("Hide Folders"));
	elm_object_tooltip_text_set(ephoto->folders_button, _("Hide Folders"));
     }
   if (ephoto->config->firstrun)
     {
        _settings_icon_clicked(ephoto, NULL, NULL);
        ephoto->config->firstrun = 0;
     }

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
   int i = 0;
   long long moda, modb;

   i = strcasecmp(a->basename, b->basename);
   moda = ecore_file_mod_time(a->path);
   modb = ecore_file_mod_time(b->path);
   switch (a->ephoto->sort)
     {
        case EPHOTO_SORT_ALPHABETICAL_ASCENDING:
           return i;
        case EPHOTO_SORT_ALPHABETICAL_DESCENDING:
           return i * -1;
        case EPHOTO_SORT_MODTIME_ASCENDING:
           if (moda < modb)
             return -1;
           else if (moda > modb)
             return 1;
           else
             return i;
        case EPHOTO_SORT_MODTIME_DESCENDING:
           if (moda < modb)
             return 1;
           else if (moda > modb)
             return -1;
           else
             return i * -1;
        case EPHOTO_SORT_SIMILARITY:
           if (!a->sort_id || !b->sort_id)
             return 0;
           else
             return strcmp(a->sort_id, b->sort_id);
        default:
           return i;
     }
   return i;
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
        if (ed->ephoto->sort != EPHOTO_SORT_SIMILARITY)
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
   char *rp;

   if (bname[0] == '.')
      return EINA_FALSE;
   rp = ecore_file_realpath(info->path);
   if (info->type == EINA_FILE_DIR && !ed->thumbs_only)
     {
        free(rp);
        return EINA_TRUE;
     }
   else if (info->type == EINA_FILE_LNK && ecore_file_is_dir((const char *)rp))
     {
        Eina_Bool _is_dir = ecore_file_is_dir(rp);
        if (ed->thumbs_only)
          {
             free(rp);
             return EINA_FALSE;
          }
        free(rp);
        return _is_dir;
     }
   else if (!ed->dirs_only)
     {
        free(rp);
        return _ephoto_eina_file_direct_info_image_useful(info);
     }
   free(rp);
   return EINA_FALSE;
}

static void
_ephoto_populate_end(void *data, Eio_File *handler EINA_UNUSED)
{
   Ephoto_Dir_Data *ed = data;

   ed->ephoto->ls = NULL;

   if (eina_list_count(ed->ephoto->entries))
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
   _ephoto_populate_end(ed, handler);
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
   ed->ephoto->entries = NULL;

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

static Eina_Bool
_monitor_cb(void *data, int type,
    void *event)
{
   Ephoto *ephoto = data;
   Eio_Monitor_Event *ev = event;
   char file[PATH_MAX], dir[PATH_MAX];
   char *freedir;

   snprintf(file, PATH_MAX, "%s", ev->filename);
   freedir = ecore_file_dir_get(file);
   snprintf(dir, PATH_MAX, "%s", freedir);
   if (freedir) free(freedir);

   if (!type)
     return ECORE_CALLBACK_PASS_ON;
   if (strcmp(ephoto->config->directory, dir))
     return ECORE_CALLBACK_PASS_ON;

   if (evas_object_image_extension_can_load_get(ev->filename))
     {
        if (type == EIO_MONITOR_FILE_CREATED)
          {
             Ephoto_Entry *entry;
             char buf[PATH_MAX];

             if (ephoto_entry_exists(ephoto, ev->filename))
               return ECORE_CALLBACK_PASS_ON;
             snprintf(buf, PATH_MAX, "%s", ev->filename);
             entry = ephoto_entry_new(ephoto, ev->filename, basename(buf),
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
             return ECORE_CALLBACK_PASS_ON;
          }
        else if (type == EIO_MONITOR_FILE_DELETED)
          {
             Eina_List *l;
             Ephoto_Entry *entry;

             EINA_LIST_FOREACH(ephoto->entries, l, entry)
               {
                  if (!strcmp(entry->path, ev->filename))
                    {
                       ephoto_thumb_browser_remove(ephoto, entry);
                       break;
                    }
               }
             return ECORE_CALLBACK_PASS_ON;
          }
        else if (type == EIO_MONITOR_FILE_MODIFIED)
          {
             Eina_List *l;
             Ephoto_Entry *entry;

             EINA_LIST_FOREACH(ephoto->entries, l, entry)
               {
                  if (!strcmp(entry->path, ev->filename))
                    {
                       ephoto_thumb_browser_update(ephoto, entry);
                       break;
                    }
               }
          }
     }
   return ECORE_CALLBACK_PASS_ON;
}

void
ephoto_directory_set(Ephoto *ephoto, const char *path, Evas_Object *expanded,
    Eina_Bool dirs_only, Eina_Bool thumbs_only)
{
   Ephoto_Dir_Data *ed;
   Ecore_Event_Handler *handler;
   Evas_Object *o;
   char *rp;

   ed = malloc(sizeof(Ephoto_Dir_Data));
   ed->ephoto = ephoto;
   ed->expanded = expanded;
   ed->dirs_only = dirs_only;
   ed->thumbs_only = thumbs_only;

   if (!ecore_file_can_read(path))
     {
        free(ed);
        return;
     }

   EINA_LIST_FREE(ed->ephoto->thumbs, o)
     evas_object_del(o);

   ephoto_title_set(ephoto, NULL);
   rp = ecore_file_realpath(path);
   eina_stringshare_replace(&ephoto->config->directory, rp);

   if (ed->ephoto->job.change_dir)
      ecore_job_del(ed->ephoto->job.change_dir);
   ed->ephoto->job.change_dir = ecore_job_add(_ephoto_change_dir, ed);
   if (ephoto->monitor)
     {
        eio_monitor_del(ephoto->monitor);
        EINA_LIST_FREE(ephoto->monitor_handlers, handler)
          ecore_event_handler_del(handler);
     }
   ephoto->monitor = eio_monitor_add(path);
   ephoto->monitor_handlers =
       eina_list_append(ephoto->monitor_handlers,
           ecore_event_handler_add(EIO_MONITOR_FILE_CREATED,
               _monitor_cb, ephoto));
   ephoto->monitor_handlers =
       eina_list_append(ephoto->monitor_handlers,
           ecore_event_handler_add(EIO_MONITOR_FILE_MODIFIED,
               _monitor_cb, ephoto));
   ephoto->monitor_handlers =
       eina_list_append(ephoto->monitor_handlers,
           ecore_event_handler_add(EIO_MONITOR_FILE_DELETED,
               _monitor_cb, ephoto));
   free(rp);
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
       ecore_timer_loop_add(0.1, _thumb_gen_size_changed_timer_cb, ephoto);
}

static void
_thumb_gen_cb(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_data EINA_UNUSED)
{
   Ephoto_Entry *entry = data;
   const char *id;
   char *dir;

   if (!entry)
     return;
   if (!entry->thumb)
     return;
   dir = ecore_file_dir_get(entry->path);
   if (strcmp(dir, entry->ephoto->config->directory))
     {
        evas_object_del(entry->thumb);
        return;
     }
   free(dir);
   id = e_thumb_sort_id_get(entry->thumb);
   evas_object_smart_callback_del(entry->thumb, "e_thumb_gen", _thumb_gen_cb);
   e_thumb_icon_end(entry->thumb);
   if (!id || entry->sort_id)
     return;

   if (entry->sort_id)
     eina_stringshare_replace(&entry->sort_id, id);
   else
     entry->sort_id = eina_stringshare_add(id);

   if (entry->ephoto->sort == EPHOTO_SORT_SIMILARITY)
     {
        ephoto_thumb_browser_insert(entry->ephoto, entry);
        if (!entry->ephoto->entries)
          entry->ephoto->entries = eina_list_append(entry->ephoto->entries, entry);
        else
          {
             int near_cmp;
             Eina_List *near_node =
                 eina_list_search_sorted_near_list(entry->ephoto->entries,
                 ephoto_entries_cmp, entry, &near_cmp);

             if (near_cmp < 0)
                entry->ephoto->entries =
                    eina_list_append_relative_list(entry->ephoto->entries, entry,
                    near_node);
             else
                entry->ephoto->entries =
                    eina_list_prepend_relative_list(entry->ephoto->entries, entry,
                    near_node);
          }
     }
}

static void
_thumb_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj,
    void *event_info EINA_UNUSED)
{
   Ephoto *ephoto = data;

   ephoto->thumbs = eina_list_remove(ephoto->thumbs, obj);
}

Evas_Object *
ephoto_thumb_add(Ephoto *ephoto, Evas_Object *parent, Ephoto_Entry *entry)
{
   Evas_Object *o = NULL;

   if (entry->path)
     {
	const char *ext = strrchr(entry->path, '.');

	if (ext)
	  {
	     ext++;
	     if ((strcasecmp(ext, "edj") == 0))
               {
                  const char *group = NULL;

		  o = elm_icon_add(parent);
                  if (edje_file_group_exists(entry->path, "e/desktop/background"))
                    group = "e/desktop/background";
                  else
                    {
                       Eina_List *g = edje_file_collection_list(entry->path);

                       group = eina_list_data_get(g);
                       edje_file_collection_list_free(g);
                    }
                  elm_image_file_set(o, entry->path, group);
               }
	  }
        if (!o)
          {
	     o = e_thumb_icon_add(parent, ephoto->config->thumbnail_aspect);
             evas_object_smart_callback_add(o, "e_thumb_gen", _thumb_gen_cb, entry);
	     e_thumb_icon_file_set(o, entry->path, NULL);
             e_thumb_icon_size_set(o, ephoto->thumb_gen_size,
                 ephoto->thumb_gen_size);
             e_thumb_icon_begin(o);
             entry->thumb = o;
          }
     }
   else
     return NULL;

   ephoto->thumbs = eina_list_append(ephoto->thumbs, o);
   evas_object_event_callback_add(o, EVAS_CALLBACK_DEL, _thumb_del, ephoto);

   return o;
}

Ephoto_Entry *
ephoto_entry_new(Ephoto *ephoto, const char *path, const char *label,
    Eina_File_Type type)
{
   Ephoto_Entry *entry;
   char *rp;

   entry = calloc(1, sizeof(Ephoto_Entry));
   entry->ephoto = ephoto;
   entry->path = eina_stringshare_add(path);
   entry->basename = ecore_file_file_get(entry->path);
   entry->label = eina_stringshare_add(label);
   entry->sort_id = NULL;
   rp = ecore_file_realpath(entry->path);
   if (type == EINA_FILE_DIR)
     entry->is_dir = EINA_TRUE;
   else if (type == EINA_FILE_LNK && ecore_file_is_dir((const char *)rp))
     entry->is_dir = EINA_TRUE;
   else
     entry->is_dir = EINA_FALSE;
   if (type == EINA_FILE_LNK)
     entry->is_link = EINA_TRUE;
   else
     entry->is_link = EINA_FALSE;

   free(rp);
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
   Ecore_Event_Handler *handler;
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
        if (entry->sort_id)
          eina_stringshare_del(entry->sort_id);
     }
   eina_stringshare_del(entry->path);
   eina_stringshare_del(entry->label);
   if (entry->monitor)
     {
        eio_monitor_del(entry->monitor);
        EINA_LIST_FREE(entry->monitor_handlers, handler)
          ecore_event_handler_del(handler);
     }
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
             if (eina_list_data_find(entry->free_listeners, l))
               {
	          entry->free_listeners =
	              eina_list_remove_list(entry->free_listeners, l);
	          break;
               }
          }
     }
}

void
ephoto_entries_free(Ephoto *ephoto)
{
   Ephoto_Entry *entry;

   EINA_LIST_FREE(ephoto->entries, entry)
     ephoto_entry_free(ephoto, entry);
   ephoto->entries = NULL;
}
