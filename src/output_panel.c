/*
    tg
    Copyright (C) 2015 Marcello Mamino

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "tg.h"
#include <time.h>

cairo_pattern_t *black,*white,*red,*green,*blue,*blueish,*yellow,*goldenrod;

static void define_color(cairo_pattern_t **gc,double r,double g,double b)
{
	*gc = cairo_pattern_create_rgb(r,g,b);
}

void initialize_palette()
{
	define_color(&black,0,0,0);
	define_color(&white,1,1,1);
	define_color(&red,1,0,0);
	define_color(&green,0,0.8,0);
	define_color(&blue,0,0,1);
	define_color(&blueish,0,0,.5);
	define_color(&yellow,1,1,0);
	define_color(&goldenrod,.980,.761,.020);
}

static void draw_graph(double a, double b, cairo_t *c, struct processing_buffers *p, GtkWidget *da)
{
	GtkAllocation temp;
	gtk_widget_get_allocation (da, &temp);
	int width = temp.width;
	int height = temp.height;

	int n;

	if(!(p->period > 0) || !(p->waveform_max > 0)) {
		/* M4: stale/invalid processing buffer -- nothing sensible to draw */
		return;
	}

	int first = 1;
	for(n=0; n<2*width; n++) {
		int i = n < width ? n : 2*width - 1 - n;
		double x = fmod(a + i * (b-a) / width, p->period);
		if(x < 0) x += p->period;
		int j = floor(x);
		double y;

		if(p->waveform[j] <= 0) y = 0;
		else y = p->waveform[j] * 0.4 / p->waveform_max;

		int k = round(y*height);
		if(n < width) k = -k;
		if(first) {
			cairo_move_to(c,i+.5,height/2+k+.5);
			first = 0;
		} else
			cairo_line_to(c,i+.5,height/2+k+.5);
	}
}
#ifdef DEBUG
static void draw_debug_graph(double a, double b, cairo_t *c, struct processing_buffers *p, GtkWidget *da)
{
	if(!p->debug) return;

	GtkAllocation temp;
	gtk_widget_get_allocation (da, &temp);
	int width = temp.width;
	int height = temp.height;

	int i;
	float max = 0;

	int ai = round(a);
	int bi = 1+round(b);
	if(ai < 0) ai = 0;
	if(bi > p->sample_count) bi = p->sample_count;
	for(i=ai; i<bi; i++)
		if(p->debug[i] > max)
			max = p->debug[i];

	int first = 1;
	for(i=0; i<width; i++) {
		if( round(a + i*(b-a)/width) != round(a + (i+1)*(b-a)/width) ) {
			int j = round(a + i*(b-a)/width);
			if(j < 0) j = 0;
			if(j >= p->sample_count) j = p->sample_count-1;

			int k = round((0.1+p->debug[j]/max)*0.8*height);

			if(first) {
				cairo_move_to(c,i+.5,height-k-.5);
				first = 0;
			} else
				cairo_line_to(c,i+.5,height-k-.5);
		}
	}
}
#endif

static double amplitude_to_time(double lift_angle, double amp)
{
	double ratio = lift_angle / (2 * amp);
	if(ratio > 1.0) ratio = 1.0;
	if(ratio < -1.0) ratio = -1.0;
	return asin(ratio) / M_PI;
}

static double draw_watch_icon(cairo_t *c, int signal, int happy, int light)
{
	happy = !!happy;
	cairo_set_line_width(c,3);
	cairo_set_source(c,happy?green:red);
	cairo_move_to(c, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.5);
	cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.75, OUTPUT_WINDOW_HEIGHT * (0.75 - 0.5*happy));
	cairo_move_to(c, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.5);
	cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.35, OUTPUT_WINDOW_HEIGHT * (0.65 - 0.3*happy));
	cairo_stroke(c);
	cairo_arc(c, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.4, 0, 2*M_PI);
	cairo_stroke(c);
	int l = OUTPUT_WINDOW_HEIGHT * 0.8 / (2*NSTEPS - 1);
	int i;
	cairo_set_line_width(c,1);
	for(i = 0; i < signal; i++) {
		// Color gradient: red (stage 0, coarse) -> yellow (stage 1) -> green (stage 2+, fine)
		if (i == 0) cairo_set_source(c, red);
		else if (i == 1) cairo_set_source(c, yellow);
		else cairo_set_source(c, green);
		cairo_move_to(c, OUTPUT_WINDOW_HEIGHT + 0.5*l, OUTPUT_WINDOW_HEIGHT * 0.9 - 2*i*l);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT + 1.5*l, OUTPUT_WINDOW_HEIGHT * 0.9 - 2*i*l);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT + 1.5*l, OUTPUT_WINDOW_HEIGHT * 0.9 - (2*i+1)*l);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT + 0.5*l, OUTPUT_WINDOW_HEIGHT * 0.9 - (2*i+1)*l);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT + 0.5*l, OUTPUT_WINDOW_HEIGHT * 0.9 - 2*i*l);
		cairo_stroke_preserve(c);
		cairo_fill(c);
	}
	if(light) {
		int l = OUTPUT_WINDOW_HEIGHT * 0.15;
		cairo_set_line_width(c,2);
		cairo_move_to(c, OUTPUT_WINDOW_HEIGHT * 0.5 - 0.5*l, OUTPUT_WINDOW_HEIGHT * 0.2);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.5 - 0.5*l, OUTPUT_WINDOW_HEIGHT * 0.2 + l);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.5        , OUTPUT_WINDOW_HEIGHT * 0.2 + l);
		cairo_move_to(c, OUTPUT_WINDOW_HEIGHT * 0.5 - 0.2*l, OUTPUT_WINDOW_HEIGHT * 0.2 + 1);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.5 + 0.8*l, OUTPUT_WINDOW_HEIGHT * 0.2 + 1);
		cairo_move_to(c, OUTPUT_WINDOW_HEIGHT * 0.5 + 0.3*l, OUTPUT_WINDOW_HEIGHT * 0.2 + 1);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.5 + 0.3*l, OUTPUT_WINDOW_HEIGHT * 0.2 + l + 1);
		cairo_stroke(c);
	}
	return OUTPUT_WINDOW_HEIGHT + 3*l;
}

static void cairo_init(cairo_t *c)
{
	cairo_set_line_width(c,1);

	cairo_set_source(c,black);
	cairo_paint(c);
}

static double print_s(cairo_t *c, double x, double y, char *s)
{
	cairo_text_extents_t extents;
	cairo_move_to(c,x,y);
	cairo_show_text(c,s);
	cairo_text_extents(c,s,&extents);
	x += extents.x_advance;
	return x;
}

static double print_number(cairo_t *c, double x, double y, char *s)
{
	cairo_text_extents_t extents;
	cairo_text_extents(c,"0",&extents);
	double z = extents.x_advance;
	char t[2];
	t[1] = 0;
	while((t[0] = *s++)) {
		cairo_text_extents(c,t,&extents);
		cairo_move_to(c, x + (z - extents.x_advance) / 2, y);
		cairo_show_text(c,t);
		x += z;
	}
	return x;
}

/** Aggregated statistics over a time window. */
struct window_stats {
	int nsamples;
	int is_snapshot;  /**< C4: single-shot stats from a loaded file, not a live window */
	double rate_mean, rate_min, rate_max, rate_std;
	double be_mean, be_min, be_max, be_std;
	double amp_mean, amp_min, amp_max, amp_std;
};

/** Compute windowed statistics for rate (s/d), beat error (ms) and
 *  amplitude (deg) from the snapshot's per-beat history ring buffers.
 *
 * Walks the ring buffers backwards from the most recent entry (hist_wp-1)
 * and accumulates entries whose timestamp falls within [now - window, now],
 * where now is the timestamp of the newest history entry.
 *
 * @param s       snapshot with history ring data
 * @param window  time window in seconds
 * @param ws      caller-allocated struct to receive results  */
static void compute_window_stats(
	const struct snapshot *s,
	int window,
	struct window_stats *ws)
{
	memset(ws, 0, sizeof(*ws));
	ws->rate_min = ws->be_min = ws->amp_min = HUGE_VAL;
	ws->rate_max = ws->be_max = ws->amp_max = -HUGE_VAL;

	if(!s->rate_hist || s->hist_count == 0) {
		/* C4: a loaded snapshot (rate_hist == NULL because the
		 * history arrays were never serialised) has no windowed
		 * history, but it does carry a perfectly good single-shot
		 * rate/BE/amp. Surface them as single-sample stats so the
		 * panel shows real numbers instead of "collecting data..."
		 * forever. A live session keeps rate_hist non-NULL while it
		 * is still gathering beats (hist_count == 0 there) and should
		 * keep showing "collecting data..." until it has beats. */
		if(!s->rate_hist && (s->rate || s->be || s->amp)) {
			ws->nsamples = 1;
			ws->is_snapshot = 1;
			ws->rate_mean = ws->rate_min = ws->rate_max = s->rate;
			ws->rate_std = 0;
			ws->be_mean = ws->be_min = ws->be_max = s->be;
			ws->be_std = 0;
			ws->amp_mean = ws->amp_min = ws->amp_max = s->amp;
			ws->amp_std = 0;
		}
		return;
	}

	// Walk backwards from the most recent entry by timestamp
	int i = s->hist_wp - 1;
	if(i < 0) i = s->hist_max - 1;
	uint64_t now = s->hist_time[i];
	if(now == 0) return;  // No real timestamps yet (zero-initialized ring)
	uint64_t window_frames = (uint64_t)(window * s->nominal_sr);
	// Guard against underflow at startup when we have fewer frames than the window
	uint64_t threshold = (now >= window_frames) ? (now - window_frames) : 0;

	double sum = 0, sumsq = 0;
	int cnt = 0;
	for(int j = 0; j < s->hist_count; j++) {
		if(s->hist_time[i] < threshold)
			break;
		double r = s->rate_hist[i];
		sum += r;
		sumsq += r * r;
		if(r < ws->rate_min) ws->rate_min = r;
		if(r > ws->rate_max) ws->rate_max = r;
		cnt++;
		i--;
		if(i < 0) i = s->hist_max - 1;
	}
	ws->rate_mean = cnt > 0 ? sum / cnt : 0;
	if(cnt > 1)
		ws->rate_std = sqrt((sumsq - sum * sum / cnt) / (cnt - 1));
	else
		ws->rate_std = 0;
	ws->nsamples = cnt;

	sum = sumsq = 0;
	i = s->hist_wp - 1;
	if(i < 0) i = s->hist_max - 1;
	for(int j = 0; j < cnt; j++) {
		double b = s->be_hist[i];
		sum += b;
		sumsq += b * b;
		if(b < ws->be_min) ws->be_min = b;
		if(b > ws->be_max) ws->be_max = b;
		i--;
		if(i < 0) i = s->hist_max - 1;
	}
	ws->be_mean = cnt > 0 ? sum / cnt : 0;
	if(cnt > 1)
		ws->be_std = sqrt((sumsq - sum * sum / cnt) / (cnt - 1));
	else
		ws->be_std = 0;

	sum = sumsq = 0;
	i = s->hist_wp - 1;
	if(i < 0) i = s->hist_max - 1;
	for(int j = 0; j < cnt; j++) {
		double a = s->amp_hist[i];
		sum += a;
		sumsq += a * a;
		if(a < ws->amp_min) ws->amp_min = a;
		if(a > ws->amp_max) ws->amp_max = a;
		i--;
		if(i < 0) i = s->hist_max - 1;
	}
	ws->amp_mean = cnt > 0 ? sum / cnt : 0;
	if(cnt > 1)
		ws->amp_std = sqrt((sumsq - sum * sum / cnt) / (cnt - 1));
	else
		ws->amp_std = 0;
}

void handle_copy_stats(GtkButton *b, struct output_panel *op)
{
	UNUSED(b);
	GtkWidget *widget = op->output_drawing_area;
	gchar *text = g_object_get_data(G_OBJECT(widget), "stats-text");
	if (text) {
		GtkClipboard *clip = gtk_widget_get_clipboard(widget, GDK_SELECTION_CLIPBOARD);
		gtk_clipboard_set_text(clip, text, -1);
	}
}

static gboolean output_query_tooltip(GtkWidget *widget, gint x, gint y, gboolean keyboard_mode, GtkTooltip *tooltip, struct output_panel *op)
{
	UNUSED(widget);
	UNUSED(y);
	UNUSED(keyboard_mode);
	
	struct snapshot *snst = op->snst;
	if (!snst || !snst->pb)
		return FALSE;
	
	// Approximate positions of each field in the output display
	// Watch icon takes ~OUTPUT_WINDOW_HEIGHT width
	int icon_width = OUTPUT_WINDOW_HEIGHT;
	int field_width = 200;
	int field_start = icon_width + 10;
	
	// s/d field
	if (x >= field_start && x < field_start + field_width) {
		gtk_tooltip_set_text(tooltip, "Rate (seconds per day). Positive = fast, negative = slow.");
		return TRUE;
	}
	field_start += field_width;
	
	// Beat Error field
	if (x >= field_start && x < field_start + field_width) {
		gtk_tooltip_set_text(tooltip, "Beat error (milliseconds). Difference between tic and toc duration.");
		return TRUE;
	}
	field_start += field_width;
	
	// Amplitude field
	if (x >= field_start && x < field_start + field_width) {
		if (snst->amp > 0) {
			char buf[256];
			snprintf(buf, sizeof(buf), "Amplitude: %.0f degrees. Lift angle: %.0f°.", snst->amp, snst->la);
			gtk_tooltip_set_text(tooltip, buf);
		} else {
			const char *reason;
			switch (snst->amp_fail_reason) {
				case AMP_NO_SIGNAL: reason = "No signal detected"; break;
				case AMP_WRONG_LIFT_ANGLE: reason = "Wrong lift angle set"; break;
				case AMP_NOISE_TOO_HIGH: reason = "Noise level too high"; break;
				case AMP_TIC_TOC_DETECTION_FAILED: reason = "Tic/toc detection failed"; break;
				case AMP_PERIOD_ESTIMATION_FAILED: reason = "Period estimation failed"; break;
				case AMP_THRESHOLD_EXCEEDED: reason = "Amplitude threshold exceeded (20% of max)"; break;
				case AMP_TIC_TOC_OUT_OF_RANGE: reason = "Amplitude outside valid range (135-360°)"; break;
				case AMP_TIC_TOC_DIFF_TOO_LARGE: reason = "Tic/toc amplitude difference > 60°"; break;
				default: reason = "Unknown"; break;
			}
			char buf[256];
			snprintf(buf, sizeof(buf), "Amplitude unavailable. Reason: %s. Check lift angle, microphone placement, or noise.", reason);
			gtk_tooltip_set_text(tooltip, buf);
		}
		return TRUE;
	}
	field_start += field_width;
	
	// bph field
	if (x >= field_start && x < field_start + field_width) {
		gtk_tooltip_set_text(tooltip, "Beats per hour (BPH). Auto-detected or manually set.");
		return TRUE;
	}
	
	// Watch icon area - signal quality
	if (x >= 0 && x < icon_width) {
		const char *signal_quality;
		if (snst->signal == 0) signal_quality = "No signal detected";
		else if (snst->signal < NSTEPS / 2) signal_quality = "Weak signal (coarse filter only)";
		else if (snst->signal < NSTEPS) signal_quality = "Moderate signal";
		else signal_quality = "Strong signal (all filter stages passed)";
		char buf[256];
		snprintf(buf, sizeof(buf), "Signal quality: %s (%d/%d stages)", signal_quality, snst->signal, NSTEPS);
		gtk_tooltip_set_text(tooltip, buf);
		return TRUE;
	}
	
	return FALSE;
}

static gboolean output_draw_event(GtkWidget *widget, cairo_t *c, struct output_panel *op)
{
	UNUSED(widget);
	cairo_init(c);

	struct snapshot *snst = op->snst;
	struct processing_buffers *p = snst->pb;
	int old = snst->is_old;

	/* Column x-positions captured from instant labels for stats alignment */
	double st_col[3] = {0, 0, 0};

	double x = draw_watch_icon(c,snst->signal,snst->calibrate ? snst->signal==NSTEPS : snst->signal, snst->is_light);

	cairo_text_extents_t extents;

	cairo_set_font_size(c, OUTPUT_FONT);
	cairo_text_extents(c,"0",&extents);
	double y = 30 - extents.y_bearing;

	if(snst->calibrate) {
		cairo_set_source(c, white);
		x = print_s(c,x,y,"cal");
		cairo_set_font_size(c, OUTPUT_FONT*2/3);
		x = print_s(c,x,y," (");
		cairo_move_to(c,x,y);
		{
			double a = 0;
			char *s[] = {"wait", "acq.", "done", "fail", NULL}, **t = s;
			for(;*t;t++) {
				cairo_text_extents(c,*t,&extents);
				if(a < extents.x_advance) a = extents.x_advance;
			}
			x += a;
		}
		switch(snst->cal_state) {
			case 1:
				cairo_set_source(c,green);
				cairo_show_text(c,"done");
				break;
			case 0:
				cairo_set_source(c, snst->signal == NSTEPS ? white : yellow);
				cairo_show_text(c, snst->signal == NSTEPS ? "acq." : "wait");
				break;
			case -1:
				cairo_set_source(c,red);
				cairo_show_text(c,"fail");
				break;
		}
		cairo_set_source(c, white);
		x = print_s(c,x,y,")");
		cairo_set_font_size(c, OUTPUT_FONT);
		char s[20];
		switch(snst->cal_state) {
			case 1:
				sprintf(s, " %s%d.%d",
						snst->cal_result < 0 ? "-" : "+",
						abs(snst->cal_result) / 10,
						abs(snst->cal_result) % 10 );
				x = print_s(c,x,y,s);
				cairo_set_font_size(c, OUTPUT_FONT*2/3);
				x = print_s(c,x,y," s/d");
				break;
			case 0:
				sprintf(s, " %d", snst->cal_percent);
				x = print_number(c,x,y,s);
				x = print_s(c,x,y," %");
				break;
		}
	} else {
		char outputs[4][64];
		if(p) {
			int rate = round(snst->rate);
			double be = snst->be;
			sprintf(outputs[0], "Accuracy (s/d) %s%d    ", rate > 0 ? "+" : rate < 0 ? "-" : "", abs(rate));
			sprintf(outputs[1], "Beat Error (ms) %4.1f    ", be);
			if(snst->amp > 0)
				sprintf(outputs[2], "Amplitude (deg) %3.0f    ", snst->amp);
			else
				strcpy(outputs[2], "Amplitude (deg) ---    ");
			sprintf(outputs[3], "bph %d    ", snst->guessed_bph);
		} else {
			strcpy(outputs[0], "Accuracy (s/d) ---    ");
			strcpy(outputs[1], "Beat Error (ms) ---    ");
			strcpy(outputs[2], "Amplitude (deg) ---    ");
			strcpy(outputs[3], "bph ---    ");
		}

		int i;
		for(i=0; i<4; i++) {
			cairo_set_source(c, i > 2 || !p || !old ? white : yellow);
			cairo_set_font_size(c, OUTPUT_FONT);
			if(i < 3) st_col[i] = x;
			x = print_s(c,x,y,outputs[i]);
		}
	}

	/* ---- Windowed stats section (below the main info line) ---- */
	if(!snst->calibrate) {
		int aw = snst->avg_window > 0 ? snst->avg_window : AVG_WINDOW_DEFAULT;
		struct window_stats ws;
		compute_window_stats(snst, aw, &ws);

		if(ws.nsamples > 0) {
			/* Build clipboard text */
			char clipbuf[512];
			sprintf(clipbuf,
				"Accuracy (s/d): mean %+d min %+d max %+d std dev %d\n"
				"Beat Error (ms):   mean %.1f min %.1f max %.1f std dev %.1f\n"
				"Amplitude (deg):   mean %.1f min %.1f max %.1f std dev %.1f",
				(int)round(ws.rate_mean), (int)round(ws.rate_min), (int)round(ws.rate_max), (int)round(ws.rate_std),
				ws.be_mean, ws.be_min, ws.be_max, ws.be_std,
				ws.amp_mean, ws.amp_min, ws.amp_max, ws.amp_std);

			g_object_set_data_full(G_OBJECT(widget), "stats-text",
				g_strdup(clipbuf), (GDestroyNotify)g_free);

			cairo_text_extents_t sext;
			cairo_set_font_size(c, OUTPUT_FONT * 0.42);
			cairo_text_extents(c, "0", &sext);
			double sh = sext.height * 1.5;

			double stats_y0 = y > 0 ? y + sext.y_bearing + sext.height + 24 : 6 + sext.y_bearing + sext.height + 24;

			/* Accuracy column header */
/*			cairo_set_source(c, yellow);
			cairo_set_font_size(c, OUTPUT_FONT * 0.42);
			print_s(c, st_col[0], stats_y0, "Accuracy");
*/
			cairo_set_source(c, white);
			double srow = stats_y0 + sh;
			char sbuf[64];
			sprintf(sbuf, "mean %+d", (int)round(ws.rate_mean));
			print_s(c, st_col[0], srow, sbuf); srow += sh;
			sprintf(sbuf, "min %+d", (int)round(ws.rate_min));
			print_s(c, st_col[0], srow, sbuf); srow += sh;
			sprintf(sbuf, "max %+d", (int)round(ws.rate_max));
			print_s(c, st_col[0], srow, sbuf); srow += sh;
			sprintf(sbuf, "std dev %d", (int)round(ws.rate_std));
			print_s(c, st_col[0], srow, sbuf);

			/* Beat Error column */
/*			cairo_set_source(c, blue);
			cairo_set_font_size(c, OUTPUT_FONT * 0.42);
			print_s(c, st_col[1], stats_y0, "Beat Error");
*/
			cairo_set_source(c, white);
			srow = stats_y0 + sh;
			sprintf(sbuf, "mean %.1f", ws.be_mean);
			print_s(c, st_col[1], srow, sbuf); srow += sh;
			sprintf(sbuf, "min %.1f", ws.be_min);
			print_s(c, st_col[1], srow, sbuf); srow += sh;
			sprintf(sbuf, "max %.1f", ws.be_max);
			print_s(c, st_col[1], srow, sbuf); srow += sh;
			sprintf(sbuf, "std dev %.1f", ws.be_std);
			print_s(c, st_col[1], srow, sbuf);

			/* Amplitude column */
/*			cairo_set_source(c, red);
			cairo_set_font_size(c, OUTPUT_FONT * 0.42);
			print_s(c, st_col[2], stats_y0, "Amplitude");
*/
			cairo_set_source(c, white);
			srow = stats_y0 + sh;
			sprintf(sbuf, "mean %.1f", ws.amp_mean);
			print_s(c, st_col[2], srow, sbuf); srow += sh;
			sprintf(sbuf, "min %.1f", ws.amp_min);
			print_s(c, st_col[2], srow, sbuf); srow += sh;
			sprintf(sbuf, "max %.1f", ws.amp_max);
			print_s(c, st_col[2], srow, sbuf); srow += sh;
			sprintf(sbuf, "std dev %.1f", ws.amp_std);
			print_s(c, st_col[2], srow, sbuf);
		}
	}
#ifdef DEBUG
	{
		static GTimer *timer = NULL;
		if (!timer) timer = g_timer_new();
		else {
			char s[100];
			sprintf(s,"  %.2f fps",1./g_timer_elapsed(timer, NULL));
			cairo_set_source(c, white);
			cairo_set_font_size(c, OUTPUT_FONT);
			x = print_s(c,x,y,s);
			g_timer_reset(timer);
		}
	}
#endif

	return FALSE;
}

static void expose_waveform(
			struct output_panel *op,
			GtkWidget *da,
			cairo_t *c,
			int (*get_offset)(struct processing_buffers*),
			double (*get_pulse)(struct processing_buffers*))
{
	cairo_init(c);

	GtkAllocation temp;
	gtk_widget_get_allocation(da, &temp);

	int width = temp.width;
	int height = temp.height;

	gtk_widget_get_allocation(gtk_widget_get_toplevel(da), &temp);
	int font = temp.width / 90;
	if(font < 12)
		font = 12;
	int i;

	cairo_set_font_size(c,font);

	for(i = 1-NEGATIVE_SPAN; i < POSITIVE_SPAN; i++) {
		int x = (NEGATIVE_SPAN + i) * width / (POSITIVE_SPAN + NEGATIVE_SPAN);
		cairo_move_to(c, x + .5, height / 2 + .5);
		cairo_line_to(c, x + .5, height - .5);
		if(i%5)
			cairo_set_source(c,green);
		else
			cairo_set_source(c,red);
		cairo_stroke(c);
	}
	cairo_set_source(c,white);
	for(i = 1-NEGATIVE_SPAN; i < POSITIVE_SPAN; i++) {
		if(!(i%5)) {
			int x = (NEGATIVE_SPAN + i) * width / (POSITIVE_SPAN + NEGATIVE_SPAN);
			char s[10];
			sprintf(s,"%d",i);
			cairo_move_to(c,x+font/4,height-font/2);
			cairo_show_text(c,s);
		}
	}

	cairo_text_extents_t extents;

	cairo_text_extents(c,"ms",&extents);
	cairo_move_to(c,width - extents.x_advance - font/4,height-font/2);
	cairo_show_text(c,"ms");

	struct snapshot *snst = op->snst;
	struct processing_buffers *p = snst->pb;
	int old = snst->is_old;
	double period = p ? p->period / snst->sample_rate : 7200. / snst->guessed_bph;

	for(i = 10; i < 360; i+=10) {
		if(2*i < snst->la) continue;
		double t = period*amplitude_to_time(snst->la,i);
		if(t > .001 * NEGATIVE_SPAN) continue;
		int x = round(width * (NEGATIVE_SPAN - 1000*t) / (NEGATIVE_SPAN + POSITIVE_SPAN));
		cairo_move_to(c, x+.5, .5);
		cairo_line_to(c, x+.5, height / 2 + .5);
		if(i % 50)
			cairo_set_source(c,green);
		else
			cairo_set_source(c,red);
		cairo_stroke(c);
	}

	double last_x = 0;
	cairo_set_source(c,white);
	for(i = 50; i < 360; i+=50) {
		double t = period*amplitude_to_time(snst->la,i);
		if(t > .001 * NEGATIVE_SPAN) continue;
		int x = round(width * (NEGATIVE_SPAN - 1000*t) / (NEGATIVE_SPAN + POSITIVE_SPAN));
		if(x > last_x) {
			char s[10];

			sprintf(s,"%d",abs(i));
			cairo_move_to(c, x + font/4, font * 3 / 2);
			cairo_show_text(c,s);
			cairo_text_extents(c,s,&extents);
			last_x = x + font/4 + extents.x_advance;
		}
	}

	cairo_text_extents(c,"deg",&extents);
	cairo_move_to(c,width - extents.x_advance - font/4,font * 3 / 2);
	cairo_show_text(c,"deg");

	if(p) {
		double span = 0.001 * snst->sample_rate;
		int offset = get_offset(p);

		double a = offset - span * NEGATIVE_SPAN;
		double b = offset + span * POSITIVE_SPAN;

		draw_graph(a,b,c,p,da);

		cairo_set_source(c,old?yellow:white);
		cairo_stroke_preserve(c);
		cairo_fill(c);

		double pulse = get_pulse(p);
		if(pulse > 0) {
			int x = round((NEGATIVE_SPAN - pulse / span) * width / (POSITIVE_SPAN + NEGATIVE_SPAN));
			cairo_move_to(c, x, 1);
			cairo_line_to(c, x, height - 1);
			cairo_set_source(c,blue);
			cairo_set_line_width(c,2);
			cairo_stroke(c);
		}
	} else {
		cairo_move_to(c, .5, height / 2 + .5);
		cairo_line_to(c, width - .5, height / 2 + .5);
		cairo_set_source(c,yellow);
		cairo_stroke(c);
	}
}

static int get_tic(struct processing_buffers *p)
{
	return p->tic;
}

static int get_toc(struct processing_buffers *p)
{
	return p->toc;
}

static double get_tic_pulse(struct processing_buffers *p)
{
	return p->tic_pulse;
}

static double get_toc_pulse(struct processing_buffers *p)
{
	return p->toc_pulse;
}

static gboolean tic_draw_event(GtkWidget *widget, cairo_t *c, struct output_panel *op)
{
	UNUSED(widget);
	expose_waveform(op, op->tic_drawing_area, c, get_tic, get_tic_pulse);
	return FALSE;
}

static gboolean toc_draw_event(GtkWidget *widget, cairo_t *c, struct output_panel *op)
{
	UNUSED(widget);
	expose_waveform(op, op->toc_drawing_area, c, get_toc, get_toc_pulse);
	return FALSE;
}

static gboolean period_draw_event(GtkWidget *widget, cairo_t *c, struct output_panel *op)
{
	UNUSED(widget);
	cairo_init(c);

	GtkAllocation temp;
	gtk_widget_get_allocation (op->period_drawing_area, &temp);

	int width = temp.width;
	int height = temp.height;

	struct snapshot *snst = op->snst;
	struct processing_buffers *p = snst->pb;
	int old = snst->is_old;

	double toc,a=0,b=0;

	if(p) {
		if(!(p->period > 0)) {
			/* M4: stale/invalid buffer (period <= 0) -- skip drawing */
		} else {
			toc = p->tic < p->toc ? p->toc : p->toc + p->period;
			a = ((double)p->tic + toc)/2 - p->period/2;
			b = ((double)p->tic + toc)/2 + p->period/2;

			cairo_move_to(c, (p->tic - a - NEGATIVE_SPAN*.001*snst->sample_rate) * width/p->period, 0);
			cairo_line_to(c, (p->tic - a - NEGATIVE_SPAN*.001*snst->sample_rate) * width/p->period, height);
			cairo_line_to(c, (p->tic - a + POSITIVE_SPAN*.001*snst->sample_rate) * width/p->period, height);
			cairo_line_to(c, (p->tic - a + POSITIVE_SPAN*.001*snst->sample_rate) * width/p->period, 0);
			cairo_set_source(c,blueish);
			cairo_fill(c);

			cairo_move_to(c, (toc - a - NEGATIVE_SPAN*.001*snst->sample_rate) * width/p->period, 0);
			cairo_line_to(c, (toc - a - NEGATIVE_SPAN*.001*snst->sample_rate) * width/p->period, height);
			cairo_line_to(c, (toc - a + POSITIVE_SPAN*.001*snst->sample_rate) * width/p->period, height);
			cairo_line_to(c, (toc - a + POSITIVE_SPAN*.001*snst->sample_rate) * width/p->period, 0);
			cairo_set_source(c,blueish);
			cairo_fill(c);
		}
	}

	int i;
	for(i = 1; i < 16; i++) {
		int x = i * width / 16;
		cairo_move_to(c, x+.5, .5);
		cairo_line_to(c, x+.5, height - .5);
		if(i % 4)
			cairo_set_source(c,green);
		else
			cairo_set_source(c,red);
		cairo_stroke(c);
	}

	if(p) {
		draw_graph(a,b,c,p,op->period_drawing_area);

		cairo_set_source(c,old?yellow:white);
		cairo_stroke_preserve(c);
		cairo_fill(c);
	} else {
		cairo_move_to(c, .5, height / 2 + .5);
		cairo_line_to(c, width - .5, height / 2 + .5);
		cairo_set_source(c,yellow);
		cairo_stroke(c);
	}

	return FALSE;
}

static gboolean paperstrip_draw_event(GtkWidget *widget, cairo_t *c, struct output_panel *op)
{
	int i;
	struct snapshot *snst = op->snst;
	const int valid_events_wp =
		snst->events &&
		snst->events_count > 0 &&
		snst->events_wp >= 0 &&
		snst->events_wp < snst->events_count;
	uint64_t time = snst->timestamp ? snst->timestamp : get_timestamp(snst->is_light);
	double sweep;
	double zoom_factor;
	double safe_sample_rate = snst->sample_rate > 0 ? snst->sample_rate : snst->nominal_sr;
	int safe_bph = snst->guessed_bph > 0 ? snst->guessed_bph : (snst->bph ? snst->bph : DEFAULT_BPH);
	const double trace_zoom = snst->trace_zoom > 0 ? snst->trace_zoom : 1.0;
	double slope = 1000; // detected rate: 1000 -> do not display
	if(snst->calibrate) {
		sweep = snst->nominal_sr;
		zoom_factor = PAPERSTRIP_ZOOM_CAL * trace_zoom;
		slope = (double) snst->cal * zoom_factor / (10 * 3600 * 24);
	} else {
		sweep = safe_sample_rate * 3600. / safe_bph;
		zoom_factor = PAPERSTRIP_ZOOM * trace_zoom;
		if(valid_events_wp && snst->events[snst->events_wp])
			slope = - snst->rate * zoom_factor / (3600. * 24.);
	}
	if(!isfinite(sweep) || sweep <= 0 || !isfinite(zoom_factor) || zoom_factor <= 0)
		return FALSE;

	cairo_init(c);

	GtkAllocation temp;
	gtk_widget_get_allocation (op->paperstrip_drawing_area, &temp);

	int width = temp.width;
	int height = temp.height;

	int stopped = 0;
	if( valid_events_wp &&
	    snst->events[snst->events_wp] &&
	    time > 5 * snst->nominal_sr + snst->events[snst->events_wp]) {
		time = 5 * snst->nominal_sr + snst->events[snst->events_wp];
		stopped = 1;
	}

	int strip_width = round(width / (1 + PAPERSTRIP_MARGIN));

	cairo_set_line_width(c,1.3);

	slope *= strip_width;
	if(slope <= 2 && slope >= -2) {
		for(i=0; i<4; i++) {
			double y = 0;
			cairo_move_to(c, (double)width * (i+.5) / 4, 0);
			for(;;) {
				double x = y * slope + (double)width * (i+.5) / 4;
				x = fmod(x, width);
				if(x < 0) x += width;
				double nx = x + slope * (height - y);
				if(nx >= 0 && nx <= width) {
					cairo_line_to(c, nx, height);
					break;
				} else {
					double d = slope > 0 ? width - x : x;
					y += d / fabs(slope);
					cairo_line_to(c, slope > 0 ? width : 0, y);
					y += 1;
					if(y > height) break;
					cairo_move_to(c, slope > 0 ? 0 : width, y);
				}
			}
		}
		cairo_set_source(c, blue);
		cairo_stroke(c);
	}

	cairo_set_line_width(c,1);

	int left_margin = (width - strip_width) / 2;
	int right_margin = (width + strip_width) / 2;
	cairo_move_to(c, left_margin + .5, .5);
	cairo_line_to(c, left_margin + .5, height - .5);
	cairo_move_to(c, right_margin + .5, .5);
	cairo_line_to(c, right_margin + .5, height - .5);
	cairo_set_source(c, green);
	cairo_stroke(c);

	double now = sweep*ceil(time/sweep);
	double ten_s = safe_sample_rate * 10 / sweep;
	double last_line = fmod(now/sweep, ten_s);
	int last_tenth = floor(now/(sweep*ten_s));
	for(i=0;;i++) {
		double y = 0.5 + round(last_line + i*ten_s);
		if(y > height) break;
		cairo_move_to(c, .5, y);
		cairo_line_to(c, width-.5, y);
		cairo_set_source(c, (last_tenth-i)%6 ? green : red);
		cairo_stroke(c);
	}

	if(!snst->calibrate && snst->amps_count && snst->amps && snst->amps_time) {
		int path_started = 0;
		cairo_set_source(c, yellow);
		for(i = snst->amps_wp;;) {
			if(!snst->amps_time[i]) break;
			double event_age = now - snst->amps_time[i];
			double row = floor(event_age / sweep);
			if(row >= height) break;

			double amp_deg = snst->la * snst->amps[i];
			double amp_norm = (amp_deg - 135.0) / (360.0 - 135.0);
			if(amp_norm < 0) amp_norm = 0;
			if(amp_norm > 1) amp_norm = 1;

			double column = right_margin - amp_norm * (strip_width - 1);
			if(!path_started) {
				cairo_move_to(c, column, row);
				path_started = 1;
			} else {
				cairo_line_to(c, column, row);
			}

			if(--i < 0) i = snst->amps_count - 1;
			if(i == snst->amps_wp) break;
		}
		if(path_started)
			cairo_stroke(c);
	}

	for(i = snst->events_wp; valid_events_wp;) {
		if(!snst->events_count || !snst->events[i]) break;
		double event = now - snst->events[i] + snst->trace_centering + sweep * PAPERSTRIP_MARGIN / (2 * zoom_factor);
		int column = floor(fmod(event, (sweep / zoom_factor)) * strip_width / (sweep / zoom_factor));
		int row = floor(event / sweep);
		if(row >= height) break;

		if(stopped) {
			cairo_set_source(c, yellow);
		} else if(snst->events_tictoc) {
			cairo_set_source(c, snst->events_tictoc[i] ? white : goldenrod);
		} else {
			cairo_set_source(c, white);
		}

		cairo_move_to(c,column,row);
		cairo_line_to(c,column+1,row);
		cairo_line_to(c,column+1,row+1);
		cairo_line_to(c,column,row+1);
		cairo_line_to(c,column,row);
		cairo_fill(c);
		if(column < width - strip_width && row > 0) {
			column += strip_width;
			row -= 1;
			cairo_move_to(c,column,row);
			cairo_line_to(c,column+1,row);
			cairo_line_to(c,column+1,row+1);
			cairo_line_to(c,column,row+1);
			cairo_line_to(c,column,row);
			cairo_fill(c);
		}
		if(--i < 0) i = snst->events_count - 1;
		if(i == snst->events_wp) break;
	}

	cairo_set_source(c,white);
	cairo_set_line_width(c,2);
	cairo_move_to(c, left_margin + 3, height - 20.5);
	cairo_line_to(c, right_margin - 3, height - 20.5);
	cairo_stroke(c);
	cairo_set_line_width(c,1);
	cairo_move_to(c, left_margin + .5, height - 20.5);
	cairo_line_to(c, left_margin + 5.5, height - 15.5);
	cairo_line_to(c, left_margin + 5.5, height - 25.5);
	cairo_line_to(c, left_margin + .5, height - 20.5);
	cairo_fill(c);
	cairo_move_to(c, right_margin + .5, height - 20.5);
	cairo_line_to(c, right_margin - 4.5, height - 15.5);
	cairo_line_to(c, right_margin - 4.5, height - 25.5);
	cairo_line_to(c, right_margin + .5, height - 20.5);
	cairo_fill(c);

	char s[100];
	cairo_text_extents_t extents;

	gtk_widget_get_allocation(gtk_widget_get_toplevel(widget), &temp);
	int font = temp.width / 90;
	if(font < 12)
		font = 12;
	cairo_set_font_size(c,font);

	sprintf(s, "%.1f ms", snst->calibrate ?
				1000. / zoom_factor :
				3600000. / (safe_bph * zoom_factor));
	cairo_text_extents(c,s,&extents);
	cairo_move_to(c, (width - extents.x_advance)/2, height - 30);
	cairo_show_text(c,s);

	return FALSE;
}

#ifdef DEBUG
static gboolean debug_draw_event(GtkWidget *widget, cairo_t *c, struct output_panel *op)
{
	UNUSED(widget);
	cairo_init(c);

	struct snapshot *snst = op->snst;
	struct processing_buffers *p;
	if(snst->calibrate)
		p = &op->computer->pdata->buffers[0];
	else
		p = snst->pb;

	if(p) {
		double a = snst->nominal_sr / 10;
		double b = snst->nominal_sr * 2;

		draw_debug_graph(a,b,c,p,op->debug_drawing_area);

		cairo_set_source(c,snst->is_old?yellow:white);
		cairo_stroke(c);
	}

	return FALSE;
}
#endif

static void handle_clear_trace(GtkButton *b, struct output_panel *op)
{
	UNUSED(b);
	if(op->computer) {
		lock_computer(op->computer);
		if(!op->snst->calibrate) {
			memset(op->snst->events,0,op->snst->events_count*sizeof(uint64_t));
			memset(op->snst->events_tictoc,0,op->snst->events_count*sizeof(unsigned char));
			memset(op->snst->amps,0,op->snst->amps_count*sizeof(*op->snst->amps));
			memset(op->snst->amps_time,0,op->snst->amps_count*sizeof(*op->snst->amps_time));
			op->snst->trace_zoom = 1.0;
			op->computer->clear_trace = 1;
		}
		unlock_computer(op->computer);
		gtk_widget_queue_draw(op->paperstrip_drawing_area);
	}
}

void handle_clear_stats(GtkButton *b, struct output_panel *op)
{
	UNUSED(b);
	if(op->computer) {
		lock_computer(op->computer);
		if(!op->snst->calibrate) {
			memset(op->snst->events,0,op->snst->events_count*sizeof(uint64_t));
			memset(op->snst->events_tictoc,0,op->snst->events_count*sizeof(unsigned char));
			memset(op->snst->amps,0,op->snst->amps_count*sizeof(*op->snst->amps));
			memset(op->snst->amps_time,0,op->snst->amps_count*sizeof(*op->snst->amps_time));
			op->snst->trace_zoom = 1.0;
			op->computer->clear_trace = 1;
		}
		unlock_computer(op->computer);
		gtk_widget_queue_draw(op->paperstrip_drawing_area);
	}
}

static void handle_center_trace(GtkButton *b, struct output_panel *op)
{
	UNUSED(b);
	struct snapshot *snst = op->snst;
	if(!snst || !snst->events || snst->events_count <= 0)
		return;
	if(snst->events_wp < 0 || snst->events_wp >= snst->events_count)
		return;
	uint64_t last_ev = snst->events[snst->events_wp];
	double new_centering;
	if(last_ev) {
		double sweep;
		double zoom_factor;
		double time = snst->timestamp ? snst->timestamp : get_timestamp(snst->is_light);
		double now;
		double chart_width;
		if(snst->calibrate) {
			sweep = (double) snst->nominal_sr;
			zoom_factor = PAPERSTRIP_ZOOM_CAL * (snst->trace_zoom > 0 ? snst->trace_zoom : 1.0);
		} else {
			if(snst->sample_rate <= 0 || snst->guessed_bph <= 0)
				return;
			sweep = snst->sample_rate * 3600. / snst->guessed_bph;
			zoom_factor = PAPERSTRIP_ZOOM * (snst->trace_zoom > 0 ? snst->trace_zoom : 1.0);
		}
		if(!isfinite(sweep) || sweep <= 0 || !isfinite(zoom_factor) || zoom_factor <= 0)
			return;
		now = sweep * ceil(time / sweep);
		chart_width = sweep / zoom_factor;
		if(!isfinite(now) || !isfinite(chart_width) || chart_width <= 0)
			return;
		new_centering = fmod(0.5 * chart_width - fmod(now - last_ev, chart_width), chart_width);
		if(new_centering < 0)
			new_centering += chart_width;
	} else 
		new_centering = 0;
	if(!isfinite(new_centering))
		return;
	snst->trace_centering = new_centering;
	gtk_widget_queue_draw(op->paperstrip_drawing_area);
}

static void shift_trace(struct output_panel *op, double direction)
{
	struct snapshot *snst = op->snst;
	if(!snst)
		return;
	double chart_width;
	double zoom_factor;
	if(snst->calibrate) {
		chart_width = (double) snst->nominal_sr;
		zoom_factor = PAPERSTRIP_ZOOM_CAL * (snst->trace_zoom > 0 ? snst->trace_zoom : 1.0);
	} else {
		if(snst->sample_rate <= 0 || snst->guessed_bph <= 0)
			return;
		chart_width = snst->sample_rate * 3600. / snst->guessed_bph;
		zoom_factor = PAPERSTRIP_ZOOM * (snst->trace_zoom > 0 ? snst->trace_zoom : 1.0);
	}
	if(!isfinite(chart_width) || chart_width <= 0 || !isfinite(zoom_factor) || zoom_factor <= 0)
		return;
	chart_width /= zoom_factor;
	if(!isfinite(chart_width) || chart_width <= 0)
		return;
	snst->trace_centering = fmod(snst->trace_centering + chart_width * 0.1 * direction, chart_width);
	if(snst->trace_centering < 0)
		snst->trace_centering += chart_width;
	gtk_widget_queue_draw(op->paperstrip_drawing_area);
}

static void zoom_trace(struct output_panel *op, double factor)
{
	struct snapshot *snst = op->snst;
	if(!snst)
		return;
	if(snst->trace_zoom <= 0)
		snst->trace_zoom = 1.0;
	snst->trace_zoom *= factor;
	if(snst->trace_zoom < 0.25)
		snst->trace_zoom = 0.25;
	if(snst->trace_zoom > 8.0)
		snst->trace_zoom = 8.0;
	gtk_widget_queue_draw(op->paperstrip_drawing_area);
}

static void handle_zoom_out(GtkButton *b, struct output_panel *op)
{
	UNUSED(b);
	zoom_trace(op, 1.0 / 1.25);
}

static void handle_zoom_in(GtkButton *b, struct output_panel *op)
{
	UNUSED(b);
	zoom_trace(op, 1.25);
}

static void handle_zoom_reset(GtkButton *b, struct output_panel *op)
{
	UNUSED(b);
	if(!op->snst)
		return;
	op->snst->trace_zoom = 1.0;
	gtk_widget_queue_draw(op->paperstrip_drawing_area);
}

static void handle_left(GtkButton *b, struct output_panel *op)
{
	UNUSED(b);
	shift_trace(op,-1);
}

static void handle_right(GtkButton *b, struct output_panel *op)
{
	UNUSED(b);
	shift_trace(op,1);
}

void op_set_snapshot(struct output_panel *op, struct snapshot *snst)
{
	op->snst = snst;
	if(op->snst && op->snst->trace_zoom <= 0)
		op->snst->trace_zoom = 1.0;
	gtk_widget_set_sensitive(op->clear_button, !snst->calibrate);
}

void op_set_border(struct output_panel *op, int i)
{
	gtk_container_set_border_width(GTK_CONTAINER(op->panel), i);
}

void op_destroy(struct output_panel *op)
{
	snapshot_destroy(op->snst);
	free(op);
}

// Right-click context menu for Save As Image
static void handle_save_as_image(GtkMenuItem *item, struct output_panel *op)
{
	UNUSED(item);
	if (!op || !op->snst || !op->snst->pb)
		return;

	/* Switch to the tab being screenshotted */
	GtkWidget *notebook = gtk_widget_get_parent(op->panel);
	if(notebook) {
		gint page = gtk_notebook_page_num(GTK_NOTEBOOK(notebook), op->panel);
		if(page >= 0)
			gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), page);
	}

	// Get the selected orientation from the menu item's data
	const char *orientation = g_object_get_data(G_OBJECT(item), "orientation");
	if (!orientation)
		return;

	// Generate filename: orientation + snapshot timestamp
	uint64_t snap_ts = op->snst->timestamp;
	time_t now = (time_t)(snap_ts / 1000000);  // timestamp is in microseconds
	struct tm *tm_info = localtime(&now);
	char timestamp[32];
	strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", tm_info);

	char filename[512];
	snprintf(filename, sizeof(filename), "%s_%s.png", orientation, timestamp);

	// Create a file chooser dialog
	GtkWidget *dialog = gtk_file_chooser_dialog_new("Save Snapshot As",
		GTK_WINDOW(gtk_widget_get_toplevel(op->panel)),
		GTK_FILE_CHOOSER_ACTION_SAVE,
		"_Cancel", GTK_RESPONSE_CANCEL,
		"_Save", GTK_RESPONSE_ACCEPT,
		NULL);

	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), filename);
	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

	GtkFileFilter *png_filter = gtk_file_filter_new();
	gtk_file_filter_set_name(png_filter, "PNG Image");
	gtk_file_filter_add_pattern(png_filter, "*.png");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), png_filter);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		char *chosen_filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		if (chosen_filename) {
			// Capture the FULL toplevel window (includes tabs, menus, all panels)
			GtkWidget *toplevel = gtk_widget_get_toplevel(op->panel);
			GdkWindow *window = gtk_widget_get_window(toplevel);
			if (window) {
				int x, y, width, height;
				gdk_window_get_geometry(window, &x, &y, &width, &height);
				
				cairo_surface_t *surface = gdk_window_create_similar_surface(window,
					CAIRO_CONTENT_COLOR, width, height);
				cairo_t *cr = cairo_create(surface);
				
				// Render the entire window
				gdk_cairo_set_source_window(cr, window, 0, 0);
				cairo_paint(cr);
				
				cairo_surface_write_to_png(surface, chosen_filename);
				
				cairo_destroy(cr);
				cairo_surface_destroy(surface);
			}

			/* Rename the tab to the saved filename (strip path + .png) */
			char *bn = g_path_get_basename(chosen_filename);
			if(bn) {
				/* Strip trailing ".png" if present */
				size_t len = strlen(bn);
				if(len > 4 && !g_ascii_strcasecmp(bn + len - 4, ".png"))
					bn[len - 4] = '\0';

				free(g_object_get_data(G_OBJECT(op->panel), "tab-name"));
				g_object_set_data(G_OBJECT(op->panel), "tab-name", strdup(bn));
				GtkLabel *label = g_object_get_data(G_OBJECT(op->panel), "tab-label");
				if(label)
					gtk_label_set_text(label, bn);

				/* Update the snapshot name entry field if it's the current tab */
				GtkWidget *toplevel = gtk_widget_get_toplevel(op->panel);
				GtkWidget *snapshot_name_entry = g_object_get_data(G_OBJECT(toplevel), "snapshot-name-entry");
				if(snapshot_name_entry)
					gtk_entry_set_text(GTK_ENTRY(snapshot_name_entry), bn);

				g_free(bn);
			}
			
			g_free(chosen_filename);
		}
	}
	gtk_widget_destroy(dialog);
}

static gboolean handle_panel_button_press(GtkWidget *widget, GdkEventButton *event, struct output_panel *op)
{
	UNUSED(widget);
	if (event->type == GDK_BUTTON_PRESS && event->button == 3) { // Right click
		GtkWidget *menu = gtk_menu_new();

		const char *orientations[] = {
			"Dial Up", "Dial Down", "12PM Down", 
			"3PM Down", "6PM Down", "9PM Down"
		};

		for (int i = 0; i < 6; i++) {
			GtkWidget *item = gtk_menu_item_new_with_label(orientations[i]);
			g_object_set_data_full(G_OBJECT(item), "orientation", g_strdup(orientations[i]), g_free);
			g_signal_connect(item, "activate", G_CALLBACK(handle_save_as_image), op);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		}

		gtk_widget_show_all(menu);
		gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
		return TRUE;
	}
	return FALSE;
}

struct output_panel *init_output_panel(struct computer *comp, struct snapshot *snst, int border)
{
	struct output_panel *op = malloc(sizeof(struct output_panel));

	op->computer = comp;
	op->snst = snst;

	op->panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_container_set_border_width(GTK_CONTAINER(op->panel), border);

	// Info area on top
	op->output_drawing_area = gtk_drawing_area_new();
	gtk_widget_set_size_request(op->output_drawing_area, 0, OUTPUT_WINDOW_HEIGHT);
	gtk_box_pack_start(GTK_BOX(op->panel),op->output_drawing_area, FALSE, TRUE, 0);
	g_signal_connect (op->output_drawing_area, "draw", G_CALLBACK(output_draw_event), op);
	gtk_widget_set_events(op->output_drawing_area, GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_BUTTON_PRESS_MASK);
	gtk_widget_add_events(op->output_drawing_area, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(op->output_drawing_area, "query-tooltip", G_CALLBACK(output_query_tooltip), op);
	gtk_widget_set_has_tooltip(op->output_drawing_area, TRUE);

	GtkWidget *hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_box_pack_start(GTK_BOX(op->panel), hbox2, TRUE, TRUE, 0);

	GtkWidget *vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_box_pack_start(GTK_BOX(hbox2), vbox2, FALSE, TRUE, 0);

	// Paperstrip
	GtkWidget *paperstrip_title = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(paperstrip_title), "<b><span color='black' size='18000'>Paperstrip</span></b>");
	gtk_box_pack_start(GTK_BOX(vbox2), paperstrip_title, FALSE, FALSE, 0);
	op->paperstrip_drawing_area = gtk_drawing_area_new();
	gtk_widget_set_size_request(op->paperstrip_drawing_area, 300, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), op->paperstrip_drawing_area, TRUE, TRUE, 0);
	g_signal_connect (op->paperstrip_drawing_area, "draw", G_CALLBACK(paperstrip_draw_event), op);
	gtk_widget_set_events(op->paperstrip_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_add_events(op->paperstrip_drawing_area, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(op->paperstrip_drawing_area, "button-press-event", G_CALLBACK(handle_panel_button_press), op);

	GtkWidget *hbox3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox3, FALSE, TRUE, 0);

	// < button
	GtkWidget *left_button = gtk_button_new_with_label("<");
	gtk_box_pack_start(GTK_BOX(hbox3), left_button, TRUE, TRUE, 0);
	g_signal_connect (left_button, "clicked", G_CALLBACK(handle_left), op);

	// CLEAR button
	if(comp) {
		op->clear_button = gtk_button_new_with_label("Clear");
		gtk_box_pack_start(GTK_BOX(hbox3), op->clear_button, TRUE, TRUE, 0);
		g_signal_connect (op->clear_button, "clicked", G_CALLBACK(handle_clear_trace), op);
		gtk_widget_set_sensitive(op->clear_button, !snst->calibrate);
	}

	// CENTER button
	GtkWidget *center_button = gtk_button_new_with_label("Center");
	gtk_box_pack_start(GTK_BOX(hbox3), center_button, TRUE, TRUE, 0);
	g_signal_connect (center_button, "clicked", G_CALLBACK(handle_center_trace), op);

	GtkWidget *zoom_out_button = gtk_button_new_with_label("-");
	gtk_box_pack_start(GTK_BOX(hbox3), zoom_out_button, TRUE, TRUE, 0);
	g_signal_connect (zoom_out_button, "clicked", G_CALLBACK(handle_zoom_out), op);

	GtkWidget *zoom_reset_button = gtk_button_new_with_label("1x");
	gtk_box_pack_start(GTK_BOX(hbox3), zoom_reset_button, TRUE, TRUE, 0);
	g_signal_connect (zoom_reset_button, "clicked", G_CALLBACK(handle_zoom_reset), op);

	GtkWidget *zoom_in_button = gtk_button_new_with_label("+");
	gtk_box_pack_start(GTK_BOX(hbox3), zoom_in_button, TRUE, TRUE, 0);
	g_signal_connect (zoom_in_button, "clicked", G_CALLBACK(handle_zoom_in), op);

	// > button
	GtkWidget *right_button = gtk_button_new_with_label(">");
	gtk_box_pack_start(GTK_BOX(hbox3), right_button, TRUE, TRUE, 0);
	g_signal_connect (right_button, "clicked", G_CALLBACK(handle_right), op);

	GtkWidget *vbox3 = gtk_box_new(GTK_ORIENTATION_VERTICAL,10);
	gtk_box_pack_start(GTK_BOX(hbox2), vbox3, TRUE, TRUE, 0);

	// Tic waveform area
	GtkWidget *tic_title = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(tic_title), "<b><span color='black' size='18000'>Tic Waveform</span></b>");
	gtk_box_pack_start(GTK_BOX(vbox3), tic_title, FALSE, FALSE, 0);
	op->tic_drawing_area = gtk_drawing_area_new();
	gtk_box_pack_start(GTK_BOX(vbox3), op->tic_drawing_area, TRUE, TRUE, 0);
	g_signal_connect (op->tic_drawing_area, "draw", G_CALLBACK(tic_draw_event), op);
	gtk_widget_set_events(op->tic_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_add_events(op->tic_drawing_area, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(op->tic_drawing_area, "button-press-event", G_CALLBACK(handle_panel_button_press), op);

	// Toc waveform area
	GtkWidget *toc_title = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(toc_title), "<b><span color='black' size='18000'>Toc Waveform</span></b>");
	gtk_box_pack_start(GTK_BOX(vbox3), toc_title, FALSE, FALSE, 0);
	op->toc_drawing_area = gtk_drawing_area_new();
	gtk_box_pack_start(GTK_BOX(vbox3), op->toc_drawing_area, TRUE, TRUE, 0);
	g_signal_connect (op->toc_drawing_area, "draw", G_CALLBACK(toc_draw_event), op);
	gtk_widget_set_events(op->toc_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_add_events(op->toc_drawing_area, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(op->toc_drawing_area, "button-press-event", G_CALLBACK(handle_panel_button_press), op);

	// Period waveform area
	GtkWidget *period_title = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(period_title), "<b><span color='black' size='18000'>Period Waveform</span></b>");
	gtk_box_pack_start(GTK_BOX(vbox3), period_title, FALSE, FALSE, 0);
	op->period_drawing_area = gtk_drawing_area_new();
	gtk_box_pack_start(GTK_BOX(vbox3), op->period_drawing_area, TRUE, TRUE, 0);
	g_signal_connect (op->period_drawing_area, "draw", G_CALLBACK(period_draw_event), op);
	gtk_widget_set_events(op->period_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_add_events(op->period_drawing_area, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(op->period_drawing_area, "button-press-event", G_CALLBACK(handle_panel_button_press), op);

#ifdef DEBUG
	op->debug_drawing_area = gtk_drawing_area_new();
	gtk_box_pack_start(GTK_BOX(vbox3), op->debug_drawing_area, TRUE, TRUE, 0);
	g_signal_connect (op->debug_drawing_area, "draw", G_CALLBACK(debug_draw_event), op);
	gtk_widget_set_events(op->debug_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_add_events(op->debug_drawing_area, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(op->debug_drawing_area, "button-press-event", G_CALLBACK(handle_panel_button_press), op);
#endif

	// Right-click context menu for Save As (on all drawing areas)
	g_signal_connect(op->output_drawing_area, "button-press-event", G_CALLBACK(handle_panel_button_press), op);

	return op;
}
