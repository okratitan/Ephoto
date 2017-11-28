#include "ephoto.h"

static void _ephoto_display_usage(void);

int
main(int argc, char *argv[])
{
   int gadget = 0, id_num = 0, r = 0;
   char buf[4096];

   elm_init(argc, (char **)argv);
   eio_init();
   elm_need_efreet();
   elm_language_set("");
   elm_app_compile_data_dir_set(PACKAGE_DATA_DIR);
   elm_app_info_set(main, "ephoto", "themes/ephoto.edj");
#if HAVE_GETTEXT && ENABLE_NLS
   elm_app_compile_locale_set(LOCALEDIR);
   bindtextdomain(PACKAGE, elm_app_locale_dir_get());
   bind_textdomain_codeset(PACKAGE, "UTF-8");
   textdomain(PACKAGE);
#endif

   if (!efreet_mime_init())
     printf("Could not initialize Efreet_Mime!\n");
   if (!e_ipc_init())
     printf("Could not initialize IPC!\n");
   if (!e_thumb_init())
     printf("Could not initialize Thumbnailer!\n");

   elm_policy_set(ELM_POLICY_QUIT, ELM_POLICY_QUIT_LAST_WINDOW_CLOSED);

   if (getenv("E_GADGET_ID"))
     {
        gadget = 1;
        snprintf(buf, sizeof(buf), "%s", getenv("E_GADGET_ID"));
        id_num = atoi(buf);
     }
   if (id_num < 0)
     {
        Evas_Object *win, *icon;

        win = elm_win_add(NULL, "ephoto", ELM_WIN_BASIC);
        elm_win_title_set(win, "Ephoto");
        elm_win_alpha_set(win, 1);
        elm_win_autodel_set(win, 1);
        evas_object_size_hint_aspect_set(win, EVAS_ASPECT_CONTROL_BOTH, 1, 1);

        icon = elm_image_add(win);
        elm_image_file_set(icon, PACKAGE_DATA_DIR "/images/ephoto.png", NULL);
        elm_win_resize_object_add(win, icon);
        evas_object_show(icon);

        evas_object_show(win);
     }
   else if (argc > 2)
     {
        printf("Too Many Arguments!\n");
        _ephoto_display_usage();
        r = 1;
        goto end;
     }
   else if (argc < 2)
     {
        Evas_Object *win = ephoto_window_add(NULL, gadget);

        if (!win)
          {
             r = 1;
             goto end;
          }
     }
   else if (!strncmp(argv[1], "--help", 6))
     {
        _ephoto_display_usage();
        r = 0;
        goto end;
     }
   else
     {
        char *real = ecore_file_realpath(argv[1]);

        if (!real)
          {
             printf("invalid file or directory: '%s'\n", argv[1]);
             r = 1;
             goto end;
          }
        Evas_Object *win = ephoto_window_add(real, gadget);

        free(real);
        if (!win)
          {
             r = 1;
             goto end;
          }
     }

   ecore_main_loop_begin();
end:
   e_thumb_shutdown();
   efreet_mime_shutdown();
   eio_shutdown();
   elm_shutdown();

   return r;
}

/* Display useage commands for ephoto */
static void
_ephoto_display_usage(void)
{
   printf("Ephoto Usage: \n" "ephoto --help   : This page\n"
                             "ephoto filename : Specifies a file to open\n"
                             "ephoto dirname  : Specifies a directory to open\n");
}
