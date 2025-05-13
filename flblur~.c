#include "flblur~.h"

void ext_main(void *r)
{
	t_class *c = class_new("flblur~", (method)fl_blur_new, (method)fl_blur_free, sizeof(t_fl_blur), 0, A_GIMME, 0);

	class_addmethod(c, (method)fl_blur_dsp64, "dsp64", A_CANT, 0);
	class_addmethod(c, (method)fl_blur_assist, "assist", A_CANT, 0);

	class_addmethod(c, (method)fl_blur_brange, "range", A_GIMME, 0);
	class_addmethod(c, (method)fl_blur_wmake, "mode", A_GIMME, 0);

	class_addmethod(c, (method)fl_blur_float, "float", A_FLOAT, 0);
	class_addmethod(c, (method)fl_blur_int, "int", A_LONG, 0);
	class_addmethod(c, (method)fl_blur_bang, "bang", 0);

	class_dspinit(c);

	class_register(CLASS_BOX, fl_blur_class);
	fl_blur_class = c;

	ps_spfft = gensym("__pfft~__");
	blur_warning = 1;
}

void *fl_blur_new(t_symbol *s, short argc, t_atom *argv)
{
	t_fl_blur *x = (t_fl_blur *)object_alloc(fl_blur_class);

	dsp_setup((t_pxobject *)x, NUM_INLETS);
	outlet_new((t_object *)x, "signal");
	outlet_new((t_object *)x, "signal");
	x->obj.z_misc |= Z_NO_INPLACE;

	x->i_now = 0;
	x->brange = DEFAULT_BIN_RANGE;
	x->bmode = DEFAULT_BLUR_MODE;

	x->x_n = DEFAULT_FRAMESIZE;

	x->wei_length = DEFAULT_WEIGHT_LENGTH;
	x->wei_buffer = (double *)sysmem_newptr(DEFAULT_WEIGHT_LENGTH * sizeof(double));
	if (x->wei_buffer) {
		for (long i = 0; i < DEFAULT_WEIGHT_LENGTH; i++) {
			x->wei_buffer[i] = exp(-pow((i / (double)(DEFAULT_WEIGHT_LENGTH - 1.0)) - 0.5, 2.0) / 0.04);
		}
	}
	else { object_error((t_object *)x, "no memory for weight buffer"); }

	fl_blur_init_memory(x);

	return x;
}

void fl_blur_init_memory(t_fl_blur *x)
{
	long framesize = x->x_n;

	if (framesize <= 0) { object_error((t_object *)x, "bad frame size: %d", framesize); return; }
	//object_post((t_object *)x, "frame size: %d", framesize);

	if (!x->p_amp_buffer) { 
		x->p_amp_buffer = (double **)sysmem_newptr(2 * sizeof(double *)); 
		//if(!x->p_a_buffer){ object_error((t_object *)x, "no memory for p_a_buffer"); return; } //necessary?
		x->p_amp_buffer[0] = (double *)sysmem_newptr(framesize * sizeof(double));
		x->p_amp_buffer[1] = (double *)sysmem_newptr(framesize * sizeof(double));
	}
	else {
		if (!x->p_amp_buffer[0]) { x->p_amp_buffer[0] = (double *)sysmem_newptr(framesize * sizeof(double)); }
		else{ x->p_amp_buffer[0] = (double *)sysmem_resizeptr(x->p_amp_buffer[0], framesize * sizeof(double)); }
		if (!x->p_amp_buffer[1]) { x->p_amp_buffer[1] = (double *)sysmem_newptr(framesize * sizeof(double)); }
		else { x->p_amp_buffer[1] = (double *)sysmem_resizeptr(x->p_amp_buffer[1], framesize * sizeof(double)); }
	}

	if (!x->p_pha_buffer) {
		x->p_pha_buffer = (double **)sysmem_newptr(2 * sizeof(double *));
		x->p_pha_buffer[0] = (double *)sysmem_newptr(framesize * sizeof(double));
		x->p_pha_buffer[1] = (double *)sysmem_newptr(framesize * sizeof(double));
	}
	else {
		if (!x->p_pha_buffer[0]) { x->p_pha_buffer[0] = (double *)sysmem_newptr(framesize * sizeof(double)); }
		else { x->p_pha_buffer[0] = (double *)sysmem_resizeptr(x->p_pha_buffer[0], framesize * sizeof(double)); }
		if (!x->p_pha_buffer[1]) { x->p_pha_buffer[1] = (double *)sysmem_newptr(framesize * sizeof(double)); }
		else { x->p_pha_buffer[1] = (double *)sysmem_resizeptr(x->p_pha_buffer[1], framesize * sizeof(double)); }
	}

	for (long i = 0; i < framesize; i++) {
		x->p_amp_buffer[0][i] = 0.0;
		x->p_amp_buffer[1][i] = 0.0;
		x->p_pha_buffer[0][i] = 0.0;
		x->p_pha_buffer[1][i] = 0.0;
	}
}

void fl_blur_bang(t_fl_blur *x) {}

void fl_blur_int(t_fl_blur *x, long n) {}

void fl_blur_float(t_fl_blur *x, double farg) {}

void fl_blur_assist(t_fl_blur *x, void *b, long msg, long arg, char *dst)
{
	if (msg == ASSIST_INLET) {
		switch (arg) {
		case I_AMP_INPUT: sprintf(dst, "(sig~) amp input"); break;
		case I_PHA_INPUT: sprintf(dst, "(sig~) pha input"); break;
		}
	}
	else if (msg == ASSIST_OUTLET) {
		switch (arg) {
		case O_AMP_OUTPUT: sprintf(dst, "(sig~) amp output"); break;
		case O_PHA_OUTPUT: sprintf(dst, "(sig~) pha output"); break;
		}
	}
}

void fl_blur_brange(t_fl_blur *x, t_symbol *msg, short argc, t_atom *argv) 
{
	t_atom *ap = argv;
	long ac = argc;
	long range;

	if (ac != 1) { object_error((t_object *)x, "range: only 1 argument"); return; }
	if (atom_gettype(ap) != A_FLOAT && atom_gettype(ap) != A_LONG) { object_error((t_object *)x, "range: argument must be a number"); return; }

	range = (long)atom_getlong(ap);
	if (range < 0) { object_warn((t_object *)x, "range: argument must be equal or more than 0"); return; }

	x->brange = range;
}

void fl_blur_wmake(t_fl_blur *x, t_symbol *msg, short argc, t_atom *argv)
{
	t_atom *ap = argv;
	long ac = argc;
	long mode;
	double xi;
	double val;
	double *p_wbuffer = x->wei_buffer;
	long wsize = x->wei_length;

	if (ac != 1) { object_error((t_object *)x, "mode: only 1 argument"); return; }
	if (atom_gettype(ap) != A_FLOAT && atom_gettype(ap) != A_LONG) { object_error((t_object *)x, "mode: argument must be a number"); return; }

	mode = (long)atom_getlong(ap);
	if (mode < BMODE_GAUSSIAN || mode > BMODE_POW) { object_warn((t_object *)x, "mode: argument must be equal or more than 0 and less than 5"); return; }

	switch (mode) {
	case BMODE_GAUSSIAN:
		for (long i = 0; i < wsize; i++) {
			xi = i / (double)(wsize - 1.0);
			val = exp( - pow(xi - 0.5, 2) / 0.04);
			p_wbuffer[i] = val;
		}
		break;
	case BMODE_TRIANGLE:
		for (long i = 0; i < wsize; i++) {
			xi = i / (double)(wsize - 1.0);
			val = 1.0 - fabs(1.0 - 2.0 * xi);
			p_wbuffer[i] = val;
		}
		break;
	case BMODE_COS:
		for (long i = 0; i < wsize; i++) {
			xi = i / (double)(wsize - 1.0);
			val = 0.5 + 0.5 * cos(PI * (1.0 + 2.0 * xi));
			p_wbuffer[i] = val;
		}
		break;
	case BMODE_HALFSIN:
		for (long i = 0; i < wsize; i++) {
			xi = i / (double)(wsize - 1.0);
			val = sin(PI * xi);
			p_wbuffer[i] = val;
		}
		break;
	case BMODE_POW:
		for (long i = 0; i < wsize; i++) {
			xi = i / (double)(wsize - 1.0);
			val = 1.0 - pow(1.0 - 2.0 * xi, 4.0);
			p_wbuffer[i] = val;
		}
		break;
	default:
		return;
	}

	x->bmode = (short)mode;
}

void fl_blur_free(t_fl_blur *x)
{
	dsp_free((t_pxobject *)x);

	if (x->p_amp_buffer) {
		if (x->p_amp_buffer[0]) { sysmem_freeptr(x->p_amp_buffer[0]); }
		if (x->p_amp_buffer[1]) { sysmem_freeptr(x->p_amp_buffer[1]); }
		sysmem_freeptr(x->p_amp_buffer);
	}
	if (x->p_pha_buffer) {
		if (x->p_pha_buffer[0]) { sysmem_freeptr(x->p_pha_buffer[0]); }
		if (x->p_pha_buffer[1]) { sysmem_freeptr(x->p_pha_buffer[1]); }
		sysmem_freeptr(x->p_pha_buffer);
	}
	sysmem_freeptr(x->wei_buffer);
}

void fl_blur_dsp64(t_fl_blur *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
	x->amp_connected = count[0];
	x->pha_connected = count[1];

	if (!x->x_pfft) {
		x->x_pfft = (t_pfftpub *)ps_spfft->s_thing;
	}
	if (x->x_pfft) {
		//x->x_fullspect = x->x_pfft->x_fullspect ? 1 : 0;
		//x->x_ffthop = x->x_pfft->x_ffthop;
		x->x_n = maxvectorsize;
		//x->x_fftsize = x->x_pfft->x_fftsize;

		fl_blur_init_memory(x);
	}
	else if (blur_warning) {
		object_warn((t_object *)x, "flblur~ only functions inside a pfft~", 0);
		blur_warning = 0;
	}

	if (x->x_n != maxvectorsize) {
		x->x_n = maxvectorsize;
		fl_blur_init_memory(x);
	}

	object_method(dsp64, gensym("dsp_add64"), x, fl_blur_perform64, 0, NULL);
}

void fl_blur_perform64(t_fl_blur *x, t_object *dsp64, double **inputs, long numinputs, double **outputs, long numoutputs, long vectorsize, long flags, void *userparams)
{
	long n = vectorsize;

	t_double *amp_in = inputs[0];
	t_double *pha_in = inputs[1];
	t_double *amp_out = outputs[0];
	t_double *pha_out = outputs[1];

	long j;
	float rat_weight;
	float f_weight;
	long i_weight;
	long next_i_weight;
	long max_i_weight = x->wei_length - 1;
	long r = x->brange;
	float interp;
	double w_val;

	double amp_val;
	double pha_val;

	long i_past = x->i_now;
	long i_now = (i_past + 1) % 2;

	long max_i_frame = n - 1;

	short amp_connected = x->amp_connected;
	short pha_connected = x->pha_connected;
	
	double *amp_now = x->p_amp_buffer[i_now];
	double *amp_past = x->p_amp_buffer[i_past];
	double *pha_now = x->p_pha_buffer[i_now];
	double *pha_past = x->p_pha_buffer[i_past];
	double *weight = x->wei_buffer;	

	while (n--) {
		
		if (amp_connected) { amp_val = (double)(*amp_in++); }
		else { amp_val = 0.0; }
		if (pha_connected) { pha_val = (double)(*pha_in++); }
		else { pha_val = 0.0; }

		//store the phase vector (unaffected phase version)
		//pha_now[n] = pha_val;

		//process input, store it to retrieve it next frame
		for (long i = -r; i <= r; i++) {
			j = n + i;
			if (j < 0) { continue; }
			if (j > max_i_frame) { continue; }

			rat_weight = (float)((i + MAX(r, 1.0)) / (2.0 * MAX(r, 1.0)));
			f_weight = max_i_weight * rat_weight;
			i_weight = (long)trunc(f_weight);
			if (i_weight < 0) { i_weight = 0; }
			interp = f_weight - i_weight;

			next_i_weight = i_weight + 1;
			if (next_i_weight > max_i_weight) { next_i_weight = max_i_weight; }
			w_val = weight[next_i_weight] + interp * (weight[i_weight] - weight[next_i_weight]);
			
			amp_now[j] += amp_val * w_val;
			pha_now[j] += pha_val * w_val; //processed phase version
		}
		//retrieve last frame
		amp_val = amp_past[n];
		pha_val = pha_past[n];

		//output
		*amp_out++ = amp_val;
		*pha_out++ = pha_val;

		//reset buffer to accumulate the sum the next frame
		amp_past[n] = 0.0;
		pha_past[n] = 0.0;
	}

	x->i_now = i_now;
}