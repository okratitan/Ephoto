#include "ephoto.h"

typedef struct _Ephoto_Filter Ephoto_Filter;

struct _Ephoto_Filter
{
   Evas_Object *main;
   Evas_Object *image;
   Evas_Object *popup;
   Ecore_Idler *idler;
   Eina_List *queue;
   Evas_Coord w;
   Evas_Coord h;
   int pos;
   int rad;
   int qpos;
   int qcount;
   double drad;
   unsigned int *im_data;
   unsigned int *im_data_new;
   unsigned int *im_data_two;
};

static Eina_Bool _blur(void *data);
static Eina_Bool _sharpen(void *data);
static Eina_Bool _grayscale(void *data);
static Eina_Bool _negative(void *data);
static Eina_Bool _posterize(void *data);
static Eina_Bool _dodge(void *data);

static int
_normalize_color(int color)
{
   if (color < 0)
      return 0;
   else if (color > 255)
      return 255;
   else
      return color;
}

static int
_mul_color_alpha(int color, int alpha)
{
   if (alpha > 0 && alpha < 255)
      return color * (255 / alpha);
   else
      return color;
}

static int
_demul_color_alpha(int color, int alpha)
{
   if (alpha > 0 && alpha < 255)
      return (color * alpha) / 255;
   else
      return color;
}

static Evas_Object *
_processing(Evas_Object *main)
{
   Evas_Object *popup, *box, *label, *pb;

   evas_object_freeze_events_set(main, EINA_TRUE);

   popup = elm_popup_add(main);
   elm_object_part_text_set(popup, "title,text", _("Applying Filter"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   evas_object_size_hint_weight_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_size_hint_align_set(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   evas_object_show(box);

   label = elm_label_add(box);
   elm_object_text_set(label,
       _("Please wait while this filter is applied to your image."));
   evas_object_size_hint_weight_set(label, EVAS_HINT_EXPAND, EVAS_HINT_EXPAND);
   evas_object_size_hint_align_set(label, EVAS_HINT_FILL, EVAS_HINT_FILL);
   elm_box_pack_end(box, label);
   evas_object_show(label);

   pb = elm_progressbar_add(box);
   evas_object_size_hint_weight_set(pb, EVAS_HINT_EXPAND, 0.0);
   evas_object_size_hint_align_set(pb, EVAS_HINT_FILL, 0.5);
   elm_object_style_set(pb, "wheel");
   elm_progressbar_pulse_set(pb, EINA_TRUE);
   elm_box_pack_end(box, pb);
   evas_object_show(pb);
   elm_progressbar_pulse(pb, EINA_TRUE);

   elm_object_part_content_set(popup, "default", box);
   evas_object_show(popup);
   return popup;
}

static void
_idler_finishing_cb(Ephoto_Filter *ef, Eina_Bool im_data_two)
{
   if (ef->qcount == 0)
     {
        ephoto_single_browser_image_data_done(ef->main, ef->im_data_new,
            ef->w, ef->h);
        ecore_idler_del(ef->idler);
        if (ef->popup)
          {
             evas_object_del(ef->popup);
             evas_object_freeze_events_set(ef->main, EINA_FALSE);
          }
        if (ef->im_data)
          free(ef->im_data);
        if (ef->im_data_two)
          free(ef->im_data_two);
        free(ef);
     }
   else
     {
        if (im_data_two)
          {
             if (ef->im_data_two)
               {
                  free(ef->im_data_two);
                  ef->im_data_two = NULL;
               }
             ef->im_data_two = malloc(sizeof(unsigned int) * ef->w * ef->h);
             ef->im_data_two = memcpy(ef->im_data_two, ef->im_data,
                 sizeof(unsigned int) * ef->w * ef->h);
          }
        free(ef->im_data);
        ef->im_data = NULL;
        ef->im_data = malloc(sizeof(unsigned int) * ef->w * ef->h);
        ef->im_data = memcpy(ef->im_data, ef->im_data_new,
            sizeof(unsigned int) * ef->w * ef->h);
        ef->pos = 0;
        ef->qpos++;
        ecore_idler_del(ef->idler);
        if (ef->qpos-1 < ef->qcount)
          {
             free(ef->im_data_new);
             ef->im_data_new = NULL;
             ef->im_data_new = malloc(sizeof(unsigned int) * ef->w * ef->h);
             if (ef->idler)
               {
                  ecore_idler_del(ef->idler);
                  ef->idler = NULL;
               }
             ef->idler = ecore_idler_add(eina_list_nth(ef->queue, ef->qpos-1), ef);
          }
        else
          {
             ephoto_single_browser_image_data_done(ef->main, ef->im_data_new,
                 ef->w, ef->h);
             ecore_idler_del(ef->idler);
             if (ef->popup)
               {
                  evas_object_del(ef->popup);
                  evas_object_freeze_events_set(ef->main, EINA_FALSE);
               }
             if (ef->im_data)
               free(ef->im_data);
             if (ef->im_data_two)
               free(ef->im_data_two);
             free(ef);
          }
     }
}

static Eina_Bool
_blur(void *data)
{
   Ephoto_Filter *ef = data;
   unsigned int *im_data, *p1, *p2;
   Evas_Coord x, y, w, h, mx, my, mw, mh, mt, xx, yy;
   int a, r, g, b, rad, passes = 0;
   int *as, *rs, *gs, *bs;

   w = ef->w;
   h = ef->h;
   im_data = ef->im_data;
   rad = ef->rad;

   as = malloc(sizeof(int) * w);
   rs = malloc(sizeof(int) * w);
   gs = malloc(sizeof(int) * w);
   bs = malloc(sizeof(int) * w);

   for (y = ef->pos; y < h; y++)
     {
	my = y - rad;
	mh = (rad << 1) + 1;
	if (my < 0)
	  {
	     mh += my;
	     my = 0;
	  }
	if ((my + mh) > h)
	  {
	     mh = h - my;
	  }
	p1 = ef->im_data_new + (y * w);
	memset(as, 0, w * sizeof(int));
	memset(rs, 0, w * sizeof(int));
	memset(gs, 0, w * sizeof(int));
	memset(bs, 0, w * sizeof(int));

	for (yy = 0; yy < mh; yy++)
	  {
	     p2 = im_data + ((yy + my) * w);
	     for (x = 0; x < w; x++)
	       {
		  as[x] += (*p2 >> 24) & 0xff;
		  rs[x] += (*p2 >> 16) & 0xff;
		  gs[x] += (*p2 >> 8) & 0xff;
		  bs[x] += *p2 & 0xff;
		  p2++;
	       }
	  }
	if (w > ((rad << 1) + 1))
	  {
	     for (x = 0; x < w; x++)
	       {
		  a = 0;
		  r = 0;
		  g = 0;
		  b = 0;
		  mx = x - rad;
		  mw = (rad << 1) + 1;
		  if (mx < 0)
		    {
		       mw += mx;
		       mx = 0;
		    }
		  if ((mx + mw) > w)
		    {
		       mw = w - mx;
		    }
		  mt = mw * mh;
		  for (xx = mx; xx < (mw + mx); xx++)
		    {
		       a += as[xx];
		       r += rs[xx];
		       g += gs[xx];
		       b += bs[xx];
		    }
		  a = a / mt;
		  r = r / mt;
		  g = g / mt;
		  b = b / mt;
		  *p1 = (a << 24) | (r << 16) | (g << 8) | b;
		  p1++;
	       }
	  }
        passes++;
        if (passes == 5)
          {
             free(as);
             free(rs);
             free(gs);
             free(bs);
             ef->pos = y++;
             return EINA_TRUE;
          }
     }
   free(as);
   free(rs);
   free(gs);
   free(bs);

   _idler_finishing_cb(ef, EINA_FALSE);

   return EINA_FALSE;
}

static Eina_Bool
_sharpen(void *data)
{
   Ephoto_Filter *ef = data;
   unsigned int *p1, *p2;
   int a, r, g, b, passes = 0;
   Evas_Coord x, y, w, h;

   w = ef->w;
   h = ef->h;

   for (y = ef->pos; y < (h - 1); y++)
     {
        p1 = ef->im_data + 1 + (y * w);
        p2 = ef->im_data_new + 1 + (y * w);
        for (x = 1; x < (w - 1); x++)
          {
             b = (int) ((p1[0]) & 0xff) * 5;
             g = (int) ((p1[0] >> 8) & 0xff) * 5;
             r = (int) ((p1[0] >> 16) & 0xff) * 5;
             a = (int) ((p1[0] >> 24) & 0xff) * 5;
             b -= (int) ((p1[-1]) & 0xff);
             g -= (int) ((p1[-1] >> 8) & 0xff);
             r -= (int) ((p1[-1] >> 16) & 0xff);
             a -= (int) ((p1[-1] >> 24) & 0xff);
             b -= (int) ((p1[1]) & 0xff);
             g -= (int) ((p1[1] >> 8) & 0xff);
             r -= (int) ((p1[1] >> 16) & 0xff);
             a -= (int) ((p1[1] >> 24) & 0xff);
             b -= (int) ((p1[-w]) & 0xff);
             g -= (int) ((p1[-w] >> 8) & 0xff);
             r -= (int) ((p1[-w] >> 16) & 0xff);
             a -= (int) ((p1[-w] >> 24) & 0xff);
             b -= (int) ((p1[-w]) & 0xff);
             g -= (int) ((p1[-w] >> 8) & 0xff);
             r -= (int) ((p1[-w] >> 16) & 0xff);
             a -= (int) ((p1[-w] >> 24) & 0xff);
             a = (a & ((~a) >> 16));
             a = ((a | ((a & 256) - ((a & 256) >> 8))));
             r = (r & ((~r) >> 16));
             r = ((r | ((r & 256) - ((r & 256) >> 8))));
             g = (g & ((~g) >> 16));
             g = ((g | ((g & 256) - ((g & 256) >> 8))));
             b = (b & ((~b) >> 16));
             b = ((b | ((b & 256) - ((b & 256) >> 8))));
             *p2 = (a << 24) | (r << 16) | (g << 8) | b;
             p2++;
             p1++;
          }
        passes++;
        if (passes == 5)
          {
             ef->pos = y++;
             return EINA_TRUE;
          }
     }

   _idler_finishing_cb(ef, EINA_FALSE);

   return EINA_FALSE;  
}

static Eina_Bool
_grayscale(void *data)
{
   Ephoto_Filter *ef = data;
   int gray, i, r, g, b, a, passes = 0;
   Evas_Coord w, h;

   w = ef->w;
   h = ef->h;
   for (i = ef->pos; i < (w * h); i++)
     {
        b = (int) ((ef->im_data[i]) & 0xff);
        g = (int) ((ef->im_data[i] >> 8) & 0xff);
        r = (int) ((ef->im_data[i] >> 16) & 0xff);
        a = (int) ((ef->im_data[i] >> 24) & 0xff);
        b = _mul_color_alpha(b, a);
        g = _mul_color_alpha(g, a);
        r = _mul_color_alpha(r, a);
        gray = (int) ((0.3 * r) + (0.59 * g) + (0.11 * b));
        if (a >= 0 && a < 255)
           gray = (gray * a) / 255;
        ef->im_data_new[i] = (a << 24) | (gray << 16) | (gray << 8) | gray;
        passes++;
        if (passes == 5)
          {
             ef->pos = i++;
             return EINA_TRUE;
          }
     }

   _idler_finishing_cb(ef, EINA_FALSE);

   return EINA_FALSE;
}

static Eina_Bool
_posterize(void *data)
{
   Ephoto_Filter *ef = data;
   int i, rr, gg, bb, a, passes = 0;
   double fr, fg, fb, rad;
   Evas_Coord w, h;

   w = ef->w;
   h = ef->h;
   rad = ef->drad;
   for (i = ef->pos; i < (w * h); i++)
     {
        fb = ((ef->im_data[i]) & 0xff);
        fg = ((ef->im_data[i] >> 8) & 0xff);
        fr = ((ef->im_data[i] >> 16) & 0xff);
        a = (int) ((ef->im_data[i] >> 24) & 0xff);
        fr /= 255;
        fg /= 255;
        fb /= 255;
        rr = 255 * rint((fr * rad)) / rad;
        rr = _normalize_color(rr);
        gg = 255 * rint((fg * rad)) / rad;
        gg = _normalize_color(gg);
        bb = 255 * rint((fb * rad)) / rad;
        bb = _normalize_color(bb);
        ef->im_data_new[i] = (a << 24) | (rr << 16) | (gg << 8) | bb;
        passes++;
        if (passes == 5)
          {
             ef->pos = i++;
             return EINA_TRUE;
          }
     }

   _idler_finishing_cb(ef, EINA_FALSE);

   return EINA_FALSE;
}

static Eina_Bool
_negative(void *data)
{
   Ephoto_Filter *ef = data;
   int i, r, g, b, rr, gg, bb, a;
   int passes = 0;
   Evas_Coord w, h;

   w = ef->w;
   h = ef->h;
   for (i = ef->pos; i < (w * h); i++)
     {
        b = (int) ((ef->im_data[i]) & 0xff);
        g = (int) ((ef->im_data[i] >> 8) & 0xff);
        r = (int) ((ef->im_data[i] >> 16) & 0xff);
        a = (int) ((ef->im_data[i] >> 24) & 0xff);
        b = _mul_color_alpha(b, a);
        g = _mul_color_alpha(g, a);
        r = _mul_color_alpha(r, a);
        rr = 255 - r;
        gg = 255 - g;
        bb = 255 - b;
        rr = _normalize_color(rr);
        gg = _normalize_color(gg);
        bb = _normalize_color(bb);
        bb = _demul_color_alpha(bb, a);
        gg = _demul_color_alpha(gg, a);
        rr = _demul_color_alpha(rr, a);
        ef->im_data_new[i] = (a << 24) | (rr << 16) | (gg << 8) | bb;

        passes++;
        if (passes == 5)
          {
             ef->pos = i++;
             return EINA_TRUE;
          }
     }

   _idler_finishing_cb(ef, EINA_TRUE);

   return EINA_FALSE;
}

static Eina_Bool
_dodge(void *data)
{
   Ephoto_Filter *ef = data;
   double r, g, b, rr, gg, bb;
   int i, rrr, ggg, bbb, passes = 0;;
   Evas_Coord w, h;

   w = ef->w;
   h = ef->h;
   for (i = ef->pos; i < (w * h); i++)
     {
        b = ((ef->im_data_two[i]) & 0xff);
        g = ((ef->im_data_two[i] >> 8) & 0xff);
        r = ((ef->im_data_two[i] >> 16) & 0xff);

        bb = ((ef->im_data[i]) & 0xff);
        gg = ((ef->im_data[i] >> 8) & 0xff);
        rr = ((ef->im_data[i] >> 16) & 0xff);
  
        b *= 255;
        g *= 255;
        r *= 255;
  
        bbb = rint(b / (255 - bb));
        ggg = rint(g / (255 - gg));
        rrr = rint(r / (255 - rr));

        rrr = _normalize_color(rrr);
        ggg = _normalize_color(ggg);
        bbb = _normalize_color(bbb);

        ef->im_data_new[i] = (255 << 24) | (rrr << 16) | (ggg << 8) | bbb;

        passes++;
        if (passes == 5)
          {
             ef->pos = i++;
             return EINA_TRUE;
          }
     }

   _idler_finishing_cb(ef, EINA_FALSE);

   return EINA_FALSE;
}

void
ephoto_filter_blur(Evas_Object *main, Evas_Object *image)
{
   Ephoto_Filter *ef = calloc(1, sizeof(Ephoto_Filter));
   Evas_Coord w, h;
   unsigned int *im_data;

   im_data =
       evas_object_image_data_get(elm_image_object_get(image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(image), &w, &h);

   ef->main = main;
   ef->image = image;
   ef->im_data = malloc(sizeof(unsigned int) * w * h);
   ef->im_data = memcpy(ef->im_data, im_data, sizeof(unsigned int) * w * h);
   ef->im_data_new = malloc(sizeof(unsigned int) * w * h);
   ef->im_data_two = NULL;
   ef->rad = 9;
   ef->drad = 0.0;
   ef->pos = 0;
   ef->w = w;
   ef->h = h;
   ef->qpos = 0;
   ef->qcount = 0;
   ef->queue = NULL;
   ef->popup = _processing(main);
   ef->idler = ecore_idler_add(_blur, ef);
}

void
ephoto_filter_sharpen(Evas_Object *main, Evas_Object *image)
{
   Ephoto_Filter *ef = calloc(1, sizeof(Ephoto_Filter));
   unsigned int *im_data;
   Evas_Coord w, h;

   im_data =
       evas_object_image_data_get(elm_image_object_get(image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(image), &w, &h);

   ef->main = main;
   ef->image = image;
   ef->im_data = malloc(sizeof(unsigned int) * w * h);
   ef->im_data = memcpy(ef->im_data, im_data, sizeof(unsigned int) * w * h);
   ef->im_data_new = malloc(sizeof(unsigned int) * w * h);
   ef->im_data_two = NULL;
   ef->rad = 0;
   ef->drad = 0.0;
   ef->pos = 1;
   ef->w = w;
   ef->h = h;
   ef->qpos = 0;
   ef->qcount = 0;
   ef->queue = NULL;
   ef->popup = _processing(main);
   ef->idler = ecore_idler_add(_sharpen, ef);
}

void
ephoto_filter_black_and_white(Evas_Object *main, Evas_Object *image)
{
   Ephoto_Filter *ef = calloc(1, sizeof(Ephoto_Filter));
   unsigned int *im_data;
   Evas_Coord w, h;

   im_data =
       evas_object_image_data_get(elm_image_object_get(image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(image), &w, &h);

   ef->main = main;
   ef->image = image;
   ef->im_data = malloc(sizeof(unsigned int) * w * h);
   ef->im_data = memcpy(ef->im_data, im_data, sizeof(unsigned int) * w * h);
   ef->im_data_new = malloc(sizeof(unsigned int) * w * h);
   ef->im_data_two = NULL;
   ef->rad = 0;
   ef->drad = 0.0;
   ef->pos = 0;
   ef->w = w;
   ef->h = h;
   ef->qpos = 0;
   ef->qcount = 0;
   ef->queue = NULL;
   ef->popup = _processing(main);
   ef->idler = ecore_idler_add(_grayscale, ef);
}

void
ephoto_filter_old_photo(Evas_Object *main, Evas_Object *image)
{
   unsigned int *im_data, *im_data_new;
   int i, r, rr, g, gg, b, bb, a;
   Evas_Coord w, h;

   im_data =
       evas_object_image_data_get(elm_image_object_get(image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(image), &w, &h);
   im_data_new = malloc(sizeof(unsigned int) * w * h);
   for (i = 0; i < (w * h); i++)
     {
	b = (int) ((im_data[i]) & 0xff);
	g = (int) ((im_data[i] >> 8) & 0xff);
	r = (int) ((im_data[i] >> 16) & 0xff);
	a = (int) ((im_data[i] >> 24) & 0xff);
	b = _mul_color_alpha(b, a);
	g = _mul_color_alpha(g, a);
	r = _mul_color_alpha(r, a);
	rr = (int) ((r * .393) + (g * .769) + (b * .189));
	rr = _normalize_color(rr);
	gg = (int) ((r * .349) + (g * .686) + (b * .168));
	gg = _normalize_color(gg);
	bb = (int) ((r * .272) + (g * .534) + (b * .131));
	bb = _normalize_color(bb);
	bb = _demul_color_alpha(bb, a);
	gg = _demul_color_alpha(gg, a);
	rr = _demul_color_alpha(rr, a);
	im_data_new[i] = (a << 24) | (rr << 16) | (gg << 8) | bb;
     }
   ephoto_single_browser_image_data_done(main, im_data_new,
       w, h);
}

void
ephoto_filter_posterize(Evas_Object *main, Evas_Object *image)
{
   Ephoto_Filter *ef = calloc(1, sizeof(Ephoto_Filter));
   unsigned int *im_data;
   Evas_Coord w, h;

   im_data =
       evas_object_image_data_get(elm_image_object_get(image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(image), &w, &h);

   ef->main = main;
   ef->image = image;
   ef->im_data = malloc(sizeof(unsigned int) * w * h);
   ef->im_data = memcpy(ef->im_data, im_data, sizeof(unsigned int) * w * h);
   ef->im_data_new = malloc(sizeof(unsigned int) * w * h);
   ef->im_data_two = NULL;
   ef->rad = 0;
   ef->drad = 2.0;
   ef->pos = 0;
   ef->w = w;
   ef->h = h;
   ef->qpos = 0;
   ef->qcount = 0;
   ef->queue = NULL;
   ef->popup = _processing(main);
   ef->idler = ecore_idler_add(_posterize, ef);
}

void
ephoto_filter_cartoon(Evas_Object *main, Evas_Object *image)
{
   Ephoto_Filter *ef = calloc(1, sizeof(Ephoto_Filter));
   unsigned int *im_data;
   Evas_Coord w, h;

   im_data =
       evas_object_image_data_get(elm_image_object_get(image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(image), &w, &h);

   ef->main = main;
   ef->image = image;
   ef->im_data = malloc(sizeof(unsigned int) * w * h);
   ef->im_data = memcpy(ef->im_data, im_data, sizeof(unsigned int) * w * h);
   ef->im_data_new = malloc(sizeof(unsigned int) * w * h);
   ef->im_data_two = NULL;
   ef->rad = 5;
   ef->drad = 5.0;
   ef->pos = 0;
   ef->w = w;
   ef->h = h;
   ef->qpos = 0;
   ef->qcount = 1;
   ef->queue = eina_list_append(ef->queue, _posterize);
   ef->popup = _processing(main);
   ef->idler = ecore_idler_add(_blur, ef);
}

void ephoto_filter_invert(Evas_Object *main, Evas_Object *image)
{
   Ephoto_Filter *ef = calloc(1, sizeof(Ephoto_Filter));
   unsigned int *im_data;
   Evas_Coord w, h;

   im_data =
       evas_object_image_data_get(elm_image_object_get(image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(image), &w, &h);

   ef->main = main;
   ef->image = image;
   ef->im_data = malloc(sizeof(unsigned int) * w * h);
   ef->im_data = memcpy(ef->im_data, im_data, sizeof(unsigned int) * w * h); 
   ef->im_data_new = malloc(sizeof(unsigned int) * w * h);
   ef->im_data_two = NULL;
   ef->rad = 0;
   ef->rad = 0.0;
   ef->pos = 0;
   ef->w = w;
   ef->h = h;
   ef->qpos = 0;
   ef->qcount = 0;
   ef->queue = NULL;
   ef->popup = _processing(main);
   ef->idler = ecore_idler_add(_negative, ef);
}

void ephoto_filter_sketch(Evas_Object *main, Evas_Object *image)
{
   Ephoto_Filter *ef = calloc(1, sizeof(Ephoto_Filter));
   unsigned int *im_data;
   Evas_Coord w, h;

   im_data =
       evas_object_image_data_get(elm_image_object_get(image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(image), &w, &h);

   ef->main = main;
   ef->image = image;
   ef->im_data = malloc(sizeof(unsigned int) * w * h);
   ef->im_data = memcpy(ef->im_data, im_data, sizeof(unsigned int) * w * h);
   ef->im_data_new = malloc(sizeof(unsigned int) * w * h);
   ef->im_data_two = NULL;
   ef->rad = 4;
   ef->drad = 0.0;
   ef->pos = 0;
   ef->w = w;
   ef->h = h;
   ef->qpos = 0;
   ef->qcount = 3;
   ef->queue = eina_list_append(ef->queue, _negative);
   ef->queue = eina_list_append(ef->queue, _blur);
   ef->queue = eina_list_append(ef->queue, _dodge);
   ef->popup = _processing(main);
   ef->idler = ecore_idler_add(_grayscale, ef);  
}

void
ephoto_filter_histogram_eq(Evas_Object *main, Evas_Object *image)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   Evas_Coord x, y, w, h;
   int i, hist[256], cdf[256];
   int a, r, g, b, bb, gg, rr, norm, total;
   float hh, s, v, nv, sum;

   im_data =
       evas_object_image_data_get(elm_image_object_get(image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(image), &w, &h);
   im_data_new = malloc(sizeof(unsigned int) * w * h);
   total = w * h;
   for (i = 0; i < 256; i++)
      hist[i] = 0;
   for (y = 0; y < h; y++)
     {
	p1 = im_data + (y * w);
	for (x = 0; x < w; x++)
	  {
	     b = (int) ((*p1) & 0xff);
	     g = (int) ((*p1 >> 8) & 0xff);
	     r = (int) ((*p1 >> 16) & 0xff);
	     a = (int) ((*p1 >> 24) & 0xff);
	     b = _mul_color_alpha(b, a);
	     g = _mul_color_alpha(g, a);
	     r = _mul_color_alpha(r, a);
	     evas_color_rgb_to_hsv(r, g, b, &hh, &s, &v);
	     norm = (int) round((double) v * (double) 255);
	     hist[norm] += 1;
	     p1++;
	  }
     }
   sum = 0;
   for (i = 0; i < 256; i++)
     {
	sum += ((double) hist[i] / (double) total);
	cdf[i] = (int) round(sum * 255);
     }
   for (y = 0; y < h; y++)
     {
	p1 = im_data + (y * w);
	p2 = im_data_new + (y * w);
	for (x = 0; x < w; x++)
	  {
	     b = (int) ((*p1) & 0xff);
	     g = (int) ((*p1 >> 8) & 0xff);
	     r = (int) ((*p1 >> 16) & 0xff);
	     a = (int) ((*p1 >> 24) & 0xff);
	     b = _mul_color_alpha(b, a);
	     g = _mul_color_alpha(g, a);
	     r = _mul_color_alpha(r, a);
	     evas_color_rgb_to_hsv(r, g, b, &hh, &s, &v);
	     norm = (int) round((double) v * (double) 255);
	     nv = (float) cdf[norm] / (float) 255;
	     evas_color_hsv_to_rgb(hh, s, nv, &rr, &gg, &bb);
	     bb = _normalize_color(bb);
	     gg = _normalize_color(gg);
	     rr = _normalize_color(rr);
	     bb = _demul_color_alpha(bb, a);
	     gg = _demul_color_alpha(gg, a);
	     rr = _demul_color_alpha(rr, a);
	     *p2 = (a << 24) | (rr << 16) | (gg << 8) | bb;
	     p2++;
	     p1++;
	  }
     }
   ephoto_single_browser_image_data_done(main, im_data_new,
       w, h);
}
