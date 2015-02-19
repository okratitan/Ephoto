#include "ephoto.h"

typedef struct _Ephoto_Cropper Ephoto_Cropper;
struct _Ephoto_Cropper
{
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

static void 
_cropper_both_mouse_move(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source)
{
   Ephoto_Cropper *ec = data;
   Edje_Message_Int_Set *msg;
   int mx, my, cx, cy, cw, ch, nx, ny, lx, ly, lw, lh;

   evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper), &mx, &my);
   evas_object_geometry_get(ec->cropper, &cx, &cy, &cw, &ch);
   evas_object_geometry_get(ec->layout, &lx, &ly, &lw, &lh);

   nx = mx - ec->startx;
   ny = my - ec->starty;
   ec->startx = mx;
   ec->starty = my;

   if (cx+nx < lx)
     nx = lx-cx;
   else if (cx+cw+nx > lx+lw)
     nx = (lx+lw)-(cx+cw);
   else if (cy+ny < ly)
     ny = ly-cy;
   else if (cy+ch+ny > ly+lh)
     ny = (ly+lh)-(cy+ch);

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
}

static void 
_cropper_both_mouse_up(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source)
{
   Ephoto_Cropper *ec = data;

   edje_object_signal_callback_del_full(ec->cropper, "mouse,move", source, _cropper_both_mouse_move, ec);
   edje_object_signal_callback_del_full(ec->cropper, "mouse,up,1", source, _cropper_both_mouse_up, ec);
   ec->resizing = 0;
}

static void 
_cropper_resize_both(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source)
{
   Ephoto_Cropper *ec = data;
   int mx, my, cx, cy;

   ec->resizing = 1;
   evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper), &mx, &my);
   evas_object_geometry_get(ec->cropper, &cx, &cy, 0, 0);
   ec->offsetx = mx - cx;
   ec->offsety = my - cy;
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
   int mx, cx, cy, cw, ch, nx, lx, ly, lw, lh;

   evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper), &mx, 0);
   evas_object_geometry_get(ec->cropper, &cx, &cy, &cw, &ch);
   evas_object_geometry_get(ec->layout, &lx, &ly, &lw, &lh);

   nx = mx - ec->startx;
   ec->startx = mx;

   if (cx+nx < lx)
     nx = lx-cx;
   else if (cx+cw+nx > lx+lw)
     nx = (lx+lw)-(cx+cw);

   msg = alloca(sizeof(Edje_Message_Int_Set) + (3*sizeof(int)));
   msg->count = 3;
   if (!strcmp(source, "handle4"))
     msg->val[0] = 4;
   else if (!strcmp(source, "handle8"))
     msg->val[0] = 8;
   msg->val[1] = nx;
   msg->val[2] = 0;
   edje_object_message_send(elm_layout_edje_get(ec->layout), EDJE_MESSAGE_INT_SET, 1, msg);
}

static void 
_cropper_horiz_mouse_up(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source)
{
   Ephoto_Cropper *ec = data;

   edje_object_signal_callback_del_full(ec->cropper, "mouse,move", source, _cropper_horiz_mouse_move, ec);
   edje_object_signal_callback_del_full(ec->cropper, "mouse,up,1", source, _cropper_horiz_mouse_up, ec);
   ec->resizing = 0;
}


static void 
_cropper_resize_horiz(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;
   int mx, my, cx, cy;

   ec->resizing = 1;
   evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper), &mx, &my);
   evas_object_geometry_get(ec->cropper, &cx, &cy, 0, 0);
   ec->offsetx = mx - cx;
   ec->offsety = my - cy;
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
   int my, cx, cy, cw, ch, ny, lx, ly, lw, lh;

   evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper), 0, &my);
   evas_object_geometry_get(ec->cropper, &cx, &cy, &cw, &ch);
   evas_object_geometry_get(ec->layout, &lx, &ly, &lw, &lh);

   ny = my - ec->starty;
   ec->starty = my;

   if (cy+ny < ly)
     ny = ly-cy;
   else if (cy+ch+ny > ly+lh)
     ny = (ly+lh)-(cy+ch);

   msg = alloca(sizeof(Edje_Message_Int_Set) + (3*sizeof(int)));
   msg->count = 3;
   if (!strcmp(source, "handle2"))
     msg->val[0] = 2;
   else if (!strcmp(source, "handle6"))
     msg->val[0] = 6;
   msg->val[1] = 0;
   msg->val[2] = ny;
   edje_object_message_send(elm_layout_edje_get(ec->layout), EDJE_MESSAGE_INT_SET, 1, msg);
}

static void 
_cropper_vert_mouse_up(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source)
{
   Ephoto_Cropper *ec = data;

   edje_object_signal_callback_del_full(ec->cropper, "mouse,move", source, _cropper_vert_mouse_move, ec);
   edje_object_signal_callback_del_full(ec->cropper, "mouse,up,1", source, _cropper_vert_mouse_up, ec);
   ec->resizing = 0;
}

static void 
_cropper_resize_vert(void *data, Evas_Object *obj EINA_UNUSED, const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;
   int mx, my, cx, cy;

   ec->resizing = 1;
   evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper), &mx, &my);
   evas_object_geometry_get(ec->cropper, &cx, &cy, 0, 0);
   ec->offsetx = mx - cx;
   ec->offsety = my - cy;
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

        nx = mx - ec->startx;
        ny = my - ec->starty;
        ec->startx = mx;
        ec->starty = my;

        if (cx+nx < lx)
          nx = lx-cx;
        else if (cx+cw+nx > lx+lw)
          nx = (lx+lw)-(cx+cw);
        else if (cy+ny < ly)
          ny = ly-cy;
        else if (cy+ch+ny > ly+lh)
          ny = (ly+lh)-(cy+ch);

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
   ec->offsetx = mx - cx;
   ec->offsety = my - cy;

   edje_object_signal_callback_add(ec->cropper, "mouse,move", "dragger", _cropper_mouse_move, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,up,1", "dragger", _cropper_mouse_up, ec);
}

static void 
_image_resize(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;

   int sx, sy, sw, sh, iw, ih, diffw, diffh;
   int cx, cy, cw, ch, ix, iy;

   evas_object_geometry_get(ec->layout, &sx, &sy, &sw, &sh);
   evas_object_image_size_get(elm_image_object_get(ec->image), &iw, &ih);
   if (iw < sw && ih < sh)
     {
        diffw = sw - iw;
        diffh = sh - ih;
        diffw /= 2;
        diffh /= 2;
        ix = sx+diffw;
        iy = sy+diffh;
        cw = iw/2;
        ch = ih/2;
        cx = (cw/2)+ix;
        cy = (ch/2)+iy;

        evas_object_resize(ec->layout, iw, ih);
        evas_object_move(ec->layout, ix, iy);
     }
   else
     {
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
        diffw = sw - nw;
        diffh = sh - nh;
        diffw /= 2;
        diffh /= 2;
        ix = sx+diffw;
        iy = sy+diffh;
        cw = nw/2;
        ch = nh/2;
        cx = ix+(cw/2);
        cy = iy+(ch/2);

        evas_object_resize(ec->layout, nw, nh);
        evas_object_move(ec->layout, ix, iy);
     }
}

static void 
_cropper_free(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED, void *event_data EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;
   free(ec);
}

Evas_Object *
ephoto_cropper_add(Evas_Object *parent, const char *file, const char *key)
{
   Ephoto_Cropper *ec = calloc(1, sizeof(Ephoto_Cropper));
   ec->resizing = 0;
 
   ec->box = elm_box_add(parent);
   evas_object_size_hint_weight_set(ec->box, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(ec->box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(ec->box);

   ec->layout = elm_layout_add(ec->box);
   elm_layout_file_set(ec->layout, PACKAGE_DATA_DIR "/themes/crop.edj", "ephoto,image,cropper,base");
   evas_object_size_hint_weight_set(ec->layout, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(ec->layout, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(ec->box, ec->layout);
   evas_object_show(ec->layout);

   ec->image = elm_image_add(ec->layout);
   elm_image_file_set(ec->image, file, key);
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

   evas_object_data_set(ec->box, "image", ec->image);
   evas_object_data_set(ec->box, "layout", ec->layout);
   evas_object_event_callback_add(ec->layout, EVAS_CALLBACK_RESIZE, _image_resize, ec);
   evas_object_event_callback_add(ec->box, EVAS_CALLBACK_FREE, _cropper_free, ec);

   return ec->box;
}
