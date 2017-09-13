#include "ephoto.h"

#define CONFIG_VERSION 21

static int       _ephoto_config_load(Ephoto *ephoto);
static Eina_Bool _ephoto_on_config_save(void *data);

static Eet_Data_Descriptor *edd = NULL;

static void
_config_save_cb(void *data, Evas_Object *obj EINA_UNUSED,
                void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto *ephoto = evas_object_data_get(popup, "ephoto");
   const char *path;
   const char *text = elm_object_text_get(ephoto->config->open_dir);

   if (!strcmp(text, _("Root Directory")))
     path = "/";
   else if (!strcmp(text, _("Home Directory")))
     path = eina_environment_home_get();
   else if (!strcmp(text, _("Last Open Directory")))
     path = "Last";
   else
     path = elm_object_text_get(ephoto->config->open_dir_custom);

   if (ecore_file_is_dir(path) || !strcmp(path, "Last"))
     eina_stringshare_replace(&ephoto->config->open, path);
   if (strcmp(path, ephoto->config->directory) && strcmp(path, "Last") &&
       ecore_file_exists(path))
     {
        char *rp = ecore_file_realpath(path);
        ephoto_directory_browser_clear(ephoto);
        ephoto_thumb_browser_clear(ephoto);
        eina_stringshare_replace(&ephoto->config->directory, rp);
        ephoto_directory_browser_top_dir_set(ephoto, ephoto->config->directory);
        ephoto_directory_browser_initialize_structure(ephoto);
        free(rp);
     }
   ephoto->config->prompts = elm_check_state_get(ephoto->config->show_prompts);
   ephoto->config->drop = elm_check_state_get(ephoto->config->move_drop);
   ephoto->config->movess = elm_check_state_get(ephoto->config->slide_move);
   ephoto->config->smooth = elm_check_state_get(ephoto->config->smooth_scale);
   ephoto->config->folders = elm_check_state_get(ephoto->config->show_folders);
   ephoto->config->thumbnail_aspect = elm_check_state_get(ephoto->config->thumb_aspect);
   if (elm_spinner_value_get(ephoto->config->panel_size) > 0)
     {
        ephoto->config->left_size = (elm_spinner_value_get(ephoto->config->panel_size) / 0.05) * 0.05;
        ephoto->config->right_size = (elm_spinner_value_get(ephoto->config->panel_size) / 0.05) * 0.05;
        evas_object_size_hint_weight_set(ephoto->dir_browser, ephoto->config->left_size, EVAS_HINT_EXPAND);
     }

   if (elm_spinner_value_get(ephoto->config->slide_time) > 0)
     ephoto->config->slideshow_timeout =
       elm_spinner_value_get(ephoto->config->slide_time);
   if (elm_object_text_get(ephoto->config->slide_trans))
     eina_stringshare_replace(&ephoto->config->slideshow_transition,
                              elm_object_text_get(ephoto->config->slide_trans));

   evas_object_del(popup);
   elm_object_focus_set(ephoto->pager, EINA_TRUE);
   ephoto_thumb_browser_recalc(ephoto);
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

static void
_config_general(Ephoto *ephoto, Evas_Object *parent)
{
   Evas_Object *table, *check, *hoversel, *entry, *label, *spinner;
   char buf[PATH_MAX];

   table = elm_table_add(parent);
   EPHOTO_EXPAND(table);
   EPHOTO_FILL(table);
   elm_box_pack_end(parent, table);
   evas_object_show(table);

   check = elm_check_add(table);
   elm_object_text_set(check, _("Show Folders On Start"));
   EPHOTO_ALIGN(check, EVAS_HINT_FILL, 0.5);
   elm_check_state_set(check, ephoto->config->folders);
   elm_table_pack(table, check, 0, 0, 1, 1);
   evas_object_show(check);
   ephoto->config->show_folders = check;

   check = elm_check_add(table);
   elm_object_text_set(check, _("Prompt Before Changing The Filesystem"));
   EPHOTO_FILL(check);
   elm_check_state_set(check, ephoto->config->prompts);
   elm_table_pack(table, check, 0, 1, 1, 1);
   evas_object_show(check);
   ephoto->config->show_prompts = check;

   check = elm_check_add(table);
   elm_object_text_set(check, _("Move Files When Dropped"));
   EPHOTO_FILL(check);
   elm_check_state_set(check, ephoto->config->drop);
   elm_table_pack(table, check, 0, 2, 1, 1);
   evas_object_show(check);
   ephoto->config->move_drop = check;

   check = elm_check_add(table);
   elm_object_text_set(check, _("Smooth Scale Images"));
   EPHOTO_FILL(check);
   elm_check_state_set(check, ephoto->config->smooth);
   elm_table_pack(table, check, 0, 3, 1, 1);
   evas_object_show(check);
   ephoto->config->smooth_scale = check;

   check = elm_check_add(table);
   elm_object_text_set(check, _("Keep Aspect on Thumbnails"));
   EPHOTO_FILL(check);
   elm_check_state_set(check, ephoto->config->thumbnail_aspect);
   elm_table_pack(table, check, 0, 4, 1, 1);
   evas_object_show(check);
   ephoto->config->thumb_aspect = check;

   label = elm_label_add(table);
   elm_object_text_set(label, _("File Panel Size"));
   EPHOTO_ALIGN(label, 0.5, 0.5);
   elm_table_pack(table, label, 0, 5, 1, 1);
   evas_object_show(label);

   spinner = elm_spinner_add(table);
   elm_spinner_editable_set(spinner, EINA_TRUE);
   snprintf(buf, PATH_MAX, "%%1.2f %s", _("Weight (1.0 Max)"));
   elm_spinner_label_format_set(spinner, buf);
   elm_spinner_step_set(spinner, .05);
   elm_spinner_value_set(spinner, ephoto->config->left_size);
   elm_spinner_min_max_set(spinner, 0, 1);
   EPHOTO_EXPAND(spinner);
   EPHOTO_FILL(spinner);
   elm_table_pack(table, spinner, 0, 6, 1, 1);
   evas_object_show(spinner);
   ephoto->config->panel_size = spinner;

   label = elm_label_add(table);
   elm_object_text_set(label, _("Top Level Directory"));
   EPHOTO_ALIGN(label, 0.5, 0.5);
   elm_table_pack(table, label, 0, 7, 1, 1);
   evas_object_show(label);

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
   elm_object_text_set(hoversel, ephoto->config->open);
   evas_object_data_set(hoversel, "ephoto", ephoto);
   EPHOTO_WEIGHT(hoversel, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   EPHOTO_FILL(hoversel);
   elm_table_pack(table, hoversel, 0, 8, 1, 1);
   evas_object_show(hoversel);
   ephoto->config->open_dir = hoversel;

   entry = elm_entry_add(table);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_object_text_set(entry, _("Custom Directory"));
   elm_object_disabled_set(entry, EINA_TRUE);
   elm_scroller_policy_set(entry, ELM_SCROLLER_POLICY_OFF,
                           ELM_SCROLLER_POLICY_OFF);
   EPHOTO_EXPAND(entry);
   EPHOTO_FILL(entry);
   elm_table_pack(table, entry, 0, 9, 1, 1);
   evas_object_show(entry);
   ephoto->config->open_dir_custom = entry;
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

static Eina_List *
_ephoto_transitions_list_get(const char *str)
{
   Eina_List *list = NULL;
   const char *s, *b;
   if (!str) return NULL;
   for (b = s = str; 1; s++)
     {
        if ((*s == ' ') || (!*s))
          {
             char *t = malloc(s - b + 1);
             if (t)
               {
                  strncpy(t, b, s - b);
                  t[s - b] = 0;
                  list = eina_list_append(list, eina_stringshare_add(t));
                  free(t);
               }
             b = s + 1;
          }
        if (!*s) break;
     }
   return list;
}

static void
_config_slideshow(Ephoto *ephoto, Evas_Object *parent)
{
   Eina_List *transitions;
   Evas_Object *table, *check, *label, *spinner, *hoversel;
   const Eina_List *l;
   const char *transition;
   char buf[PATH_MAX];

   table = elm_table_add(parent);
   EPHOTO_EXPAND(table);
   EPHOTO_FILL(table);
   elm_box_pack_end(parent, table);
   evas_object_show(table);

   check = elm_check_add(table);
   elm_object_text_set(check, _("Moving Slideshow"));
   EPHOTO_ALIGN(check, 0.5, 0.5);
   elm_check_state_set(check, ephoto->config->movess);
   elm_table_pack(table, check, 0, 0, 2, 1);
   evas_object_show(check);
   ephoto->config->slide_move = check;

   label = elm_label_add(table);
   memset(buf, 0, PATH_MAX);
   snprintf(buf, PATH_MAX, "%s:", _("Show Each Slide For"));
   elm_object_text_set(label, buf);
   EPHOTO_FILL(label);
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
   EPHOTO_FILL(label);
   elm_table_pack(table, label, 0, 2, 1, 1);
   evas_object_show(label);

   transitions = _ephoto_transitions_list_get(edje_object_data_get(elm_layout_edje_get
                                                                     (ephoto->slideshow), "transitions"));

   hoversel = elm_hoversel_add(table);
   elm_hoversel_hover_parent_set(hoversel, ephoto->win);
   EINA_LIST_FOREACH(transitions, l,
                      transition)
     elm_hoversel_item_add(hoversel, transition, NULL, 0,
                           _hv_select, transition);
   elm_hoversel_item_add(hoversel, "none", NULL, 0, _hv_select, NULL);
   elm_object_text_set(hoversel, ephoto->config->slideshow_transition);
   EPHOTO_EXPAND(hoversel);
   EPHOTO_FILL(hoversel);
   elm_table_pack(table, hoversel, 1, 2, 1, 1);
   evas_object_show(hoversel);
   ephoto->config->slide_trans = hoversel;
}

static Evas_Object *
_config_settings(Ephoto *ephoto, Evas_Object *parent, Eina_Bool slideshow)
{
   Evas_Object *frame, *vbox;

   frame = elm_frame_add(parent);
   if (!slideshow)
     elm_object_text_set(frame, _("General"));
   else
     elm_object_text_set(frame, _("Slideshow"));
   EPHOTO_EXPAND(frame);
   EPHOTO_FILL(frame);
   evas_object_show(frame);

   vbox = elm_box_add(frame);
   elm_box_horizontal_set(vbox, EINA_FALSE);
   EPHOTO_EXPAND(vbox);
   EPHOTO_FILL(vbox);
   elm_object_content_set(frame, vbox);
   evas_object_show(vbox);

   if (!slideshow)
     _config_general(ephoto, vbox);
   else
     _config_slideshow(ephoto, vbox);

   return frame;
}

static void
_link_anchor_bt(void *data, Evas_Object *obj,
                void *event_info EINA_UNUSED)
{
   char buf[PATH_MAX];
   Evas_Object *av = data;
   const char *link = evas_object_data_get(obj, "link");

   elm_entry_anchor_hover_end(av);
#ifdef _WIN32
   snprintf(buf, PATH_MAX, "start %s", link);
#else
   snprintf(buf, PATH_MAX, "xdg-open %s", link);
#endif
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

static Evas_Object *
_config_bindings(Evas_Object *parent)
{
   Evas_Object *frame, *box, *scroller, *entry;
   Eina_Strbuf *sbuf = eina_strbuf_new();

   frame = elm_frame_add(parent);
   elm_object_text_set(frame, _("Bindings"));
   EPHOTO_EXPAND(frame);
   EPHOTO_FILL(frame);
   evas_object_show(frame);

   box = elm_box_add(parent);
   elm_box_horizontal_set(box, EINA_FALSE);
   EPHOTO_EXPAND(box);
   EPHOTO_FILL(box);
   elm_object_content_set(frame, box);
   evas_object_show(box);

   scroller = elm_scroller_add(box);
   EPHOTO_EXPAND(scroller);
   EPHOTO_FILL(scroller);
   elm_box_pack_end(box, scroller);
   evas_object_show(scroller);

   entry = elm_entry_add(scroller);
   elm_entry_editable_set(entry, EINA_FALSE);
   elm_entry_line_wrap_set(entry, ELM_WRAP_NONE);
   EPHOTO_WEIGHT(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   EPHOTO_FILL(entry);
   eina_strbuf_append_printf(sbuf,
                             _("<b><hilight>General Bindings</hilight></b><br/>"
                               "<b>F1:</b> Settings Panel<br/>"
                               "<b>F5:</b> Start Slideshow<br/>"
                               "<b>F11:</b> Toggle Fullscreen<br/>"
                               "<b>Ctrl+Shift+f:</b> Toggle File Selector<br/><br/>"
                               "<b><hilight>Thumbnail Browser Bindings</hilight></b><br/>"
                               "<b>Ctrl++:</b> Zoom In<br/>"
                               "<b>Ctrl+-:</b> Zoom Out<br/>"
                               "<b>Ctrl+Tab:</b> View Image<br/>"
                               "<b>Ctrl+c:</b> Copy Image<br/>"
                               "<b>Ctrl+x:</b> Cut Image<br/>"
                               "<b>Ctrl+v:</b> Paste Image<br/>"
                               "<b>Ctrl+a:</b> Select All<br/>"
                               "<b>Ctrl+f:</b> Toggle Search<br/>"
                               "<b>Ctrl+Delete:</b> Delete Image<br/>"
                               "<b>F2:</b> Rename Image<br/>"
                               "<b>Escape:</b> Clear Selection<br/><br/>"
                               "<b><hilight>Single Browser Bindings</hilight></b><br/>"
                               "<b>Ctrl+Shift+0:</b> Zoom 1:1<br/>"
                               "<b>Ctrl++:</b> Zoom In<br/>"
                               "<b>Ctrl+-:</b> Zoom Out<br/>"
                               "<b>Ctrl+0:</b> Zoom Fit<br/>"
                               "<b>Ctrl+Shift+l:</b> Rotate Counter Clockwise<br/>"
                               "<b>Ctrl+l:</b> Flip Horizontal<br/>"
                               "<b>Ctrl+Shift+r:</b> Rotate Clockwise<br/>"
                               "<b>Ctrl+r:</b> Flip Vertical<br/>"
                               "<b>Ctrl+Shift+s:</b> Save Image As<br/>"
                               "<b>Ctrl+s:</b> Save Image<br/>"
                               "<b>Ctrl+u:</b> Reset Image<br/>"
                               "<b>Ctrl+y:</b> Redo<br/>"
                               "<b>Ctrl+Shift+z:</b> Redo<br/>"
                               "<b>Ctrl+z:</b> Undo<br/>"
                               "<b>Home:</b> Navigate First<br/>"
                               "<b>Left Arrow:</b> Navigate Previous<br/>"
                               "<b>Right Arrow:</b> Navigate Next<br/>"
                               "<b>Space:</b> Navigate Next<br/>"
                               "<b>End:</b> Navigate Last<br/>"
                               "<b>Ctrl+Delete:</b> Delete Image<br/>"
                               "<b>F2</b> Rename Image<br/>"
                               "<b>Escape:</b> Return to Thumbnail Browser<br/><br/>"
                               "<b><hilight>Slideshow Bindings</hilight></b><br/>"
                               "<b>Space:</b> Play/Pause Slideshow<br/>"
                               "<b>Home:</b> Navigate First<br/>"
                               "<b>Left Arrow:</b> Navigate Previous<br/>"
                               "<b>Right Arrow:</b> Navigate Next<br/>"
                               "<b>End:</b> Navigate Last<br/>"
                               "<b>Escape:</b> Quit Slideshow<br/>"));
   elm_object_text_set(entry, eina_strbuf_string_get(sbuf));
   elm_object_content_set(scroller, entry);
   evas_object_show(entry);

   return frame;
}

static Evas_Object *
_config_about(Evas_Object *parent)
{
   Evas_Object *frame, *box, *entry, *img, *lbl;
   Evas_Object *scroller;
   Eina_Strbuf *sbuf = eina_strbuf_new();
   char ver[PATH_MAX];
   FILE *f;

   frame = elm_frame_add(parent);
   elm_object_text_set(frame, _("About"));
   EPHOTO_EXPAND(frame);
   EPHOTO_FILL(frame);
   evas_object_show(frame);

   scroller = elm_scroller_add(frame);
   EPHOTO_EXPAND(scroller);
   EPHOTO_FILL(scroller);
   elm_object_content_set(frame, scroller);
   evas_object_show(scroller);

   box = elm_box_add(frame);
   elm_box_horizontal_set(box, EINA_FALSE);
   EPHOTO_EXPAND(box);
   EPHOTO_FILL(box);
   elm_object_content_set(scroller, box);
   evas_object_show(box);

   img = elm_image_add(box);
   evas_object_image_size_set(elm_image_object_get(img), 100, 100);
   evas_object_size_hint_min_set(img, 200, 100);
   evas_object_size_hint_max_set(img, 200, 100);
   elm_image_preload_disabled_set(img, EINA_FALSE);
   elm_image_file_set(img, PACKAGE_DATA_DIR "/images/ephoto.png", NULL);
   elm_box_pack_end(box, img);
   evas_object_show(img);

   snprintf(ver, PATH_MAX, "<hilight><b>Ephoto<br/>Version: %s</b></hilight>", PACKAGE_VERSION);

   lbl = elm_label_add(box);
   elm_object_text_set(lbl, ver);
   EPHOTO_EXPAND(lbl);
   EPHOTO_FILL(lbl);
   elm_box_pack_end(box, lbl);
   evas_object_show(lbl);

   entry = elm_entry_add(box);
   elm_entry_anchor_hover_style_set(entry, "popout");
   elm_entry_anchor_hover_parent_set(entry, parent);
   elm_entry_editable_set(entry, EINA_FALSE);
   elm_entry_context_menu_disabled_set(entry, EINA_TRUE);
   elm_entry_line_wrap_set(entry, ELM_WRAP_WORD);
   EPHOTO_WEIGHT(entry, EVAS_HINT_FILL, EVAS_HINT_FILL);
   EPHOTO_FILL(entry);
   eina_strbuf_append_printf(sbuf,
                             _("Ephoto is a comprehensive image viewer based on the EFL. For more"
                               "information, please visit the Ephoto project page:<br/>"
                               "<a href=http://www.smhouston.us/ephoto/>"
                               "http://www.smhouston.us/ephoto/</a><br/><br/>"
                               "Ephoto also has a page on the Enlightenment wiki:<br/>"
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
   elm_box_pack_end(box, entry);
   evas_object_show(entry);

   return frame;
}

static void
_list_clicked(void *data, Evas_Object *o, void *event EINA_UNUSED)
{
   Evas_Object *page = data;

   Evas_Object *settings = evas_object_data_get(o, "settings");
   Evas_Object *slideshow = evas_object_data_get(o, "slideshow");
   Evas_Object *kb = evas_object_data_get(o, "bindings");
   Evas_Object *about = evas_object_data_get(o, "about");

   evas_object_hide(settings);
   evas_object_hide(slideshow);
   evas_object_hide(kb);
   evas_object_hide(about);
   evas_object_show(page);
}

static int
_ephoto_config_load(Ephoto *ephoto)
{
   Eet_File *ef;
   char buf[4096], buf2[4096];

   snprintf(buf2, sizeof(buf2), "%s/ephoto", efreet_config_home_get());
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

   snprintf(buf, sizeof(buf), "%s/ephoto/ephoto.cfg", efreet_config_home_get());
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

void
ephoto_config_main(Ephoto *ephoto)
{
   Evas_Object *popup, *table, *list, *ic;
   Evas_Object *slideshow, *settings, *kb, *about;
   Elm_Object_Item *slideshowi, *settingsi, *kbi, *abouti;

   popup = elm_popup_add(ephoto->win);
   elm_popup_scrollable_set(popup, EINA_TRUE);
   elm_object_part_text_set(popup, "title,text", _("Settings Panel"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);
   evas_object_smart_callback_add(popup, "block,clicked", _config_save_cb, popup);

   table = elm_table_add(popup);
   elm_table_homogeneous_set(table, EINA_FALSE);
   EPHOTO_EXPAND(table);
   EPHOTO_FILL(table);

   settings = _config_settings(ephoto, table, EINA_FALSE);
   elm_table_pack(table, settings, 1, 0, 1, 1);
   kb = _config_bindings(table);
   elm_table_pack(table, kb, 1, 0, 1, 1);
   about = _config_about(table);
   elm_table_pack(table, about, 1, 0, 1, 1);
   slideshow = _config_settings(ephoto, table, EINA_TRUE);
   elm_table_pack(table, slideshow, 1, 0, 1, 1);

   list = elm_list_add(table);
   elm_list_select_mode_set(list, ELM_OBJECT_SELECT_MODE_ALWAYS);
   elm_scroller_content_min_limit(list, 1, 1);
   EPHOTO_EXPAND(list);
   EPHOTO_FILL(list);
   elm_table_pack(table, list, 0, 0, 1, 1);
   evas_object_show(list);

   evas_object_data_set(list, "settings", settings);
   evas_object_data_set(list, "slideshow", slideshow);
   evas_object_data_set(list, "bindings", kb);
   evas_object_data_set(list, "about", about);

   ic = elm_icon_add(list);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "preferences-system");
   evas_object_show(ic);
   settingsi = elm_list_item_append(list, _("General"), ic, NULL,
                                    _list_clicked, settings);

   ic = elm_icon_add(list);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "media-playback-start");
   evas_object_show(ic);
   slideshowi = elm_list_item_append(list, _("Slideshow"), ic, NULL,
                                     _list_clicked, slideshow);

   ic = elm_icon_add(list);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "input-keyboard");
   evas_object_show(ic);
   kbi = elm_list_item_append(list, _("Bindings"), ic, NULL,
                              _list_clicked, kb);

   ic = elm_icon_add(list);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "help-about");
   evas_object_show(ic);
   abouti = elm_list_item_append(list, _("About"), ic, NULL,
                                 _list_clicked, about);

   elm_list_go(list);

   elm_object_item_data_set(settingsi, settings);
   elm_object_item_data_set(slideshowi, slideshow);
   elm_object_item_data_set(kbi, kb);
   elm_object_item_data_set(abouti, about);
   evas_object_hide(slideshow);
   evas_object_hide(kb);
   evas_object_hide(about);

   elm_list_item_selected_set(settingsi, EINA_TRUE);

   evas_object_show(table);
   elm_object_content_set(popup, table);
   evas_object_data_set(popup, "ephoto", ephoto);
   evas_object_show(popup);

   if (ephoto->config->firstrun)
     elm_list_item_selected_set(abouti, EINA_TRUE);
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
   C_VAL(D, T, left_size, EET_T_DOUBLE);
   C_VAL(D, T, right_size, EET_T_DOUBLE);
   C_VAL(D, T, open, EET_T_STRING);
   C_VAL(D, T, prompts, EET_T_INT);
   C_VAL(D, T, drop, EET_T_INT);
   C_VAL(D, T, movess, EET_T_INT);
   C_VAL(D, T, smooth, EET_T_INT);
   C_VAL(D, T, firstrun, EET_T_INT);
   C_VAL(D, T, folders, EET_T_INT);
   C_VAL(D, T, thumbnail_aspect, EET_T_INT);
   switch (_ephoto_config_load(ephoto))
     {
      case 0:
        /* Start a new config */
        ephoto->config->config_version = CONFIG_VERSION;
        ephoto->config->slideshow_timeout = 4.0;
        ephoto->config->slideshow_transition = eina_stringshare_add("fade");
        ephoto->config->window_width = 900 * elm_config_scale_get();
        ephoto->config->window_height = 500 * elm_config_scale_get();
        ephoto->config->fsel_hide = 0;
        ephoto->config->left_size = .25;
        ephoto->config->right_size = .25;
        ephoto->config->open = eina_stringshare_add(eina_environment_home_get());
        ephoto->config->prompts = 1;
        ephoto->config->drop = 0;
        ephoto->config->movess = 1;
        ephoto->config->smooth = 1;
        ephoto->config->firstrun = 1;
        ephoto->config->folders = 1;
        ephoto->config->thumbnail_aspect = 0;
        break;

      default:
        return EINA_TRUE;
     }

   ephoto_config_save(ephoto);
   return EINA_TRUE;
}

