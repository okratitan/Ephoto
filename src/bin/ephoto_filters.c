#include "ephoto.h"

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
     return (color * (255 / alpha));
   else
     return color;
}

static int
_demul_color_alpha(int color, int alpha)
{
   if (alpha > 0 && alpha < 255)
     return ((color * alpha) / 255);
   else
     return color;
}   

void
ephoto_filter_blur(Evas_Object *main, Evas_Object *image)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   int rad = 3;
   int x, y, w, h, mx, my, mw, mh, mt, xx, yy;
   int a, r, g, b;
   int *as, *rs, *gs, *bs;

   im_data = evas_object_image_data_get(elm_image_object_get(image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(image), &w, &h);

   im_data_new = malloc(sizeof(unsigned int) * w * h);
   as = malloc(sizeof(int) * w);
   rs = malloc(sizeof(int) * w);
   gs = malloc(sizeof(int) * w);
   bs = malloc(sizeof(int) * w);

   for (y = 0; y < h; y++)
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
        p1 = im_data_new + (y * w);
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
                  p1 ++;
               }
          }
     }
   free(as);
   free(rs);
   free(gs);
   free(bs);
   
   ephoto_single_browser_image_data_update(main, image, EINA_TRUE, im_data_new, w, h);
}

void
ephoto_filter_sharpen(Evas_Object *main, Evas_Object *image) 
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   int a, r, g, b, x, y, w, h;
   int mul, mul2, tot;
   int rad = 3;

   im_data = evas_object_image_data_get(elm_image_object_get(image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(image), &w, &h);

   im_data_new = malloc(sizeof(unsigned int) * w * h);

   mul = (rad * 4) + 1;
   mul2 = rad;
   tot = mul - (mul2 * 4);
   for (y = 1; y < (h - 1); y ++) 
     {
        p1 = im_data + 1 + (y * w);
        p2 = im_data_new + 1 + (y * w);
        for (x = 1; x < (w - 1); x++) 
          {
             b = (int)((p1[0]) & 0xff) * 5;
             g = (int)((p1[0] >> 8) & 0xff) * 5;
             r = (int)((p1[0] >> 16) & 0xff) * 5;
             a = (int)((p1[0] >> 24) & 0xff) * 5;
             b -= (int)((p1[-1]) & 0xff);
             g -= (int)((p1[-1] >> 8) & 0xff);
             r -= (int)((p1[-1] >> 16) & 0xff);
             a -= (int)((p1[-1] >> 24) & 0xff);
             b -= (int)((p1[1]) & 0xff);
             g -= (int)((p1[1] >> 8) & 0xff);
             r -= (int)((p1[1] >> 16) & 0xff);
             a -= (int)((p1[1] >> 24) & 0xff);
             b -= (int)((p1[-w]) & 0xff);
             g -= (int)((p1[-w] >> 8) & 0xff);
             r -= (int)((p1[-w] >> 16) & 0xff);
             a -= (int)((p1[-w] >> 24) & 0xff);
             b -= (int)((p1[-w]) & 0xff);
             g -= (int)((p1[-w] >> 8) & 0xff);
             r -= (int)((p1[-w] >> 16) & 0xff);
             a -= (int)((p1[-w] >> 24) & 0xff);
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
     }
   ephoto_single_browser_image_data_update(main, image, EINA_TRUE, im_data_new, w, h);
}

void
ephoto_filter_black_and_white(Evas_Object *main, Evas_Object *image)
{
   unsigned int *im_data, *im_data_new;
   int gray, i, r, g, b, a, w, h;

   im_data = evas_object_image_data_get(elm_image_object_get(image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(image), &w, &h);

   im_data_new = malloc(sizeof(unsigned int) * w * h);

   for (i = 0; i < (w * h); i++)
     {
        b = (int)((im_data[i]) & 0xff);
        g = (int)((im_data[i] >> 8) & 0xff);
        r = (int)((im_data[i] >> 16) & 0xff);
        a = (int)((im_data[i] >> 24) & 0xff);
        b = _mul_color_alpha(b, a);
        g = _mul_color_alpha(g, a);
        r = _mul_color_alpha(r, a);
        gray = (int)((0.3 * r) + (0.59 * g) + (0.11 * b));
        if (a >= 0 && a < 255) 
          gray = (gray * a) / 255;
        im_data_new[i] = (a << 24) | (gray << 16) | (gray << 8) | gray;
     }
   ephoto_single_browser_image_data_update(main, image, EINA_TRUE, im_data_new, w, h);
}

void
ephoto_filter_old_photo(Evas_Object *main, Evas_Object *image)
{
   unsigned int *im_data, *im_data_new;
   int i, r, rr, g, gg, b, bb, a, w, h;

   im_data = evas_object_image_data_get(elm_image_object_get(image), EINA_FALSE); 
   evas_object_image_size_get(elm_image_object_get(image), &w, &h);
   im_data_new = malloc(sizeof(unsigned int) * w * h);
   for (i = 0; i < (w * h); i++)
     {
        b = (int)((im_data[i]) & 0xff);
        g = (int)((im_data[i] >> 8) & 0xff);
        r = (int)((im_data[i] >> 16) & 0xff);
        a = (int)((im_data[i] >> 24) & 0xff);
        b = _mul_color_alpha(b, a);
        g = _mul_color_alpha(g, a);
        r = _mul_color_alpha(r, a);
        rr = (int)((r* .393) + (g*.769) + (b*.189));
        rr = _normalize_color(rr);
        gg = (int)((r* .349) + (g*.686) + (b*.168));
        gg = _normalize_color(gg);
        bb = (int)((r* .272) + (g*.534) + (b*.131));              
        bb = _normalize_color(bb);
        bb = _demul_color_alpha(bb, a);
        gg = _demul_color_alpha(gg, a);
        rr = _demul_color_alpha(rr, a);
        im_data_new[i] = (a << 24) | (rr << 16) | (gg << 8) | bb;
     }
   ephoto_single_browser_image_data_update(main, image, EINA_TRUE, im_data_new, w, h);
}

void
ephoto_filter_histogram_eq(Evas_Object *main, Evas_Object *image)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   int x, y, w, h, i, hist[256], cdf[256];
   int a, r, g, b, bb, gg, rr, norm, total;
   float hh, s, v, nv, sum;

   im_data = evas_object_image_data_get(elm_image_object_get(image), EINA_FALSE);
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
             b = (int)((*p1) & 0xff);
             g = (int)((*p1 >> 8) & 0xff);
             r = (int)((*p1 >> 16) & 0xff);
             a = (int)((*p1 >> 24) & 0xff);
             b = _mul_color_alpha(b, a);
             g = _mul_color_alpha(g, a);
             r = _mul_color_alpha(r, a);
             evas_color_rgb_to_hsv(r, g, b, &hh, &s, &v);
             norm = (int)round((double)v * (double)255);
             hist[norm] += 1;
             p1++;
          }
     }
   sum = 0;
   for (i = 0; i < 256; i++)
     {
        sum += ((double)hist[i] / (double)total);
        cdf[i] = (int)round(sum * 255);
     }
   for (y = 0; y < h; y++)
     {
        p1 = im_data + (y * w);
        p2 = im_data_new + (y * w);
        for (x = 0; x < w; x++)
          {
             b = (int)((*p1) & 0xff);
             g = (int)((*p1 >> 8) & 0xff);
             r = (int)((*p1 >> 16) & 0xff);
             a = (int)((*p1 >> 24) & 0xff);
             b = _mul_color_alpha(b, a);
             g = _mul_color_alpha(g, a);
             r = _mul_color_alpha(r, a);
             evas_color_rgb_to_hsv(r, g, b, &hh, &s, &v);
             norm = (int)round((double)v * (double)255);
             nv = (float)cdf[norm] / (float)255;
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
   ephoto_single_browser_image_data_update(main, image, EINA_TRUE, im_data_new, w, h);
}
