// 4 september 2015
#include "dtp.h"

// TODO imitate gnome-calendar's day/month/year entries

#define dateTimePickerWidgetType (dateTimePickerWidget_get_type())
#define dateTimePickerWidget(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), dateTimePickerWidgetType, dateTimePickerWidget))
#define isDateTimePickerWidget(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), dateTimePickerWidgetType))
#define dateTimePickerWidgetClass(class) (G_TYPE_CHECK_CLASS_CAST((class), dateTimePickerWidgetType, dateTimePickerWidgetClass))
#define isDateTimePickerWidgetClass(class) (G_TYPE_CHECK_CLASS_TYPE((class), dateTimePickerWidget))
#define getDateTimePickerWidgetClass(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), dateTimePickerWidgetType, dateTimePickerWidgetClass))

typedef struct dateTimePickerWidget dateTimePickerWidget;
typedef struct dateTimePickerWidgetClass dateTimePickerWidgetClass;

struct dateTimePickerWidget {
	GtkToggleButton parent_instance;

	gulong toggledSignal;

	GtkWidget *window;
	GtkWidget *box;
	GtkWidget *calendar;
	GtkWidget *timebox;
	GtkWidget *hours;
	GtkWidget *minutes;
	GtkWidget *seconds;
	GtkWidget *ampm;

	GdkDevice *keyboard;
	GdkDevice *mouse;
};

struct dateTimePickerWidgetClass {
	GtkToggleButtonClass parent_class;
};

G_DEFINE_TYPE(dateTimePickerWidget, dateTimePickerWidget, GTK_TYPE_TOGGLE_BUTTON)

static int realSpinValue(GtkSpinButton *spinButton)
{
	GtkAdjustment *adj;

	adj = gtk_spin_button_get_adjustment(spinButton);
	return (int) gtk_adjustment_get_value(adj);
}

// TODO switch to GDateTime?
static void setLabel(dateTimePickerWidget *d)
{
	guint year, month, day;
	struct tm tm;
	time_t tt;
	gchar *str;

	gtk_calendar_get_date(GTK_CALENDAR(d->calendar), &year, &month, &day);
	tm.tm_hour = realSpinValue(GTK_SPIN_BUTTON(d->hours));
	if (realSpinValue(GTK_SPIN_BUTTON(d->ampm)) != 0)
		tm.tm_hour += 12;
	tm.tm_min = realSpinValue(GTK_SPIN_BUTTON(d->minutes));
	tm.tm_sec = realSpinValue(GTK_SPIN_BUTTON(d->seconds));
	tm.tm_mon = month;
	tm.tm_mday = day;
	tm.tm_year = year - 1900;
	tm.tm_isdst = -1;		// not available
	// fill in the missing fields
	tt = mktime(&tm);
	tm = *localtime(&tt);
	// and strip the newline
	str = g_strdup(asctime(&tm));
	str[strlen(str) - 1] = '\0';
	gtk_button_set_label(GTK_BUTTON(d), str);
	g_free(str);
}

static void dateTimeChanged(dateTimePickerWidget *d)
{
	setLabel(d);
	// TODO fire event here
}

// we don't want ::toggled to be sent again
static void setActive(dateTimePickerWidget *d, gboolean active)
{
	g_signal_handler_block(d, d->toggledSignal);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(d), active);
	g_signal_handler_unblock(d, d->toggledSignal);
}

// like startGrab() below, a lot of this is in the order that GtkComboBox does it
static void endGrab(dateTimePickerWidget *d)
{
	if (d->keyboard != NULL)
		gdk_device_ungrab(d->keyboard, GDK_CURRENT_TIME);
	gdk_device_ungrab(d->mouse, GDK_CURRENT_TIME);
	gtk_device_grab_remove(d->window, d->mouse);
	d->keyboard = NULL;
	d->mouse = NULL;
}

static void hidePopup(dateTimePickerWidget *d)
{
	endGrab(d);
	gtk_widget_hide(d->window);
	setActive(d, FALSE);
}

// this consolidates a good chunk of what GtkComboBox does
static gboolean startGrab(dateTimePickerWidget *d)
{
	GdkDevice *dev;
	guint32 time;
	GdkWindow *window;
	GdkDevice *keyboard, *mouse;

	dev = gtk_get_current_event_device();
	if (dev == NULL)
		return FALSE; // TODO

	time = gtk_get_current_event_time();
	keyboard = dev;
	mouse = gdk_device_get_associated_device(dev);
	if (gdk_device_get_source(dev) != GDK_SOURCE_KEYBOARD) {
		dev = mouse;
		mouse = keyboard;
		keyboard = dev;
	}

	window = gtk_widget_get_window(d->window);
	if (keyboard != NULL)
		if (gdk_device_grab(keyboard, window,
			GDK_OWNERSHIP_WINDOW, TRUE,
			GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK,
			NULL, time) != GDK_GRAB_SUCCESS)
			return FALSE;
	if (mouse != NULL)
		if (gdk_device_grab(mouse, window,
			GDK_OWNERSHIP_WINDOW, TRUE,
			GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK,
			NULL, time) != GDK_GRAB_SUCCESS) {
			if (keyboard != NULL)
				gdk_device_ungrab(keyboard, time);
			return FALSE;
		}

	gtk_device_grab_add(d->window, mouse, TRUE);
	d->keyboard = keyboard;
	d->mouse = mouse;
	return TRUE;
}

// based on gtk_combo_box_list_position() in the GTK+ source code
static void allocationToScreen(dateTimePickerWidget *d, gint *x, gint *y)
{
	GdkWindow *window;
	GtkAllocation a;

	gtk_widget_get_allocation(GTK_WIDGET(d), &a);
	*x = 0;
	*y = 0;
	if (!gtk_widget_get_has_window(GTK_WIDGET(d))) {
		*x = a.x;
		*y = a.y;
	}
	window = gtk_widget_get_window(GTK_WIDGET(d));
	gdk_window_get_root_coords(window, *x, *y, x, y);
	if (gtk_widget_get_direction(GTK_WIDGET(d)) == GTK_TEXT_DIR_RTL)
		*x += a.width;		// TODO subtract target width
	// TODO monitor detection
	*y += a.height;
}

static void showPopup(dateTimePickerWidget *d)
{
	GtkWidget *toplevel;
	gint x, y;

	// GtkComboBox does it
	toplevel = gtk_widget_get_toplevel(GTK_WIDGET(d));
	if (GTK_IS_WINDOW(toplevel))
		gtk_window_group_add_window(gtk_window_get_group(GTK_WINDOW(toplevel)), GTK_WINDOW(d->window));

	allocationToScreen(d, &x, &y);
	gtk_window_move(GTK_WINDOW(d->window), x, y);

	gtk_widget_show(d->window);
	setActive(d, TRUE);

	if (!startGrab(d))
		hidePopup(d);
}

static void onToggled(GtkToggleButton *b, gpointer data)
{
	dateTimePickerWidget *d = dateTimePickerWidget(b);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(d)))
		showPopup(d);
	else
		hidePopup(d);
}

static gboolean grabBroken(GtkWidget *w, GdkEventGrabBroken *e, gpointer data)
{
	dateTimePickerWidget *d = dateTimePickerWidget(data);

	hidePopup(d);
	return TRUE;		// this is what GtkComboBox does
}

static gboolean buttonReleased(GtkWidget *w, GdkEventButton *e, gpointer data)
{
	dateTimePickerWidget *d = dateTimePickerWidget(data);
	int winx, winy;
	GtkAllocation wina;
	gboolean in;

	gtk_widget_get_allocation(d->window, &wina);
	winx = 0;
	winy = 0;
	if (!gtk_widget_get_has_window(d->window)) {
		winx = wina.x;
		winy = wina.y;
	}
	gdk_window_get_root_coords(gtk_widget_get_window(d->window), winx, winy, &winx, &winy);
	in = TRUE;
	if (e->x_root < winx)
		in = FALSE;
	if (e->x_root >= (winx + wina.width))
		in = FALSE;
	if (e->y_root < winy)
		in = FALSE;
	if (e->y_root >= (winy + wina.height))
		in = FALSE;
	if (!in)
		hidePopup(d);
	return TRUE;		// this is what GtkComboBox does
}

static gint hoursSpinboxInput(GtkSpinButton *sb, gpointer ptr, gpointer data)
{
	double *out = (double *) ptr;
	const gchar *text;
	int value;

	text = gtk_entry_get_text(GTK_ENTRY(sb));
	value = (int) g_strtod(text, NULL);
	if (value < 0 || value > 12)
		return GTK_INPUT_ERROR;
	if (value == 12)		// 12 to the user is 0 internally
		value = 0;
	*out = (double) value;
	return TRUE;
}

static gboolean hoursSpinboxOutput(GtkSpinButton *sb, gpointer data)
{
	gchar *text;
	int value;

	value = realSpinValue(sb);
	if (value == 0)		// 0 internally is 12 to the user
		value = 12;
	text = g_strdup_printf("%d", value);
	gtk_entry_set_text(GTK_ENTRY(sb), text);
	g_free(text);
	return TRUE;
}

static gboolean zeroPadSpinbox(GtkSpinButton *sb, gpointer data)
{
	gchar *text;
	int value;

	value = realSpinValue(sb);
	text = g_strdup_printf("%02d", value);
	gtk_entry_set_text(GTK_ENTRY(sb), text);
	g_free(text);
	return TRUE;
}

static gint ampmSpinboxInput(GtkSpinButton *sb, gpointer ptr, gpointer data)
{
	double *out = (double *) ptr;
	const gchar *text;
	char firstAM, firstPM;

	text = gtk_entry_get_text(GTK_ENTRY(sb));
	firstAM = nl_langinfo(AM_STR)[0];
	firstPM = nl_langinfo(PM_STR)[0];
	for (; *text != '\0'; text++)
		if (*text == firstAM) {
			*out = 0;
			return TRUE;
		} else if (*text == firstPM) {
			*out = 1;
			return TRUE;
		}
	return GTK_INPUT_ERROR;
}

static gboolean ampmSpinboxOutput(GtkSpinButton *sb, gpointer data)
{
	int value;

	value = gtk_spin_button_get_value_as_int(sb);
	if (value == 0)
		gtk_entry_set_text(GTK_ENTRY(sb), nl_langinfo(AM_STR));
	else
		gtk_entry_set_text(GTK_ENTRY(sb), nl_langinfo(PM_STR));
	return TRUE;
}

static void spinboxChanged(GtkSpinButton *sb, gpointer data)
{
	dateTimePickerWidget *d = dateTimePickerWidget(data);

	dateTimeChanged(d);
}

static GtkWidget *newSpinbox(dateTimePickerWidget *d, int min, int max, gint (*input)(GtkSpinButton *, gpointer, gpointer), gboolean (*output)(GtkSpinButton *, gpointer))
{
	GtkWidget *sb;

	sb = gtk_spin_button_new_with_range(min, max, 1);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(sb), 0);
	gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(sb), TRUE);
	gtk_orientable_set_orientation(GTK_ORIENTABLE(sb), GTK_ORIENTATION_VERTICAL);
	g_signal_connect(sb, "value-changed", G_CALLBACK(spinboxChanged), d);
	if (input != NULL)
		g_signal_connect(sb, "input", G_CALLBACK(input), NULL);
	if (output != NULL)
		g_signal_connect(sb, "output", G_CALLBACK(output), NULL);
	return sb;
}

static void dateChanged(GtkCalendar *c, gpointer data)
{
	dateTimePickerWidget *d = dateTimePickerWidget(data);

	dateTimeChanged(d);
}

static void dateTimePickerWidget_init(dateTimePickerWidget *d)
{
	d->window = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_window_set_resizable(GTK_WINDOW(d->window), FALSE);
	gtk_window_set_attached_to(GTK_WINDOW(d->window), GTK_WIDGET(d));
	// TODO set_keep_above()?
	gtk_window_set_decorated(GTK_WINDOW(d->window), FALSE);
	gtk_window_set_deletable(GTK_WINDOW(d->window), FALSE);
	gtk_window_set_type_hint(GTK_WINDOW(d->window), GDK_WINDOW_TYPE_HINT_COMBO);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(d->window), TRUE);
	gtk_window_set_skip_pager_hint(GTK_WINDOW(d->window), TRUE);
	// TODO accept_focus()?
	// TODO focus_on_map()?
	gtk_window_set_has_resize_grip(GTK_WINDOW(d->window), FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(d->window), 12);

	d->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_add(GTK_CONTAINER(d->window), d->box);

	d->calendar = gtk_calendar_new();
	g_signal_connect(d->calendar, "day-selected", G_CALLBACK(dateChanged), d);
	gtk_container_add(GTK_CONTAINER(d->box), d->calendar);

	d->timebox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_valign(d->timebox, GTK_ALIGN_CENTER);
	gtk_container_add(GTK_CONTAINER(d->box), d->timebox);

	d->hours = newSpinbox(d, 0, 11, hoursSpinboxInput, hoursSpinboxOutput);
	gtk_container_add(GTK_CONTAINER(d->timebox), d->hours);

	gtk_container_add(GTK_CONTAINER(d->timebox),
		gtk_label_new(":"));

	d->minutes = newSpinbox(d, 0, 59, NULL, zeroPadSpinbox);
	gtk_container_add(GTK_CONTAINER(d->timebox), d->minutes);

	gtk_container_add(GTK_CONTAINER(d->timebox),
		gtk_label_new(":"));

	d->seconds = newSpinbox(d, 0, 59, NULL, zeroPadSpinbox);
	gtk_container_add(GTK_CONTAINER(d->timebox), d->seconds);

	// TODO this should be the case, but that interferes with grabs
	// switch to it when we can drop GTK+ 3.10 and use popovers
#if 0
	d->ampm = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(d->ampm), NULL, "AM");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(d->ampm), NULL, "PM");
#endif
	// TODO why does pressing the button work oddly?
	d->ampm = newSpinbox(d, 0, 1, ampmSpinboxInput, ampmSpinboxOutput);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(d->ampm), FALSE);
	gtk_widget_set_valign(d->ampm, GTK_ALIGN_CENTER);
	gtk_container_add(GTK_CONTAINER(d->timebox), d->ampm);

	gtk_widget_show_all(d->box);

	g_signal_connect(d->window, "grab-broken-event", G_CALLBACK(grabBroken), d);
	g_signal_connect(d->window, "button-release-event", G_CALLBACK(buttonReleased), d);

	d->toggledSignal = g_signal_connect(d, "toggled", G_CALLBACK(onToggled), NULL);
	d->keyboard = NULL;
	d->mouse = NULL;

	// TODO set current time to now
}

static void dateTimePickerWidget_dispose(GObject *obj)
{
	// TODO destroy window
	G_OBJECT_CLASS(dateTimePickerWidget_parent_class)->dispose(obj);
}

static void dateTimePickerWidget_finalize(GObject *obj)
{
	G_OBJECT_CLASS(dateTimePickerWidget_parent_class)->finalize(obj);
}

static void dateTimePickerWidget_class_init(dateTimePickerWidgetClass *class)
{
	G_OBJECT_CLASS(class)->dispose = dateTimePickerWidget_dispose;
	G_OBJECT_CLASS(class)->finalize = dateTimePickerWidget_finalize;
}

GtkWidget *newDTP(void)
{
	GtkWidget *w;

	w = GTK_WIDGET(g_object_new(dateTimePickerWidgetType, "label", "", NULL));
	setLabel(dateTimePickerWidget(w));
	return w;
}
