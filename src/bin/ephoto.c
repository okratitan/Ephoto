#include "ephoto.h"

static void _ephoto_display_usage(void);

EAPI_MAIN int
elm_main(int argc, char **argv)
{
   int r = 0;

   eio_init();
   elm_need_efreet();
   elm_language_set("");
   elm_app_compile_data_dir_set(PACKAGE_DATA_DIR);
   elm_app_info_set(elm_main, "ephoto", "themes/ephoto.edj");
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

   if (argc > 2)
     {
        printf("Too Many Arguments!\n");
        _ephoto_display_usage();
        r = 1;
        goto end;
     }
   else if (argc < 2)
     {
        Evas_Object *win = ephoto_window_add(NULL);

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
        Evas_Object *win = ephoto_window_add(real);

        free(real);
        if (!win)
          {
             r = 1;
             goto end;
          }
     }

   elm_run();

end:
   e_thumb_shutdown();
   efreet_mime_shutdown();
   eio_shutdown();

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

ELM_MAIN()
