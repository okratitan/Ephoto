#include "ephoto.h"

typedef enum _Ephoto_Image_Filter Ephoto_Image_Filter;
typedef struct _Ephoto_Filter     Ephoto_Filter;

enum _Ephoto_Image_Filter
{
   EPHOTO_IMAGE_FILTER_BLUR,
   EPHOTO_IMAGE_FILTER_DITHER,
   EPHOTO_IMAGE_FILTER_EMBOSS,
   EPHOTO_IMAGE_FILTER_EQUALIZE,
   EPHOTO_IMAGE_FILTER_GRAYSCALE,
   EPHOTO_IMAGE_FILTER_INVERT,
   EPHOTO_IMAGE_FILTER_PAINTING,
   EPHOTO_IMAGE_FILTER_POSTERIZE,
   EPHOTO_IMAGE_FILTER_SEPIA,
   EPHOTO_IMAGE_FILTER_SHARPEN,
   EPHOTO_IMAGE_FILTER_SKETCH,
   EPHOTO_IMAGE_FILTER_SOBEL
};

struct _Ephoto_Filter
{
   Ephoto_Image_Filter filter;
   Ephoto             *ephoto;
   Evas_Object        *image;
   Evas_Object        *popup;
   Ecore_Thread       *thread;
   Eina_List          *queue;
   Evas_Coord          w;
   Evas_Coord          h;
   int                 rad;
   int                 qpos;
   int                 qcount;
   double              drad;
   unsigned int       *hist;
   unsigned int       *cdf;
   unsigned int       *im_data;
   unsigned int       *im_data_new;
   unsigned int       *im_data_orig;
   unsigned int       *im_data_two;
   Eina_Bool           save_data;
};

static void _blur(void *data, Ecore_Thread *th EINA_UNUSED);
static void _sharpen(void *data, Ecore_Thread *th EINA_UNUSED);
static void _dither(void *data, Ecore_Thread *th EINA_UNUSED);
static void _grayscale(void *data, Ecore_Thread *th EINA_UNUSED);
static void _sepia(void *data, Ecore_Thread *th EINA_UNUSED);
static void _negative(void *data, Ecore_Thread *th EINA_UNUSED);
static void _posterize(void *data, Ecore_Thread *th EINA_UNUSED);
static void _dodge(void *data, Ecore_Thread *th EINA_UNUSED);
static void _sobel(void *data, Ecore_Thread *th EINA_UNUSED);
static void _emboss(void *data, Ecore_Thread *th EINA_UNUSED);
static void _histogram_eq(void *data, Ecore_Thread *th EINA_UNUSED);

static Ephoto_Filter *
_initialize_filter(Ephoto_Image_Filter filter,
                   Ephoto *ephoto, Evas_Object *image)
{
   Ephoto_Filter *ef = calloc(1, sizeof(Ephoto_Filter));
   Evas_Coord w, h;
   unsigned int *im_data;

   im_data =
     evas_object_image_data_get(image, EINA_FALSE);
   evas_object_image_size_get(image, &w, &h);

   ef->filter = filter;
   ef->ephoto = ephoto;
   ef->image = image;
   ef->im_data = malloc(sizeof(unsigned int) * w * h);
   memcpy(ef->im_data, im_data, sizeof(unsigned int) * w * h);
   ef->im_data_new = malloc(sizeof(unsigned int) * w * h);
   ef->im_data_two = NULL;
   ef->im_data_orig = NULL;
   ef->rad = 0;
   ef->drad = 0.0;
   ef->w = w;
   ef->h = h;
   ef->qpos = 0;
   ef->qcount = 0;
   ef->queue = NULL;
   ef->hist = NULL;
   ef->cdf = NULL;
   ef->save_data = EINA_FALSE;

   return ef;
}

static void
_create_hist(Ephoto_Filter *ef)
{
   int i;
   for (i = 0; i < 256; i++)
     ef->hist[i] = 0;
}

static Evas_Object *
_processing(Evas_Object *main)
{
   Evas_Object *popup, *box, *label, *pb;

   popup = elm_popup_add(main);
   elm_object_part_text_set(popup, "title,text", _("Applying Filter"));
   elm_popup_orient_set(popup, ELM_POPUP_ORIENT_CENTER);

   box = elm_box_add(popup);
   elm_box_horizontal_set(box, EINA_FALSE);
   EPHOTO_WEIGHT(box, EVAS_HINT_FILL, EVAS_HINT_FILL);
   EPHOTO_FILL(box);
   evas_object_show(box);

   label = elm_label_add(box);
   elm_object_text_set(label,
                       _("Please wait while this filter is applied to your image."));
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

   elm_object_part_content_set(popup, "default", box);
   evas_object_show(popup);

   return popup;
}

static void
_thread_finished_cb(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Ephoto_Filter *ef = data;
   if (ef->qcount == 0)
     {
        ephoto_single_browser_image_data_done(ef->ephoto->single_browser,
                                              ef->im_data_new, ef->w, ef->h);
        if (ef->popup)
          {
             evas_object_del(ef->popup);
          }
        if (ef->im_data)
          free(ef->im_data);
        if (ef->im_data_two)
          free(ef->im_data_two);
        if (ef->im_data_orig)
          free(ef->im_data_orig);
        free(ef);
     }
   else
     {
        if (ef->save_data)
          {
             if (ef->im_data_two)
               {
                  free(ef->im_data_two);
                  ef->im_data_two = NULL;
               }
             ef->im_data_two = malloc(sizeof(unsigned int) * ef->w * ef->h);
             memcpy(ef->im_data_two, ef->im_data,
                    sizeof(unsigned int) * ef->w * ef->h);
             ef->save_data = EINA_FALSE;
          }
        free(ef->im_data);
        ef->im_data = NULL;
        ef->im_data = malloc(sizeof(unsigned int) * ef->w * ef->h);
        memcpy(ef->im_data, ef->im_data_new,
               sizeof(unsigned int) * ef->w * ef->h);
        if (ef->hist)
          free(ef->hist);
        ef->hist = NULL;
        if (ef->cdf)
          free(ef->cdf);
        ef->cdf = NULL;
        ef->qpos++;
        if (ef->qpos - 1 < ef->qcount)
          {
             free(ef->im_data_new);
             ef->im_data_new = NULL;
             ef->im_data_new = malloc(sizeof(unsigned int) * ef->w * ef->h);
             ef->thread = ecore_thread_run(eina_list_nth(ef->queue, ef->qpos - 1),
                                           _thread_finished_cb, NULL, ef);
          }
        else
          {
             ephoto_single_browser_image_data_done(ef->ephoto->single_browser,
                                                   ef->im_data_new, ef->w, ef->h);
             if (ef->popup)
               {
                  evas_object_del(ef->popup);
               }
             if (ef->im_data)
               free(ef->im_data);
             if (ef->im_data_two)
               free(ef->im_data_two);
             if (ef->im_data_orig)
               free(ef->im_data_orig);
             free(ef);
          }
     }
}

static void
_blur_vertical(Ephoto_Filter *ef, double rad)
{
   Evas_Coord y, w, h;
   int i, at, rt, gt, bt;
   unsigned int *as, *rs, *gs, *bs;
   double iarr;

   w = ef->w;
   h = ef->h;

   as = malloc(sizeof(unsigned int) * w * h);
   rs = malloc(sizeof(unsigned int) * w * h);
   gs = malloc(sizeof(unsigned int) * w * h);
   bs = malloc(sizeof(unsigned int) * w * h);

   for (i = 0; i < w * h; i++)
     {
        bs[i] = ef->im_data[i] & 0xff;
        gs[i] = (ef->im_data[i] >> 8) & 0xff;
        rs[i] = (ef->im_data[i] >> 16) & 0xff;
        as[i] = (ef->im_data[i] >> 24) & 0xff;
     }

   iarr = (double)1 / (rad + rad + 1);
   for (y = 0; y < h; y++)
     {
        int t, l, rr;
        int fr, fg, fb, fa;
        int lvr, lvg, lvb, lva;
        int valr, valg, valb, vala;

        t = y * w;
        l = t;
        rr = t + rad;

        fb = bs[t];
        fg = gs[t];
        fr = rs[t];
        fa = as[t];

        lvb = bs[t + w - 1];
        lvg = gs[t + w - 1];
        lvr = rs[t + w - 1];
        lva = as[t + w - 1];

        valb = (rad + 1) * fb;
        valg = (rad + 1) * fg;
        valr = (rad + 1) * fr;
        vala = (rad + 1) * fa;

        for (i = 0; i < rad; i++)
          {
             valb += bs[t + i];
             valg += gs[t + i];
             valr += rs[t + i];
             vala += as[t + i];
          }
        for (i = 0; i <= rad; i++)
          {
             int r = rr++;

             valb += bs[r] - fb;
             valg += gs[r] - fg;
             valr += rs[r] - fr;
             vala += as[r] - fa;

             bt = (int)round(valb * iarr);
             gt = (int)round(valg * iarr);
             rt = (int)round(valr * iarr);
             at = (int)round(vala * iarr);

             bt = ephoto_normalize_color(bt);
             gt = ephoto_normalize_color(gt);
             rt = ephoto_normalize_color(rt);
             at = ephoto_normalize_color(at);
             ef->im_data_new[t++] = (at << 24) | (rt << 16)
               | (gt << 8) | bt;
          }
        for (i = rad + 1; i < w - rad; i++)
          {
             int r = rr++;
             int ll = l++;

             valb += bs[r] - bs[ll];
             valg += gs[r] - gs[ll];
             valr += rs[r] - rs[ll];
             vala += as[r] - as[ll];

             bt = (int)round(valb * iarr);
             gt = (int)round(valg * iarr);
             rt = (int)round(valr * iarr);
             at = (int)round(vala * iarr);
             bt = ephoto_normalize_color(bt);
             gt = ephoto_normalize_color(gt);
             rt = ephoto_normalize_color(rt);
             at = ephoto_normalize_color(at);
             ef->im_data_new[t++] = (at << 24) | (rt << 16)
               | (gt << 8) | bt;
          }
        for (i = w - rad; i < w; i++)
          {
             int ll = l++;

             valb += lvb - bs[ll];
             valg += lvg - gs[ll];
             valr += lvr - rs[ll];
             vala += lva - as[ll];

             bt = (int)round(valb * iarr);
             gt = (int)round(valg * iarr);
             rt = (int)round(valr * iarr);
             at = (int)round(vala * iarr);
             bt = ephoto_normalize_color(bt);
             gt = ephoto_normalize_color(gt);
             rt = ephoto_normalize_color(rt);
             at = ephoto_normalize_color(at);
             ef->im_data_new[t++] = (at << 24) | (rt << 16)
               | (gt << 8) | bt;
          }
     }
   free(bs);
   free(gs);
   free(rs);
   free(as);
}

static void
_blur_horizontal(Ephoto_Filter *ef, double rad)
{
   Evas_Coord x, w, h;
   int i, at, rt, gt, bt;
   unsigned int *as, *rs, *gs, *bs;
   double iarr;

   w = ef->w;
   h = ef->h;

   as = malloc(sizeof(unsigned int) * w * h);
   rs = malloc(sizeof(unsigned int) * w * h);
   gs = malloc(sizeof(unsigned int) * w * h);
   bs = malloc(sizeof(unsigned int) * w * h);

   for (i = 0; i < w * h; i++)
     {
        bs[i] = ef->im_data_new[i] & 0xff;
        gs[i] = (ef->im_data_new[i] >> 8) & 0xff;
        rs[i] = (ef->im_data_new[i] >> 16) & 0xff;
        as[i] = (ef->im_data_new[i] >> 24) & 0xff;
     }

   iarr = (double)1 / (rad + rad + 1);
   for (x = 0; x < w; x++)
     {
        int t, l, rr;
        int fr, fg, fb, fa;
        int lvr, lvg, lvb, lva;
        int valr, valg, valb, vala;

        t = x;
        l = t;
        rr = t + rad * w;

        fb = bs[t];
        fg = gs[t];
        fr = rs[t];
        fa = as[t];

        lvb = bs[t + w * (h - 1)];
        lvg = gs[t + w * (h - 1)];
        lvr = rs[t + w * (h - 1)];
        lva = as[t + w * (h - 1)];

        valb = (rad + 1) * fb;
        valg = (rad + 1) * fg;
        valr = (rad + 1) * fr;
        vala = (rad + 1) * fa;

        for (i = 0; i < rad; i++)
          {
             valb += bs[t + i * w];
             valg += gs[t + i * w];
             valr += rs[t + i * w];
             vala += as[t + i * w];
          }
        for (i = 0; i <= rad; i++)
          {
             valb += bs[rr] - fb;
             valg += gs[rr] - fg;
             valr += rs[rr] - fr;
             vala += as[rr] - fa;

             bt = (int)round(valb * iarr);
             gt = (int)round(valg * iarr);
             rt = (int)round(valr * iarr);
             at = (int)round(vala * iarr);
             bt = ephoto_normalize_color(bt);
             gt = ephoto_normalize_color(gt);
             rt = ephoto_normalize_color(rt);
             at = ephoto_normalize_color(at);
             ef->im_data[t] = (at << 24) | (rt << 16)
               | (gt << 8) | bt;

             rr += w;
             t += w;
          }
        for (i = rad + 1; i < h - rad; i++)
          {
             valb += bs[rr] - bs[l];
             valg += gs[rr] - gs[l];
             valr += rs[rr] - rs[l];
             vala += as[rr] - as[l];

             bt = (int)round(valb * iarr);
             gt = (int)round(valg * iarr);
             rt = (int)round(valr * iarr);
             at = (int)round(vala * iarr);
             bt = ephoto_normalize_color(bt);
             gt = ephoto_normalize_color(gt);
             rt = ephoto_normalize_color(rt);
             at = ephoto_normalize_color(at);
             ef->im_data[t] = (at << 24) | (rt << 16)
               | (gt << 8) | bt;

             l += w;
             rr += w;
             t += w;
          }
        for (i = h - rad; i < h; i++)
          {
             valb += lvb - bs[l];
             valg += lvg - gs[l];
             valr += lvr - rs[l];
             vala += lva - as[l];

             bt = (int)round(valb * iarr);
             gt = (int)round(valg * iarr);
             rt = (int)round(valr * iarr);
             at = (int)round(vala * iarr);
             bt = ephoto_normalize_color(bt);
             gt = ephoto_normalize_color(gt);
             rt = ephoto_normalize_color(rt);
             at = ephoto_normalize_color(at);
             ef->im_data[t] = (at << 24) | (rt << 16)
               | (gt << 8) | bt;

             l += w;
             t += w;
          }
     }
   free(bs);
   free(gs);
   free(rs);
   free(as);
}

static void
_blur(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Ephoto_Filter *ef = data;
   Evas_Coord w, h;
   int wl, wu, m, i;
   unsigned int sizes[3];
   double ideal, rad;

   w = ef->w;
   h = ef->h;
   rad = ef->w * .001 + ef->rad;

   ideal = sqrt((12 * rad * rad / 3) + 1);
   wl = floor(ideal);
   if (wl % 2 == 0)
     wl--;
   wu = wl + 2;
   ideal = (12 * rad * rad - 3 * wl * wl -
            4 * 3 * wl - 3 * 3) / (-4 * wl - 4);
   m = round(ideal);

   for (i = 0; i < 3; i++)
     {
        if (i < m)
          sizes[i] = wl;
        else
          sizes[i] = wu;

        rad = (sizes[i] - 1) / 2;

        memcpy(ef->im_data_new, ef->im_data,
               sizeof(unsigned int) * w * h);
        _blur_vertical(ef, rad);
        _blur_horizontal(ef, rad);
     }
}

static void
_sharpen(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Ephoto_Filter *ef = data;
   unsigned int *p1, *p2, *p3;
   int a, r, g, b;
   int aa, rr, gg, bb;
   int aaa, rrr, ggg, bbb;
   Evas_Coord x, y, w, h;

   w = ef->w;
   h = ef->h;

   for (y = 0; y < h; y++)
     {
        p1 = ef->im_data + (y * w);
        p2 = ef->im_data_orig + (y * w);
        p3 = ef->im_data_new + (y * w);
        for (x = 1; x < (w - 1); x++)
          {
             b = (int)(*p1 & 0xff);
             g = (int)((*p1 >> 8) & 0xff);
             r = (int)((*p1 >> 16) & 0xff);
             a = (int)((*p1 >> 24) & 0xff);
             bb = (int)(*p2 & 0xff);
             gg = (int)((*p2 >> 8) & 0xff);
             rr = (int)((*p2 >> 16) & 0xff);
             aa = (int)((*p2 >> 24) & 0xff);

             bbb = (int)((2 * bb) - b);
             ggg = (int)((2 * gg) - g);
             rrr = (int)((2 * rr) - r);
             aaa = (int)((2 * aa) - a);

             bbb = ephoto_normalize_color(bbb);
             ggg = ephoto_normalize_color(ggg);
             rrr = ephoto_normalize_color(rrr);
             aaa = ephoto_normalize_color(aaa);

             *p3 = (aaa << 24) | (rrr << 16) | (ggg << 8) | bbb;
             p3++;
             p2++;
             p1++;
          }
     }
}

static void
_dither(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Ephoto_Filter *ef = data;
   Evas_Coord x, y, w, h;
   int a, r, g, b;
   int rr, gg, bb;
   int errr, errg, errb;

   w = ef->w;
   h = ef->h;

   for (y = 0; y < h; y++)
     {
        for (x = 0; x < w; x++)
          {
             int index = y * w + x;

             b = (ef->im_data_new[index]) & 0xff;
             g = ((ef->im_data_new[index] >> 8) & 0xff);
             r = ((ef->im_data_new[index] >> 16) & 0xff);
             a = ((ef->im_data_new[index] >> 24) & 0xff);
             b = ephoto_mul_color_alpha(b, a);
             g = ephoto_mul_color_alpha(g, a);
             r = ephoto_mul_color_alpha(r, a);
             bb = (b > 127) ? 255 : 0;
             gg = (g > 127) ? 255 : 0;
             rr = (r > 127) ? 255 : 0;
             rr = ephoto_normalize_color(rr);
             gg = ephoto_normalize_color(gg);
             bb = ephoto_normalize_color(bb);
             bb = ephoto_demul_color_alpha(bb, a);
             gg = ephoto_demul_color_alpha(gg, a);
             rr = ephoto_demul_color_alpha(rr, a);
             ef->im_data_new[index] = (a << 24) | (rr << 16) |
               (gg << 8) | bb;
             errb = b - bb;
             errg = g - gg;
             errr = r - rr;

             if ((x + 1) < w)
               {
                  index = y * w + x + 1;
                  b = (ef->im_data_new[index] & 0xff);
                  g = ((ef->im_data_new[index] >> 8) & 0xff);
                  r = ((ef->im_data_new[index] >> 16) & 0xff);
                  a = ((ef->im_data_new[index] >> 24) & 0xff);
                  b = ephoto_mul_color_alpha(b, a);
                  g = ephoto_mul_color_alpha(g, a);
                  r = ephoto_mul_color_alpha(r, a);
                  bb = b + ((7 * errb) >> 4);
                  gg = g + ((7 * errg) >> 4);
                  rr = r + ((7 * errr) >> 4);
                  bb = ephoto_normalize_color(bb);
                  gg = ephoto_normalize_color(gg);
                  rr = ephoto_normalize_color(rr);
                  bb = ephoto_demul_color_alpha(bb, a);
                  gg = ephoto_demul_color_alpha(gg, a);
                  rr = ephoto_demul_color_alpha(rr, a);
                  ef->im_data_new[index] = (a << 24) | (rr << 16) |
                    (gg << 8) | bb;
               }
             if (x > 0 && (y + 1) < h)
               {
                  index = (y + 1) * w + (x - 1);
                  b = (ef->im_data_new[index] & 0xff);
                  g = ((ef->im_data_new[index] >> 8) & 0xff);
                  r = ((ef->im_data_new[index] >> 16) & 0xff);
                  a = ((ef->im_data_new[index] >> 24) & 0xff);
                  b = ephoto_mul_color_alpha(b, a);
                  g = ephoto_mul_color_alpha(g, a);
                  r = ephoto_mul_color_alpha(r, a);
                  bb = b + ((3 * errb) >> 4);
                  gg = g + ((3 * errg) >> 4);
                  rr = r + ((3 * errr) >> 4);
                  bb = ephoto_normalize_color(bb);
                  gg = ephoto_normalize_color(gg);
                  rr = ephoto_normalize_color(rr);
                  bb = ephoto_demul_color_alpha(bb, a);
                  gg = ephoto_demul_color_alpha(gg, a);
                  rr = ephoto_demul_color_alpha(rr, a);
                  ef->im_data_new[index] = (a << 24) | (rr << 16) |
                    (gg << 8) | bb;
               }
             if ((y + 1) < h)
               {
                  index = (y + 1) * w + x;
                  b = (ef->im_data_new[index] & 0xff);
                  g = ((ef->im_data_new[index] >> 8) & 0xff);
                  r = ((ef->im_data_new[index] >> 16) & 0xff);
                  a = ((ef->im_data_new[index] >> 24) & 0xff);
                  b = ephoto_mul_color_alpha(b, a);
                  g = ephoto_mul_color_alpha(g, a);
                  r = ephoto_mul_color_alpha(r, a);
                  bb = b + ((5 * errb) >> 4);
                  gg = g + ((5 * errg) >> 4);
                  rr = r + ((5 * errr) >> 4);
                  bb = ephoto_normalize_color(bb);
                  gg = ephoto_normalize_color(gg);
                  rr = ephoto_normalize_color(rr);
                  bb = ephoto_demul_color_alpha(bb, a);
                  gg = ephoto_demul_color_alpha(gg, a);
                  rr = ephoto_demul_color_alpha(rr, a);
                  ef->im_data_new[index] = (a << 24) | (rr << 16) |
                    (gg << 8) | bb;
               }
             if ((y + 1) < h && (x + 1) < w)
               {
                  index = (y + 1) * w + (x + 1);
                  b = (ef->im_data_new[index] & 0xff);
                  g = ((ef->im_data_new[index] >> 8) & 0xff);
                  r = ((ef->im_data_new[index] >> 16) & 0xff);
                  a = ((ef->im_data_new[index] >> 24) & 0xff);
                  b = ephoto_mul_color_alpha(b, a);
                  g = ephoto_mul_color_alpha(g, a);
                  r = ephoto_mul_color_alpha(r, a);
                  bb = b + ((1 * errb) >> 4);
                  gg = g + ((1 * errg) >> 4);
                  rr = r + ((1 * errr) >> 4);
                  bb = ephoto_normalize_color(bb);
                  gg = ephoto_normalize_color(gg);
                  rr = ephoto_normalize_color(rr);
                  bb = ephoto_demul_color_alpha(bb, a);
                  gg = ephoto_demul_color_alpha(gg, a);
                  rr = ephoto_demul_color_alpha(rr, a);
                  ef->im_data_new[index] = (a << 24) | (rr << 16) |
                    (gg << 8) | bb;
               }
          }
     }
}

static void
_grayscale(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Ephoto_Filter *ef = data;
   int gray, i, r, g, b, a;
   Evas_Coord w, h, x, y;

   w = ef->w;
   h = ef->h;
   for (y = 0; y < h; y++)
     {
        for (x = 0; x < w; x++)
          {
             i = y * w + x;
             b = (int)((ef->im_data[i]) & 0xff);
             g = (int)((ef->im_data[i] >> 8) & 0xff);
             r = (int)((ef->im_data[i] >> 16) & 0xff);
             a = (int)((ef->im_data[i] >> 24) & 0xff);
             b = ephoto_mul_color_alpha(b, a);
             g = ephoto_mul_color_alpha(g, a);
             r = ephoto_mul_color_alpha(r, a);
             gray = (int)((0.3 * r) + (0.59 * g) + (0.11 * b));
             if (a >= 0 && a < 255)
               gray = (gray * a) / 255;
             ef->im_data_new[i] = (a << 24) | (gray << 16) | (gray << 8) | gray;
          }
     }
}

static void
_sepia(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Ephoto_Filter *ef = data;
   int i, r, rr, g, gg, b, bb, a;
   Evas_Coord w, h, x, y;

   w = ef->w;
   h = ef->h;
   for (y = 0; y < h; y++)
     {
        for (x = 0; x < w; x++)
          {
             i = y * w + x;

             b = ((ef->im_data[i]) & 0xff);
             g = ((ef->im_data[i] >> 8) & 0xff);
             r = ((ef->im_data[i] >> 16) & 0xff);
             a = ((ef->im_data[i] >> 24) & 0xff);
             b = ephoto_mul_color_alpha(b, a);
             g = ephoto_mul_color_alpha(g, a);
             r = ephoto_mul_color_alpha(r, a);
             rr = (int)((r * .393) + (g * .769) + (b * .189));
             rr = ephoto_normalize_color(rr);
             gg = ((r * .349) + (g * .686) + (b * .168));
             gg = ephoto_normalize_color(gg);
             bb = (int)((r * .272) + (g * .534) + (b * .131));
             bb = ephoto_normalize_color(bb);
             bb = ephoto_demul_color_alpha(bb, a);
             gg = ephoto_demul_color_alpha(gg, a);
             rr = ephoto_demul_color_alpha(rr, a);
             ef->im_data_new[i] = (a << 24) | (rr << 16) | (gg << 8) | bb;
          }
     }
}

static void
_posterize(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Ephoto_Filter *ef = data;
   int i, rr, gg, bb, a;
   double fr, fg, fb, rad;
   Evas_Coord w, h, x, y;

   w = ef->w;
   h = ef->h;
   rad = ef->drad;
   for (y = 0; y < h; y++)
     {
        for (x = 0; x < w; x++)
          {
             i = y * w + x;
             fb = ((ef->im_data[i]) & 0xff);
             fg = ((ef->im_data[i] >> 8) & 0xff);
             fr = ((ef->im_data[i] >> 16) & 0xff);
             a = ((ef->im_data[i] >> 24) & 0xff);
             fb = ephoto_mul_color_alpha(fb, a);
             fg = ephoto_mul_color_alpha(fg, a);
             fr = ephoto_mul_color_alpha(fr, a);
             fr /= 255;
             fg /= 255;
             fb /= 255;
             rr = 255 * rint((fr * rad)) / rad;
             rr = ephoto_normalize_color(rr);
             gg = 255 * rint((fg * rad)) / rad;
             gg = ephoto_normalize_color(gg);
             bb = 255 * rint((fb * rad)) / rad;
             bb = ephoto_normalize_color(bb);
             bb = ephoto_demul_color_alpha(bb, a);
             gg = ephoto_demul_color_alpha(gg, a);
             rr = ephoto_demul_color_alpha(rr, a);
             ef->im_data_new[i] = (a << 24) | (rr << 16) | (gg << 8) | bb;
          }
     }
}

static void
_negative(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Ephoto_Filter *ef = data;
   int i, r, g, b, rr, gg, bb, a;
   Evas_Coord w, h, x, y;

   w = ef->w;
   h = ef->h;
   for (y = 0; y < h; y++)
     {
        for (x = 0; x < w; x++)
          {
             i = y * w + x;
             b = ((ef->im_data[i]) & 0xff);
             g = ((ef->im_data[i] >> 8) & 0xff);
             r = ((ef->im_data[i] >> 16) & 0xff);
             a = ((ef->im_data[i] >> 24) & 0xff);
             b = ephoto_mul_color_alpha(b, a);
             g = ephoto_mul_color_alpha(g, a);
             r = ephoto_mul_color_alpha(r, a);
             rr = 255 - r;
             gg = 255 - g;
             bb = 255 - b;
             rr = ephoto_normalize_color(rr);
             gg = ephoto_normalize_color(gg);
             bb = ephoto_normalize_color(bb);
             bb = ephoto_demul_color_alpha(bb, a);
             gg = ephoto_demul_color_alpha(gg, a);
             rr = ephoto_demul_color_alpha(rr, a);
             ef->im_data_new[i] = (a << 24) | (rr << 16) | (gg << 8) | bb;
          }
     }
   if (ef->filter == EPHOTO_IMAGE_FILTER_SKETCH)
     ef->save_data = EINA_TRUE;
}

static void
_dodge(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Ephoto_Filter *ef = data;
   double a, r, g, b, aa, rr, gg, bb;
   int i, aaa, rrr, ggg, bbb;
   Evas_Coord w, h, x, y;

   w = ef->w;
   h = ef->h;
   for (y = 0; y < h; y++)
     {
        for (x = 0; x < w; x++)
          {
             i = y * w + x;
             b = ((ef->im_data_two[i]) & 0xff);
             g = ((ef->im_data_two[i] >> 8) & 0xff);
             r = ((ef->im_data_two[i] >> 16) & 0xff);
             a = ((ef->im_data_two[i] >> 24) & 0xff);

             bb = ((ef->im_data[i]) & 0xff);
             gg = ((ef->im_data[i] >> 8) & 0xff);
             rr = ((ef->im_data[i] >> 16) & 0xff);
             aa = ((ef->im_data[i] >> 24) & 0xff);

             b *= 255;
             g *= 255;
             r *= 255;
             a *= 255;

             bbb = rint(b / (255 - bb));
             ggg = rint(g / (255 - gg));
             rrr = rint(r / (255 - rr));
             aaa = rint(a / (255 - aa));

             rrr = ephoto_normalize_color(rrr);
             ggg = ephoto_normalize_color(ggg);
             bbb = ephoto_normalize_color(bbb);
             aaa = ephoto_normalize_color(aaa);

             ef->im_data_new[i] = (aaa << 24) | (rrr << 16) | (ggg << 8) | bbb;
          }
     }
}

static void
_sobel(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Ephoto_Filter *ef = data;
   Evas_Coord x, y, w, h;
   int i, j;
   unsigned int *p;
   float sobx[3][3] = {{-1, 0, 1},
                       {-2, 0, 2},
                       {-1, 0, 1}};
   float soby[3][3] = {{-1, -2, -1},
                       {0, 0, 0},
                       {1, 2, 1}};

   w = ef->w;
   h = ef->h;
   for (y = 0; y < h; y++)
     {
        p = ef->im_data_new + (y * w);
        for (x = 0; x < w; x++)
          {
             int pval = 0, a, r, g, b;
             double hpval = 0.0, vpval = 0.0;
             if (y > 0 && x > 0 && y < (h - 2) && x < (w - 2))
               {
                  for (i = -1; i <= 1; i++)
                    {
                       for (j = -1; j <= 1; j++)
                         {
                            int index, pix;

                            index = (y + i) * w + x + j;
                            pix = ef->im_data[index];
                            hpval += pix * sobx[i + 1][j + 1];
                            vpval += pix * soby[i + 1][j + 1];
                         }
                    }
               }
             pval = abs(hpval) + abs(vpval);
             *p = pval;
             b = ((*p) & 0xff);
             g = ((*p >> 8) & 0xff);
             r = ((*p >> 16) & 0xff);
             a = ((*p >> 24) & 0xff);
             b = ephoto_normalize_color(b);
             g = ephoto_normalize_color(g);
             r = ephoto_normalize_color(r);
             a = ephoto_normalize_color(a);
             *p = (a << 24) | (r << 16) | (g << 8) | b;
             p++;
          }
     }
}

static void
_emboss(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Ephoto_Filter *ef = data;
   Evas_Coord x, y, w, h;
   int i, j;
   unsigned int *p;
   float emboss[3][3] = {{-2, -1, 0},
                         {-1, 1, 1},
                         {0, 1, 2}};

   w = ef->w;
   h = ef->h;
   for (y = 0; y < h; y++)
     {
        p = ef->im_data_new + (y * w);
        for (x = 0; x < w; x++)
          {
             int aa = 0, rr = 0, gg = 0, bb = 0;
             if (y > 0 && x > 0 && y < (h - 2) && x < (w - 2))
               {
                  for (i = -1; i <= 1; i++)
                    {
                       for (j = -1; j <= 1; j++)
                         {
                            int index, pix;
                            index = (y + i) * w + x + j;
                            pix = ef->im_data[index];
                            bb += (int)((pix) & 0xff) *
                              emboss[i + 1][j + 1];
                            gg += (int)((pix >> 8) & 0xff) *
                              emboss[i + 1][j + 1];
                            rr += (int)((pix >> 16) & 0xff) *
                              emboss[i + 1][j + 1];
                            aa += (int)((pix >> 24) & 0xff) *
                              emboss[i + 1][j + 1];
                         }
                    }
               }
             aa = ephoto_normalize_color(aa);
             bb = ephoto_normalize_color(bb);
             gg = ephoto_normalize_color(gg);
             rr = ephoto_normalize_color(rr);
             *p = (aa << 24) | (rr << 16) | (gg << 8) | bb;
             p++;
          }
     }
}

static void
_histogram_eq(void *data, Ecore_Thread *th EINA_UNUSED)
{
   Ephoto_Filter *ef = data;
   Evas_Coord x, y, w, h;
   unsigned int *p1, *p2;
   int i, a, r, g, b, bb, gg, rr, norm;
   int total;
   double sum;
   float hh, s, v, nv;

   w = ef->w;
   h = ef->h;
   total = ef->w * ef->h;
   for (y = 0; y < h; y++)
     {
        p1 = ef->im_data + (y * w);
        for (x = 0; x < w; x++)
          {
             b = ((*p1) & 0xff);
             g = ((*p1 >> 8) & 0xff);
             r = ((*p1 >> 16) & 0xff);
             a = ((*p1 >> 24) & 0xff);
             b = ephoto_mul_color_alpha(b, a);
             g = ephoto_mul_color_alpha(g, a);
             r = ephoto_mul_color_alpha(r, a);
             evas_color_rgb_to_hsv(r, g, b, &hh, &s, &v);
             norm = (int)round((double)v * (double)255);
             ef->hist[norm] += 1;
             p1++;
          }
     }
   sum = 0;
   for (i = 0; i < 256; i++)
     {
        sum += ((double)ef->hist[i] /
                (double)total);
        ef->cdf[i] = (int)round(sum * 255);
     }
   for (y = 0; y < h; y++)
     {
        p1 = ef->im_data + (y * w);
        p2 = ef->im_data_new + (y * w);
        for (x = 0; x < w; x++)
          {
             b = ((*p1) & 0xff);
             g = ((*p1 >> 8) & 0xff);
             r = ((*p1 >> 16) & 0xff);
             a = ((*p1 >> 24) & 0xff);
             b = ephoto_mul_color_alpha(b, a);
             g = ephoto_mul_color_alpha(g, a);
             r = ephoto_mul_color_alpha(r, a);
             evas_color_rgb_to_hsv(r, g, b, &hh, &s, &v);
             norm = (int)round((double)v * (double)255);
             nv = (float)ef->cdf[norm] / (float)255;
             evas_color_hsv_to_rgb(hh, s, nv, &rr, &gg, &bb);
             bb = ephoto_normalize_color(bb);
             gg = ephoto_normalize_color(gg);
             rr = ephoto_normalize_color(rr);
             bb = ephoto_demul_color_alpha(bb, a);
             gg = ephoto_demul_color_alpha(gg, a);
             rr = ephoto_demul_color_alpha(rr, a);
             *p2 = (a << 24) | (rr << 16) | (gg << 8) | bb;
             p2++;
             p1++;
          }
     }
}

void
ephoto_filter_blur(Ephoto *ephoto, Evas_Object *image)
{
   Ephoto_Filter *ef = _initialize_filter(EPHOTO_IMAGE_FILTER_BLUR,
                                          ephoto, image);

   ef->rad = 9;
   ef->popup = _processing(ephoto->win);
   ef->thread = ecore_thread_run(_blur, _thread_finished_cb,
                                 NULL, ef);
}

void
ephoto_filter_sharpen(Ephoto *ephoto, Evas_Object *image)
{
   Ephoto_Filter *ef = _initialize_filter(EPHOTO_IMAGE_FILTER_SHARPEN,
                                          ephoto, image);
   ef->im_data_orig = malloc(sizeof(unsigned int) * ef->w * ef->h);
   memcpy(ef->im_data_orig, ef->im_data,
          sizeof(unsigned int) * ef->w * ef->h);

   ef->rad = 9;
   ef->qcount = 1;
   ef->qpos = 0;
   ef->queue = eina_list_append(ef->queue, _sharpen);
   ef->popup = _processing(ephoto->win);
   ef->thread = ecore_thread_run(_blur, _thread_finished_cb,
                                 NULL, ef);
}

void
ephoto_filter_dither(Ephoto *ephoto, Evas_Object *image)
{
   Ephoto_Filter *ef = _initialize_filter(EPHOTO_IMAGE_FILTER_DITHER,
                                          ephoto, image);
   memcpy(ef->im_data_new, ef->im_data,
          sizeof(unsigned int) * ef->w * ef->h);

   ef->popup = _processing(ephoto->win);
   ef->thread = ecore_thread_run(_dither, _thread_finished_cb,
                                 NULL, ef);
}

void
ephoto_filter_black_and_white(Ephoto *ephoto, Evas_Object *image)
{
   Ephoto_Filter *ef = _initialize_filter(EPHOTO_IMAGE_FILTER_GRAYSCALE,
                                          ephoto, image);

   ef->popup = _processing(ephoto->win);
   ef->thread = ecore_thread_run(_grayscale, _thread_finished_cb,
                                 NULL, ef);
}

void
ephoto_filter_old_photo(Ephoto *ephoto, Evas_Object *image)
{
   Ephoto_Filter *ef = _initialize_filter(EPHOTO_IMAGE_FILTER_SEPIA,
                                          ephoto, image);

   ef->popup = _processing(ephoto->win);
   ef->thread = ecore_thread_run(_sepia, _thread_finished_cb,
                                 NULL, ef);
}

void
ephoto_filter_posterize(Ephoto *ephoto, Evas_Object *image)
{
   Ephoto_Filter *ef = _initialize_filter(EPHOTO_IMAGE_FILTER_POSTERIZE,
                                          ephoto, image);

   ef->drad = 2.0;
   ef->popup = _processing(ephoto->win);
   ef->thread = ecore_thread_run(_posterize, _thread_finished_cb,
                                 NULL, ef);
}

void
ephoto_filter_painting(Ephoto *ephoto, Evas_Object *image)
{
   Ephoto_Filter *ef = _initialize_filter(EPHOTO_IMAGE_FILTER_PAINTING,
                                          ephoto, image);

   ef->rad = 5;
   ef->drad = 5.0;
   ef->qpos = 0;
   ef->qcount = 1;
   ef->queue = eina_list_append(ef->queue, _posterize);
   ef->popup = _processing(ephoto->win);
   ef->thread = ecore_thread_run(_blur, _thread_finished_cb,
                                 NULL, ef);
}

void
ephoto_filter_invert(Ephoto *ephoto, Evas_Object *image)
{
   Ephoto_Filter *ef = _initialize_filter(EPHOTO_IMAGE_FILTER_INVERT,
                                          ephoto, image);

   ef->popup = _processing(ephoto->win);
   ef->thread = ecore_thread_run(_negative, _thread_finished_cb,
                                 NULL, ef);
}

void
ephoto_filter_sketch(Ephoto *ephoto, Evas_Object *image)
{
   Ephoto_Filter *ef = _initialize_filter(EPHOTO_IMAGE_FILTER_SKETCH,
                                          ephoto, image);

   ef->rad = 4;
   ef->qpos = 0;
   ef->qcount = 3;
   ef->queue = eina_list_append(ef->queue, _negative);
   ef->queue = eina_list_append(ef->queue, _blur);
   ef->queue = eina_list_append(ef->queue, _dodge);
   ef->popup = _processing(ephoto->win);
   ef->thread = ecore_thread_run(_grayscale, _thread_finished_cb,
                                 NULL, ef);
}

void
ephoto_filter_edge(Ephoto *ephoto, Evas_Object *image)
{
   Ephoto_Filter *ef = _initialize_filter(EPHOTO_IMAGE_FILTER_SOBEL,
                                          ephoto, image);

   ef->rad = 3;
   ef->qpos = 0;
   ef->qcount = 2;
   ef->queue = eina_list_append(ef->queue, _grayscale);
   ef->queue = eina_list_append(ef->queue, _sobel);
   ef->popup = _processing(ephoto->win);
   ef->thread = ecore_thread_run(_blur, _thread_finished_cb,
                                 NULL, ef);
}

void
ephoto_filter_emboss(Ephoto *ephoto, Evas_Object *image)
{
   Ephoto_Filter *ef = _initialize_filter(EPHOTO_IMAGE_FILTER_EMBOSS,
                                          ephoto, image);

   ef->popup = _processing(ephoto->win);
   ef->thread = ecore_thread_run(_emboss, _thread_finished_cb,
                                 NULL, ef);
}

void
ephoto_filter_histogram_eq(Ephoto *ephoto, Evas_Object *image)
{
   Ephoto_Filter *ef = _initialize_filter(EPHOTO_IMAGE_FILTER_EQUALIZE,
                                          ephoto, image);

   ef->hist = malloc(sizeof(unsigned int) * 256);
   ef->cdf = malloc(sizeof(unsigned int) * 256);
   _create_hist(ef);
   ef->popup = _processing(ephoto->win);
   ef->thread = ecore_thread_run(_histogram_eq, _thread_finished_cb,
                                 NULL, ef);
}

