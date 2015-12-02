#include "ephoto.h"

#define CONFIG_VERSION 13

static int _ephoto_config_load(Ephoto *ephoto);
static Eina_Bool _ephoto_on_config_save(void *data);

static Eet_Data_Descriptor *edd = NULL;

Eina_Bool
ephoto_config_init(Ephoto *ephoto)
{
   Eet_Data_Descriptor_Class eddc;

   if (!eet_eina_stream_data_descriptor_class_set(&eddc, sizeof(eddc),
	   "Ephoto_Config", sizeof(Ephoto_Config)))
     {
	return EINA_FALSE;
     }

   if (!edd)
      edd = eet_data_descriptor_stream_new(&eddc);

#undef T
#undef D
#define T Ephoto_Config
#define D edd
#define C_VAL(edd, type, member, dtype) \
  EET_DATA_DESCRIPTOR_ADD_BASIC(edd, type, #member, member, dtype)

   C_VAL(D, T, config_version, EET_T_INT);
   C_VAL(D, T, thumb_size, EET_T_INT);
   C_VAL(D, T, thumb_gen_size, EET_T_INT);
   C_VAL(D, T, directory, EET_T_STRING);
   C_VAL(D, T, slideshow_timeout, EET_T_DOUBLE);
   C_VAL(D, T, slideshow_transition, EET_T_STRING);
   C_VAL(D, T, window_width, EET_T_INT);
   C_VAL(D, T, window_height, EET_T_INT);
   C_VAL(D, T, fsel_hide, EET_T_INT);
   C_VAL(D, T, tool_hide, EET_T_INT);
   C_VAL(D, T, open, EET_T_STRING);
   C_VAL(D, T, prompts, EET_T_INT);
   C_VAL(D, T, drop, EET_T_INT);
   switch (_ephoto_config_load(ephoto))
     {
       case 0:
	  /* Start a new config */
	  ephoto->config->config_version = CONFIG_VERSION;
	  ephoto->config->slideshow_timeout = 4.0;
	  ephoto->config->slideshow_transition = eina_stringshare_add("fade");
	  ephoto->config->window_width = 900;
	  ephoto->config->window_height = 600;
	  ephoto->config->fsel_hide = 0;
	  ephoto->config->tool_hide = 0;
	  ephoto->config->open = eina_stringshare_add(getenv("HOME"));
	  ephoto->config->prompts = 1;
	  ephoto->config->drop = 0;
	  break;

       default:
	  return EINA_TRUE;
     }

   ephoto_config_save(ephoto);
   return EINA_TRUE;
}

void
ephoto_config_save(Ephoto *ephoto)
{
   _ephoto_on_config_save(ephoto);
}

void
ephoto_config_free(Ephoto *ephoto)
{
   free(ephoto->config);
   ephoto->config = NULL;
}

static void
_close(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   
   evas_object_del(popup);
}

static void
_save_general(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto *ephoto = evas_object_data_get(popup, "ephoto");
   const char *path;
   const char *text = elm_object_text_get(ephoto->config->open_dir);

   if (!strcmp(text, _("Root Directory")))
      path = "/";
   else if (!strcmp(text, _("Home Directory")))
      path = getenv("HOME");
   else if (!strcmp(text, _("Last Open Directory")))
      path = "Last";
   else
      path = elm_object_text_get(ephoto->config->open_dir_custom);

   if (ecore_file_is_dir(path) || !strcmp(path, "Last"))
      eina_stringshare_replace(&ephoto->config->open, path);
   ephoto->config->tool_hide =
       elm_check_state_get(ephoto->config->hide_toolbar);
   ephoto->config->prompts = elm_check_state_get(ephoto->config->show_prompts);
   ephoto->config->drop = elm_check_state_get(ephoto->config->move_drop);
   evas_object_del(popup);
}

static void
_open_hv_select(void *data, Evas_Object *obj, void *event_info)
{
   Ephoto *ephoto = data;

   elm_object_text_set(obj, elm_object_item_text_get(event_info));

   if (!strcmp(elm_object_item_text_get(event_info), _("Custom Directory")))
      elm_object_disabled_set(ephoto->config->open_dir_custom, EINA_FALSE);
   else
      elm_object_disabled_set(ephoto->config->open_dir_custom, EINA_TRUE);
}

void
ephoto_config_general(Ephoto *ephoto)
{
   Evas_Object *popup, *box, *table, *check, *hoversel, *entry, *ic, *button;

   popup = elm_popup_add(ephoto->win);
   elm_popup_scrollable_set(popup, EINA_TRUE);
   elm_object_part_text_set(popup, "title,text", _("General Settings"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, 0.0, 0.0);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);

   table = elm_table_add(box);
   evas_object_size_hint_weight_set(table, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(table);

   check = elm_check_add(table);
   elm_object_text_set(check, _("Hide Toolbar On Fullscreen"));
   evas_object_size_hint_align_set(check, 0.0, EVAS_HINT_FILL);
   elm_check_state_set(check, ephoto->config->tool_hide);
   elm_table_pack(table, check, 0, 1, 1, 1);
   evas_object_show(check);
   ephoto->config->hide_toolbar = check;

   check = elm_check_add(table);
   elm_object_text_set(check, _("Prompt Before Changing The Filesystem"));
   evas_object_size_hint_align_set(check, 0.0, EVAS_HINT_FILL);
   elm_check_state_set(check, ephoto->config->prompts);
   elm_table_pack(table, check, 0, 2, 1, 1);
   evas_object_show(check);
   ephoto->config->show_prompts = check;

   check = elm_check_add(table);
   elm_object_text_set(check, _("Move Files When Dropped"));
   evas_object_size_hint_align_set(check, 0.0, EVAS_HINT_FILL);
   elm_check_state_set(check, ephoto->config->drop);
   elm_table_pack(table, check, 0, 3, 1, 1);
   evas_object_show(check);
   ephoto->config->move_drop = check;

   hoversel = elm_hoversel_add(table);
   elm_hoversel_hover_parent_set(hoversel, ephoto->win);
   elm_hoversel_item_add(hoversel, _("Root Directory"), NULL, 0,
       _open_hv_select, ephoto);
   elm_hoversel_item_add(hoversel, _("Home Directory"), NULL, 0,
       _open_hv_select, ephoto);
   elm_hoversel_item_add(hoversel, _("Last Open Directory"), NULL, 0,
       _open_hv_select, ephoto);
   elm_hoversel_item_add(hoversel, _("Custom Directory"), NULL, 0,
       _open_hv_select, ephoto);
   elm_object_text_set(hoversel, _("Directory To Open Ephoto In"));
   evas_object_data_set(hoversel, "ephoto", ephoto);
   evas_object_size_hint_weight_set(hoversel, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(hoversel, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(table, hoversel, 0, 4, 1, 1);
   evas_object_show(hoversel);
   ephoto->config->open_dir = hoversel;

   entry = elm_entry_add(table);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_object_text_set(entry, _("Custom Directory"));
   elm_object_disabled_set(entry, EINA_TRUE);
   elm_scroller_policy_set(entry, ELM_SCROLLER_POLICY_OFF,
       ELM_SCROLLER_POLICY_OFF);
   evas_object_size_hint_weight_set(entry, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(table, entry, 0, 5, 1, 1);
   evas_object_show(entry);
   ephoto->config->open_dir_custom = entry;

   elm_box_pack_end(box, table);

   ic = elm_icon_add(popup);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "document-save");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Save"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _save_general, popup);
   elm_object_part_content_set(popup, "button1", button);
   evas_object_show(button);

   ic = elm_icon_add(popup);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "window-close");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Cancel"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _close, popup);
   elm_object_part_content_set(popup, "button2", button);
   evas_object_show(button);

   evas_object_data_set(popup, "ephoto", ephoto);
   elm_object_part_content_set(popup, "default", box);
   evas_object_show(popup);
}

static void
_save(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto *ephoto = evas_object_data_get(popup, "ephoto");

   if (elm_spinner_value_get(ephoto->config->slide_time) > 0)
      ephoto->config->slideshow_timeout =
	  elm_spinner_value_get(ephoto->config->slide_time);
   if (elm_object_text_get(ephoto->config->slide_trans))
      eina_stringshare_replace(&ephoto->config->slideshow_transition,
	  elm_object_text_get(ephoto->config->slide_trans));
   evas_object_del(popup);
}

static void
_hv_select(void *data EINA_UNUSED, Evas_Object *obj, void *event_info)
{
   elm_object_text_set(obj, elm_object_item_text_get(event_info));
}

static void
_spinner_changed(void *data EINA_UNUSED, Evas_Object *obj,
    void *event_info EINA_UNUSED)
{
   double val;
   char buf[PATH_MAX];

   val = elm_spinner_value_get(obj);
   snprintf(buf, PATH_MAX, "%%1.0f %s", ngettext("second", "seconds", val));
   elm_spinner_label_format_set(obj, buf);
}

void
ephoto_config_slideshow(Ephoto *ephoto)
{
   Evas_Object *popup, *box, *table, *label, *spinner, *hoversel, *ic, *button;
   const Eina_List *l;
   const char *transition;
   char buf[PATH_MAX];

   popup = elm_popup_add(ephoto->win);
   elm_popup_scrollable_set(popup, EINA_TRUE);
   elm_object_part_text_set(popup, "title,text", _("Slideshow Settings"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, 0.0, 0.0);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);

   table = elm_table_add(box);
   evas_object_size_hint_weight_set(table, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(table, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(table);

   label = elm_label_add(table);
   memset(buf, 0, PATH_MAX);
   snprintf(buf, PATH_MAX, "%s:", _("Show Each Slide For"));
   elm_object_text_set(label, buf);
   evas_object_size_hint_align_set(label, 0.0, EVAS_HINT_FILL);
   elm_table_pack(table, label, 0, 1, 1, 1);
   evas_object_show(label);

   spinner = elm_spinner_add(table);
   elm_spinner_editable_set(spinner, EINA_TRUE);
   memset(buf, 0, PATH_MAX);
   snprintf(buf, PATH_MAX, "%%1.0f %s", _("seconds"));
   evas_object_smart_callback_add(spinner, "changed", _spinner_changed, NULL);
   elm_spinner_label_format_set(spinner, buf);
   elm_spinner_step_set(spinner, 1);
   elm_spinner_value_set(spinner, ephoto->config->slideshow_timeout);
   elm_spinner_min_max_set(spinner, 1, 60);
   elm_table_pack(table, spinner, 1, 1, 1, 1);
   evas_object_show(spinner);
   ephoto->config->slide_time = spinner;

   label = elm_label_add(table);
   memset(buf, 0, PATH_MAX);
   snprintf(buf, PATH_MAX, "%s:", _("Slide Transition"));
   elm_object_text_set(label, buf);
   evas_object_size_hint_align_set(label, 0.0, EVAS_HINT_FILL);
   elm_table_pack(table, label, 0, 2, 1, 1);
   evas_object_show(label);

   hoversel = elm_hoversel_add(table);
   elm_hoversel_hover_parent_set(hoversel, ephoto->win);
   EINA_LIST_FOREACH(elm_slideshow_transitions_get(ephoto->slideshow), l,
       transition) elm_hoversel_item_add(hoversel, transition, NULL, 0,
       _hv_select, transition);
   elm_hoversel_item_add(hoversel, "None", NULL, 0, _hv_select, NULL);
   elm_object_text_set(hoversel, ephoto->config->slideshow_transition);
   evas_object_size_hint_weight_set(hoversel, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(hoversel, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(table, hoversel, 1, 2, 1, 1);
   evas_object_show(hoversel);
   ephoto->config->slide_trans = hoversel;

   elm_box_pack_end(box, table);

   ic = elm_icon_add(popup);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "document-save");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Save"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _save, popup);
   elm_object_part_content_set(popup, "button1", button);
   evas_object_show(button);

   ic = elm_icon_add(popup);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "window-close");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Cancel"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _close, popup);
   elm_object_part_content_set(popup, "button2", button);
   evas_object_show(button);

   evas_object_data_set(popup, "ephoto", ephoto);
   elm_object_part_content_set(popup, "default", box);
   evas_object_show(popup);
}

static void
_link_anchor_bt(void *data, Evas_Object *obj,
    void *event_info EINA_UNUSED)
{
   char buf[PATH_MAX];
   Evas_Object *av = data;
   const char *link = evas_object_data_get(obj, "link");

   elm_entry_anchor_hover_end(av);
   snprintf(buf, PATH_MAX, "xdg-open %s", link);
   ecore_exe_run(buf, NULL);
}

static void
_copy_anchor_bt(void *data, Evas_Object *obj,
    void *event_info EINA_UNUSED)
{
   char buf[PATH_MAX];
   Evas_Object *av = data;
   const char *link = evas_object_data_get(obj, "link");

   elm_entry_anchor_hover_end(av);
   snprintf(buf, PATH_MAX, "%s", link);
   elm_cnp_selection_set(av, ELM_SEL_TYPE_CLIPBOARD, ELM_SEL_FORMAT_MARKUP,
       buf, strlen(buf));
}

static void
_link_anchor(void *data, Evas_Object *obj, void *event_info)
{
   Evas_Object *av = data;
   Evas_Object *button;
   Elm_Entry_Anchor_Hover_Info *ei = event_info;

   button = elm_button_add(obj);
   elm_object_text_set(button, _("Open Link In Browser"));
   elm_object_part_content_set(ei->hover, "middle", button);
   evas_object_smart_callback_add(button, "clicked", _link_anchor_bt,
       av);
   evas_object_data_set(button, "link", strdup(ei->anchor_info->name));
   evas_object_show(button);

   button = elm_button_add(obj);
   elm_object_text_set(button, _("Copy Link"));
   elm_object_part_content_set(ei->hover, "bottom", button);
   evas_object_smart_callback_add(button, "clicked", _copy_anchor_bt,
       av);
   evas_object_data_set(button, "link", strdup(ei->anchor_info->name));
   evas_object_show(button);
}

void
ephoto_config_about(Ephoto *ephoto)
{
   Evas_Object *popup, *box, *bb, *entry, *ic, *button;
   Eina_Strbuf *sbuf = eina_strbuf_new();
   FILE *f;

   popup = elm_popup_add(ephoto->win);
   elm_popup_scrollable_set(popup, EINA_TRUE);
   elm_object_part_text_set(popup, "title,text", _("About Ephoto"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);

   bb = elm_bubble_add(box);
   evas_object_size_hint_weight_set(bb, 0.0, 0.0);
   evas_object_size_hint_align_set(bb, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, bb);
   evas_object_show(bb);

   entry = elm_entry_add(bb);
   elm_entry_anchor_hover_style_set(entry, "popout");
   elm_entry_anchor_hover_parent_set(entry, popup);
   elm_entry_editable_set(entry, EINA_FALSE);
   elm_entry_context_menu_disabled_set(entry, EINA_TRUE);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   evas_object_size_hint_weight_set(entry, 0.0, 0.0);
   evas_object_size_hint_align_set(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   eina_strbuf_append_printf(sbuf,
       _("Ephoto is a comprehensive image viewer based on the EFL.<br/>"
	   "For more information, please visit the Ephoto project page on<br/>"
           "the Enlightenment wiki:<br/>"
	   "<a href=https://phab.enlightenment.org/w/projects/ephoto>"
           "https://phab.enlightenment.org/w/projects/ephoto</a><br/><br/>"
	   "Ephoto's source can be found through Enlightenment's git:<br/>"
	   "<a href=http://git.enlightenment.org/apps/ephoto.git>"
           "http://git.enlightenment.org/apps/ephoto.git</a><br/><br/>"
	   "<b>Authors:</b><br/>"));
   f = fopen(PACKAGE_DATA_DIR "/AUTHORS", "r");
   if (f)
     {
	char buf[PATH_MAX];

	while (fgets(buf, sizeof(buf), f))
	  {
	     int len;

	     len = strlen(buf);
	     if (len > 0)
	       {
		  if (buf[len - 1] == '\n')
		    {
		       buf[len - 1] = 0;
		       len--;
		    }
		  if (len > 0)
		    {
		       char *p;

		       do
			 {
			    p = strchr(buf, '<');
			    if (p)
			       *p = 0;
			 }
		       while (p);
		       do
			 {
			    p = strchr(buf, '>');
			    if (p)
			       *p = 0;
			 }
		       while (p);
		       eina_strbuf_append_printf(sbuf, "%s<br/>", buf);
		    }
		  if (len == 0)
		     eina_strbuf_append_printf(sbuf, "<br/>");
	       }
	  }
	fclose(f);
     }
   elm_object_text_set(entry, eina_strbuf_string_get(sbuf));
   evas_object_smart_callback_add(entry, "anchor,hover,opened",
       _link_anchor, entry);
   elm_object_content_set(bb, entry);
   evas_object_show(entry);

   ic = elm_icon_add(box);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "window-close");

   button = elm_button_add(box);
   elm_object_text_set(button, _("Close"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _close, popup);
   elm_object_part_content_set(popup, "button1", button);
   evas_object_show(button);

   evas_object_data_set(popup, "ephoto", ephoto);
   elm_object_part_content_set(popup, "default", box);
   evas_object_show(popup);
}

static int
_ephoto_config_load(Ephoto *ephoto)
{
   Eet_File *ef;
   char buf[4096], buf2[4096];

   snprintf(buf2, sizeof(buf2), "%s/.config/ephoto", getenv("HOME"));
   ecore_file_mkpath(buf2);
   snprintf(buf, sizeof(buf), "%s/ephoto.cfg", buf2);

   ef = eet_open(buf, EET_FILE_MODE_READ);
   if (!ef)
     {
	ephoto_config_free(ephoto);
	ephoto->config = calloc(1, sizeof(Ephoto_Config));
	return 0;
     }

   ephoto->config = eet_data_read(ef, edd, "config");
   eet_close(ef);

   if (!ephoto->config || ephoto->config->config_version > CONFIG_VERSION)
     {
	ephoto_config_free(ephoto);
	ephoto->config = calloc(1, sizeof(Ephoto_Config));
	return 0;
     }

   if (ephoto->config->config_version < CONFIG_VERSION)
     {
	ecore_file_unlink(buf);
	ephoto_config_free(ephoto);
	ephoto->config = calloc(1, sizeof(Ephoto_Config));
	return 0;
     }
   return 1;
}

static Eina_Bool
_ephoto_on_config_save(void *data)
{
   Ephoto *ephoto = data;
   Eet_File *ef;
   char buf[4096], buf2[4096];

   snprintf(buf, sizeof(buf), "%s/.config/ephoto/ephoto.cfg", getenv("HOME"));
   snprintf(buf2, sizeof(buf2), "%s.tmp", buf);

   ef = eet_open(buf2, EET_FILE_MODE_WRITE);
   if (!ef)
      goto save_end;

   eet_data_write(ef, edd, "config", ephoto->config, 1);
   if (eet_close(ef))
      goto save_end;

   if (!ecore_file_mv(buf2, buf))
      goto save_end;

  save_end:
   ecore_file_unlink(buf2);

   return ECORE_CALLBACK_CANCEL;
}
