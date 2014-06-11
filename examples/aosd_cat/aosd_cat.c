/* aosd -- OSD with transparency, cairo, and pango.
 *
 * Copyright (C) 2008 Eugene Paskevich <eugene@raptor.kiev.ua>
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <time.h>

#include <glib.h>

#include <aosd-text.h>
#include "aosd_cat.h"
#include "config.h"

static gboolean
parse_options(int* argc, char** argv[])
{
  GOptionContext* ctx = NULL;
  GOptionGroup* group = NULL;
  GSList* opt_list = NULL;
  gchar* str = NULL;
  guint n = 0;

  PREPARE_CATCH;
  START_CATCH;

  /* Fill the linked list with default option values.
   * Order is important and must match entries below. */
#define ADD_ELEM(data, name) \
  CATCH(data != NULL, "Memory allocation error for %s", #name); \
  opt_list = g_slist_append(opt_list, data)
#define ADD_NUMB(var) \
  str = g_strdup_printf("%i", config.var); \
  ADD_ELEM(str, config.var)
#define ADD_STRN(var) \
  str = g_strdup(((config.var) == NULL ? "None" : (config.var))); \
  ADD_ELEM(str, config.var)

  ADD_NUMB(back_alpha);
  ADD_NUMB(shadow_alpha);
  ADD_NUMB(fore_alpha);
  ADD_NUMB(output);
  ADD_NUMB(position);
  ADD_NUMB(x_offset);
  ADD_NUMB(y_offset);
  ADD_NUMB(shadow_offset);
  ADD_NUMB(padding);
  ADD_NUMB(transparency);
  ADD_STRN(font);
  ADD_NUMB(width);
  ADD_NUMB(alignment);
  ADD_STRN(back_color);
  ADD_STRN(shadow_color);
  ADD_STRN(fore_color);
  ADD_NUMB(fade_in);
  ADD_NUMB(fade_full);
  ADD_NUMB(fade_out);
  ADD_NUMB(age);
  ADD_NUMB(lines);
  ADD_NUMB(wait);
  ADD_NUMB(keep_reading);
  ADD_NUMB(markup);
  ADD_STRN(input);
  ADD_NUMB(input_timeout);
#undef ADD_ELEM
#undef ADD_NUMB
#undef ADD_STRN

  /* Create option entry arrays with default values.
   * Order must match the list above. */
#define DEF (gchar*)g_slist_nth_data(opt_list, n++)
#define OPT_ADD(type, lname, sname, conf, desc) \
  { lname, sname, 0, type, &config.conf, "Sets the " desc, DEF }

#define OPT_INT(...) OPT_ADD(G_OPTION_ARG_INT,## __VA_ARGS__)
#define OPT_STR(...) OPT_ADD(G_OPTION_ARG_STRING,## __VA_ARGS__)

  GOptionEntry opacity[] =
  {
    OPT_INT("back-opacity", 'b', back_alpha, "background opacity. *"),
    OPT_INT("shadow-opacity", 's', shadow_alpha, "shadow opacity. *"),
    OPT_INT("fore-opacity", 'r', fore_alpha, "foreground opacity. *"),
    { NULL }
  };

  GOptionEntry geometry[] =
  {
    OPT_INT("output", 'O', output, "Xinerama output (0-N), -1 for whole X screen. **"),
    OPT_INT("position", 'p', position, "OSD window position."),
    OPT_INT("x-offset", 'x', x_offset, "x-axis window offset. **"),
    OPT_INT("y-offset", 'y', y_offset, "y-axis window offset. **"),
    OPT_INT("shadow-offset", 'e', shadow_offset, "shadow offset. **"),
    OPT_INT("padding", 'd', padding, "margin from the edge to the contents. *"),
    { NULL }
  };

  GOptionEntry appearance[] =
  {
    OPT_INT("transparency", 't', transparency, "transparency mode."),
    OPT_STR("font", 'n', font, "OSD font."),
    OPT_INT("width", 'w', width, "OSD wrapping width in pixels."),
    OPT_INT("alignment", 'A', alignment, "text alignment (0=left, 1=center, 2=right)."),
    { NULL }
  };

  GOptionEntry coloring[] =
  {
    OPT_STR("back-color", 'B', back_color, "background color."),
    OPT_STR("shadow-color", 'S', shadow_color, "shadow color."),
    OPT_STR("fore-color", 'R', fore_color, "foreground color."),
    { NULL }
  };

  GOptionEntry timing[] =
  {
    OPT_INT("fade-in", 'f', fade_in, "fade in time."),
    OPT_INT("fade-full", 'u', fade_full, "time to show with full opacity."),
    OPT_INT("fade-out", 'o', fade_out, "fade out time."),
    { NULL }
  };

  GOptionEntry scrollback[] =
  {
    OPT_INT("age", 'a', age, "line age removal limit."),
    OPT_INT("lines", 'l', lines, "line amount removal limit."),
    { NULL }
  };

  GOptionEntry source[] =
  {
    { "wait", 'W', 0, G_OPTION_ARG_NONE, &config.wait,
      "Wait until the display is clear.", NULL },
    { "keep-reading", 'k', 0, G_OPTION_ARG_NONE, &config.keep_reading,
      "Reopen input on EOF (doesn't have any effect when reading from stdin).", NULL },
    { "markup", 'm', 0, G_OPTION_ARG_NONE, &config.markup,
      "Enable Pango markup language.", NULL },
    OPT_STR("input", 'i', input, "input text source."),
    OPT_INT("input-timeout", 'T', input_timeout, "input timeout (in seconds)."),
    { NULL }
  };

#undef DEF
#undef OPT_ADD
#undef OPT_INT
#undef OPT_STR

  /* Create and setup option context */
  CATCH((ctx = g_option_context_new(NULL)) != NULL,
      "Unable to allocate option context");

  g_option_context_set_ignore_unknown_options(ctx, FALSE);
  g_option_context_set_summary(ctx,
      "Displays UTF-8 text in a transparent OSD frame.\n"
      "Built on " PACKAGE_STRING);
  g_option_context_set_description(ctx,
      "Notes on command line parameters:\n"
      "- Default values are specified after the equal sign.\n"
      "- For parameters marked with asterisk (*) valid range is 0-255.\n"
      "- Those, which are marked with double asterisk (**), accept negative values.\n"
      "- Valid position range is 0-8,\n"
      "  where 0 is top-left corner and 8 is bottom-right corner.\n"
      "- Transparency: 0=none, 1=fake, 2=composite\n"
      "- Coloring parameters are specified in either #RGB format or from rgb.txt.\n"
      "- Timing parameters are specified in milliseconds.\n"
      "- Scrollback limits are cancelled with 0 parameter. Age is in seconds.\n"
      "- If wrapping width is set to zero, text will be wrapped on screen width\n"
      "  or will not be wrapped at all if other parameters make it impossible to\n"
      "  layout correctly.\n"
      "\n"
      "Thank you for using " PACKAGE_NAME "!\n"
      "Please send bug reports to: " PACKAGE_BUGREPORT);

  /* Fill context option groups */
#define ADD_GROUP(name, desc) \
  group = g_option_group_new(#name, desc " Options:", \
      "Show " #name " help options", NULL, NULL); \
  CATCH(group != NULL, "Unable to create an option group for %s", desc); \
  g_option_group_add_entries(group, name); \
  g_option_context_add_group(ctx, group)

  ADD_GROUP(geometry, "Geometry");
  ADD_GROUP(appearance, "Appearance");
  ADD_GROUP(coloring, "Coloring");
  ADD_GROUP(opacity, "Opacity");
  ADD_GROUP(timing, "Timing");
  ADD_GROUP(scrollback, "Scrollback");
#undef ADD_GROUP

  g_option_context_add_main_entries(ctx, source, NULL);

  CATCH(g_option_context_parse(ctx, argc, argv, NULL), "");

  END_CATCH;

  /* Destroy the context and linked list of default values */
  if (ctx != NULL)
    g_option_context_free(ctx);
  if (opt_list != NULL)
    g_slist_free_full(opt_list, g_free);

  RETURN_CATCH;
}

static gboolean
verify_vals(void)
{
  PREPARE_CATCH;
  START_CATCH;

#define NUM(var, min, max) \
  CATCH(config.var >= min && config.var <= max, \
      "Invalid falue for " #var " parameter.")

  NUM(back_alpha, 0, G_MAXUINT8);
  NUM(shadow_alpha, 0, G_MAXUINT8);
  NUM(fore_alpha, 0, G_MAXUINT8);

  NUM(output, -1, G_MAXINT);
  NUM(padding, 0, G_MAXUINT8);
  NUM(shadow_offset, G_MININT8, G_MAXINT8);
  NUM(position, 0, 8);

  NUM(transparency, 0, 2);
  NUM(alignment, 0, 2);

  NUM(fade_in, 0, G_MAXINT);
  NUM(fade_full, 0, G_MAXINT);
  NUM(fade_out, 0, G_MAXINT);

  NUM(width, 0, G_MAXINT);
  NUM(age, 0, G_MAXINT);
  NUM(lines, 0, G_MAXINT);

  NUM(input_timeout, 0, G_MAXINT);
#undef NUM

  if (strcmp(config.input, "-") == 0)
    data.input = stdin;

  END_CATCH;
  RETURN_CATCH;
}

static gboolean
open_input(void)
{
  if (data.input == stdin)
    return TRUE;

  PREPARE_CATCH;
  START_CATCH;

  int fd;
  CATCH((fd = open(config.input, O_RDONLY | O_NONBLOCK)) >= 0,
      "Unable to open specified file for reading.");
  CATCH((data.input = fdopen(fd, "r")) != NULL,
      "Unable to open specified file for reading.");

  END_CATCH;
  RETURN_CATCH;
}

static gboolean
setup(void)
{
  PREPARE_CATCH;
  START_CATCH;

  CATCH((data.aosd = aosd_new()) != NULL, "Unable to create aosd object.");
  CATCH((data.rend = calloc(1, sizeof(TextRenderData))) != NULL,
      "Unable to allocate memory for TextRenderData object.");
  CATCH((data.rend->lay = pango_layout_new_aosd()) != NULL,
      "Unable to create Pango rendering layout.");

  aosd_set_renderer(data.aosd, aosd_text_renderer, data.rend);
  aosd_set_transparency(data.aosd, config.transparency);

  data.rend->geom.x_offset = config.padding;
  data.rend->geom.y_offset = config.padding;

  data.rend->back.color = config.back_color;
  data.rend->back.opacity = config.back_alpha;

  data.rend->shadow.color = config.shadow_color;
  data.rend->shadow.opacity = config.shadow_alpha;
  data.rend->shadow.x_offset = config.shadow_offset;
  data.rend->shadow.y_offset = config.shadow_offset;

  data.rend->fore.color = config.fore_color;
  data.rend->fore.opacity = config.fore_alpha;

  if (config.font != NULL)
    pango_layout_set_font_aosd(data.rend->lay, config.font);
  pango_layout_set_wrap(data.rend->lay, PANGO_WRAP_WORD_CHAR);
  pango_layout_set_alignment(data.rend->lay, config.alignment);

  END_CATCH;
  RETURN_CATCH;
}

static void
clean_queue(void)
{
  if (config.lines != 0)
    while (g_queue_get_length(data.list) > config.lines)
      KILL_FIRST;

  if (config.age != 0)
  {
    time_t now = time(NULL);
    Line* first = g_queue_peek_head(data.list);
    while (first->stamp + config.age <= now)
    {
      KILL_FIRST;
      first = g_queue_peek_head(data.list);
    }
  }
}

static void
concat_queue(gpointer data, gpointer user_data)
{
  Line* elem = data;
  gchar** arr = user_data;
  unsigned i = 0;

  while (arr[i] != NULL)
    i++;

  arr[i++] = elem->str;
  arr[i] = NULL;
}

static int
get_data(gchar** text)
{
  Line* elem = NULL;
  *text = NULL;

  PREPARE_CATCH;
  START_CATCH;

  int poll_timeout = -1;
  if (config.input_timeout > 0)
    poll_timeout = config.input_timeout * 1000;

  struct pollfd pollfd = { fileno(data.input), POLLIN, 0 };
  int ret = poll(&pollfd, 1, poll_timeout);
  if (ret < 0)
  {
    DEBUG("%s", strerror(errno));
    return -1;
  }
  if (ret == 0)
    return -2;

  char buf[1024];
  if (fgets(buf, 1024, data.input) == NULL)
    return 1;

  char* end = buf + strlen(buf) - 1;
  if (*end == '\n')
    *end = '\0';

  if (*buf != 7 || strlen(buf) != 1)
  {
    elem = calloc(1, sizeof(Line));
    CATCH(elem != NULL, "Unable to allocate scrollbuffer line element.");

    elem->str = g_strdup(buf);
    CATCH(elem->str != NULL, "Unable to allocate scrollbuffer line string.");

    elem->stamp = time(NULL);
    g_queue_push_tail(data.list, elem);
  }

  clean_queue();

  gchar* arr[g_queue_get_length(data.list) + 1];
  arr[0] = NULL;
  g_queue_foreach(data.list, concat_queue, arr);

  *text = g_strjoinv("\n", arr);

  END_CATCH;

  if (!GOOD_CATCH && elem != NULL)
  {
    if (elem->str != NULL)
      g_free(elem->str);
    free(elem);
  }

  return GOOD_CATCH ? 0 : -1;
}

static void
setup_width(void)
{
  if (config.width == 0)
  {
    data.width = PANGO_PIXELS(aosd_text_get_screen_wrap_width_xinerama(
      data.aosd, config.output, data.rend));
    data.width += (config.position % 3 == 2 ? 1 : -1) * config.x_offset;
  }

  data.width *= PANGO_SCALE;
  if (data.width < 0)
    data.width = -1;

  pango_layout_set_width(data.rend->lay, data.width);
}

static int
irq_poll_func(void)
{
  int ret;
  g_mutex_lock(&data.input_mutex);
  ret = (data.input_status == 1);
  g_mutex_unlock(&data.input_mutex);
  return ret;
}

static void
resize_and_show(void)
{
  aosd_text_get_size(data.rend, &data.width, &data.height);
  aosd_set_position_with_offset_xinerama(data.aosd,
      config.position % 3, config.position / 3,
      data.width, data.height, config.x_offset, config.y_offset, config.output);
  aosd_flash_with_irq_poll(data.aosd, (config.wait ? NULL : &irq_poll_func), 25,
      config.fade_in, config.fade_full, config.fade_out);
}

static void
cleanup(void)
{
  if (data.aosd != NULL)
    aosd_destroy(data.aosd);
  if (data.rend != NULL)
  {
    if (data.rend->lay != NULL)
      pango_layout_unref_aosd(data.rend->lay);
    free(data.rend);
  }
  if (data.input != NULL)
    fclose(data.input);
  if (data.list != NULL)
  {
    while (!g_queue_is_empty(data.list))
      KILL_FIRST;
    g_queue_free(data.list);
  }
}

static gpointer
read_input(gpointer arg)
{
  int ret;
  gchar* text = NULL;

  PREPARE_CATCH;
  while (1)
  {
    ret = get_data(&text);
    if (ret == 1)
    {
      if (data.input == stdin || !config.keep_reading)
        break;
      fclose(data.input);
      CATCH(open_input(), "");
      continue;
    }
    if (ret != 0)
      break;

    g_mutex_lock(&data.input_mutex);
    while (data.input_status != 0)
      g_cond_wait(&data.input_cond, &data.input_mutex);
    data.text = text;
    data.input_status = 1;
    g_cond_signal(&data.input_cond);
    g_mutex_unlock(&data.input_mutex);
  }

  g_mutex_lock(&data.input_mutex);
  while (data.input_status != 0)
    g_cond_wait(&data.input_cond, &data.input_mutex);
  data.input_status = -1;
  g_cond_signal(&data.input_cond);
  g_mutex_unlock(&data.input_mutex);

  return NULL;
}

int
main(int argc, char* argv[])
{
  /* deprecated since GLib 2.36 */
  g_type_init();

  /* deprecated since GLib 2.32 */
  g_thread_init(NULL);

  gchar* text = NULL;
  gchar* text_ = NULL;
  PangoAttrList* attr_list = NULL;

  PREPARE_CATCH;
  START_CATCH;

  CATCH(parse_options(&argc, &argv), "Error parsing options, try --help.");
  CATCH(setup(), "");
  CATCH(verify_vals(), "");
  CATCH(open_input(), "");

  data.list = g_queue_new();
  CATCH(data.list != NULL, "Unable to allocate scrollbuffer list.");

  GThread* reader_thread = g_thread_new("reader", read_input, NULL);

  while (1)
  {
    g_mutex_lock(&data.input_mutex);
    while (data.input_status == 0)
      g_cond_wait(&data.input_cond, &data.input_mutex);
    if (data.input_status < 0)
      break;
    text = data.text;
    data.input_status = 0;
    g_cond_signal(&data.input_cond);
    g_mutex_unlock(&data.input_mutex);

    if (config.markup)
    {
      text_ = NULL;
      if (pango_parse_markup(text, -1, 0, &attr_list, &text_, NULL, NULL))
      {
        pango_layout_set_attributes(data.rend->lay, attr_list);
        pango_attr_list_unref(attr_list);
        g_free(text);
        text = text_;
      }
    }

    pango_layout_set_text(data.rend->lay, text, -1);
    g_free(text);

    setup_width();
    resize_and_show();
  }

  g_thread_join(reader_thread);

  END_CATCH;

  cleanup();

  return GOOD_CATCH ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* vim: set ts=2 sw=2 et : */
