#include "ephoto.h"
#ifdef HAVE_LIBEXIF
#include <libexif/exif-data.h>

typedef struct _Ephoto_Exif_Animator Ephoto_Exif_Animator;
struct _Ephoto_Exif_Animator
{
   Ecore_Idler *todo;
   Eina_List *todo_items;
   Evas_Object *parent;
   int count;
   int processed;
};

typedef struct _Ephoto_Exif_Item Ephoto_Exif_Item;
struct _Ephoto_Exif_Item
{
   unsigned int tag;
   unsigned int ifd;
   const char *title;
   const char *value;
};

Eina_Bool
ephoto_file_has_exif(const char *file)
{
   ExifData *ed = exif_data_new_from_file(file);
   if (!ed) return EINA_FALSE;
   
   exif_data_unref(ed);
   return EINA_TRUE;
}

Eina_List *
ephoto_file_get_exif_data(const char *file)
{
   ExifData *ed = exif_data_new_from_file(file);
   ExifEntry *ee = NULL;
   Eina_List *list = NULL;
   unsigned int tag, val;
   const char *title = NULL;
   char value[1024];

   if (!ed) return NULL;
   for (tag = 0; tag < 0xffff; tag++)
     {
        title = exif_tag_get_title(tag);
        for (val = 0; val < EXIF_IFD_COUNT; val++)
          {
             ee = exif_content_get_entry(ed->ifd[val], tag);
             if (ee)
               {
                  Ephoto_Exif_Item *eei;

                  eei = calloc(1, sizeof(Ephoto_Exif_Item));
                  eei->tag = tag;
                  eei->ifd = val;
                  eei->title = title;
                  exif_entry_ref(ee);
                  exif_entry_get_value(ee, value, 1024);
                  eei->value = eina_stringshare_add(value);
                  list = eina_list_append(list, eei);
		  exif_entry_unref(ee);
               }
          }
     }
   exif_data_unref(ed);
   return list;
}

static Eina_Bool
_exif_items_process(void *data)
{
   Ephoto_Exif_Animator *animator = data;
   Evas_Object *label, *entry;
   int i = 0;
   const char *value, *key;
   Ephoto_Exif_Item *eei;

   if (animator->processed == animator->count)
     {
        ecore_idler_del(animator->todo);
        free(animator);
        return EINA_FALSE;
     }
   EINA_LIST_FREE(animator->todo_items, eei)
     {
        if (i > 3)
          return EINA_TRUE;

        key = eei->title;
        value = eei->value;

        label = elm_label_add(animator->parent);
        elm_object_text_set(label, key);
        EPHOTO_ALIGN(label, 0.0, 0.5);
        elm_table_pack(animator->parent, label, 0,
                       animator->processed, 1, 1);
        evas_object_show(label);

        entry = elm_entry_add(animator->parent);
        elm_entry_single_line_set(entry, EINA_TRUE);
        elm_entry_scrollable_set(entry, EINA_TRUE);
        elm_scroller_policy_set(entry, ELM_SCROLLER_POLICY_OFF,
                                ELM_SCROLLER_POLICY_OFF);
        elm_object_text_set(entry, value);
        EPHOTO_EXPAND(entry);
        EPHOTO_FILL(entry);
        elm_table_pack(animator->parent, entry, 1,
                       animator->processed, 1, 1);
        evas_object_show(entry);
        animator->processed++;
        i++;
     }
   return EINA_TRUE;
}

static void
_exif_list_free(Eina_List *list)
{
   Ephoto_Exif_Item *eei;

   EINA_LIST_FREE(list, eei)
     {
        eina_stringshare_del(eei->value);
	free(eei);
     }
}

static void
_exif_save_cb(void *data, Evas_Object *obj EINA_UNUSED,
                void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Eina_List *list = evas_object_data_get(popup, "list");

   _exif_list_free(list);
   evas_object_del(popup);
}

void
ephoto_file_exif_data(Ephoto *ephoto, const char *file)
{
   Eina_List *list = NULL, *l = NULL;
   Evas_Object *popup, *box, *scroller, *table;
   Ephoto_Exif_Animator *animator;
   Ephoto_Exif_Item *eei;

   list = ephoto_file_get_exif_data(file);
   if (!list)
     return;

   animator = calloc(1, sizeof(Ephoto_Exif_Animator));
   animator->todo_items = NULL;
   animator->count = 0;
   animator->processed = 0;

   popup = elm_popup_add(ephoto->win);
   elm_popup_scrollable_set(popup, EINA_TRUE);
   elm_object_part_text_set(popup, "title,text", _("Image Properties"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);
   evas_object_smart_callback_add(popup, "block,clicked", _exif_save_cb, popup);

   box = elm_box_add(popup);
   EPHOTO_EXPAND(box);
   EPHOTO_FILL(box);
   evas_object_size_hint_min_set(box, ephoto->config->window_width / 2,
                                 ephoto->config->window_height / 2);
   evas_object_show(box);

   scroller = elm_scroller_add(box);
   EPHOTO_EXPAND(scroller);
   EPHOTO_FILL(scroller);
   elm_box_pack_end(box, scroller);
   evas_object_show(scroller);

   table = elm_table_add(scroller);
   elm_table_homogeneous_set(table, EINA_FALSE);
   EPHOTO_EXPAND(table);
   EPHOTO_FILL(table);
   elm_object_content_set(scroller, table);
   evas_object_show(table);

   animator->parent = table;

   EINA_LIST_FOREACH(list, l, eei)
     {
        animator->todo_items = eina_list_append(animator->todo_items, eei);
        animator->count++;
     }
   animator->todo = ecore_idler_add(_exif_items_process, animator);

   evas_object_data_set(popup, "list", list);
   evas_object_data_set(popup, "ephoto", ephoto);
   elm_object_part_content_set(popup, "default", box);
   evas_object_show(popup);
}
#endif

static void
_complete_ok(void *data, Evas_Object *obj EINA_UNUSED,
             void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto *ephoto = evas_object_data_get(popup, "ephoto");

   evas_object_del(popup);
   elm_object_focus_set(ephoto->pager, EINA_TRUE);
}

static void
_complete(Ephoto *ephoto, const char *title, const char *text)
{
   Evas_Object *popup, *box, *label, *ic, *button;

   popup = elm_popup_add(ephoto->win);
   elm_object_part_text_set(popup, "title,text", title);
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   EPHOTO_EXPAND(box);
   EPHOTO_FILL(box);
   evas_object_show(box);

   label = elm_label_add(box);
   elm_object_text_set(label, text);
   EPHOTO_EXPAND(label);
   EPHOTO_FILL(label);
   elm_box_pack_end(box, label);
   evas_object_show(label);

   ic = elm_icon_add(popup);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "window-close");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Ok"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _complete_ok, popup);
   elm_object_part_content_set(popup, "button1", button);
   evas_object_show(button);

   evas_object_data_set(popup, "ephoto", ephoto);
   elm_object_part_content_set(popup, "default", box);
   evas_object_show(popup);
}

static void
_prompt_cancel(void *data, Evas_Object *obj EINA_UNUSED,
               void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto *ephoto = evas_object_data_get(popup, "ephoto");

   evas_object_del(popup);
   elm_object_focus_set(ephoto->pager, EINA_TRUE);
}

static Evas_Object *
_prompt(Ephoto *ephoto, const char *title, const char *text)
{
   Evas_Object *popup, *box, *label;

   popup = elm_popup_add(ephoto->win);
   elm_object_part_text_set(popup, "title,text", title);
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   EPHOTO_WEIGHT(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   EPHOTO_FILL(box);
   evas_object_show(box);

   label = elm_label_add(box);
   elm_object_text_set(label, text);
   EPHOTO_EXPAND(label);
   EPHOTO_FILL(label);
   elm_box_pack_end(box, label);
   evas_object_show(label);

   evas_object_data_set(popup, "ephoto", ephoto);
   elm_object_part_content_set(popup, "default", box);

   return popup;
}

static void
_save_image_as_overwrite(void *data, Evas_Object *obj EINA_UNUSED,
                         void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   char tmp_path[PATH_MAX];
   const char *base;
   const char *file = evas_object_data_get(popup, "file");
   Ephoto *ephoto = evas_object_data_get(popup, "ephoto");
   Ephoto_Entry *entry = evas_object_data_get(popup, "entry");
   Evas_Object *image = evas_object_data_get(popup, "image");
   Eina_Bool success;

   if (ecore_file_exists(file))
     {
        base = ecore_file_file_get(file);
        snprintf(tmp_path, PATH_MAX, "%s/ephoto/%s", efreet_config_home_get(), base);
        success = ecore_file_cp(file, tmp_path);
        if (!success)
          {
             _complete(ephoto, _("Save Failed"),
                       _("Error: Image could not be saved here!"));
             ephoto_single_browser_entry_set(ephoto->single_browser, entry);
             evas_object_del(popup);
             elm_object_focus_set(ephoto->pager, EINA_TRUE);
             ecore_file_unlink(tmp_path);
             return;
          }
        success = ecore_file_unlink(file);
        if (!success)
          {
             ecore_file_cp(tmp_path, file);
             _complete(ephoto, _("Save Failed"),
                       _("Error: Image could not be saved here!"));
             ephoto_single_browser_entry_set(ephoto->single_browser, entry);
             evas_object_del(popup);
             elm_object_focus_set(ephoto->pager, EINA_TRUE);
             ecore_file_unlink(tmp_path);
             return;
          }
     }
   ephoto_single_browser_path_pending_set(ephoto->single_browser, file);
   success =
     evas_object_image_save(image, file,
                            NULL, NULL);
   if (!success)
     {
        if (!ecore_file_exists(file) && strlen(tmp_path))
          ecore_file_cp(tmp_path, file);
        _complete(ephoto, _("Save Failed"),
                  _("Error: Image could not be saved here!"));
        ephoto_single_browser_path_pending_unset(ephoto->single_browser);
     }
   if (strlen(tmp_path))
     ecore_file_unlink(tmp_path);
   evas_object_del(popup);
   elm_object_focus_set(ephoto->pager, EINA_TRUE);
}

static void
_upload_entry_anchor_bt(void *data, Evas_Object *obj EINA_UNUSED,
                        void *event_info EINA_UNUSED)
{
   char buf[PATH_MAX];
   Evas_Object *av = data;
   const char *link = evas_object_data_get(av, "link");

   elm_entry_anchor_hover_end(av);
#ifdef _WIN32
   snprintf(buf, PATH_MAX, "start %s", link);
#else
   snprintf(buf, PATH_MAX, "xdg-open %s", link);
#endif
   ecore_exe_run(buf, NULL);
}

static void
_upload_entry_anchor(void *data, Evas_Object *obj, void *event_info)
{
   Evas_Object *av = data;
   Evas_Object *button;
   Elm_Entry_Anchor_Hover_Info *ei = event_info;

   button = elm_button_add(obj);
   elm_object_text_set(button, _("Open Link In Browser"));
   elm_object_part_content_set(ei->hover, "middle", button);
   evas_object_smart_callback_add(button, "clicked", _upload_entry_anchor_bt,
                                  av);
   evas_object_show(button);
}

static void
_upload_image_url_copy(void *data, Evas_Object *obj EINA_UNUSED,
                       void *event_info EINA_UNUSED)
{
   Evas_Object *entry = data;

   elm_entry_select_all(entry);
   elm_entry_selection_copy(entry);
   elm_entry_select_none(entry);
}

static Eina_Bool
_upload_image_complete_cb(void *data, int ev_type EINA_UNUSED, void *event)
{
   Evas_Object *ppopup = data;
   Ephoto *ephoto = evas_object_data_get(ppopup, "ephoto");
   Ecore_Con_Event_Url_Complete *ev = event;
   Ecore_Event_Handler *handler;
   Evas_Object *popup, *box, *hbox, *label, *entry, *ic, *button;
   int ret;

   if (ev->url_con != ephoto->url_up)
     return ECORE_CALLBACK_RENEW;

   evas_object_del(ppopup);
   elm_object_focus_set(ephoto->pager, EINA_TRUE);

   popup = elm_popup_add(ephoto->win);
   elm_object_part_text_set(popup, "title,text", _("Image Uploaded"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   EPHOTO_WEIGHT(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   EPHOTO_FILL(box);
   evas_object_show(box);

   label = elm_label_add(box);
   EPHOTO_EXPAND(label);
   EPHOTO_FILL(label);
   elm_box_pack_end(box, label);
   evas_object_show(label);

   hbox = elm_box_add(box);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   EPHOTO_WEIGHT(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   EPHOTO_FILL(hbox);
   elm_box_pack_end(box, hbox);
   evas_object_show(hbox);

   entry = elm_entry_add(hbox);
   elm_entry_anchor_hover_style_set(entry, "popout");
   elm_entry_anchor_hover_parent_set(entry, ephoto->pager);
   elm_entry_editable_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_context_menu_disabled_set(entry, EINA_TRUE);
   elm_scroller_policy_set(entry, ELM_SCROLLER_POLICY_OFF,
                           ELM_SCROLLER_POLICY_OFF);
   EPHOTO_EXPAND(entry);
   EPHOTO_FILL(entry);
   evas_object_smart_callback_add(entry, "anchor,hover,opened",
                                  _upload_entry_anchor, entry);
   elm_box_pack_end(hbox, entry);
   evas_object_show(entry);

   ic = elm_icon_add(hbox);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_HORIZONTAL, 1, 1);
   ret = elm_icon_standard_set(ic, "edit-copy");

   button = elm_button_add(hbox);
   elm_object_part_content_set(button, "icon", ic);
   if (!ret)
     elm_object_text_set(button, _("Copy"));
   evas_object_smart_callback_add(button, "clicked", _upload_image_url_copy,
                                  entry);
   elm_box_pack_end(hbox, button);
   evas_object_show(button);

   ic = elm_icon_add(popup);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "window-close");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Ok"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _prompt_cancel,
                                  popup);
   elm_object_part_content_set(popup, "button1", button);
   evas_object_show(button);

   evas_object_data_set(popup, "ephoto", ephoto);
   elm_object_part_content_set(popup, "default", box);
   evas_object_show(popup);

   EINA_LIST_FREE(ephoto->upload_handlers,
                   handler)
     ecore_event_handler_del(handler);

   if (!ephoto->url_ret || ev->status != 200)
     {
        elm_object_text_set(label,
                            _("There was an error uploading your image!"));
        elm_entry_single_line_set(entry, EINA_TRUE);
        elm_object_text_set(entry, ephoto->upload_error);
        evas_object_show(popup);
        ecore_con_url_free(ephoto->url_up);
        ephoto->url_up = NULL;
        free(ephoto->upload_error);
        ephoto->upload_error = NULL;
        return EINA_FALSE;
     }
   else
     {
        char buf[PATH_MAX], link[PATH_MAX];

        snprintf(buf, PATH_MAX, "<a href=\"%s\"><link>%s</link</a>",
                 ephoto->url_ret, ephoto->url_ret);
        snprintf(link, PATH_MAX, "%s", ephoto->url_ret);
        evas_object_data_set(entry, "link", strdup(link));
        elm_object_text_set(label,
                            _("Your image was uploaded to the following link:"));
        elm_entry_single_line_set(entry, EINA_TRUE);
        elm_object_text_set(entry, buf);
        evas_object_show(popup);
        ecore_con_url_free(ephoto->url_up);
        ephoto->url_up = NULL;
        free(ephoto->url_ret);
        ephoto->url_ret = NULL;
        return ECORE_CALLBACK_RENEW;
     }
}

static Eina_Bool
_upload_image_xml_parse(void *data, Eina_Simple_XML_Type type,
                        const char *content, unsigned offset EINA_UNUSED,
                        unsigned length EINA_UNUSED)
{
   Ephoto *ephoto = data;
   char *linkf, *linkl;

   if (type == EINA_SIMPLE_XML_OPEN)
     {
        if (!strncmp("link>", content, strlen("link>")))
          {
             linkf = strchr(content, '>') + 1;
             linkl = strtok(linkf, "<");
             ephoto->url_ret = strdup(linkl);
          }
     }
   return EINA_TRUE;
}

static Eina_Bool
_upload_image_cb(void *data, int ev_type EINA_UNUSED, void *event)
{
   Ephoto *ephoto = data;
   Ecore_Con_Event_Url_Data *ev = event;
   const char *string = (const char *)ev->data;

   if (ev->url_con != ephoto->url_up)
     return EINA_TRUE;
   eina_simple_xml_parse(string, strlen(string) + 1, EINA_TRUE,
                         _upload_image_xml_parse, ephoto);
   if (!ephoto->url_ret)
     ephoto->upload_error = strdup(string);

   return EINA_FALSE;
}

static void
_new_dir_confirm(void *data, Evas_Object *obj EINA_UNUSED,
                 void *event_data EINA_UNUSED)
{
   Evas_Object *popup = data;
   Evas_Object *entry = evas_object_data_get(popup, "entry");
   Ephoto *ephoto = evas_object_data_get(popup, "ephoto");
   const char *file = evas_object_data_get(popup, "file");
   const char *text = elm_object_text_get(entry);
   char new_file_name[PATH_MAX];
   int ret;

   snprintf(new_file_name, PATH_MAX, "%s/%s", file, text);
   ret = ecore_file_mkdir(new_file_name);
   if (!ret)
     {
        _complete(ephoto, _("Error"),
                  _("There was an error creating this directory."));
     }
   evas_object_del(popup);
   elm_object_focus_set(ephoto->pager, EINA_TRUE);
}

static void
_new_dir_cancel(void *data, Evas_Object *obj EINA_UNUSED,
                void *event_data EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto *ephoto = evas_object_data_get(popup, "ephoto");

   evas_object_del(popup);
   elm_object_focus_set(ephoto->pager, EINA_TRUE);
}

static void
_new_dir(Ephoto *ephoto, const char *file)
{
   Evas_Object *popup, *box, *entry, *button, *ic;

   popup = elm_popup_add(ephoto->win);
   elm_object_part_text_set(popup, "title,text", _("New Directory"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);
   evas_object_data_set(popup, "ephoto", ephoto);
   evas_object_data_set(popup, "file", file);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   EPHOTO_WEIGHT(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   EPHOTO_FILL(box);
   evas_object_show(box);

   entry = elm_entry_add(box);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_TRUE);
   elm_object_text_set(entry, _("New Directory"));
   elm_entry_select_all(entry);
   elm_scroller_policy_set(entry, ELM_SCROLLER_POLICY_OFF,
                           ELM_SCROLLER_POLICY_OFF);
   EPHOTO_EXPAND(entry);
   EPHOTO_FILL(entry);
   elm_box_pack_end(box, entry);
   evas_object_show(entry);
   evas_object_data_set(popup, "entry", entry);

   ic = elm_icon_add(popup);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "document-save");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Save"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _new_dir_confirm, popup);
   elm_object_part_content_set(popup, "button1", button);
   evas_object_show(button);

   ic = elm_icon_add(popup);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "window-close");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Cancel"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _new_dir_cancel, popup);
   elm_object_part_content_set(popup, "button2", button);
   evas_object_show(button);

   elm_object_part_content_set(popup, "default", box);
   evas_object_show(popup);
}

static void
_rename_confirm(void *data, Evas_Object *obj EINA_UNUSED,
                void *event_data EINA_UNUSED)
{
   Evas_Object *popup = data;
   Evas_Object *entry = evas_object_data_get(popup, "entry");
   Ephoto *ephoto = evas_object_data_get(popup, "ephoto");
   const char *file = evas_object_data_get(popup, "file");
   const char *text = elm_object_text_get(entry);
   char *escaped = ecore_file_escape_name(text);
   char new_file_name[PATH_MAX], dir[PATH_MAX];
   int ret;

   if (!escaped)
     {
        evas_object_del(popup);
        elm_object_focus_set(ephoto->pager, EINA_TRUE);
        return;
     }
   snprintf(dir, PATH_MAX, "%s", file);
   if (ecore_file_is_dir(file))
     snprintf(new_file_name, PATH_MAX, "%s/%s", ecore_file_dir_get(dir), text);
   else
     snprintf(new_file_name, PATH_MAX, "%s/%s.%s", ecore_file_dir_get(dir), escaped,
              strrchr(dir, '.') + 1);
   ret = ecore_file_mv(file, new_file_name);
   if (!ret)
     {
        if (ecore_file_is_dir(new_file_name))
          _complete(ephoto, _("Error"),
                    _("There was an error renaming this directory."));
        else
          _complete(ephoto, _("Error"),
                    _("There was an error renaming this file."));
     }
   else
     {
        if (ephoto->state == EPHOTO_STATE_SINGLE)
          ephoto_single_browser_path_pending_set(ephoto->single_browser,
                                                 new_file_name);
     }
   evas_object_del(popup);
   elm_object_focus_set(ephoto->pager, EINA_TRUE);
   free(escaped);
}

static void
_rename_cancel(void *data, Evas_Object *obj EINA_UNUSED,
               void *event_data EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto *ephoto = evas_object_data_get(popup, "ephoto");

   evas_object_del(popup);
   elm_object_focus_set(ephoto->pager, EINA_TRUE);
}

static void
_rename_file(Ephoto *ephoto, const char *file)
{
   Evas_Object *popup, *box, *entry, *button, *ic;
   char buf[PATH_MAX], *bn, *string;

   popup = elm_popup_add(ephoto->win);
   if (ecore_file_is_dir(file))
     elm_object_part_text_set(popup, "title, text", _("Rename Directory"));
   else
     elm_object_part_text_set(popup, "title,text", _("Rename File"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);
   evas_object_data_set(popup, "ephoto", ephoto);
   evas_object_data_set(popup, "file", file);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   EPHOTO_WEIGHT(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   EPHOTO_FILL(box);
   evas_object_show(box);

   snprintf(buf, PATH_MAX, "%s", file);
   bn = basename(buf);
   string = ecore_file_strip_ext(bn);

   entry = elm_entry_add(box);
   elm_entry_single_line_set(entry, EINA_TRUE);
   elm_entry_scrollable_set(entry, EINA_TRUE);
   elm_entry_editable_set(entry, EINA_TRUE);
   elm_object_text_set(entry, string);
   elm_entry_select_all(entry);
   elm_scroller_policy_set(entry, ELM_SCROLLER_POLICY_OFF,
                           ELM_SCROLLER_POLICY_OFF);
   EPHOTO_EXPAND(entry);
   EPHOTO_FILL(entry);
   elm_box_pack_end(box, entry);
   evas_object_show(entry);
   evas_object_data_set(popup, "entry", entry);

   free(string);

   ic = elm_icon_add(popup);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "document-save");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Rename"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _rename_confirm, popup);
   elm_object_part_content_set(popup, "button1", button);
   evas_object_show(button);

   ic = elm_icon_add(popup);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "window-close");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Cancel"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _rename_cancel, popup);
   elm_object_part_content_set(popup, "button2", button);
   evas_object_show(button);

   elm_object_part_content_set(popup, "default", box);
   evas_object_show(popup);
}

static Evas_Object *
_processing(Ephoto *ephoto, const char *title, const char *text)
{
   Evas_Object *popup, *box, *label, *pb;

   popup = elm_popup_add(ephoto->win);
   elm_object_part_text_set(popup, "title,text", title);
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   EPHOTO_WEIGHT(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   EPHOTO_FILL(box);
   evas_object_show(box);

   label = elm_label_add(box);
   elm_object_text_set(label, text);
   EPHOTO_EXPAND(label);
   EPHOTO_FILL(label);
   elm_box_pack_end(box, label);
   evas_object_show(label);

   pb = elm_progressbar_add(box);
   EPHOTO_WEIGHT(pb, EVAS_HINT_EXPAND, EVAS_HINT_FILL);
   EPHOTO_ALIGN(pb, EVAS_HINT_FILL, 0.5);
   elm_object_style_set(pb, "wheel");
   elm_progressbar_pulse_set(pb, EINA_TRUE);
   elm_box_pack_end(box, pb);
   evas_object_show(pb);
   elm_progressbar_pulse(pb, EINA_TRUE);

   evas_object_data_set(popup, "ephoto", ephoto);
   elm_object_part_content_set(popup, "default", box);
   return popup;
}

static void
_thread_end_cb(void *data, Ecore_Thread *et EINA_UNUSED)
{
   Ephoto *ephoto = data;
   char msg[PATH_MAX];

   if (ephoto->file_errors > 0)
     {
        snprintf(msg, PATH_MAX, "%s %d %s.",
                 _("There was an error completing your action on"), ephoto->file_errors,
                 ngettext("file", "files", ephoto->file_errors));
        _complete(ephoto, _("Error"), msg);
     }
   ephoto->file_pos = NULL;
   ephoto->file_errors = 0;
   ephoto->destination = NULL;

   evas_object_del(ephoto->file_popup);
   elm_object_focus_set(ephoto->pager, EINA_TRUE);
}

static void
_move_thread_cb(void *data, Ecore_Thread *et EINA_UNUSED)
{
   Ephoto *ephoto = data;
   const char *destination = ephoto->destination;
   const char *file;

   if (!ephoto->file_pos)
     ephoto->file_pos = eina_list_nth(ephoto->file_pos, 0);
   EINA_LIST_FREE(ephoto->file_pos, file)
     {
        if (!file)
          break;
        if (ecore_file_exists(file) && ecore_file_is_dir(destination) &&
            evas_object_image_extension_can_load_get(file))
          {
             char dest[PATH_MAX], fp[PATH_MAX], extra[PATH_MAX];
             int ret;

             snprintf(fp, PATH_MAX, "%s", file);
             snprintf(dest, PATH_MAX, "%s/%s", destination, basename(fp));
             if (ecore_file_exists(dest))
               {
                  snprintf(extra, PATH_MAX, "%s/CopyOf%s", destination,
                           basename(fp));
                  if (ecore_file_exists(extra))
                    {
                       int count;

                       for (count = 2; ecore_file_exists(extra); count++)
                         {
                            memset(extra, 0, sizeof(extra));
                            snprintf(extra, PATH_MAX, "%s/Copy%dOf%s",
                                     destination, count, basename(fp));
                         }
                    }
                  ret = ecore_file_mv(file, extra);
               }
             else
               ret = ecore_file_mv(file, dest);
             if (!ret)
               ephoto->file_errors++;
          }
     }
}

static void
_move_files(Ephoto *ephoto, Eina_List *files,
            const char *destination)
{
   Evas_Object *popup = _processing(ephoto, _("Moving Files"),
                                    _("Please wait while your files are moved."));

   ephoto->file_popup = popup;
   ephoto->destination = destination;
   evas_object_show(popup);

   ephoto->file_pos = eina_list_clone(files);
   if (eina_list_count(files))
     eina_list_free(files);
   ephoto->file_thread = ecore_thread_run(_move_thread_cb,
                                          _thread_end_cb, _thread_end_cb, ephoto);
}

static void
_copy_thread_cb(void *data, Ecore_Thread *et EINA_UNUSED)
{
   Ephoto *ephoto = data;
   const char *destination = ephoto->destination;
   const char *file;

   if (!ephoto->file_pos)
     ephoto->file_pos = eina_list_nth(ephoto->file_pos, 0);
   EINA_LIST_FREE(ephoto->file_pos, file)
     {
        if (ecore_file_exists(file) && ecore_file_is_dir(destination) &&
            evas_object_image_extension_can_load_get(file))
          {
             char dest[PATH_MAX], fp[PATH_MAX], extra[PATH_MAX];
             int ret;

             snprintf(fp, PATH_MAX, "%s", file);
             snprintf(dest, PATH_MAX, "%s/%s", destination, basename(fp));
             if (ecore_file_exists(dest))
               {
                  snprintf(extra, PATH_MAX, "%s/CopyOf%s", destination,
                           basename(fp));
                  if (ecore_file_exists(extra))
                    {
                       int count;

                       for (count = 2; ecore_file_exists(extra); count++)
                         {
                            memset(extra, 0, PATH_MAX);
                            snprintf(extra, PATH_MAX, "%s/Copy%dOf%s",
                                     destination, count, basename(fp));
                         }
                    }
                  ret = ecore_file_cp(file, extra);
               }
             else
               ret = ecore_file_cp(file, dest);
             if (!ret)
               ephoto->file_errors++;
          }
     }
}

static void
_copy_files(Ephoto *ephoto, Eina_List *files,
            const char *destination)
{
   Evas_Object *popup = _processing(ephoto, _("Copying Files"),
                                    _("Please wait while your files are copied."));
   ephoto->file_popup = popup;
   ephoto->destination = destination;
   evas_object_show(popup);

   ephoto->file_pos = eina_list_clone(files);
   if (eina_list_count(files))
     eina_list_free(files);
   ephoto->file_thread = ecore_thread_run(_copy_thread_cb,
                                          _thread_end_cb, NULL, ephoto);
}

static void
_delete_thread_cb(void *data, Ecore_Thread *et EINA_UNUSED)
{
   Ephoto *ephoto = data;
   const char *file;

   if (!ecore_file_exists(ephoto->trash_path))
     ecore_file_mkpath(ephoto->trash_path);

   if (!ephoto->file_pos)
     ephoto->file_pos = eina_list_nth(ephoto->file_pos, 0);
   EINA_LIST_FREE(ephoto->file_pos, file)
     {
        if (!file)
          break;
        if (ecore_file_exists(file) && ecore_file_is_dir(ephoto->trash_path))
          {
             char dest[PATH_MAX], fp[PATH_MAX], extra[PATH_MAX];
             int ret;
             struct stat s;
#ifdef _WIN32
             if (stat(file, &s) == 0)
#else
             if (lstat(file, &s) == 0)
#endif
               {
                  if (S_ISLNK(s.st_mode))
                    {
                       ret = ecore_file_unlink(file);
                    }
                  else
                    {
                       snprintf(fp, PATH_MAX, "%s", file);
                       snprintf(dest, PATH_MAX, "%s/%s", ephoto->trash_path, basename(fp));
                       if (ecore_file_exists(dest))
                         {
                            snprintf(extra, PATH_MAX, "%s/CopyOf%s", ephoto->trash_path,
                                     basename(fp));
                            if (ecore_file_exists(extra))
                              {
                                 int count;

                                 for (count = 2; ecore_file_exists(extra); count++)
                                   {
                                      memset(extra, 0, sizeof(extra));
                                      snprintf(extra, PATH_MAX, "%s/Copy%dOf%s",
                                               ephoto->trash_path, count, basename(fp));
                                   }
                              }
                            ret = ecore_file_mv(file, extra);
                         }
                       else
                         ret = ecore_file_mv(file, dest);
                    }
                  if (!ret)
                    ephoto->file_errors++;
               }
          }
     }
}

static void
_delete_files(Ephoto *ephoto, Eina_List *files)
{
   Evas_Object *popup = _processing(ephoto, _("Deleting Files"),
                                    _("Please wait while your files are deleted."));

   ephoto->file_popup = popup;
   evas_object_show(popup);

   ephoto->file_pos = eina_list_clone(files);
   if (eina_list_count(files))
     eina_list_free(files);
   ephoto->file_thread = ecore_thread_run(_delete_thread_cb,
                                          _thread_end_cb, NULL, ephoto);
}

static void
_delete_dir_thread_cb(void *data, Ecore_Thread *et EINA_UNUSED)
{
   Ephoto *ephoto = data;
   const char *dir = eina_list_data_get(ephoto->file_pos);

   if (!ecore_file_exists(ephoto->trash_path))
     ecore_file_mkpath(ephoto->trash_path);

   if (dir)
     {
        char fp[PATH_MAX], dest[PATH_MAX], extra[PATH_MAX];
        int ret;

        snprintf(fp, PATH_MAX, "%s", dir);
        snprintf(dest, PATH_MAX, "%s/%s", ephoto->trash_path, basename(fp));
        if (ecore_file_exists(dir) && ecore_file_is_dir(ephoto->trash_path))
          {
             if (ecore_file_exists(dest))
               {
                  snprintf(extra, PATH_MAX, "%s/CopyOf%s", ephoto->trash_path,
                           basename(fp));
                  if (ecore_file_exists(extra))
                    {
                       int count;
                       for (count = 2; ecore_file_exists(extra); count++)
                         {
                            memset(extra, 0, sizeof(extra));
                            snprintf(extra, PATH_MAX, "%s/Copy%dOf%s",
                                     ephoto->trash_path, count, basename(fp));
                         }
                    }
                  ret = ecore_file_mv(dir, extra);
               }
             else
               ret = ecore_file_mv(dir, dest);
             if (!ret)
               ephoto->file_errors++;
          }
     }
   if (!dir)
     ephoto->file_errors++;
}

static void
_delete_dir(Ephoto *ephoto, Eina_List *files)
{
   Evas_Object *popup = _processing(ephoto, _("Deleting Directory"),
                                    _("Please wait while your directory is deleted."));
   ephoto->file_popup = popup;
   evas_object_show(popup);

   ephoto->file_pos = eina_list_clone(files);
   ephoto->file_thread = ecore_thread_run(_delete_dir_thread_cb,
                                          _thread_end_cb, NULL, ephoto);
}

static void
_empty_trash_thread_cb(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Ephoto *ephoto = data;
   const char *file;

   if (!ephoto->file_pos)
     ephoto->file_pos = eina_list_nth(ephoto->file_pos, 0);
   EINA_LIST_FREE(ephoto->file_pos, file)
     {
        if (!file)
          break;
        if (ecore_file_exists(file))
          {
             int ret;

             if (ecore_file_is_dir(file))
               ret = ecore_file_recursive_rm(file);
             else
               ret = ecore_file_unlink(file);
             if (!ret)
               ephoto->file_errors++;
          }
     }
}

static void
_empty_trash(Ephoto *ephoto, Eina_List *files)
{
   Evas_Object *popup = _processing(ephoto, _("Emptying Trash"),
                                    _("Please wait while your files are deleted."));
   ephoto->file_popup = popup;
   evas_object_show(popup);

   ephoto->file_pos = eina_list_clone(files);
   if (eina_list_count(files))
     eina_list_free(files);
   ephoto->file_thread = ecore_thread_run(_empty_trash_thread_cb,
                                          _thread_end_cb, NULL, ephoto);
}

static void
_prompt_upload_apply(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Evas_Object *ppopup = data;
   Evas_Object *popup;
   Ephoto *ephoto = evas_object_data_get(ppopup, "ephoto");
   Ephoto_Entry *entry = evas_object_data_get(ppopup, "entry");
   char buf[PATH_MAX];
   FILE *f;
   unsigned char *fdata;
   int fsize;
   int res;

   evas_object_del(ppopup);
   popup = _processing(ephoto, _("Upload Image"),
                       ("Please wait while your image is uploaded."));
   evas_object_show(popup);

   f = fopen(entry->path, "rb");
   if (!f)
     {
        CRIT("unable to open '%s': %s", entry->path, strerror(errno));
        evas_object_del(popup);
        return;
     }
   res = fseek(f, 0, SEEK_END);
   if (!res) CRIT("fseek() failed on file '%s': %s", entry->path, strerror(errno));
   fsize = ftell(f);
   if (fsize == -1)
     {
        CRIT("ftell() failed on file '%s': %s", entry->path, strerror(errno));
        fclose(f);
        evas_object_del(popup);
        return;
     }
   rewind(f);
   fdata = malloc(fsize + 1);
   res = fread(fdata, fsize, 1, f);
   if (res < 1) CRIT("fread() failed on file '%s': %s", entry->path, strerror(errno));
   fdata[fsize] = '\0';
   res = fclose(f);
   if (!res) CRIT("fclose() failed on file '%s': %s", entry->path, strerror(errno));

   snprintf(buf, PATH_MAX, "image=%s", fdata);

   ephoto->upload_handlers =
     eina_list_append(ephoto->upload_handlers,
                      ecore_event_handler_add(ECORE_CON_EVENT_URL_DATA,
                                              _upload_image_cb, ephoto));
   ephoto->upload_handlers =
     eina_list_append(ephoto->upload_handlers,
                      ecore_event_handler_add(ECORE_CON_EVENT_URL_COMPLETE,
                                              _upload_image_complete_cb, popup));

   ephoto->url_up = ecore_con_url_new("https://api.imgur.com/3/image.xml");
   ecore_con_url_additional_header_add(ephoto->url_up, "Authorization",
                                       "Client-ID 67aecc7e6662370");
   ecore_con_url_http_version_set(ephoto->url_up, ECORE_CON_URL_HTTP_VERSION_1_0);
   ecore_con_url_post(ephoto->url_up, fdata, fsize, NULL);
}

static void
_prompt_save_image_apply(void *data, Evas_Object *obj EINA_UNUSED,
                         void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto *ephoto = evas_object_data_get(popup, "ephoto");
   Ephoto_Entry *entry = evas_object_data_get(popup, "entry");
   Evas_Object *image = evas_object_data_get(popup, "image");
   char tmp_path[PATH_MAX];
   const char *base;
   Eina_Bool success;

   if (ecore_file_exists(entry->path))
     {
        base = ecore_file_file_get(entry->path);
        snprintf(tmp_path, PATH_MAX, "%s/ephoto/%s", efreet_config_home_get(), base);
        success = ecore_file_cp(entry->path, tmp_path);
        if (!success)
          {
             _complete(ephoto, _("Save Failed"),
                       _("Error: Image could not be saved here!"));
             ephoto_single_browser_entry_set(ephoto->single_browser, entry);
             evas_object_del(popup);
             elm_object_focus_set(ephoto->pager, EINA_TRUE);
             ecore_file_unlink(tmp_path);
             return;
          }
        success = ecore_file_unlink(entry->path);
        if (!success)
          {
             ecore_file_cp(tmp_path, entry->path);
             _complete(ephoto, _("Save Failed"),
                       _("Error: Image could not be saved here!"));
             ephoto_single_browser_entry_set(ephoto->single_browser, entry);
             evas_object_del(popup);
             elm_object_focus_set(ephoto->pager, EINA_TRUE);
             ecore_file_unlink(tmp_path);
             return;
          }
     }
   success =
     evas_object_image_save(image, entry->path,
                            NULL, NULL);
   if (!success)
     {
        if (!ecore_file_exists(entry->path) && strlen(tmp_path))
          ecore_file_cp(tmp_path, entry->path);
        _complete(ephoto, _("Save Failed"),
                  _("Error: Image could not be saved here!"));
     }
   if (strlen(tmp_path))
     ecore_file_unlink(tmp_path);
   ephoto_single_browser_entry_set(ephoto->single_browser, entry);
   evas_object_del(popup);
   elm_object_focus_set(ephoto->pager, EINA_TRUE);
}

static void
_prompt_save_image_as_apply(void *data, Evas_Object *obj, void *event_info)
{
   const char *selected = event_info;
   Evas_Object *opopup = data;
   Ephoto *ephoto = evas_object_data_get(opopup, "ephoto");
   Ephoto_Entry *entry = evas_object_data_get(opopup, "entry");

   if (selected)
     {
        Evas_Object *image = evas_object_data_get(opopup, "image");
        Eina_Bool success;

        char buf[PATH_MAX];
        const char *ex, *ext;

        ex = strrchr(selected, '.');
        if (!ex)
          {
             snprintf(buf, PATH_MAX, "%s.jpg", selected);
          }
        else
          {
             ext = eina_stringshare_add((strrchr(selected, '.') + 1));
             if (ext)
               {
                  if (!_ephoto_file_image_can_save(ext))
                    {
                       eina_stringshare_del(ext);
                       snprintf(buf, PATH_MAX, "%s.jpg", selected);
                    }
                  else
                    {
                       eina_stringshare_del(ext);
                       snprintf(buf, PATH_MAX, "%s", selected);
                    }
               }
             else
               {
                  eina_stringshare_del(ext);
                  snprintf(buf, PATH_MAX, "%s", selected);
               }
          }
        if (ecore_file_exists(buf))
          {
             Evas_Object *popup, *ic, *button;

             popup = _prompt(ephoto, _("Overwrite Image"),
                             _("Are you sure you want to overwrite this image?"));

             ic = elm_icon_add(popup);
             evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL,
                                              1, 1);
             elm_icon_standard_set(ic, "document-save");

             button = elm_button_add(popup);
             elm_object_text_set(button, _("Yes"));
             elm_object_part_content_set(button, "icon", ic);
             evas_object_smart_callback_add(button, "clicked",
                                            _save_image_as_overwrite, popup);
             elm_object_part_content_set(popup, "button1", button);
             evas_object_show(button);

             ic = elm_icon_add(popup);
             evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL,
                                              1, 1);
             elm_icon_standard_set(ic, "window-close");

             button = elm_button_add(popup);
             elm_object_text_set(button, _("No"));
             elm_object_part_content_set(button, "icon", ic);
             evas_object_smart_callback_add(button, "clicked",
                                            _prompt_cancel, popup);
             elm_object_part_content_set(popup, "button2", button);
             evas_object_show(button);

             evas_object_data_set(popup, "ephoto", ephoto);
             evas_object_data_set(popup, "file", strdup(buf));
             evas_object_data_set(popup, "image", image);
             evas_object_data_set(popup, "entry", entry);
             evas_object_show(popup);
          }
        else
          {
             ephoto_single_browser_path_pending_set
               (ephoto->single_browser, buf);
             success =
               evas_object_image_save(image, buf,
                                      NULL, NULL);
             if (!success)
               {
                  _complete(ephoto, _("Save Failed"),
                            _("Error: Image could not be saved here!"));
                  ephoto_single_browser_path_pending_unset
                    (ephoto->single_browser);
               }
          }
     }
   elm_object_content_unset(opopup);
   evas_object_del(obj);
   evas_object_del(opopup);
   elm_object_focus_set(ephoto->pager, EINA_TRUE);
}

static void
_prompt_empty_apply(void *data, Evas_Object *obj EINA_UNUSED,
                    void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto *ephoto = evas_object_data_get(popup, "ephoto");
   Eina_List *files = evas_object_data_get(popup, "files");

   evas_object_del(popup);
   elm_object_focus_set(ephoto->pager, EINA_TRUE);
   _empty_trash(ephoto, files);
}

static void
_prompt_delete_apply(void *data, Evas_Object *obj EINA_UNUSED,
                     void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto *ephoto = evas_object_data_get(popup, "ephoto");
   Eina_List *files = evas_object_data_get(popup, "files");
   Eina_File_Type *type = evas_object_data_get(popup, "type");

   evas_object_del(popup);
   elm_object_focus_set(ephoto->pager, EINA_TRUE);
   if (*type == EINA_FILE_DIR)
     _delete_dir(ephoto, files);
   else
     _delete_files(ephoto, files);
}

static void
_prompt_move_apply(void *data, Evas_Object *obj EINA_UNUSED,
                   void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto *ephoto = evas_object_data_get(popup, "ephoto");
   Eina_List *files = evas_object_data_get(popup, "files");
   const char *path = evas_object_data_get(popup, "path");

   evas_object_del(popup);
   elm_object_focus_set(ephoto->pager, EINA_TRUE);
   _move_files(ephoto, files, path);
}

static void
_prompt_copy_apply(void *data, Evas_Object *obj EINA_UNUSED,
                   void *event_info EINA_UNUSED)
{
   Evas_Object *popup = data;
   Ephoto *ephoto = evas_object_data_get(popup, "ephoto");
   Eina_List *files = evas_object_data_get(popup, "files");
   const char *path = evas_object_data_get(popup, "path");

   evas_object_del(popup);
   elm_object_focus_set(ephoto->pager, EINA_TRUE);
   _copy_files(ephoto, files, path);
}

void
ephoto_file_save_image(Ephoto *ephoto, Ephoto_Entry *entry, Evas_Object *image)
{
   Evas_Object *popup, *ic, *button;

   if (!_ephoto_file_image_can_save(strrchr(entry->label, '.') + 1))
     {
        _complete(ephoto, _("Save Failed"),
                  _("Error: Image could not be saved here!"));
        return;
     }

   popup = _prompt(ephoto, _("Save Image"),
                   _("Are you sure you want to overwrite this image?"));

   ic = elm_icon_add(popup);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "document-save");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Yes"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _prompt_save_image_apply, popup);
   elm_object_part_content_set(popup, "button1", button);
   evas_object_show(button);

   ic = elm_icon_add(popup);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "window-close");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("No"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _prompt_cancel, popup);
   elm_object_part_content_set(popup, "button2", button);
   evas_object_show(button);

   evas_object_data_set(popup, "ephoto", ephoto);
   evas_object_data_set(popup, "entry", entry);
   evas_object_data_set(popup, "image", image);
   evas_object_show(popup);
}

void
ephoto_file_save_image_as(Ephoto *ephoto, Ephoto_Entry *entry, Evas_Object *image)
{
   Evas_Object *popup, *fsel, *rect, *table;
   int h;

   evas_object_geometry_get(ephoto->win, 0, 0, 0, &h);

   popup = elm_popup_add(ephoto->win);
   elm_popup_scrollable_set(popup, EINA_TRUE);
   elm_object_part_text_set(popup, "title,text", _("Save Image As"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   table = elm_table_add(popup);
   EPHOTO_EXPAND(table);
   EPHOTO_FILL(table);

   rect = elm_box_add(popup);
   evas_object_size_hint_min_set(rect, 0, h / 1.5);
   EPHOTO_EXPAND(rect);
   EPHOTO_FILL(rect);
   elm_table_pack(table, rect, 0, 0, 1, 1);
   evas_object_show(rect);

   fsel = elm_fileselector_add(table);
   elm_fileselector_is_save_set(fsel, EINA_TRUE);
   elm_fileselector_expandable_set(fsel, EINA_FALSE);
   elm_fileselector_path_set(fsel, ephoto->config->directory);
   elm_fileselector_current_name_set(fsel, entry->basename);
   elm_fileselector_mime_types_filter_append(fsel, "image/*",
                                             _("Image Files"));
   EPHOTO_EXPAND(fsel);
   EPHOTO_FILL(fsel);
   evas_object_smart_callback_add(fsel, "done", _prompt_save_image_as_apply, popup);
   elm_table_pack(table, fsel, 0, 0, 1, 1);
   evas_object_show(fsel);

   evas_object_show(table);
   evas_object_data_set(popup, "ephoto", ephoto);
   evas_object_data_set(popup, "entry", entry);
   evas_object_data_set(popup, "image", image);
   elm_object_content_set(popup, table);
   evas_object_show(popup);
}

void
ephoto_file_upload_image(Ephoto *ephoto, Ephoto_Entry *entry)
{
   Evas_Object *popup, *ic, *button;

   popup =
     _prompt(ephoto, _("Upload Image"),
             _("Are you sure you want to upload this image publically to imgur.com?"));

   ic = elm_icon_add(popup);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "document-save");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("Yes"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _prompt_upload_apply,
                                  popup);
   elm_object_part_content_set(popup, "button1", button);
   evas_object_show(button);

   ic = elm_icon_add(popup);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "window-close");

   button = elm_button_add(popup);
   elm_object_text_set(button, _("No"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _prompt_cancel,
                                  popup);
   elm_object_part_content_set(popup, "button2", button);
   evas_object_show(button);

   evas_object_data_set(popup, "ephoto", ephoto);
   evas_object_data_set(popup, "entry", entry);

   evas_object_show(popup);
}

void
ephoto_file_new_dir(Ephoto *ephoto, const char *path)
{
   _new_dir(ephoto, path);
}

void
ephoto_file_rename(Ephoto *ephoto, const char *path)
{
   _rename_file(ephoto, path);
}

void
ephoto_file_move(Ephoto *ephoto, Eina_List *files, const char *path)
{
   if (ephoto->config->prompts)
     {
        Evas_Object *ic, *button;
        Evas_Object *popup;
        char move_dir[PATH_MAX];

        snprintf(move_dir, PATH_MAX, "%s:<br> %s?",
                 _("Are you sure you want to move these files to"), path);

        popup = _prompt(ephoto, _("Move Files"), move_dir);

        ic = elm_icon_add(popup);
        evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1,
                                         1);
        elm_icon_standard_set(ic, "document-save");

        button = elm_button_add(popup);
        elm_object_text_set(button, _("Yes"));
        elm_object_part_content_set(button, "icon", ic);
        evas_object_smart_callback_add(button, "clicked",
                                       _prompt_move_apply, popup);
        elm_object_part_content_set(popup, "button1", button);
        evas_object_show(button);

        ic = elm_icon_add(popup);
        evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1,
                                         1);
        elm_icon_standard_set(ic, "window-close");

        button = elm_button_add(popup);
        elm_object_text_set(button, _("No"));
        elm_object_part_content_set(button, "icon", ic);
        evas_object_smart_callback_add(button, "clicked", _prompt_cancel,
                                       popup);
        elm_object_part_content_set(popup, "button2", button);
        evas_object_show(button);

        evas_object_data_set(popup, "files", files);
        evas_object_data_set(popup, "path", path);

        evas_object_show(popup);
     }
   else
     _move_files(ephoto, files, path);
}

void
ephoto_file_copy(Ephoto *ephoto, Eina_List *files, const char *path)
{
   if (eina_list_count(files) <= 0)
     return;
   if (ephoto->config->prompts)
     {
        Evas_Object *ic, *button;
        Evas_Object *popup;
        char copy_dir[PATH_MAX];

        snprintf(copy_dir, PATH_MAX, "%s:<br> %s?",
                 _("Are you sure you want to copy these files to"), path);

        popup = _prompt(ephoto, _("Copy Files"), copy_dir);

        ic = elm_icon_add(popup);
        evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1,
                                         1);
        elm_icon_standard_set(ic, "document-save");

        button = elm_button_add(popup);
        elm_object_text_set(button, _("Yes"));
        elm_object_part_content_set(button, "icon", ic);
        evas_object_smart_callback_add(button, "clicked",
                                       _prompt_copy_apply, popup);
        elm_object_part_content_set(popup, "button1", button);
        evas_object_show(button);

        ic = elm_icon_add(popup);
        evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1,
                                         1);
        elm_icon_standard_set(ic, "window-close");

        button = elm_button_add(popup);
        elm_object_text_set(button, _("No"));
        elm_object_part_content_set(button, "icon", ic);
        evas_object_smart_callback_add(button, "clicked", _prompt_cancel,
                                       popup);
        elm_object_part_content_set(popup, "button2", button);
        evas_object_show(button);

        evas_object_data_set(popup, "files", files);
        evas_object_data_set(popup, "path", path);

        evas_object_show(popup);
     }
   else
     _copy_files(ephoto, files, path);
}

void
ephoto_file_paste(Ephoto *ephoto, Eina_List *files, Eina_Bool copy, const char *path)
{
   if (eina_list_count(files) <= 0)
     return;
   if (!copy)
     {
        if (ephoto->config->prompts)
          {
             Evas_Object *ic, *button;
             Evas_Object *popup;

             popup =
               _prompt(ephoto, _("Paste Files"),
                       _("Are you sure you want to paste these files here?"));

             ic = elm_icon_add(popup);
             evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL,
                                              1, 1);
             elm_icon_standard_set(ic, "document-save");

             button = elm_button_add(popup);
             elm_object_text_set(button, _("Yes"));
             elm_object_part_content_set(button, "icon", ic);
             evas_object_smart_callback_add(button, "clicked",
                                            _prompt_move_apply, popup);
             elm_object_part_content_set(popup, "button1", button);
             evas_object_show(button);

             ic = elm_icon_add(popup);
             evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL,
                                              1, 1);
             elm_icon_standard_set(ic, "window-close");

             button = elm_button_add(popup);
             elm_object_text_set(button, _("No"));
             elm_object_part_content_set(button, "icon", ic);
             evas_object_smart_callback_add(button, "clicked", _prompt_cancel,
                                            popup);
             elm_object_part_content_set(popup, "button2", button);
             evas_object_show(button);

             evas_object_data_set(popup, "files", files);
             evas_object_data_set(popup, "path", path);

             evas_object_show(popup);
          }
        else
          {
             _move_files(ephoto, files, path);
          }
     }
   else
     {
        if (ephoto->config->prompts)
          {
             Evas_Object *ic, *button;
             Evas_Object *popup;

             popup =
               _prompt(ephoto, _("Copy Files"),
                       _("Are you sure you want to copy these files here?"));

             ic = elm_icon_add(popup);
             evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL,
                                              1, 1);
             elm_icon_standard_set(ic, "document-save");

             button = elm_button_add(popup);
             elm_object_text_set(button, _("Yes"));
             elm_object_part_content_set(button, "icon", ic);
             evas_object_smart_callback_add(button, "clicked",
                                            _prompt_copy_apply, popup);
             elm_object_part_content_set(popup, "button1", button);
             evas_object_show(button);

             ic = elm_icon_add(popup);
             evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL,
                                              1, 1);
             elm_icon_standard_set(ic, "window-close");

             button = elm_button_add(popup);
             elm_object_text_set(button, _("No"));
             elm_object_part_content_set(button, "icon", ic);
             evas_object_smart_callback_add(button, "clicked", _prompt_cancel,
                                            popup);
             elm_object_part_content_set(popup, "button2", button);
             evas_object_show(button);

             evas_object_data_set(popup, "files", files);
             evas_object_data_set(popup, "path", path);

             evas_object_show(popup);
          }
        else
          {
             _copy_files(ephoto, files, path);
          }
     }
}

void
ephoto_file_delete(Ephoto *ephoto, Eina_List *files, Eina_File_Type type)
{
   if (eina_list_count(files) <= 0)
     return;

   if (ephoto->config->prompts)
     {
        Evas_Object *ic, *button;
        Evas_Object *popup;

        if (type == EINA_FILE_DIR)
          popup =
            _prompt(ephoto, _("Delete Directory"),
                    _("Are you sure you want to delete this directory?"));
        else
          popup =
            _prompt(ephoto, _("Delete Files"),
                    _("Are you sure you want to delete these files?"));

        ic = elm_icon_add(popup);
        evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1,
                                         1);
        elm_icon_standard_set(ic, "document-save");

        button = elm_button_add(popup);
        elm_object_text_set(button, _("Yes"));
        elm_object_part_content_set(button, "icon", ic);
        evas_object_smart_callback_add(button, "clicked", _prompt_delete_apply,
                                       popup);
        elm_object_part_content_set(popup, "button1", button);
        evas_object_show(button);

        ic = elm_icon_add(popup);
        evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1,
                                         1);
        elm_icon_standard_set(ic, "window-close");

        button = elm_button_add(popup);
        elm_object_text_set(button, _("No"));
        elm_object_part_content_set(button, "icon", ic);
        evas_object_smart_callback_add(button, "clicked", _prompt_cancel,
                                       popup);
        evas_object_show(button);
        elm_object_part_content_set(popup, "button2", button);

        evas_object_data_set(popup, "type", &type);
        evas_object_data_set(popup, "files", files);

        evas_object_show(popup);
     }
   else
     {
        if (type == EINA_FILE_DIR)
          _delete_dir(ephoto, files);
        else
          _delete_files(ephoto, files);
     }
}

void
ephoto_file_empty_trash(Ephoto *ephoto, Eina_List *files)
{
   if (eina_list_count(files) <= 0)
     return;
   if (ephoto->config->prompts)
     {
        Evas_Object *ic, *button;
        Evas_Object *popup;

        popup =
          _prompt(ephoto, _("Empty Trash"),
                  _("Are you sure you want to empty the trash?"));

        ic = elm_icon_add(popup);
        evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1,
                                         1);
        elm_icon_standard_set(ic, "document-save");

        button = elm_button_add(popup);
        elm_object_text_set(button, _("Yes"));
        elm_object_part_content_set(button, "icon", ic);
        evas_object_smart_callback_add(button, "clicked", _prompt_empty_apply,
                                       popup);
        elm_object_part_content_set(popup, "button1", button);
        evas_object_show(button);

        ic = elm_icon_add(popup);
        evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1,
                                         1);
        elm_icon_standard_set(ic, "window-close");

        button = elm_button_add(popup);
        elm_object_text_set(button, _("No"));
        elm_object_part_content_set(button, "icon", ic);
        evas_object_smart_callback_add(button, "clicked", _prompt_cancel,
                                       popup);
        elm_object_part_content_set(popup, "button2", button);
        evas_object_show(button);
        evas_object_data_set(popup, "files", files);

        evas_object_show(popup);
     }
   else
     {
        _empty_trash(ephoto, files);
     }
}

