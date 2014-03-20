/* Copyright (c) 2012, Anton S. Lobashev
 * mosaic_window_box.c - window box widget.
 */

#include "mosaic_window_box.h"

enum {
  PROP_0,
  PROP_IS_WINDOW,
  PROP_XWINDOW,
  PROP_NAME,
  PROP_OPT_NAME,
  N_PROPERTIES
};

static GObject*	mosaic_window_box_constructor (GType gtype,
					       guint n_properties,
					       GObjectConstructParam *properties);
static void mosaic_window_box_dispose (GObject *gobject);
static void mosaic_window_box_set_property (GObject *gobject,
					    guint prop_id,
					    const GValue *value,
					    GParamSpec *pspec);
static void mosaic_window_box_get_property (GObject *gobject,
					    guint prop_id,
					    GValue *value,
					    GParamSpec *pspec);

static gboolean mosaic_window_box_expose_event (GtkWidget *widget, GdkEventExpose *event);
static void mosaic_window_box_paint (MosaicWindowBox *box, cairo_t *cr, gint width, gint height);
static void mosaic_window_box_create_colors (MosaicWindowBox *box);
static void mosaic_window_box_setup_icon (MosaicWindowBox *box, GdkPixbuf *pixbuf);

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL };

G_DEFINE_TYPE (MosaicWindowBox, mosaic_window_box, MOSAIC_TYPE_BOX);

static void
mosaic_window_box_class_init (MosaicWindowBoxClass *klass)
{
  GObjectClass *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  widget_class = GTK_WIDGET_CLASS (klass);

  gobject_class->constructor = mosaic_window_box_constructor;
  gobject_class->dispose = mosaic_window_box_dispose;
  gobject_class->set_property = mosaic_window_box_set_property;
  gobject_class->get_property = mosaic_window_box_get_property;

  widget_class->expose_event = mosaic_window_box_expose_event;

  obj_properties[PROP_IS_WINDOW] =
    g_param_spec_boolean ("is-window",
			  "Is window",
			  "If set, the box stores XWindow information",
			  FALSE,
			  G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

#ifdef X11
  obj_properties[PROP_XWINDOW] =
    g_param_spec_uint ("xwindow",
		       "XWindow",
		       "XWindow ID",
		       0, G_MAXUINT,
		       0,
		       G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
#endif

#ifdef WIN32
  obj_properties[PROP_XWINDOW] =
    g_param_spec_pointer ("xwindow",
		       "Window handler",
		       "Actually HWND pointer.",
		       G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
#endif

  obj_properties[PROP_NAME] =
    g_param_spec_string ("name",
			 "Name in the box",
			 "Name displayed in the box",
			 NULL,
			 G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

  obj_properties[PROP_OPT_NAME] =
    g_param_spec_string ("opt_name",
			 "WM_CLASS",
			 "Class of the XWindow",
			 NULL,
			 G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

  g_object_class_install_property (gobject_class, PROP_IS_WINDOW, obj_properties [PROP_IS_WINDOW]);
  g_object_class_install_property (gobject_class, PROP_XWINDOW, obj_properties [PROP_XWINDOW]);
  g_object_class_install_property (gobject_class, PROP_NAME, obj_properties [PROP_NAME]);
  g_object_class_install_property (gobject_class, PROP_OPT_NAME, obj_properties [PROP_OPT_NAME]);
}

static void
mosaic_window_box_init (MosaicWindowBox *box)
{
  gtk_widget_set_can_focus (GTK_WIDGET (box), TRUE);
  gtk_widget_set_receives_default (GTK_WIDGET (box), TRUE);

  box->opt_name = NULL;
  box->icon_pixbuf = NULL;
  box->icon_surface = NULL;
  box->icon_context = NULL;
  box->desktop = -1;
}

static GObject*	mosaic_window_box_constructor (GType gtype,
					guint n_properties,
					GObjectConstructParam *properties)
{
  GObject *obj;
  MosaicWindowBox *box;

  obj = G_OBJECT_CLASS (mosaic_window_box_parent_class)->constructor (gtype, n_properties, properties);

  box = MOSAIC_WINDOW_BOX (obj);

  if (box->is_window) {
    MOSAIC_BOX (box)->name = get_window_name (box->xwindow);
    box->opt_name = get_window_class (box->xwindow);
#ifdef X11
    box->desktop = get_window_desktop (box->xwindow);
#endif
  }
  box->show_desktop = FALSE;
  box->show_titles = TRUE;
  box->has_icon = FALSE;
  box->colorize = TRUE;
  box->color_offset = 0;
  box->icon_size = 0;
  box->icon_position = LEFT;
  mosaic_window_box_create_colors (box);

  return obj;
}

GtkWidget* mosaic_window_box_new (void)
{
  return g_object_new (MOSAIC_TYPE_WINDOW_BOX, "is-window", FALSE, NULL);
}

GtkWidget* mosaic_window_box_new_with_xwindow (Position icon_position, int icon_size, Window win)
{
  gpointer result = g_object_new (MOSAIC_TYPE_WINDOW_BOX, "is-window", TRUE, "xwindow", win, NULL);
  MosaicWindowBox *box = ((MosaicWindowBox *) result);
  box->icon_position = icon_position;
  box->icon_size = icon_size;
  return result;
}

GtkWidget* mosaic_window_box_new_with_name (Position icon_position, int icon_size, gchar *name)
{
  gpointer result = g_object_new (MOSAIC_TYPE_WINDOW_BOX, "is-window", FALSE, "name", name, NULL);
  MosaicWindowBox *box = ((MosaicWindowBox *) result);
  box->icon_position = icon_position;
  box->icon_size = icon_size;
  return result;
}

static void
mosaic_window_box_dispose (GObject *gobject)
{
  MosaicWindowBox *box = MOSAIC_WINDOW_BOX (gobject);

  if (box->opt_name)
    g_free (box->opt_name);
  box->opt_name = NULL;

  if (box->icon_context) {
    cairo_destroy (box->icon_context);
    cairo_surface_destroy (box->icon_surface);
    box->icon_context = NULL;
    box->icon_surface = NULL;
  }
  if (box->icon_pixbuf)
    g_object_unref (box->icon_pixbuf);
  box->icon_pixbuf = NULL;

  G_OBJECT_CLASS (mosaic_window_box_parent_class)->dispose (gobject);
}

static void mosaic_window_box_set_property (GObject *gobject,
					    guint prop_id,
					    const GValue *value,
					    GParamSpec *pspec)
{
  MosaicWindowBox *box = MOSAIC_WINDOW_BOX (gobject);

  switch (prop_id) {
  case PROP_IS_WINDOW:
    mosaic_window_box_set_is_window (box, g_value_get_boolean (value));
    break;
  case PROP_XWINDOW:
#ifdef X11
    mosaic_window_box_set_xwindow (box, g_value_get_uint (value));
#endif
#ifdef WIN32
    mosaic_window_box_set_xwindow (box, g_value_get_pointer(value));
#endif
    break;
  case PROP_NAME:
    mosaic_window_box_set_name (box, g_value_get_string (value));
    break;
  case PROP_OPT_NAME:
    mosaic_window_box_set_opt_name (box, g_value_get_string (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    break;
  }
}

static void mosaic_window_box_get_property (GObject *gobject,
					    guint prop_id,
					    GValue *value,
					    GParamSpec *pspec)
{
  MosaicWindowBox *box = MOSAIC_WINDOW_BOX (gobject);

  switch (prop_id) {
  case PROP_IS_WINDOW:
    g_value_set_boolean (value, box->is_window);
    break;
  case PROP_XWINDOW:
#ifdef X11
    g_value_set_uint (value, box->xwindow);
#endif
#ifdef WIN32
    g_value_set_pointer (value, box->xwindow);
#endif
    break;
  case PROP_NAME:
    g_value_set_string (value, MOSAIC_BOX(box)->name);
    break;
  case PROP_OPT_NAME:
    g_value_set_string (value, box->opt_name);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
    break;
  }
}

static gboolean
mosaic_window_box_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
  g_return_val_if_fail (MOSAIC_IS_WINDOW_BOX (widget), FALSE);

  cairo_t *cr;
  cr = gdk_cairo_create (widget->window);
  cairo_rectangle (cr,
		   event->area.x, event->area.y,
		   event->area.width, event->area.height);
  cairo_clip (cr);
  mosaic_window_box_paint (MOSAIC_WINDOW_BOX (widget), cr, widget->allocation.width, widget->allocation.height);
  cairo_destroy (cr);
  return TRUE;
}

static void
mosaic_window_box_paint (MosaicWindowBox *box, cairo_t *cr, gint width, gint height)
{
  gboolean has_focus = gtk_widget_has_focus (GTK_WIDGET (box));
  if (MOSAIC_BOX (box)->on_box)
    cairo_set_source_rgb (cr, box->r-0.2, box->g-0.2, box->b-0.2);
  else if (has_focus)
    cairo_set_source_rgb (cr, box->r-0.4, box->g-0.4, box->b-0.4);
  else
    cairo_set_source_rgb (cr, box->r, box->g, box->b);
  cairo_rectangle (cr, 0, 0, width, height);
  cairo_fill (cr);

  PangoLayout *pl;
  PangoFontDescription *pfd;
  pl = pango_cairo_create_layout (cr);

  /* Shall we draw the desktop number */
  if (box->show_desktop) {
    gchar desk [4] = { 0 };
    if (box->desktop > -1)
      sprintf (desk, "%d", box->desktop+1);
    else
      sprintf (desk, "A");

    pango_layout_set_text (pl, desk, -1);
    pfd = pango_font_description_from_string (MOSAIC_BOX (box)->font);
    pango_font_description_set_weight (pfd, PANGO_WEIGHT_BOLD);

    int font_size;
    switch (box->icon_position) {
    case TOP:
      font_size = (height-10)/2;
      break;
    default:
      font_size = height-10;
      break;
    }
    pango_font_description_set_size (pfd, font_size * PANGO_SCALE);

    pango_layout_set_font_description (pl, pfd);
    pango_font_description_free (pfd);

    if (has_focus)
      cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.5);
    else
      cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.5);

    int pwidth, pheight;
    pango_layout_get_pixel_size (pl, &pwidth, &pheight);

    int x, y;
    switch (box->icon_position) {
    case TOP:
      x = 5;
      y = 5;
      break;
    default:
      x = (width - pwidth)/2;
      y = (height - pheight)/2;
      break;
    }
    cairo_move_to (cr, x, y);

    pango_cairo_show_layout (cr, pl);
  }

  gint text_offset = 0;

  if (box->has_icon) {
    if (box->icon_pixbuf) {
      guint iwidth = gdk_pixbuf_get_width (box->icon_pixbuf);
      guint iheight = gdk_pixbuf_get_height (box->icon_pixbuf);
      cairo_save (cr);

      switch (box->icon_position) {
      case TOP: {
        int y = 5;
        if (iwidth < box->icon_size) {
          y += (box->icon_size - iwidth) / 2;
        }
        cairo_set_source_surface (cr, box->icon_surface, (width-iwidth)/2, y);
        cairo_rectangle (cr, (width-iwidth)/2, 0, iwidth+5, (height+iheight)/2);
      } break;
      default:
        cairo_set_source_surface (cr, box->icon_surface, 5, (height-iheight)/2);
        cairo_rectangle (cr, 0, 0, iwidth+5, (height+iheight)/2);
        break;
      }

      cairo_clip (cr);
      cairo_paint (cr);
      cairo_restore (cr);

      text_offset = iwidth+5;
      switch (box->icon_position) {
      case TOP:
        pango_layout_set_width (pl, (width-15) * PANGO_SCALE);
        break;
      default:
        pango_layout_set_width (pl, (width-iwidth-15) * PANGO_SCALE);
        break;
      }
    }
  } else {
    pango_layout_set_width (pl, (width-15) * PANGO_SCALE);
  }

  // Draw name.
  if (box->show_titles) {
    pango_layout_set_ellipsize (pl, PANGO_ELLIPSIZE_END);
    pango_layout_set_text (pl, MOSAIC_BOX (box)->name, -1);
    pfd = pango_font_description_from_string (MOSAIC_BOX (box)->font);
    pango_layout_set_font_description (pl, pfd);
    pango_font_description_free (pfd);

    int pwidth, pheight;
    pango_layout_get_pixel_size (pl, &pwidth, &pheight);

    if (has_focus)
      cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 1.0);
    else
      cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0);

    switch (box->icon_position) {
    case TOP:
      cairo_move_to (cr, (width - pwidth)/2, height - pheight - 10);
      break;
    default:
      if (text_offset > 0) {
        if ((width-pwidth)/2 > text_offset+5)
          cairo_move_to (cr, (width - pwidth)/2, (height - pheight)/2);
        else
          cairo_move_to (cr, text_offset+5, (height - pheight)/2);
      } else {
        if (width-5 > pwidth)
          cairo_move_to (cr, (width - pwidth)/2, (height - pheight)/2);
        else
          cairo_move_to (cr, 5, (height - pheight)/2);
      }
      break;
    }

    pango_cairo_show_layout (cr, pl);
  }
  g_object_unref (pl);

  mosaic_box_paint (MOSAIC_BOX (box), cr, width, height);
}

void
mosaic_window_box_set_is_window (MosaicWindowBox *box, gboolean is_window)
{
  g_return_if_fail (MOSAIC_IS_WINDOW_BOX (box));

  box->is_window = is_window;

  g_object_notify (G_OBJECT (box), "is-window");
}

gboolean
mosaic_window_box_get_is_window (MosaicWindowBox *box)
{
  g_return_val_if_fail (MOSAIC_IS_WINDOW_BOX (box), FALSE);

  return box->is_window;
}

void mosaic_window_box_set_xwindow (MosaicWindowBox *box, Window window)
{
  g_return_if_fail (MOSAIC_IS_WINDOW_BOX (box));

  if (box->is_window) {
    box->xwindow = window;

    g_object_notify (G_OBJECT (box), "xwindow");
#ifdef X11
    box->desktop = get_window_desktop (box->xwindow);
#endif
    mosaic_window_box_update_xwindow_name (box);
    mosaic_window_box_update_opt_name (box);
  }
}

Window mosaic_window_box_get_xwindow (MosaicWindowBox *box)
{
  g_return_val_if_fail (MOSAIC_IS_WINDOW_BOX (box), 0);

  return box->xwindow;
}

void
mosaic_window_box_set_name (MosaicWindowBox *box, const gchar *name)
{
  mosaic_box_set_name (MOSAIC_BOX (box), name);
}

const gchar *
mosaic_window_box_get_name (MosaicWindowBox *box)
{
  g_return_val_if_fail (MOSAIC_IS_WINDOW_BOX (box), NULL);

  return MOSAIC_BOX (box)->name;
}

void
mosaic_window_box_set_opt_name (MosaicWindowBox *box, const gchar *opt_name)
{
  gchar *new_opt_name;
  g_return_if_fail (MOSAIC_IS_WINDOW_BOX (box));

  new_opt_name = g_strdup (opt_name);
  g_free (box->opt_name);
  box->opt_name = new_opt_name;

  g_object_notify (G_OBJECT (box), "opt_name");
  mosaic_window_box_create_colors (box);
  gtk_widget_queue_draw (GTK_WIDGET (box));
}

const gchar *
mosaic_window_box_get_opt_name (MosaicWindowBox *box)
{
  g_return_val_if_fail (MOSAIC_IS_WINDOW_BOX (box), NULL);

  return box->opt_name;
}

void mosaic_window_box_update_xwindow_name (MosaicWindowBox *box)
{
  g_return_if_fail (MOSAIC_IS_WINDOW_BOX (box));

  if (box->is_window) {
    gchar *wname = get_window_name (box->xwindow);
    mosaic_window_box_set_name (box, wname);
    g_free (wname);
  }

  if (gtk_widget_get_has_tooltip(GTK_WIDGET(box)))
    gtk_widget_set_tooltip_text (GTK_WIDGET(box), MOSAIC_BOX(box)->name);
}

void mosaic_window_box_update_opt_name (MosaicWindowBox *box)
{
  g_return_if_fail (MOSAIC_IS_WINDOW_BOX (box));

  if (box->is_window) {
    gchar *opt_name = get_window_class (box->xwindow);
    mosaic_window_box_set_opt_name (box, opt_name);
    g_free (opt_name);
  }
}

gint mosaic_window_box_get_desktop (MosaicWindowBox *box)
{
  g_return_val_if_fail (MOSAIC_IS_WINDOW_BOX (box), -1);

  return box->desktop;
}

void mosaic_window_box_set_desktop (MosaicWindowBox *box, gint desktop)
{
  g_return_if_fail (MOSAIC_IS_WINDOW_BOX (box));

  box->desktop = desktop;
}

static gushort get_crc16 (gchar *octets, guint len)
{
  gushort crc16_poly = 0x8408;

  gushort data;
  gushort crc = 0xffff;
  if (len == 0)
    return (~crc);

  do {
    for (int i =0, data= (guint)0xff & *octets++;
	 i < 8;
	 i++, data >>= 1) {
      if ((crc & 0x0001) ^ (data & 0x0001))
	crc = (crc >> 1) ^ crc16_poly;
      else
	crc >>= 1;
    }
  } while (--len);

  crc = ~crc;
  data = crc;
  crc = (crc << 8) | (data >> 8 & 0xff);

  return (crc);
}

static gdouble hue2rgb (gdouble p, gdouble q, gdouble t)
{
  if (t < 0.0)
    t += 1.0;
  if (t > 1.0)
    t -= 1.0;
  if (t < 1.0/6.0)
    return (p + (q - p) * 6.0 * t);
  if (t < 1.0/2.0)
    return q;
  if (t < 2.0/3.0)
    return (p + (q - p) * (2.0/3.0 - t) * 6.0);
  return p;
}

static void mosaic_window_box_create_colors (MosaicWindowBox *box)
{
  g_return_if_fail (MOSAIC_IS_WINDOW_BOX (box));

  gchar *source = (box->opt_name) ? box->opt_name : MOSAIC_BOX (box)->name;
  if (box->colorize && source) {
    gdouble h, s, l;
    gulong crc = get_crc16 (source, strlen (source));
    guchar pre_h = (((crc >> 8) & 0xFF) + box->color_offset) % 256;
    guchar pre_s = ((crc << 0) & 0xFF);
    h = pre_h / 255.0;
    s = 0.5 + pre_s / 512.0;
    l = 0.6;

    gdouble q = l < 0.5 ? l * (1.0 + s) : l + s - l * s;
    gdouble p = 2.0 * l - q;
    box->r = hue2rgb (p, q, h + 1.0/3.0);
    box->g = hue2rgb (p, q, h);
    box->b = hue2rgb (p, q, h - 1.0/3.0);
  } else {
    box->r = box->g = box->b = 0.6;
  }
}

void mosaic_window_box_setup_icon_from_wm (MosaicWindowBox *box, guint req_width, guint req_height)
{
  g_return_if_fail (MOSAIC_IS_WINDOW_BOX (box));

  GdkPixbuf *pixbuf = get_window_icon (box->xwindow, req_width, req_height);
  if (!pixbuf) {
    // Try to load fallback icon.
    gchar *class1 = g_ascii_strdown (box->opt_name, -1);
    gchar *class2 = g_ascii_strdown (box->opt_name+strlen (class1)+1, -1);

    GtkIconTheme *theme = gtk_icon_theme_get_default ();
    pixbuf = gtk_icon_theme_load_icon (theme, class1, req_width,
				       GTK_ICON_LOOKUP_USE_BUILTIN |
				       GTK_ICON_LOOKUP_GENERIC_FALLBACK,
				       NULL);

    if (!pixbuf)
      pixbuf = gtk_icon_theme_load_icon (theme, class2, req_width,
					 GTK_ICON_LOOKUP_USE_BUILTIN |
					 GTK_ICON_LOOKUP_GENERIC_FALLBACK,
					 NULL);

    if (!pixbuf)
      pixbuf = gtk_icon_theme_load_icon (theme, "application-x-executable", req_width,
					 GTK_ICON_LOOKUP_USE_BUILTIN |
					 GTK_ICON_LOOKUP_GENERIC_FALLBACK,
					 NULL);
    g_free (class1);
    g_free (class2);
  }
  if (!pixbuf) {
    box->has_icon = FALSE;
    return;
  }

  mosaic_window_box_setup_icon (box, pixbuf);
}

void mosaic_window_box_setup_icon_from_theme (MosaicWindowBox *box, const gchar *name, guint req_width, guint req_height)
{
  g_return_if_fail (MOSAIC_IS_WINDOW_BOX (box));

  GtkIconTheme *theme = gtk_icon_theme_get_default ();
  GdkPixbuf *pixbuf = gtk_icon_theme_load_icon (theme, name, req_width,
				     GTK_ICON_LOOKUP_USE_BUILTIN |
				     GTK_ICON_LOOKUP_GENERIC_FALLBACK,
				     NULL);

  if (!pixbuf) {
    box->has_icon = FALSE;
    return;
  }

  mosaic_window_box_setup_icon (box, pixbuf);
}

void mosaic_window_box_setup_icon_from_file (MosaicWindowBox *box, const gchar *file, guint req_width, guint req_height)
{
  g_return_if_fail (MOSAIC_IS_WINDOW_BOX (box));

  GError *error = NULL;
  GdkPixbuf *pixbuf;
  
  if(!(pixbuf = gdk_pixbuf_new_from_file_at_size (file, req_width, req_height, &error)))
    g_printerr("%s\n", error->message);

  if (!pixbuf) {
    box->has_icon = FALSE;
    return;
  }

  mosaic_window_box_setup_icon (box, pixbuf);
}

static void mosaic_window_box_setup_icon (MosaicWindowBox *box, GdkPixbuf *pixbuf)
{
  g_return_if_fail (MOSAIC_IS_WINDOW_BOX (box));

  if (box->icon_pixbuf)
    g_object_unref (box->icon_pixbuf);
  if (box->icon_surface)
    cairo_surface_destroy (box->icon_surface);
  if (box->icon_context)
    cairo_destroy (box->icon_context);

  if (pixbuf) {
    box->icon_pixbuf = pixbuf;

    box->has_icon = TRUE;
    box->icon_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
						    gdk_pixbuf_get_width (box->icon_pixbuf),
						    gdk_pixbuf_get_height (box->icon_pixbuf));
    box->icon_context = cairo_create (box->icon_surface);
    gdk_cairo_set_source_pixbuf (box->icon_context, box->icon_pixbuf, 0, 0);
    cairo_paint (box->icon_context);
  } else {
    box->has_icon = FALSE;
  }
}

void mosaic_window_box_set_colorize (MosaicWindowBox *box, gboolean colorize)
{
  g_return_if_fail (MOSAIC_IS_WINDOW_BOX (box));

  box->colorize = colorize;
  mosaic_window_box_create_colors (box);
}

void mosaic_window_box_set_show_desktop (MosaicWindowBox *box, gboolean show_desktop)
{
  g_return_if_fail (MOSAIC_IS_WINDOW_BOX (box));

  box->show_desktop = show_desktop;
}

void mosaic_window_box_set_show_titles (MosaicWindowBox *box, gboolean show_titles)
{
  g_return_if_fail (MOSAIC_IS_WINDOW_BOX (box));

  box->show_titles = show_titles;
  if (!show_titles)
    gtk_widget_set_tooltip_text (GTK_WIDGET(box), MOSAIC_BOX(box)->name);
}

void mosaic_window_box_set_color_offset (MosaicWindowBox *box, guchar color_offset)
{
  g_return_if_fail (MOSAIC_IS_WINDOW_BOX (box));

  box->color_offset = color_offset;
  mosaic_window_box_create_colors (box);
}

void mosaic_window_box_set_color_from_string (MosaicWindowBox *box, const gchar *color)
{
  gchar *scolor = g_strdup (color);
  g_strstrip (scolor);
  int parsed = 0x888888;
  if (g_str_has_prefix (scolor, "#"))
    parsed = strtol (scolor+1, NULL, 16);
  guchar r = (parsed >> 16) & 0xff;
  guchar g = (parsed >> 8) & 0xff;
  guchar b = (parsed >> 0) & 0xff;

  box->r = r / 255.0;
  box->g = g / 255.0;
  box->b = b / 255.0;

  g_free (scolor);
}
