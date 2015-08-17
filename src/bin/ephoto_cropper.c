#include "ephoto.h"

typedef struct _Ephoto_Cropper Ephoto_Cropper;
struct _Ephoto_Cropper
{
   Evas_Object *main;
   Evas_Object *parent;
   Evas_Object *box;
   Evas_Object *image;
   Evas_Object *cropper;
   Evas_Object *layout;
   int startx;
   int starty;
   int offsetx;
   int offsety;
   int resizing;
};

static void _apply_crop(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED);
static void _cancel_crop(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED);

static void
_apply_crop(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;
   Evas_Object *edje = elm_layout_edje_get(ec->layout);

   const char *path, *key, *type;;
   int x, y, w, h, cx, cy, cw, ch, iw, ih;
   int nx, ny, nw, nh, i, j, tmpx, tmpy, ind, index;
   double scalex, scaley, scalew, scaleh;
   unsigned int *idata, *idata_new;

   evas_object_geometry_get(ec->layout, &x, &y, &w, &h);
   edje_object_part_geometry_get(edje, "ephoto.swallow.cropper", &cx, &cy, &cw, &ch);
   evas_object_image_size_get(elm_image_object_get(ec->image), &iw, &ih);

   idata = evas_object_image_data_get(elm_image_object_get(ec->image), EINA_FALSE);

   scalex = (double)cx/(double)w;
   scaley = (double)cy/(double)h;
   scalew = (double)cw/(double)w;
   scaleh = (double)ch/(double)h;

   nx = iw*scalex;
   ny = ih*scaley;
   nw = iw*scalew;
   nh = ih*scaleh;

   index = 0;
   idata_new = malloc(sizeof(unsigned int)*nw*nh);

   for (i = 0; i < nh; i++)
     {
        tmpy = (i+ny)*iw;
        for (j = 0; j < nw; j++)
          {
             tmpx = j+nx;
             ind = tmpy+tmpx;
             idata_new[index] = idata[ind];
             index++;
          }
     }
   elm_table_unpack(ec->parent, ec->box);
   elm_layout_content_unset(ec->layout, "ephoto.swallow.image");
   elm_table_pack(ec->parent, ec->image, 0, 0, 1, 1);
   ephoto_single_browser_image_data_update(ec->main, ec->image, EINA_TRUE, idata_new, nw, nh);
   evas_object_del(ec->cropper);
   evas_object_del(ec->layout);
   evas_object_del(ec->box);
}

static void
_cancel_crop(void *data, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;
   elm_table_unpack(ec->parent, ec->box);
   elm_layout_content_unset(ec->layout, "ephoto.swallow.image");
   elm_table_pack(ec->parent, ec->image, 0, 0, 1, 1);
   ephoto_single_browser_cancel_editing(ec->main, ec->image);
   evas_object_del(ec->cropper);
   evas_object_del(ec->layout);
   evas_object_del(ec->box);
}

static void
_calculate_cropper_size(Ephoto_Cropper *ec)
{
   Edje_Message_Int_Set *msg;
   int w, h, cw, ch, iw, ih, nw, nh;
   double scalew, scaleh;

   evas_object_geometry_get(ec->layout, 0, 0, &w, &h);
   edje_object_part_geometry_get(elm_layout_edje_get(ec->layout), 
                                 "ephoto.swallow.cropper", 0, 0, &cw, &ch);
   evas_object_image_size_get(elm_image_object_get(ec->image), &iw, &ih);

   scalew = (double)cw/(double)w;
   scaleh = (double)ch/(double)h;

   nw = iw*scalew;
   nh = ih*scaleh;

   msg = alloca(sizeof(Edje_Message_Int_Set) + (3*sizeof(int)));
   msg->count = 3;
   msg->val[0] = 10;
   msg->val[1] = nw;
   msg->val[2] = nh;
   edje_object_message_send(elm_layout_edje_get(ec->layout), EDJE_MESSAGE_INT_SET, 1, msg);
}

static void
_cropper_both_mouse_move(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source)
{
   Ephoto_Cropper *ec = data;
   Edje_Message_Int_Set *msg;
   int mx, my, cx, cy, cw, ch, nx, ny, lx, ly, lw, lh;

   evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper), &mx, &my);
   evas_object_geometry_get(ec->cropper, &cx, &cy, &cw, &ch);
   evas_object_geometry_get(ec->layout, &lx, &ly, &lw, &lh);

   if (mx < lx)
     mx = lx;
   else if (mx > lx+lw)
     mx = lx+lw;
   if (my < ly)
     my = ly;
   else if (my > ly+lh)
     my = ly+lh;

   nx = mx-ec->startx;
   ny = my-ec->starty;
   ec->startx = mx;
   ec->starty = my;

   msg = alloca(sizeof(Edje_Message_Int_Set) + (3*sizeof(int)));
   msg->count = 3;
   if (!strcmp(source, "handle1"))
     msg->val[0] = 1;
   else if (!strcmp(source, "handle3"))
     msg->val[0] = 3;
   else if (!strcmp(source, "handle5"))
     msg->val[0] = 5;
   else if (!strcmp(source, "handle7"))
     msg->val[0] = 7;

   msg->val[1] = nx;
   msg->val[2] = ny;
   edje_object_message_send(elm_layout_edje_get(ec->layout), EDJE_MESSAGE_INT_SET, 1, msg);
   _calculate_cropper_size(ec);
}

static void
_cropper_both_mouse_up(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source)
{
   Ephoto_Cropper *ec = data;

   edje_object_signal_callback_del_full(ec->cropper, "mouse,move", source, _cropper_both_mouse_move, ec);
   edje_object_signal_callback_del_full(ec->cropper, "mouse,up,1", source, _cropper_both_mouse_up, ec);
   ec->resizing = 0;
   _calculate_cropper_size(ec);
}

static void
_cropper_resize_both(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source)
{
   Ephoto_Cropper *ec = data;
   int mx, my, cx, cy;

   ec->resizing = 1;
   evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper), &mx, &my);
   evas_object_geometry_get(ec->cropper, &cx, &cy, 0, 0);
   ec->offsetx = mx-cx;
   ec->offsety = my-cy;
   ec->startx = mx;
   ec->starty = my;

   edje_object_signal_callback_add(ec->cropper, "mouse,move", source, _cropper_both_mouse_move, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,up,1", source, _cropper_both_mouse_up, ec);
}

static void
_cropper_horiz_mouse_move(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source)
{
   Ephoto_Cropper *ec = data;
   Edje_Message_Int_Set *msg;
   int mx, cx, cy, cw, ch, nx, lx, lw;

   evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper), &mx, 0);
   evas_object_geometry_get(ec->cropper, &cx, &cy, &cw, &ch);
   evas_object_geometry_get(ec->layout, &lx, 0, &lw, 0);

   if (mx < lx)
     mx = lx;
   else if (mx > lx+lw)
     mx = lx+lw;

   nx = mx-ec->startx;
   ec->startx = mx;

   msg = alloca(sizeof(Edje_Message_Int_Set) + (3*sizeof(int)));
   msg->count = 3;
   if (!strcmp(source, "handle4"))
     msg->val[0] = 4;
   else if (!strcmp(source, "handle8"))
     msg->val[0] = 8;
   msg->val[1] = nx;
   msg->val[2] = 0;
   edje_object_message_send(elm_layout_edje_get(ec->layout), EDJE_MESSAGE_INT_SET, 1, msg);
   _calculate_cropper_size(ec);
}

static void
_cropper_horiz_mouse_up(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source)
{
   Ephoto_Cropper *ec = data;

   edje_object_signal_callback_del_full(ec->cropper, "mouse,move", source, _cropper_horiz_mouse_move, ec);
   edje_object_signal_callback_del_full(ec->cropper, "mouse,up,1", source, _cropper_horiz_mouse_up, ec);
   ec->resizing = 0;
   _calculate_cropper_size(ec);
}

static void
_cropper_resize_horiz(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;
   int mx, my, cx, cy;

   ec->resizing = 1;
   evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper), &mx, &my);
   evas_object_geometry_get(ec->cropper, &cx, &cy, 0, 0);
   ec->offsetx = mx-cx;
   ec->offsety = my-cy;
   ec->startx = mx;
   ec->starty = my;

   edje_object_signal_callback_add(ec->cropper, "mouse,move", source, _cropper_horiz_mouse_move, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,up,1", source, _cropper_horiz_mouse_up, ec);
}

static void
_cropper_vert_mouse_move(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source)
{
   Ephoto_Cropper *ec = data;
   Edje_Message_Int_Set *msg;
   int my, cx, cy, cw, ch, ny, ly, lh;

   evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper), 0, &my);
   evas_object_geometry_get(ec->cropper, &cx, &cy, &cw, &ch);
   evas_object_geometry_get(ec->layout, 0, &ly, 0, &lh);

   if (my < ly)
     my = ly;
   else if (my > ly+lh)
     my = ly+lh;

   ny = my-ec->starty;
   ec->starty = my;

   msg = alloca(sizeof(Edje_Message_Int_Set) + (3*sizeof(int)));
   msg->count = 3;
   if (!strcmp(source, "handle2"))
     msg->val[0] = 2;
   else if (!strcmp(source, "handle6"))
     msg->val[0] = 6;
   msg->val[1] = 0;
   msg->val[2] = ny;
   edje_object_message_send(elm_layout_edje_get(ec->layout), EDJE_MESSAGE_INT_SET, 1, msg);
   _calculate_cropper_size(ec);
}

static void
_cropper_vert_mouse_up(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source)
{
   Ephoto_Cropper *ec = data;

   edje_object_signal_callback_del_full(ec->cropper, "mouse,move", source, _cropper_vert_mouse_move, ec);
   edje_object_signal_callback_del_full(ec->cropper, "mouse,up,1", source, _cropper_vert_mouse_up, ec);
   ec->resizing = 0;
   _calculate_cropper_size(ec);
}

static void
_cropper_resize_vert(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;
   int mx, my, cx, cy;

   ec->resizing = 1;
   evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper), &mx, &my);
   evas_object_geometry_get(ec->cropper, &cx, &cy, 0, 0);
   ec->offsetx = mx-cx;
   ec->offsety = my-cy;
   ec->startx = mx;
   ec->starty = my;

   edje_object_signal_callback_add(ec->cropper, "mouse,move", source, _cropper_vert_mouse_move, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,up,1", source, _cropper_vert_mouse_up, ec);

}

static void
_cropper_mouse_move(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;
   if (!ec->resizing)
     {
        Edje_Message_Int_Set *msg;
        int mx, my, cx, cy, cw, ch, nx, ny, lx, ly, lw, lh;

        evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper), &mx, &my);
        evas_object_geometry_get(ec->cropper, &cx, &cy, &cw, &ch);
        evas_object_geometry_get(ec->layout, &lx, &ly, &lw, &lh);

        if (mx < lx)
          mx = lx;
        else if (mx > lx+lw)
          mx = lx+lw;
        if (my < ly)
          my = ly;
        else if (my > ly+lh)
          my = ly+lh;

        nx = mx-ec->startx;
        ny = my-ec->starty;
        ec->startx = mx;
        ec->starty = my;

        msg = alloca(sizeof(Edje_Message_Int_Set) + (3*sizeof(int)));
        msg->count = 3;
        msg->val[0] = 0;
        msg->val[1] = nx;
        msg->val[2] = ny;
        edje_object_message_send(elm_layout_edje_get(ec->layout), EDJE_MESSAGE_INT_SET, 1, msg);
     }
}

static void
_cropper_mouse_up(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;

   edje_object_signal_callback_del_full(ec->cropper, "mouse,move", "dragger", _cropper_mouse_move, ec);
   edje_object_signal_callback_del_full(ec->cropper, "mouse,up,1", "dragger", _cropper_mouse_up, ec);
}

static void
_cropper_move(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;
   int mx, my, cx, cy;

   evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper), &mx, &my);
   evas_object_geometry_get(ec->cropper, &cx, &cy, 0, 0);
   ec->startx = mx;
   ec->starty = my;
   ec->offsetx = mx-cx;
   ec->offsety = my-cy;

   edje_object_signal_callback_add(ec->cropper, "mouse,move", "dragger", _cropper_mouse_move, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,up,1", "dragger", _cropper_mouse_up, ec);
}

static void
_image_resize(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;
   Edje_Message_Int_Set *msg;

   int sx, sy, sw, sh, ix, iy, iw, ih, diffw, diffh;

   evas_object_geometry_get(ec->layout, &sx, &sy, &sw, &sh);
   evas_object_image_size_get(elm_image_object_get(ec->image), &iw, &ih);

   int nw, nh;
   if (sw > sh)
     {
        nw = sw;
        nh = ih*((double)sw/(double)iw);
        if (nh > sh)
          {
             int onw, onh;
             onw = nw;
             onh = nh;
             nh = sh;
             nw = onw*((double)nh/(double)onh);
          }
     }
   else
     {
        nh = sh;
        nw = iw*((double)sh/(double)ih);
        if (nw > sw)
          {
             int onw, onh;
             onw = nw;
             onh = nh;
             nw = sw;
             nh = onh*((double)nw/(double)onw);
          }
     }
   diffw = sw-nw;
   diffh = sh-nh;
   diffw /= 2;
   diffh /= 2;
   ix = sx+diffw;
   iy = sy+diffh;

   evas_object_resize(ec->layout, nw, nh);
   evas_object_move(ec->layout, ix, iy);

   msg = alloca(sizeof(Edje_Message_Int_Set) + (3*sizeof(int)));
   msg->count = 3;
   msg->val[0] = 9;
   msg->val[1] = 0;
   msg->val[2] = 0;
   edje_object_message_send(elm_layout_edje_get(ec->layout), EDJE_MESSAGE_INT_SET, 1, msg);
   _calculate_cropper_size(ec);
}

static void
_cropper_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;
   free(ec);
}

Evas_Object *
ephoto_cropper_add(Evas_Object *main, Evas_Object *toolbar, Evas_Object *parent, Evas_Object *image)
{
   Ephoto_Cropper *ec = calloc(1, sizeof(Ephoto_Cropper));
   Evas_Object *hbox, *ic, *button;

   ec->resizing = 0;
   ec->main = main;
   ec->parent = parent;
   ec->image = image;

   ec->box = elm_box_add(parent);
   evas_object_size_hint_weight_set(ec->box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(ec->box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(parent, ec->box, 0, 0, 1, 1);
   evas_object_show(ec->box);

   hbox = elm_box_add(ec->box);
   elm_box_homogeneous_set(hbox, EINA_TRUE);
   elm_box_horizontal_set(hbox, EINA_TRUE);
   evas_object_size_hint_weight_set(hbox, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(hbox, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_after(ec->main, hbox, toolbar);
   evas_object_show(hbox);

   ic = elm_icon_add(hbox);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "document-save");
   button = elm_button_add(hbox);
   elm_object_text_set(button, _("Apply"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _apply_crop, ec);
   elm_box_pack_end(hbox, button);
   evas_object_show(button);

   ic = elm_icon_add(hbox);
   elm_icon_order_lookup_set(ic, ELM_ICON_LOOKUP_FDO_THEME);
   evas_object_size_hint_aspect_set(ic, EVAS_ASPECT_CONTROL_VERTICAL, 1, 1);
   elm_icon_standard_set(ic, "window-close");
   button = elm_button_add(hbox);
   elm_object_text_set(button, _("Cancel"));
   elm_object_part_content_set(button, "icon", ic);
   evas_object_smart_callback_add(button, "clicked", _cancel_crop, ec);
   elm_box_pack_end(hbox, button);
   evas_object_show(button);

   ec->layout = elm_layout_add(ec->box);
   elm_layout_file_set(ec->layout, PACKAGE_DATA_DIR "/themes/crop.edj", "ephoto,image,cropper,base");
   evas_object_size_hint_weight_set(ec->layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(ec->layout, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(ec->box, ec->layout);
   evas_object_show(ec->layout);

   evas_object_size_hint_weight_set(ec->image, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(ec->image, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_layout_content_set(ec->layout, "ephoto.swallow.image", ec->image);
   evas_object_show(ec->image);   

   ec->cropper = edje_object_add(evas_object_evas_get(ec->layout));
   edje_object_file_set(ec->cropper, PACKAGE_DATA_DIR "/themes/crop.edj", "ephoto,image,cropper");
   elm_layout_content_set(ec->layout, "ephoto.swallow.cropper", ec->cropper);
   evas_object_show(ec->cropper);

   edje_object_signal_callback_add(ec->cropper, "mouse,down,1", "dragger", _cropper_move, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,down,1", "handle1", _cropper_resize_both, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,down,1", "handle2", _cropper_resize_vert, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,down,1", "handle3", _cropper_resize_both, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,down,1", "handle4", _cropper_resize_horiz, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,down,1", "handle5", _cropper_resize_both, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,down,1", "handle6", _cropper_resize_vert, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,down,1", "handle7", _cropper_resize_both, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,down,1", "handle8", _cropper_resize_horiz, ec);

   evas_object_event_callback_add(ec->layout, EVAS_CALLBACK_RESIZE, _image_resize, ec);
   evas_object_event_callback_add(ec->box, EVAS_CALLBACK_DEL, _cropper_del, ec);

   return ec->box;
}
