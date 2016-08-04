#include "ephoto.h"

typedef struct _Ephoto_Cropper Ephoto_Cropper;
struct _Ephoto_Cropper
{
   Evas_Object *main;
   Evas_Object *parent;
   Evas_Object *image_parent;
   Evas_Object *box;
   Evas_Object *editor;
   Evas_Object *image;
   Evas_Object *cropper;
   Evas_Object *layout;
   Evas_Object *cropw;
   Evas_Object *croph;
   Eina_List *handlers;
   Evas_Coord startx;
   Evas_Coord starty;
   Evas_Coord offsetx;
   Evas_Coord offsety;
   Eina_Bool resizing;
};

static void
_calculate_cropper_size(void *data, Evas_Object *obj EINA_UNUSED,
    const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;
   Edje_Message_Int_Set *msg;
   Evas_Coord w, h, cw, ch, iw, ih, nw, nh;
   double scalew, scaleh;

   edje_object_part_geometry_get(elm_layout_edje_get(ec->layout),
       "ephoto.swallow.image", 0, 0, &w, &h);
   edje_object_part_geometry_get(elm_layout_edje_get(ec->layout),
       "ephoto.swallow.cropper", 0, 0, &cw, &ch);
   evas_object_image_size_get(ec->image, &iw, &ih);

   scalew = (double) cw / (double) w;
   scaleh = (double) ch / (double) h;

   nw = iw * scalew;
   nh = ih * scaleh;

   elm_slider_value_set(ec->cropw, nw);
   elm_slider_value_set(ec->croph, nh);

   msg = alloca(sizeof(Edje_Message_Int_Set) + (3 * sizeof(int)));
   msg->count = 3;
   msg->val[0] = 11;
   msg->val[1] = nw;
   msg->val[2] = nh;
   edje_object_message_send(elm_layout_edje_get(ec->layout),
       EDJE_MESSAGE_INT_SET, 1, msg);
}

static void
_cropper_changed_width(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;
   Edje_Message_Int_Set *msgl, *msgr;
   Evas_Coord mw, cx, cw, nw, lx, lw, iw, left, right;
   double scalew;

   mw = elm_slider_value_get(ec->cropw);

   edje_object_part_geometry_get(elm_layout_edje_get(ec->layout),
       "ephoto.swallow.image", &lx, 0, &lw, 0);
   edje_object_part_geometry_get(elm_layout_edje_get(ec->layout),
       "ephoto.swallow.cropper", &cx, 0, &cw, 0);
   evas_object_image_size_get(ec->image, &iw, 0);
   scalew = (double) mw / (double) iw;

   nw = lw * scalew;
   left = (nw - cw) / 2;
   right = (nw - cw) / 2;

   if ((cx + cw + right) >= (lx + lw))
     {
	right = (lx + lw) - (cx + cw);
	left += left - right;
     }
   else if ((cx - left) <= lx)
     {
	left = cx - lx;
	right += right - left;
     }
   left *= -1;

   msgl = alloca(sizeof(Edje_Message_Int_Set) + (3 * sizeof(int)));
   msgl->count = 3;
   msgl->val[0] = 8;
   msgl->val[1] = left;
   msgl->val[2] = 0;
   edje_object_message_send(elm_layout_edje_get(ec->layout),
       EDJE_MESSAGE_INT_SET, 1, msgl);

   msgr = alloca(sizeof(Edje_Message_Int_Set) + (3 * sizeof(int)));
   msgr->count = 3;
   msgr->val[0] = 4;
   msgr->val[1] = right;
   msgr->val[2] = 0;
   edje_object_message_send(elm_layout_edje_get(ec->layout),
       EDJE_MESSAGE_INT_SET, 1, msgr);
}

static void
_cropper_changed_height(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;
   Edje_Message_Int_Set *msgt, *msgb;
   Evas_Coord mh, ch, cy, nh, lh, ly, ih, top, bottom;
   double scaleh;

   mh = elm_slider_value_get(ec->croph);

   edje_object_part_geometry_get(elm_layout_edje_get(ec->layout),
       "ephoto.swallow.image", 0, &ly, 0, &lh);
   edje_object_part_geometry_get(elm_layout_edje_get(ec->layout),
       "ephoto.swallow.cropper", 0, &cy, 0, &ch);
   evas_object_image_size_get(ec->image, 0, &ih);

   scaleh = (double) mh / (double) ih;
   nh = lh * scaleh;
   top = (nh - ch) / 2;
   bottom = (nh - ch) / 2;

   if ((cy + ch + bottom) >= (ly + lh))
     {
	bottom = (ly + lh) - (cy + ch);
	top += top - bottom;
     }
   else if ((cy - top) <= ly)
     {
	top = cy - ly;
	bottom += bottom - top;
     }
   top *= -1;

   msgt = alloca(sizeof(Edje_Message_Int_Set) + (3 * sizeof(int)));
   msgt->count = 3;
   msgt->val[0] = 2;
   msgt->val[1] = 0;
   msgt->val[2] = top;
   edje_object_message_send(elm_layout_edje_get(ec->layout),
       EDJE_MESSAGE_INT_SET, 1, msgt);

   msgb = alloca(sizeof(Edje_Message_Int_Set) + (3 * sizeof(int)));
   msgb->count = 3;
   msgb->val[0] = 6;
   msgb->val[1] = 0;
   msgb->val[2] = bottom;
   edje_object_message_send(elm_layout_edje_get(ec->layout),
       EDJE_MESSAGE_INT_SET, 1, msgb);
}

static void
_reset_crop(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;
   Edje_Message_Int_Set *msg;

   msg = alloca(sizeof(Edje_Message_Int_Set) + (3 * sizeof(int)));
   msg->count = 3;
   msg->val[0] = 10;
   msg->val[1] = 0;
   msg->val[2] = 0;
   edje_object_message_send(elm_layout_edje_get(ec->layout),
       EDJE_MESSAGE_INT_SET, 1, msg);
}

static void
_apply_crop(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;
   Evas_Object *edje = elm_layout_edje_get(ec->layout);

   Evas_Coord x, y, w, h, cx, cy, cw, ch, iw, ih;
   Evas_Coord nx, ny, nw, nh, i, j, tmpx, tmpy, ind, index;
   double scalex, scaley, scalew, scaleh;
   unsigned int *idata, *idata_new;

   edje_object_part_geometry_get(edje, "ephoto.swallow.image", &x, &y, &w, &h);
   edje_object_part_geometry_get(edje, "ephoto.swallow.cropper", &cx, &cy, &cw,
       &ch);
   evas_object_image_size_get(ec->image, &iw, &ih);

   idata =
       evas_object_image_data_get(ec->image, EINA_FALSE);

   scalex = (double) (cx-x) / (double) w;
   scaley = (double) (cy-y) / (double) h;
   scalew = (double) cw / (double) w;
   scaleh = (double) ch / (double) h;

   nx = iw * scalex;
   ny = ih * scaley;
   nw = iw * scalew;
   nh = ih * scaleh;

   index = 0;
   idata_new = malloc(sizeof(unsigned int) * nw * nh);

   for (i = 0; i < nh; i++)
     {
	tmpy = (i + ny) * iw;
	for (j = 0; j < nw; j++)
	  {
	     tmpx = j + nx;
	     ind = tmpy + tmpx;
	     idata_new[index] = idata[ind];
	     index++;
	  }
     }
   elm_table_unpack(ec->image_parent, ec->box);
   elm_layout_content_unset(ec->layout, "ephoto.swallow.image");
   elm_table_pack(ec->image_parent, ec->image, 0, 0, 1, 1);
   ephoto_single_browser_image_data_done(ec->main,
       idata_new, nw, nh);
   evas_object_del(ec->cropper);
   evas_object_del(ec->layout);
   evas_object_del(ec->box);
   ephoto_editor_del(ec->editor);
}

static void
_cancel_crop(void *data, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;

   elm_table_unpack(ec->image_parent, ec->box);
   elm_layout_content_unset(ec->layout, "ephoto.swallow.image");
   elm_table_pack(ec->image_parent, ec->image, 0, 0, 1, 1);
   ephoto_single_browser_cancel_editing(ec->main);
   evas_object_del(ec->cropper);
   evas_object_del(ec->layout);
   evas_object_del(ec->box);
   ephoto_editor_del(ec->editor);
}

static void
_cropper_both_mouse_move(void *data, Evas_Object *obj EINA_UNUSED,
    const char *emission EINA_UNUSED, const char *source)
{
   Ephoto_Cropper *ec = data;
   Edje_Message_Int_Set *msg;
   Evas_Coord mx, my, cx, cy, cw, ch, nx, ny, lx, ly, lw, lh;

   evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper), &mx, &my);
   evas_object_geometry_get(ec->cropper, &cx, &cy, &cw, &ch);
   evas_object_geometry_get(ec->layout, &lx, &ly, &lw, &lh);

   if (mx < lx)
      mx = lx;
   else if (mx > lx + lw)
      mx = lx + lw;
   if (my < ly)
      my = ly;
   else if (my > ly + lh)
      my = ly + lh;

   nx = mx - ec->startx;
   ny = my - ec->starty;
   ec->startx = mx;
   ec->starty = my;

   msg = alloca(sizeof(Edje_Message_Int_Set) + (3 * sizeof(int)));
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
   edje_object_message_send(elm_layout_edje_get(ec->layout),
       EDJE_MESSAGE_INT_SET, 1, msg);
}

static void
_cropper_both_mouse_up(void *data, Evas_Object *obj EINA_UNUSED,
    const char *emission EINA_UNUSED, const char *source)
{
   Ephoto_Cropper *ec = data;

   edje_object_signal_callback_del_full(ec->cropper, "mouse,move", source,
       _cropper_both_mouse_move, ec);
   edje_object_signal_callback_del_full(ec->cropper, "mouse,up,1", source,
       _cropper_both_mouse_up, ec);
   ec->resizing = 0;
}

static void
_cropper_resize_both(void *data, Evas_Object *obj EINA_UNUSED,
    const char *emission EINA_UNUSED, const char *source)
{
   Ephoto_Cropper *ec = data;
   Evas_Coord mx, my, cx, cy;

   ec->resizing = 1;
   evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper), &mx, &my);
   evas_object_geometry_get(ec->cropper, &cx, &cy, 0, 0);
   ec->offsetx = mx - cx;
   ec->offsety = my - cy;
   ec->startx = mx;
   ec->starty = my;

   edje_object_signal_callback_add(ec->cropper, "mouse,move", source,
       _cropper_both_mouse_move, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,up,1", source,
       _cropper_both_mouse_up, ec);
}

static void
_cropper_horiz_mouse_move(void *data, Evas_Object *obj EINA_UNUSED,
    const char *emission EINA_UNUSED, const char *source)
{
   Ephoto_Cropper *ec = data;
   Edje_Message_Int_Set *msg;
   Evas_Coord mx, cx, cy, cw, ch, nx, lx, lw;

   evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper), &mx, 0);
   evas_object_geometry_get(ec->cropper, &cx, &cy, &cw, &ch);
   evas_object_geometry_get(ec->layout, &lx, 0, &lw, 0);

   if (mx < lx)
      mx = lx;
   else if (mx > lx + lw)
      mx = lx + lw;

   nx = mx - ec->startx;
   ec->startx = mx;

   msg = alloca(sizeof(Edje_Message_Int_Set) + (3 * sizeof(int)));
   msg->count = 3;
   if (!strcmp(source, "handle4"))
      msg->val[0] = 4;
   else if (!strcmp(source, "handle8"))
      msg->val[0] = 8;
   msg->val[1] = nx;
   msg->val[2] = 0;
   edje_object_message_send(elm_layout_edje_get(ec->layout),
       EDJE_MESSAGE_INT_SET, 1, msg);
}

static void
_cropper_horiz_mouse_up(void *data, Evas_Object *obj EINA_UNUSED,
    const char *emission EINA_UNUSED, const char *source)
{
   Ephoto_Cropper *ec = data;

   edje_object_signal_callback_del_full(ec->cropper, "mouse,move", source,
       _cropper_horiz_mouse_move, ec);
   edje_object_signal_callback_del_full(ec->cropper, "mouse,up,1", source,
       _cropper_horiz_mouse_up, ec);
   ec->resizing = 0;
}

static void
_cropper_resize_horiz(void *data, Evas_Object *obj EINA_UNUSED,
    const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;
   Evas_Coord mx, my, cx, cy;

   ec->resizing = 1;
   evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper), &mx, &my);
   evas_object_geometry_get(ec->cropper, &cx, &cy, 0, 0);
   ec->offsetx = mx - cx;
   ec->offsety = my - cy;
   ec->startx = mx;
   ec->starty = my;

   edje_object_signal_callback_add(ec->cropper, "mouse,move", source,
       _cropper_horiz_mouse_move, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,up,1", source,
       _cropper_horiz_mouse_up, ec);
}

static void
_cropper_vert_mouse_move(void *data, Evas_Object *obj EINA_UNUSED,
    const char *emission EINA_UNUSED, const char *source)
{
   Ephoto_Cropper *ec = data;
   Edje_Message_Int_Set *msg;
   Evas_Coord my, cx, cy, cw, ch, ny, ly, lh;

   evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper), 0, &my);
   evas_object_geometry_get(ec->cropper, &cx, &cy, &cw, &ch);
   evas_object_geometry_get(ec->layout, 0, &ly, 0, &lh);

   if (my < ly)
      my = ly;
   else if (my > ly + lh)
      my = ly + lh;

   ny = my - ec->starty;
   ec->starty = my;

   msg = alloca(sizeof(Edje_Message_Int_Set) + (3 * sizeof(int)));
   msg->count = 3;
   if (!strcmp(source, "handle2"))
      msg->val[0] = 2;
   else if (!strcmp(source, "handle6"))
      msg->val[0] = 6;
   msg->val[1] = 0;
   msg->val[2] = ny;
   edje_object_message_send(elm_layout_edje_get(ec->layout),
       EDJE_MESSAGE_INT_SET, 1, msg);
}

static void
_cropper_vert_mouse_up(void *data, Evas_Object *obj EINA_UNUSED,
    const char *emission EINA_UNUSED, const char *source)
{
   Ephoto_Cropper *ec = data;

   edje_object_signal_callback_del_full(ec->cropper, "mouse,move", source,
       _cropper_vert_mouse_move, ec);
   edje_object_signal_callback_del_full(ec->cropper, "mouse,up,1", source,
       _cropper_vert_mouse_up, ec);
   ec->resizing = 0;
}

static void
_cropper_resize_vert(void *data, Evas_Object *obj EINA_UNUSED,
    const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;
   Evas_Coord mx, my, cx, cy;

   ec->resizing = 1;
   evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper), &mx, &my);
   evas_object_geometry_get(ec->cropper, &cx, &cy, 0, 0);
   ec->offsetx = mx - cx;
   ec->offsety = my - cy;
   ec->startx = mx;
   ec->starty = my;

   edje_object_signal_callback_add(ec->cropper, "mouse,move", source,
       _cropper_vert_mouse_move, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,up,1", source,
       _cropper_vert_mouse_up, ec);
}

static void
_cropper_mouse_move(void *data, Evas_Object *obj EINA_UNUSED,
    const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;

   if (!ec->resizing)
     {
	Edje_Message_Int_Set *msg;
	Evas_Coord mx, my, cx, cy, cw, ch, nx, ny, lx, ly, lw, lh;

	evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper),
	    &mx, &my);
	evas_object_geometry_get(ec->cropper, &cx, &cy, &cw, &ch);
	evas_object_geometry_get(ec->layout, &lx, &ly, &lw, &lh);

	if (mx < lx)
	   mx = lx;
	else if (mx > lx + lw)
	   mx = lx + lw;
	if (my < ly)
	   my = ly;
	else if (my > ly + lh)
	   my = ly + lh;

	nx = mx - ec->startx;
	ny = my - ec->starty;
	ec->startx = mx;
	ec->starty = my;

	msg = alloca(sizeof(Edje_Message_Int_Set) + (3 * sizeof(int)));
	msg->count = 3;
	msg->val[0] = 0;
	msg->val[1] = nx;
	msg->val[2] = ny;
	edje_object_message_send(elm_layout_edje_get(ec->layout),
	    EDJE_MESSAGE_INT_SET, 1, msg);
     }
}

static void
_cropper_mouse_up(void *data, Evas_Object *obj EINA_UNUSED,
    const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;

   edje_object_signal_callback_del_full(ec->cropper, "mouse,move", "dragger",
       _cropper_mouse_move, ec);
   edje_object_signal_callback_del_full(ec->cropper, "mouse,up,1", "dragger",
       _cropper_mouse_up, ec);
}

static void
_cropper_move(void *data, Evas_Object *obj EINA_UNUSED,
    const char *emission EINA_UNUSED, const char *source EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;
   Evas_Coord mx, my, cx, cy;

   evas_pointer_canvas_xy_get(evas_object_evas_get(ec->cropper), &mx, &my);
   evas_object_geometry_get(ec->cropper, &cx, &cy, 0, 0);
   ec->startx = mx;
   ec->starty = my;
   ec->offsetx = mx - cx;
   ec->offsety = my - cy;

   edje_object_signal_callback_add(ec->cropper, "mouse,move", "dragger",
       _cropper_mouse_move, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,up,1", "dragger",
       _cropper_mouse_up, ec);
}

static void
_image_resize(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;
   Edje_Message_Int_Set *msg;

   Evas_Coord sx, sy, sw, sh, ix, iy, iw, ih, diffw, diffh;

   evas_object_geometry_get(ec->layout, &sx, &sy, &sw, &sh);
   evas_object_image_size_get(ec->image, &iw, &ih);

   Evas_Coord nw, nh;

   if (sw > sh)
     {
	nw = sw;
	nh = ih * ((double) sw / (double) iw);
	if (nh > sh)
	  {
	     Evas_Coord onw, onh;

	     onw = nw;
	     onh = nh;
	     nh = sh;
	     nw = onw * ((double) nh / (double) onh);
	  }
     }
   else
     {
	nh = sh;
	nw = iw * ((double) sh / (double) ih);
	if (nw > sw)
	  {
	     Evas_Coord onw, onh;

	     onw = nw;
	     onh = nh;
	     nw = sw;
	     nh = onh * ((double) nw / (double) onw);
	  }
     }
   diffw = sw - nw;
   diffh = sh - nh;
   diffw /= 2;
   diffh /= 2;
   ix = sx + diffw;
   iy = sy + diffh;

   evas_object_resize(ec->layout, nw, nh);
   evas_object_move(ec->layout, ix, iy);

   msg = alloca(sizeof(Edje_Message_Int_Set) + (3 * sizeof(int)));
   msg->count = 3;
   msg->val[0] = 9;
   msg->val[1] = 0;
   msg->val[2] = 0;
   edje_object_message_send(elm_layout_edje_get(ec->layout),
       EDJE_MESSAGE_INT_SET, 1, msg);
}

static Eina_Bool
_crop_reset(void *data, int type EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   _reset_crop(data, NULL, NULL);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_crop_apply(void *data, int type EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   _apply_crop(data, NULL, NULL);

   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_crop_cancel(void *data, int type EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   _cancel_crop(data, NULL, NULL);

   return ECORE_CALLBACK_PASS_ON;
}

static void
_editor_del(void *data, Evas *e EINA_UNUSED, Evas_Object *obj EINA_UNUSED,
    void *event_info EINA_UNUSED)
{
   Ephoto_Cropper *ec = data;
   Ecore_Event_Handler *handler;

   EINA_LIST_FREE(ec->handlers, handler)
     ecore_event_handler_del(handler);

   free(ec);
}

void
ephoto_cropper_add(Ephoto *ephoto, Evas_Object *main, Evas_Object *parent,
    Evas_Object *image_parent, Evas_Object *image)
{
   Ephoto_Cropper *ec;
   Evas_Coord w, h;

   EINA_SAFETY_ON_NULL_GOTO(image, error);

   ec = calloc(1, sizeof(Ephoto_Cropper));
   EINA_SAFETY_ON_NULL_GOTO(ec, error);

   ec->resizing = 0;
   ec->main = main;
   ec->parent = parent;
   ec->image_parent = image_parent;
   ec->image = image;

   evas_object_image_size_get(image, &w, &h);

   ec->box = elm_box_add(image_parent);
   elm_box_homogeneous_set(ec->box, EINA_TRUE);
   elm_box_horizontal_set(ec->box, EINA_TRUE);
   evas_object_size_hint_weight_set(ec->box, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(ec->box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_table_pack(image_parent, ec->box, 0, 0, 1, 1);
   evas_object_show(ec->box);

   ec->layout = elm_layout_add(ec->box);
   elm_layout_file_set(ec->layout, PACKAGE_DATA_DIR "/themes/ephoto.edj",
       "ephoto,image,cropper,base");
   evas_object_size_hint_weight_set(ec->layout, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(ec->layout, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(ec->box, ec->layout);
   evas_object_show(ec->layout);

   evas_object_size_hint_weight_set(ec->image, EVAS_HINT_EXPAND,
       EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(ec->image, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_layout_content_set(ec->layout, "ephoto.swallow.image", ec->image);
   evas_object_show(ec->image);

   ec->cropper = edje_object_add(evas_object_evas_get(ec->layout));
   edje_object_file_set(ec->cropper, PACKAGE_DATA_DIR "/themes/ephoto.edj",
       "ephoto,image,cropper");
   edje_object_signal_callback_add(elm_layout_edje_get(ec->layout),
       "cropper,changed", "ephoto.swallow.cropper", _calculate_cropper_size,
       ec);
   elm_layout_content_set(ec->layout, "ephoto.swallow.cropper", ec->cropper);
   evas_object_show(ec->cropper);

   edje_object_signal_callback_add(ec->cropper, "mouse,down,1", "dragger",
       _cropper_move, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,down,1", "handle1",
       _cropper_resize_both, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,down,1", "handle2",
       _cropper_resize_vert, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,down,1", "handle3",
       _cropper_resize_both, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,down,1", "handle4",
       _cropper_resize_horiz, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,down,1", "handle5",
       _cropper_resize_both, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,down,1", "handle6",
       _cropper_resize_vert, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,down,1", "handle7",
       _cropper_resize_both, ec);
   edje_object_signal_callback_add(ec->cropper, "mouse,down,1", "handle8",
       _cropper_resize_horiz, ec);

   evas_object_event_callback_add(ec->layout, EVAS_CALLBACK_RESIZE,
       _image_resize, ec);

   ec->editor = ephoto_editor_add(ephoto, _("Crop Image"),
       "ec", ec);
   evas_object_event_callback_add(ec->editor, EVAS_CALLBACK_DEL,
       _editor_del, ec);

   ec->croph = elm_slider_add(ec->editor);
   elm_slider_min_max_set(ec->croph, 1, h);
   elm_slider_step_set(ec->croph, 1);
   elm_slider_unit_format_set(ec->croph, "%1.0f");
   elm_object_text_set(ec->croph, _("Height"));
   evas_object_size_hint_weight_set(ec->croph, EVAS_HINT_EXPAND,
       EVAS_HINT_FILL);
   evas_object_size_hint_align_set(ec->croph, EVAS_HINT_FILL, 0.5);
   elm_box_pack_start(ec->editor, ec->croph);
   evas_object_smart_callback_add(ec->croph, "slider,drag,stop",
       _cropper_changed_height, ec);
   evas_object_show(ec->croph);

   ec->cropw = elm_slider_add(ec->editor);
   elm_slider_min_max_set(ec->cropw, 1, w);
   elm_slider_step_set(ec->cropw, 1);
   elm_slider_unit_format_set(ec->cropw, "%1.0f");
   elm_object_text_set(ec->cropw, _("Width"));
   evas_object_size_hint_weight_set(ec->cropw, EVAS_HINT_EXPAND,
       EVAS_HINT_FILL);
   evas_object_size_hint_align_set(ec->cropw, EVAS_HINT_FILL, 0.5);
   elm_box_pack_start(ec->editor, ec->cropw);
   evas_object_smart_callback_add(ec->cropw, "slider,drag,stop",
       _cropper_changed_width, ec);
   evas_object_show(ec->cropw);

   ec->handlers =
       eina_list_append(ec->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_EDITOR_RESET,
           _crop_reset, ec));
   ec->handlers =
       eina_list_append(ec->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_EDITOR_APPLY,
           _crop_apply, ec));
   ec->handlers =
       eina_list_append(ec->handlers,
       ecore_event_handler_add(EPHOTO_EVENT_EDITOR_CANCEL,
           _crop_cancel, ec));

   return;

  error:
   return;
}
