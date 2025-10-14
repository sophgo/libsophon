#include "bmcv_api_ext_c.h"
#include "sys/time.h"
#include <stdlib.h>
#include <float.h>
#include "bmlib_runtime.h"

#include "string.h"
#include <math.h>
#include <stdio.h>
#include <math.h>
#include "bmcv_common_bm1684.h"
#include "bmcv_bm1684x.h"
#include "bmcv_internal.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

typedef struct {
	int **response_filter;
} rl_Kernel;

typedef struct {
	rl_Kernel kernels[3];
} rl_Kernel_Layer;

typedef struct
{
	Ipoint *pt1, *pt2;
	int size;
	int count;
} IpointPair;

typedef struct {
    double **integralImage;
    int width;
    int height;
} Image;

void build_boxfilter(int filter, rl_Kernel_Layer *myBoxFilter) {
	for (int D = 0; D < 3; D++) {
		myBoxFilter->kernels[D].response_filter = (int**)malloc(filter * sizeof(int *));
	}
	for (int j = 0; j < filter; j++) {
		for (int D = 0; D < 3; D++) {
				myBoxFilter->kernels[D].response_filter[j] = (int*)malloc(filter * sizeof(int *));
		}
		for (int k = 0; k < filter; k++) {
			if (j >= (filter / 3 + 1)/2 && j < (filter * 5/3 - 1)/2 && k >= filter / 3 && k < filter * 2/3) {
				myBoxFilter->kernels[0].response_filter[j][k] = -2;
			} else if (j >= (filter/3 + 1)/2 && j < (filter *5/3 - 1)/2) {
				myBoxFilter->kernels[0].response_filter[j][k] = 1;
			} else {
				myBoxFilter->kernels[0].response_filter[j][k] = 0;
			}
			if (j >= filter / 3 && j < filter * 2/3 && k >= (filter/3 + 1)/2 && k < (filter*5/3 - 1)/2) {
				myBoxFilter->kernels[1].response_filter[j][k] = -2;
			} else if (k >= (filter/3 + 1)/2 && k < (filter * 5/3 - 1)/2) {
				myBoxFilter->kernels[1].response_filter[j][k] = 1;
			} else {
				myBoxFilter->kernels[1].response_filter[j][k] = 0;
			}
			if ((j >= (filter/3 - 1)/2 && j < (filter - 1)/2 && k >= (filter/3 - 1)/2 && k < (filter - 1)/2) || (j >=  (filter + 1)/2 && j < (filter*5/3 + 1)/2 && k >= (filter + 1)/2 && k < (filter*5/3 + 1)/2)) {
				myBoxFilter->kernels[2].response_filter[j][k] = -1;
			} else if ((j >= (filter/3 - 1)/2 && j < (filter - 1)/2 && k >= (filter + 1)/2 && k < (filter *5/3 + 1)/2) || (j >=  (filter + 1)/2 && j < (filter*5/3 + 1)/2 && k >= (filter/3 - 1)/2 && k < (filter - 1)/2)) {
				myBoxFilter->kernels[2].response_filter[j][k] = 1;
			} else {
				myBoxFilter->kernels[2].response_filter[j][k] = 0;
			}
		}
	}
}

void get_surf_kernel(rl_Kernel_Layer kernel, float* D_kernel, int ksize) {
	int index = 0;
	for (int k = 0; k < 3; k++) {
		for (int i = 0; i < ksize; i++) {
			for (int j = 0; j < ksize; j++) {
				D_kernel[index] = kernel.kernels[k].response_filter[i][j];
				index++;
			}
		}
	}
}

static bm_status_t bmcv_surf_check_bm1684x(
        bm_handle_t handle,
        int width,
        int layer_num) {
  if (handle == NULL) {
    bmlib_log("SURF", BMLIB_LOG_ERROR, "Can not get handle!\r\n");
    return BM_ERR_DEVNOTREADY;
  }
  if (width > 1059) {
    bmlib_log("SURF", BMLIB_LOG_ERROR, "The width must no larger than 1059\n");
    return BM_ERR_PARAM;
  }
  if (layer_num > 4) {
    bmlib_log("SURF", BMLIB_LOG_ERROR, "The layer_num must no larger than 4\n");
    return BM_ERR_PARAM;
  }
  return BM_SUCCESS;
}

void Integral(float *img, float *integralImg, int width, int height)
{
	float rs = 0.0f;
	for (int j = 0; j < width; j++)
	{
		rs += img[j];
		integralImg[j] = rs;
	}
	for (int i = 1; i < height; ++i)
	{
		rs = 0.0f;
		for (int j = 0; j < width; ++j)
		{
			rs += img[i*width + j];
			integralImg[i*width + j] = rs + integralImg[(i - 1)*width + j];
		}
	}
}


float getResponse(unsigned int row, unsigned int column, ResponseLayer *src, ResponseLayer *R)
{
	int scale = R->width / src->width;
	return R->responses[(scale * row) * R->width + (scale * column)];
}

float get_Response(unsigned int row, unsigned int column, ResponseLayer *R)
{
	return R->responses[row * R->width + column];
}

int isExtremum(int r, int c, ResponseLayer *t, ResponseLayer *m, ResponseLayer *b, FastHessian* fh)
{
	int rr, cc;
	int layerBorder = (t->filter + 1) / (t->step << 1);
	if (r <= layerBorder || r >= t->height - layerBorder || c <= layerBorder || c >= t->width - layerBorder)
		return 0;
	float candidate = getResponse(r, c, t, m);
	if (candidate < fh->thresh)
		return 0;
	for (rr = -1; rr <= 1; ++rr)
	{
		for (cc = -1; cc <= 1; ++cc)
		{
			if (get_Response(r + rr, c + cc, t) >= candidate || ((rr != 0 || cc != 0) && getResponse(r + rr, c + cc, t, m) >= candidate) || getResponse(r + rr, c + cc, t, b) >= candidate)
				return 0;
		}
	}
	return 1;
}

void deriv3D(int r, int c, ResponseLayer *t, ResponseLayer *m, ResponseLayer *b, double *dI)
{
	dI[0] = (getResponse(r, c + 1, t, m) - getResponse(r, c - 1, t, m)) / 2.0;
	dI[1] = (getResponse(r + 1, c, t, m) - getResponse(r - 1, c, t, m)) / 2.0;
	dI[2] = (get_Response(r, c, t) - getResponse(r, c, t, b)) / 2.0;
}
void hessian3D(int r, int c, ResponseLayer *t, ResponseLayer *m, ResponseLayer *b, double H[3][3])
{
	double v = getResponse(r, c, t, m);
	double dxx = getResponse(r, c + 1, t, m) + getResponse(r, c - 1, t, m) - 2.0 * v;
	double dyy = getResponse(r + 1, c, t, m) + getResponse(r - 1, c, t, m) - 2.0 * v;
	double dss = get_Response(r, c, t) + getResponse(r, c, t, b) - 2.0 * v;
	double dxy = (getResponse(r + 1, c + 1, t, m) - getResponse(r + 1, c - 1, t, m) - getResponse(r - 1, c + 1, t, m) + getResponse(r - 1, c - 1, t, m)) / 4.0;
	double dxs = (get_Response(r, c + 1, t) - get_Response(r, c - 1, t) - getResponse(r, c + 1, t, b) + getResponse(r, c - 1, t, b)) / 4.0;
	double dys = (get_Response(r + 1, c, t) - get_Response(r - 1, c, t) - getResponse(r + 1, c, t, b) + getResponse(r - 1, c, t, b)) / 4.0;
	H[0][0] = dxx;
	H[0][1] = dxy;
	H[0][2] = dxs;
	H[1][0] = dxy;
	H[1][1] = dyy;
	H[1][2] = dys;
	H[2][0] = dxs;
	H[2][1] = dys;
	H[2][2] = dss;
}
void solve3x3Fast(const double A[3][3], const double b[3], double x[3])
{
	double invdet = 1.0 / (DBL_EPSILON + (A[0][0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1])
		- A[0][1] * (A[1][0] * A[2][2] - A[1][2] * A[2][0])
		+ A[0][2] * (A[1][0] * A[2][1] - A[1][1] * A[2][0])));
	x[0] = invdet * (b[0] * (A[1][1] * A[2][2] - A[1][2] * A[2][1]) -
		A[0][1] * (b[1] * A[2][2] - A[1][2] * b[2]) +
		A[0][2] * (b[1] * A[2][1] - A[1][1] * b[2]));
	x[1] = invdet * (A[0][0] * (b[1] * A[2][2] - A[1][2] * b[2]) -
		b[0] * (A[1][0] * A[2][2] - A[1][2] * A[2][0]) +
		A[0][2] * (A[1][0] * b[2] - b[1] * A[2][0]));
	x[2] = invdet * (A[0][0] * (A[1][1] * b[2] - b[1] * A[2][1]) -
		A[0][1] * (A[1][0] * b[2] - b[1] * A[2][0]) +
		b[0] * (A[1][0] * A[2][1] - A[1][1] * A[2][0]));
}
unsigned char getLaplacian(unsigned int row, unsigned int column, ResponseLayer *src, ResponseLayer *R)
{
	int scale = R->width / src->width;
	return R->laplacian[(scale * row) * R->width + (scale * column)];
}
void interpolateExtremumFast(int r, int c, ResponseLayer *t, ResponseLayer *m, ResponseLayer *b, FastHessian *fh)
{
	double dI[3], H[3][3], out[3];
	deriv3D(r, c, t, m, b, dI);
	hessian3D(r, c, t, m, b, H);
	solve3x3Fast(H, dI, out);
	out[0] = -out[0];
	out[1] = -out[1];
	out[2] = -out[2];
	if (fabs(out[0]) < 0.5 && fabs(out[1]) < 0.5 && fabs(out[2]) < 0.5)
	{
		if (!fh->iptssize)
		{
			fh->iptssize = 384;
			fh->ipts = (Ipoint*)malloc(sizeof(Ipoint) * fh->iptssize);
		}
		if (fh->iptssize == fh->interestPtsLen)
		{
			fh->iptssize <<= 1;
			fh->ipts = (Ipoint*)realloc(fh->ipts, sizeof(Ipoint) * fh->iptssize);
		}
		fh->ipts[fh->interestPtsLen].x = ((float)c + (float)out[0]) * (float)t->step;
		fh->ipts[fh->interestPtsLen].y = ((float)r + (float)out[1]) * (float)t->step;
		fh->ipts[fh->interestPtsLen].scale = 0.1333f * ((float)m->filter + (float)out[2] * (float)(m->filter - b->filter));
		fh->ipts[fh->interestPtsLen].laplacian = getLaplacian(r, c, t, m);
		fh->interestPtsLen++;
	}
}

void getIpoints(FastHessian *fh)
{
	ResponseLayer *b, *m, *t;
	static const int filter_map[5][4] = { { 0,1,2,3 },{ 1,3,4,5 },{ 3,5,6,7 },{ 5,7,8,9 },{ 7,9,10,11 } };
	for (int o = 0; o < fh->octaves; ++o) {
		for (int i = 0; i <= 1; ++i) {
				b = &fh->responseMap[filter_map[o][i]];
				m = &fh->responseMap[filter_map[o][i + 1]];
				t = &fh->responseMap[filter_map[o][i + 2]];
				for (int r = 0; r < t->height; ++r)
					for (int c = 0; c < t->width; ++c)
						if (isExtremum(r, c, t, m, b, fh))
							interpolateExtremumFast(r, c, t, m, b, fh);
		}
	}
}

static float BoxIntegral(float *img, int width, int height, int row, int col, int rows, int cols)
{
	float *data = img;
	int r1 = min(row, height) - 1;
	int c1 = min(col, width) - 1;
	int r2 = min(row + rows, height) - 1;
	int c2 = min(col + cols, width) - 1;
	float A = 0.0f, B = 0.0f, C = 0.0f, D = 0.0f;
	if (r1 >= 0 && c1 >= 0) A = data[r1 * width + c1];
	if (r1 >= 0 && c2 >= 0) B = data[r1 * width + c2];
	if (r2 >= 0 && c1 >= 0) C = data[r2 * width + c1];
	if (r2 >= 0 && c2 >= 0) D = data[r2 * width + c2];
	return max(0.f, A - B - C + D);
}

int fRound(float flt)
{
	return (int)floorf(flt + 0.5f);
}

#define ANGRESLEN 109
const float gauss25[7][7] = {// lookup table for 2d gaussian (sigma = 2.5) where (0,0) is top left and (6,6) is bottom right
0.02546481f, 0.02350698f, 0.01849125f, 0.01239505f, 0.00708017f, 0.00344629f, 0.00142946f,
0.02350698f, 0.02169968f, 0.01706957f, 0.01144208f, 0.00653582f, 0.00318132f, 0.00131956f,
0.01849125f, 0.01706957f, 0.01342740f, 0.00900066f, 0.00514126f, 0.00250252f, 0.00103800f,
0.01239505f, 0.01144208f, 0.00900066f, 0.00603332f, 0.00344629f, 0.00167749f, 0.00069579f,
0.00708017f, 0.00653582f, 0.00514126f, 0.00344629f, 0.00196855f, 0.00095820f, 0.00039744f,
0.00344629f, 0.00318132f, 0.00250252f, 0.00167749f, 0.00095820f, 0.00046640f, 0.00019346f,
0.00142946f, 0.00131956f, 0.00103800f, 0.00069579f, 0.00039744f, 0.00019346f, 0.00008024f };
#ifndef M_PI
#define M_PI 3.141592653589793f
#endif
#ifndef M_2PI
#define M_2PI 6.283185307179864f
#endif
float haarX(float *img, int width, int height, const int row, const int column, const int s, const int sD)
{
	return BoxIntegral(img, width, height, row - sD, column, s, sD) - 1.0f * BoxIntegral(img, width, height, row - sD, column - sD, s, sD);
}
float haarY(float *img, int width, int height, const int row, const int column, const int s, const int sD)
{
	return BoxIntegral(img, width, height, row, column - sD, sD, s) - 1.0f * BoxIntegral(img, width, height, row - sD, column - sD, sD, s);
}
float getAngle(float X, float Y)
{
	if (X > 0.0f && Y >= 0.0f)
		return atanf(Y / X);
	if (X < 0.0f && Y >= 0.0f)
		return M_PI - atanf(-Y / X);
	if (X < 0.0f && Y < 0.0f)
		return M_PI + atanf(Y / X);
	if (X > 0.0f && Y < 0.0f)
		return M_2PI - atanf(-Y / X);
	return 0;
}

void getOrientation(SURFDescriptor *despt, int index, int width, int height)
{
	if (despt->hessians->ipts == nullptr) {
		printf("Error: despt->hessians->ipts is null.\n");
		return;
	}
	float gauss = 0.f, scale = despt->hessians->ipts[index].scale;
	const int s = fRound(scale), r = fRound(despt->hessians->ipts[index].y), c = fRound(despt->hessians->ipts[index].x);

	float resX[ANGRESLEN] = { 0.0f }, resY[ANGRESLEN] = { 0.0f }, Ang[ANGRESLEN] = { 0.0f };
	const int id[] = { 6,5,4,3,2,1,0,1,2,3,4,5,6 };
	int idx = 0;
	for (int i = -6; i <= 6; ++i)
	{
		for (int j = -6; j <= 6; ++j)
		{
			if (i*i + j * j < 36)
			{
				gauss = gauss25[id[i + 6]][id[j + 6]];
				resX[idx] = gauss * haarX(despt->integralImg, width, height, r + j * s, c + i * s, s << 2, s << 1);
				resY[idx] = gauss * haarY(despt->integralImg, width, height, r + j * s, c + i * s, s << 2, s << 1);
				Ang[idx] = getAngle(resX[idx], resY[idx]);
				++idx;
			}
		}
	}
	float sumX, sumY;
	float max = 0.f, orientation = 0.f;
	float ang1, ang2 = 0.f;
	for (ang1 = 0.0f; ang1 < M_2PI; ang1 += 0.15f)
	{
		ang2 = (ang1 + 1.04719755f > M_2PI ? ang1 - 5.23598775f : ang1 + 1.04719755f); //ang2 = (ang1 + M_PI / 3.0f > M_2PI ? ang1 - 5.0f*M_PI / 3.0f : ang1 + M_PI / 3.0f);
		sumX = sumY = 0.f;
		for (unsigned int k = 0; k < ANGRESLEN; ++k)
		{
			const float ang = Ang[k];
			if (ang1 < ang2 && ang1 < ang && ang < ang2)
			{
				sumX += resX[k];
				sumY += resY[k];
			}
			else if (ang2 < ang1 && ((ang > 0 && ang < ang2) || (ang > ang1 && ang < M_2PI)))
			{
				sumX += resX[k];
				sumY += resY[k];
			}
		}
		if (sumX*sumX + sumY * sumY > max)
		{
			max = sumX * sumX + sumY * sumY;
			orientation = getAngle(sumX, sumY);
		}
	}
	despt->hessians->ipts[index].orientation = orientation;
}


float gaussian1(int x, int y, float sig)
{
	return (1.0f / (M_2PI*sig*sig)) * expf(-(x*x + y * y) / (2.0f*sig*sig));
}
float gaussian2(float x, float y, float sig)
{
	return 1.0f / (M_2PI*sig*sig) * expf(-(x*x + y * y) / (2.0f*sig*sig));
}


void getDescriptor(SURFDescriptor *despt, int index, int width, int height)
{
	int y, x, sample_x, sample_y, count = 0, roundedScale;
	int i, ix = 0, j = 0, jx = 0, xs = 0, ys = 0;
	float scale, *desc, dx, dy, mdx, mdy, co, si;
	float gauss_s1 = 0.f, gauss_s2 = 0.f;
	float rx = 0.f, ry = 0.f, rrx = 0.f, rry = 0.f, len = 0.f;
	float cx = -0.5f, cy = 0.f;
	scale = despt->hessians->ipts[index].scale;
	roundedScale = fRound(scale);
	x = fRound(despt->hessians->ipts[index].x);
	y = fRound(despt->hessians->ipts[index].y);
	desc = despt->hessians->ipts[index].descriptor;
	co = cosf(despt->hessians->ipts[index].orientation);
	si = sinf(despt->hessians->ipts[index].orientation);
	i = -8;
	while (i < 12)
	{
		j = -8;
		i = i - 4;
		cx += 1.f;
		cy = -0.5f;
		while (j < 12)
		{
			dx = dy = mdx = mdy = 0.f;
			cy += 1.f;
			j = j - 4;
			ix = i + 5;
			jx = j + 5;
			xs = fRound(x + (-jx * scale*si + ix * scale*co));
			ys = fRound(y + (jx*scale*co + ix * scale*si));
			for (int k = i; k < i + 9; ++k)
			{
				for (int l = j; l < j + 9; ++l)
				{
					sample_x = fRound(x + (-l * scale*si + k * scale*co));
					sample_y = fRound(y + (l*scale*co + k * scale*si));
					gauss_s1 = gaussian1(xs - sample_x, ys - sample_y, 2.5f * scale);
					rx = haarX(despt->integralImg, width, height, sample_y, sample_x, roundedScale << 1, roundedScale);
					ry = haarY(despt->integralImg, width, height, sample_y, sample_x, roundedScale << 1, roundedScale);
					rrx = gauss_s1 * (-rx * si + ry * co);
					rry = gauss_s1 * (rx*co + ry * si);
					dx += rrx;
					dy += rry;
					mdx += fabsf(rrx);
					mdy += fabsf(rry);
				}
			}
			gauss_s2 = gaussian2(cx - 2.0f, cy - 2.0f, 1.5f);
			desc[count++] = dx * gauss_s2;
			desc[count++] = dy * gauss_s2;
			desc[count++] = mdx * gauss_s2;
			desc[count++] = mdy * gauss_s2;
			len += (dx*dx + dy * dy + mdx * mdx + mdy * mdy) * gauss_s2*gauss_s2;
			j += 9;
		}
		i += 9;
	}
	len = 1.0f / sqrtf(len);
	for (int i = 0; i < 64; ++i)
		desc[i] *= len;
}

void getDescriptors(SURFDescriptor *despt, int width, int height)
{
    if (!despt->hessians->interestPtsLen)
        return;

    // operating all interes points directly
    for (int i = 0; i < despt->hessians->interestPtsLen; ++i)
    {
        getOrientation(despt, i, width, height);
        getDescriptor(despt, i, width, height);
    }
}

static void SURF_tpu_compute(SURFDescriptor *surf, int width, int height) {
	if (surf->hessians->interestPtsLen > 0)
        free(surf->hessians->ipts);
    surf->hessians->iptssize = 0;
    surf->hessians->interestPtsLen = 0;
    // Extract interest points and store in vector ipts
    getIpoints(surf->hessians);
    getDescriptors(surf, width, height);
}

bm_status_t bmcv_image_surf_response(
            bm_handle_t handle,
            float* img_data,
            SURFDescriptor *surf,
            int width,
            int height,
            int layer_num) {
	bm_status_t ret = BM_SUCCESS;
  ret = bmcv_surf_check_bm1684x(handle, width, layer_num);
	bm_device_mem_t responses[layer_num];
	bm_device_mem_t laplacian[layer_num];
	for (int i = 0; i < layer_num; i++) {
    ret = bm_malloc_device_byte(handle, responses+i, surf->hessians->responseMap[i].width * surf->hessians->responseMap[i].height * 2);
		ret = bm_malloc_device_byte(handle, laplacian+i, surf->hessians->responseMap[i].width * surf->hessians->responseMap[i].height * 2);
	}
	bm_device_mem_t rl_kernel[layer_num];

	fp16 *new_img_data;
	for (int i = 0; i < layer_num; i++) {
		ret = bm_malloc_device_byte(handle, rl_kernel+i, surf->hessians->responseMap[i].filter * surf->hessians->responseMap[i].filter * 3 * 2);
		rl_Kernel_Layer myBoxFilter;
		build_boxfilter(surf->hessians->responseMap[i].filter, &myBoxFilter);
		int kernel_size = 3 * surf->hessians->responseMap[i].filter * surf->hessians->responseMap[i].filter;
		fp16 *kernel = (fp16*)malloc(kernel_size * 2);
		float *temp_kernel = (float*)malloc(kernel_size * sizeof(float));
		float *new_kernel = (float*)malloc(kernel_size * sizeof(float));
		get_surf_kernel(myBoxFilter, temp_kernel, surf->hessians->responseMap[i].filter);

		for (int i = 0; i < kernel_size; i++) {
			*(kernel + i) = fp32tofp16(*(temp_kernel + i), 1);
		}
		ret = bm_memcpy_s2d(handle, rl_kernel[i], (void*)kernel);
		free(temp_kernel);
		free(new_kernel);
		free(kernel);
	}


	new_img_data = (fp16*)malloc(width*height*2);
	for (int i = 0; i < width * height; i++) {
		*(new_img_data + i) = fp32tofp16(*(img_data + i), 1);
	}

	bm_device_mem_t surf_img_mem;
	ret = bm_malloc_device_byte(handle, &surf_img_mem, width * height * 2);
	ret = bm_memcpy_s2d(handle, surf_img_mem, new_img_data);

	sg_api_cv_surf_response api;
	api.width = width;
	api.height = height;
	api.surf_img_addr = bm_mem_get_device_addr(surf_img_mem);

	for (int i = 0; i < layer_num; i++) {
		api.rl_width[i] = surf->hessians->responseMap[i].width;
		api.rl_height[i] = surf->hessians->responseMap[i].height;
		api.rl_step[i] = surf->hessians->responseMap[i].step;
		api.rl_filter[i] = surf->hessians->responseMap[i].filter;
		api.responses_addr[i] = bm_mem_get_device_addr(responses[i]);
		api.laplacian_addr[i] = bm_mem_get_device_addr(laplacian[i]);
		api.BoxFilterKernel_addr[i] = bm_mem_get_device_addr(rl_kernel[i]);
	}


  unsigned int chipid;
  bm_get_chipid(handle, &chipid);
  switch (chipid) {
      case BM1684X:
          struct timeval t1, t2;
          for (int i = 0; i < 2; i++) {
              gettimeofday(&t1, NULL);
              ret = bm_tpu_kernel_launch(handle, "cv_surf_response", (u8 *)&api, sizeof(api));
              gettimeofday(&t2, NULL);
          }
          printf("SURF TPU using time: %ld us\n", ((t2.tv_sec - t1.tv_sec) * 1000000 + t2.tv_usec - t1.tv_usec));
          if(BM_SUCCESS != ret){
              bmlib_log("SURF", BMLIB_LOG_ERROR, "surf sync api error\n");
              return BM_ERR_FAILURE;
          }
          break;
      default:
          printf("ChipID is NOT supported\n");
          break;
  }

	for (int i = 0; i < layer_num; i++) {
		int map_size = surf->hessians->responseMap[i].width * surf->hessians->responseMap[i].height;
		fp16 *response_FP16 = (fp16*)malloc(map_size * 2);
		fp16 *laplacian_FP16 = (fp16*)malloc(map_size * 2);
		float *laplacian_FP32 = (float*)malloc(map_size * sizeof(float));
		ret = bm_memcpy_d2s(handle, response_FP16, responses[i]);
		ret = bm_memcpy_d2s(handle, laplacian_FP16, laplacian[i]);
		for (int j = 0; j < map_size; j++) {
			surf->hessians->responseMap[i].responses[j] = (float)(fp16tofp32(*(response_FP16+j)));
			*(laplacian_FP32 + j) = (float)(fp16tofp32(*(laplacian_FP16+j)));
			surf->hessians->responseMap[i].laplacian[j] = (unsigned char)(*(laplacian_FP32 + j));
		}
		free(response_FP16);
		free(laplacian_FP16);
		free(laplacian_FP32);
	}

	free(new_img_data);
  bm_free_device(handle, surf_img_mem);

  Integral(img_data, surf->integralImg, width, height);
  SURF_tpu_compute(surf, width, height);
	return ret;
}
