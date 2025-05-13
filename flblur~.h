#ifndef fl_blur_h
#define fl_blur_h

#include "ext.h"
#include "z_dsp.h"
#include "ext_obex.h"
#include "r_pfft.h"

#define DEFAULT_BIN_RANGE 0
#define DEFAULT_BLUR_MODE BMODE_GAUSSIAN
#define DEFAULT_WEIGHT_LENGTH 1024
#define DEFAULT_FRAMESIZE 256

typedef struct _fl_blur {
	t_pxobject obj;
	t_pfftpub *x_pfft;

	//long x_fftsize;
	//long x_ffthop;
	long x_n;
	//int x_fullspect;

	long brange;
	short bmode;

	long i_now;
	double **p_amp_buffer;
	double **p_pha_buffer;
	
	long wei_length;
	double *wei_buffer;

	short amp_connected;
	short pha_connected;

} t_fl_blur;

enum BMODES { BMODE_GAUSSIAN, BMODE_TRIANGLE, BMODE_COS, BMODE_HALFSIN, BMODE_POW };
enum INLETS { I_AMP_INPUT, I_PHA_INPUT, NUM_INLETS };
enum OUTLETS { O_AMP_OUTPUT, O_PHA_OUTPUT, NUM_OUTLETS };

t_symbol *ps_spfft;
static t_class *fl_blur_class;
int blur_warning;

void *fl_blur_new(t_symbol *s, short argc, t_atom *argv);
void fl_blur_int(t_fl_blur *x, long n);
void fl_blur_float(t_fl_blur *x, double f);
void fl_blur_bang(t_fl_blur *x);

void fl_blur_init_memory(t_fl_blur *x);
void fl_blur_brange(t_fl_blur *x, t_symbol *msg, short argc, t_atom *argv);
void fl_blur_wmake(t_fl_blur *x, t_symbol *msg, short argc, t_atom *argv);

void fl_blur_assist(t_fl_blur *x, void *b, long msg, long arg, char *dst);
void fl_blur_free(t_fl_blur *x);
void fl_blur_dsp64(t_fl_blur *x, t_object *dsp64, short *count, double samplerate, long maxvectorsize, long flags);
void fl_blur_perform64(t_fl_blur *x, t_object *dsp64, double **inputs, long numinputs, double **outputs, long numoutputs, long vectorsize, long flags, void *userparams);

#endif