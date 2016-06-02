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

static unsigned int *
_blur(unsigned int *im_data, int rad, Evas_Coord w, Evas_Coord h)
{
   unsigned int *im_data_new, *p1, *p2;
   Evas_Coord x, y, mx, my, mw, mh, mt, xx, yy;
   int a, r, g, b;
   int *as, *rs, *gs, *bs;

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
		  p1++;
	       }
	  }
     }
   free(as);
   free(rs);
   free(gs);
   free(bs);

   return im_data_new;
}

static unsigned int *
_grayscale(Evas_Object *image) {
   unsigned int *im_data, *im_data_new;
   int gray, i, r, g, b, a;
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
        gray = (int) ((0.3 * r) + (0.59 * g) + (0.11 * b));
        if (a >= 0 && a < 255)
           gray = (gray * a) / 255;
        im_data_new[i] = (a << 24) | (gray << 16) | (gray << 8) | gray;
     }
   return im_data_new;
}

static unsigned int *
_posterize(unsigned int *im_data, double rad, Evas_Coord w, Evas_Coord h)
{
   unsigned int *im_data_new;
   int i, rr, gg, bb, a;
   double fr, fg, fb;

   im_data_new = malloc(sizeof(unsigned int) * w * h);
   for (i = 0; i < (w * h); i++)
     {
        fb = ((im_data[i]) & 0xff);
        fg = ((im_data[i] >> 8) & 0xff);
        fr = ((im_data[i] >> 16) & 0xff);
        a = (int) ((im_data[i] >> 24) & 0xff);
        fr /= 255;
        fg /= 255;
        fb /= 255;
        rr = 255 * rint((fr * rad)) / rad;
        rr = _normalize_color(rr);
        gg = 255 * rint((fg * rad)) / rad;
        gg = _normalize_color(gg);
        bb = 255 * rint((fb * rad)) / rad;
        bb = _normalize_color(bb);
        im_data_new[i] = (a << 24) | (rr << 16) | (gg << 8) | bb;
     }
   return im_data_new;
}

static unsigned int *
_negative(unsigned int *im_data, Evas_Coord w, Evas_Coord h)
{
   unsigned int *im_data_new;
   int i, r, g, b, rr, gg, bb, a;

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
        rr = 255 - r;
        gg = 255 - g;
        bb = 255 - b;
        rr = _normalize_color(rr);
        gg = _normalize_color(gg);
        bb = _normalize_color(bb);
        bb = _demul_color_alpha(bb, a);
        gg = _demul_color_alpha(gg, a);
        rr = _demul_color_alpha(rr, a);
        im_data_new[i] = (a << 24) | (rr << 16) | (gg << 8) | bb;
     }
   return im_data_new;
}

static unsigned int *
_dodge(unsigned int *im_data, unsigned int *im_data_two, Evas_Coord w, Evas_Coord h)
{
   unsigned int *im_data_new;
   double r, g, b, rr, gg, bb;
   int i, rrr, ggg, bbb;
   im_data_new = malloc(sizeof(unsigned int) * w * h);
   for (i = 0; i < (w * h); i++)
     {
        b = ((im_data[i]) & 0xff);
        g = ((im_data[i] >> 8) & 0xff);
        r = ((im_data[i] >> 16) & 0xff);

        bb = ((im_data_two[i]) & 0xff);
        gg = ((im_data_two[i] >> 8) & 0xff);
        rr = ((im_data_two[i] >> 16) & 0xff);
  
        b *= 255;
        g *= 255;
        r *= 255;
  
        bbb = rint(b / (255 - bb));
        ggg = rint(g / (255 - gg));
        rrr = rint(r / (255 - rr));

        rrr = _normalize_color(rrr);
        ggg = _normalize_color(ggg);
        bbb = _normalize_color(bbb);

        im_data_new[i] = (255 << 24) | (rrr << 16) | (ggg << 8) | bbb;
     }
   return im_data_new;
}

void
ephoto_filter_blur(Evas_Object *main, Evas_Object *image)
{
   Evas_Coord w, h;
   unsigned int *im_data;

   im_data =
       evas_object_image_data_get(elm_image_object_get(image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(image), &w, &h);
   ephoto_single_browser_image_data_done(main, _blur(im_data, 9, w, h),
       w, h);
}

void
ephoto_filter_sharpen(Evas_Object *main, Evas_Object *image)
{
   unsigned int *im_data, *im_data_new, *p1, *p2;
   int a, r, g, b;
   Evas_Coord x, y, w, h;

   im_data =
       evas_object_image_data_get(elm_image_object_get(image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(image), &w, &h);

   im_data_new = malloc(sizeof(unsigned int) * w * h);

   for (y = 1; y < (h - 1); y++)
     {
	p1 = im_data + 1 + (y * w);
	p2 = im_data_new + 1 + (y * w);
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
     }
   ephoto_single_browser_image_data_done(main, im_data_new,
       w, h);
}

void
ephoto_filter_black_and_white(Evas_Object *main, Evas_Object *image)
{
   Evas_Coord w, h;

   evas_object_image_size_get(elm_image_object_get(image), &w, &h);

   ephoto_single_browser_image_data_done(main, _grayscale(image),
       w, h);
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
   unsigned int *im_data, *im_data_new;
   Evas_Coord w, h;

   im_data =
       evas_object_image_data_get(elm_image_object_get(image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(image), &w, &h);
   im_data_new = _posterize(im_data, 2.0, w, h);

   ephoto_single_browser_image_data_done(main, im_data_new,
       w, h);
}

void
ephoto_filter_cartoon(Evas_Object *main, Evas_Object *image)
{
   unsigned int *im_data, *im_data_new, *im_data_new_two;
   Evas_Coord w, h;

   im_data =
       evas_object_image_data_get(elm_image_object_get(image), EINA_FALSE);
   evas_object_image_size_get(elm_image_object_get(image), &w, &h);
   im_data_new = _blur(im_data, 5, w, h);
   im_data_new_two = _posterize(im_data_new, 5.0, w, h);

   ephoto_single_browser_image_data_done(main, im_data_new_two,
       w, h);
}

void ephoto_filter_invert(Evas_Object *main, Evas_Object *image)
{
   unsigned int *im_data, *im_data_new;
   Evas_Coord w, h;

   evas_object_image_size_get(elm_image_object_get(image), &w, &h);
   im_data =
       evas_object_image_data_get(elm_image_object_get(image), EINA_FALSE);
   im_data_new = _negative(im_data, w, h);

   ephoto_single_browser_image_data_done(main, im_data_new,
       w, h);
}

void ephoto_filter_sketch(Evas_Object *main, Evas_Object *image)
{
   unsigned int *im_data, *im_data_new, *im_data_new_two, *im_data_new_three;
   Evas_Coord w, h;

   evas_object_image_size_get(elm_image_object_get(image), &w, &h);
   im_data = _grayscale(image);
   im_data_new = _negative(im_data, w, h);
   im_data_new_two = _blur(im_data_new, 4, w, h);
   im_data_new_three = _dodge(im_data, im_data_new_two, w, h);

   ephoto_single_browser_image_data_done(main, im_data_new_three,
       w, h);
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
