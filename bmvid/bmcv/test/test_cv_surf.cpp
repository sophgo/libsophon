#include <stdio.h>
#include "bmcv_api_ext_c.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <pthread.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define MAX_THREADS 1000
int arr[MAX_THREADS];

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

typedef struct {
    int loop_num;
    int use_real_img;
    int width1;
    int height1;
	int width2;
	int height2;
    int format;
	int octave;
    int step;
	float threshold_surf;
	float threshold_match;
    char* input_path1;
	char* input_path2;
    char* output_path_tpu;
    bm_handle_t handle;
} cv_surf_thread_arg_t;

static void readbin(char *input_path, unsigned char *input_data, int width, int height, int channel) {
    FILE *fp_src = fopen(input_path, "rb");
    if (fp_src == NULL) {
        printf("Can not open input_file %s\n", input_path);
        return;
    }
    if (fread(input_data, sizeof(unsigned char), width * height * channel, fp_src) != (size_t)(width * height * channel)) {
		printf("Failed to read img!\n");
	}
    fclose(fp_src);
}

static void writebin(char *output_path, unsigned char *data, int byte_size) {
    FILE *fp_dst = fopen(output_path, "wb");
    if (!fp_dst) {
        perror("failed to open destination file\n");
        // free(data);
    }
    if (fwrite((void*)data, 1, byte_size, fp_dst) < (unsigned int)byte_size) {
        printf("Failed to write all required bytes\n");
    }
    fclose(fp_dst);
}


unsigned char** channel_split(unsigned char* buffer, int width, int height) {
    unsigned char** chan_buffers = NULL;
    int i, num_frames = width * height;
	int samples;
    chan_buffers = (unsigned char**)malloc(3 * sizeof(unsigned char*));
	for (i = 0; i < 3; i++) {
		chan_buffers[i] = (unsigned char*)malloc(num_frames * sizeof(unsigned char));
	}
	samples = 3 * num_frames;
	for (i = 0; i < samples; i++) {
		chan_buffers[(i % 3)][i / 3] = buffer[i];
	}

    return chan_buffers;
}

float* LoadToMatrixGreyWeighted(unsigned char *input_data, int width, int height, int format) {
	int i, num_frams = width * height;
    float tofloat = 1.0f / 255.0f;
	float *data = (float*)malloc(width*height*sizeof(float));
	switch (format)
	{
	case FORMAT_RGB_PACKED: {
		unsigned char **buffer = channel_split(input_data, width, height);
		for (i = 0; i < num_frams; i++) {
			data[i] = ((float)buffer[0][i] * 0.2989f + (float)buffer[1][i] * 0.587f + (float)buffer[2][i] * 0.114f) * tofloat;
		}
		for (i = 0; i < 3; i++) {
			free(buffer[i]);
		}
		free(buffer);
		break;
	}
	default:
		for (i = 0; i < num_frams; i++) {
			data[i] = ((float)input_data[i] * 0.2989f + (float)input_data[num_frams + i] * 0.587f + (float)input_data[num_frams * 2 + i] * 0.114f) * tofloat;
		}
		break;
	}

    return data;
}

void ResLayer(ResponseLayer *Res, int width, int height, int step, int filter)
{
	Res->width = width;
	Res->height = height;
	Res->step = step;
	Res->filter = filter;
	Res->responses = (float*)malloc(width * height * sizeof(float));
	Res->laplacian = (unsigned char*)malloc(width * height * sizeof(unsigned char));
}

void allocateResponseMap(FastHessian *fh)
{
	int w = (fh->i_width / fh->init_sample);
	int h = (fh->i_height / fh->init_sample);
	int s = fh->init_sample;
	if (fh->octaves >= 1)
	{
		ResLayer(&fh->responseMap[0], w, h, s, 9);
		ResLayer(&fh->responseMap[1], w, h, s, 15);
		ResLayer(&fh->responseMap[2], w, h, s, 21);
		ResLayer(&fh->responseMap[3], w, h, s, 27);

	}
	if (fh->octaves >= 2)
	{
		ResLayer(&fh->responseMap[4], w >> 1, h >> 1, s << 1, 39);
		ResLayer(&fh->responseMap[5], w >> 1, h >> 1, s << 1, 51);
	}
	if (fh->octaves >= 3)
	{
		ResLayer(&fh->responseMap[6], w >> 2, h >> 2, s << 2, 75);
		ResLayer(&fh->responseMap[7], w >> 2, h >> 2, s << 2, 99);
	}
	if (fh->octaves >= 4)
	{
		ResLayer(&fh->responseMap[8], w >> 3, h >> 3, s << 3, 147);
		ResLayer(&fh->responseMap[9], w >> 3, h >> 3, s << 3, 195);
	}
	if (fh->octaves >= 5)
	{
		ResLayer(&fh->responseMap[10], w >> 4, h >> 4, s << 4, 291);
		ResLayer(&fh->responseMap[11], w >> 4, h >> 4, s << 4, 387);
	}
}

void FastHessianInit(FastHessian *fhts, float *img, int width, int height, int octaves, int init_sample, float thres)
{
	fhts->img = img;
	fhts->i_height = height;
	fhts->i_width = width;
	fhts->octaves = octaves;
	fhts->init_sample = init_sample;
	fhts->thresh = thres;
	fhts->iptssize = 0;
	allocateResponseMap(fhts);
}

static void SURFInitialize(SURFDescriptor *surf, int width, int height, int octaves, int init_sample, float thres) {
    surf->hessians = (FastHessian*)malloc(sizeof(FastHessian));
    surf->integralImg = (float*)malloc(width*height*sizeof(float));
    memset(surf->hessians->responseMap, 0, 12 * sizeof(ResponseLayer));
    FastHessianInit(surf->hessians, surf->integralImg, width, height, octaves, init_sample, thres);
    surf->hessians->ipts = 0;
}

void IpointPair_add(IpointPair *v, Ipoint *first, Ipoint *second)
{
	size_t memSize;
	if (!v->size)
	{
		v->size = 10;
		memSize = sizeof(Ipoint) * v->size;
		v->pt1 = (Ipoint*)malloc(memSize);
		v->pt2 = (Ipoint*)malloc(memSize);
		memset(v->pt1, 0, memSize);
		memset(v->pt2, 0, memSize);
	}
	if (v->size == v->count)
	{
		v->size <<= 1;
		memSize = sizeof(Ipoint) * v->size;
		v->pt1 = (Ipoint*)realloc(v->pt1, memSize);
		v->pt2 = (Ipoint*)realloc(v->pt2, memSize);
	}
	memcpy(&v->pt1[v->count], first, sizeof(Ipoint));
	memcpy(&v->pt2[v->count], second, sizeof(Ipoint));
	v->count++;
}

IpointPair getMatches(SURFDescriptor *surf1, SURFDescriptor *surf2, float threshold)
{
	IpointPair pair = {
		nullptr,
		nullptr,
		0,
		0
	 };
	float dist, d1, d2;
	Ipoint *match = nullptr;
	int i, j, idx;
	for (i = 0; i < surf1->hessians->interestPtsLen; i++)
	{
		d1 = d2 = FLT_MAX;
		for (j = 0; j < surf2->hessians->interestPtsLen; j++)
		{
			dist = 0.0f;
			for (idx = 0; idx < 64; ++idx)
				dist += (surf1->hessians->ipts[i].descriptor[idx] - surf2->hessians->ipts[j].descriptor[idx])*(surf1->hessians->ipts[i].descriptor[idx] - surf2->hessians->ipts[j].descriptor[idx]);
			dist = sqrtf(dist);
			if (dist < d1)
			{
				d2 = d1;
				d1 = dist;
				match = &surf2->hessians->ipts[j];
			}
			else if (dist < d2)
				d2 = dist;
		}
		if (d1 / d2 < threshold)
		{
			surf1->hessians->ipts[i].dx = match->x - surf1->hessians->ipts[i].x;
			surf1->hessians->ipts[i].dy = match->y - surf1->hessians->ipts[i].y;
			IpointPair_add(&pair, &surf1->hessians->ipts[i], match);
		}
	}
	return pair;
}


#ifndef M_PI
#define M_PI 3.141592653589793f
#endif
#ifndef M_PI_2
#define M_PI_2 1.570796326794897f
#endif
double RandomFloat(double a, double b, double amplitude)
{
	double random = ((double)rand()) / 32768.0;
	double diff = b - a;
	double r = random * diff;
	return amplitude * (a + r);
}
void setPixel(float *mat, float fx, float fy, float r, float g, float b, int width, int height, int colourComponent, int is_added)
{
	const int
		x = (int)fx - (fx >= 0 ? 0 : 1), nx = x + 1,
		y = (int)fy - (fy >= 0 ? 0 : 1), ny = y + 1;
	const float
		dx = fx - x,
		dy = fy - y;
	if (y >= 0 && y < height)
	{
		if (x >= 0 && x < width)
		{
			const float w1 = (1.0f - dx)*(1.0f - dy);
			float w2 = is_added ? 1.0f : (1.0f - w1);
			int pos = y * width * colourComponent + x * colourComponent;
			mat[pos] = (w1 * r) + (w2 * mat[pos]);
			mat[pos + 1] = (w1 * g) + (w2 * mat[pos + 1]);
			mat[pos + 2] = (w1 * b) + (w2 * mat[pos + 2]);
		}
		if (nx >= 0 && nx < width)
		{
			const float w1 = dx * (1.0f - dy);
			float w2 = is_added ? 1.0f : (1.0f - w1);
			int pos = y * width * colourComponent + nx * colourComponent;
			mat[pos] = (w1 * r) + (w2 * mat[pos]);
			mat[pos + 1] = (w1 * g) + (w2 * mat[pos + 1]);
			mat[pos + 2] = (w1 * b) + (w2 * mat[pos + 2]);
		}
	}
	if (ny >= 0 && ny < height)
	{
		if (x >= 0 && x < width)
		{
			const float w1 = (1.0f - dx)*dy;
			float w2 = is_added ? 1.0f : (1.0f - w1);
			int pos = ny * width * colourComponent + x * colourComponent;
			mat[pos] = (w1 * r) + (w2 * mat[pos]);
			mat[pos + 1] = (w1 * g) + (w2 * mat[pos + 1]);
			mat[pos + 2] = (w1 * b) + (w2 * mat[pos + 2]);
		}
		if (nx >= 0 && nx < width)
		{
			const float w1 = dx * dy;
			float w2 = is_added ? 1.0f : (1.0f - w1);
			int pos = ny * width * colourComponent + nx * colourComponent;
			mat[pos] = (w1 * r) + (w2 * mat[pos]);
			mat[pos + 1] = (w1 * g) + (w2 * mat[pos + 1]);
			mat[pos + 2] = (w1 * b) + (w2 * mat[pos + 2]);
		}
	}
}
void DrawCircle(float *img, float x0, float y0, float r, int width, int height, int colourComponent, float red, float green, float blue)
{
	float x = 0.0f, y = 0.0f;
	float r2 = r * r;
	float d2;
	x = r;
	while (y < x)
	{
		setPixel(img, x0 + x, y0 + y, red, green, blue, width, height, colourComponent, 1);
		setPixel(img, x0 + y, y0 + x, red, green, blue, width, height, colourComponent, 1);
		setPixel(img, x0 - x, y0 - y, red, green, blue, width, height, colourComponent, 1);
		setPixel(img, x0 - y, y0 - x, red, green, blue, width, height, colourComponent, 1);
		setPixel(img, x0 + x, y0 - y, red, green, blue, width, height, colourComponent, 1);
		setPixel(img, x0 - x, y0 + y, red, green, blue, width, height, colourComponent, 1);
		setPixel(img, x0 + y, y0 - x, red, green, blue, width, height, colourComponent, 1);
		setPixel(img, x0 - y, y0 + x, red, green, blue, width, height, colourComponent, 1);
		d2 = x * x + y * y;
		if (d2 > r2)
			x--;
		else
			y++;
	}
}

void drawPoint(float *img, int width, int height, int colourComponent, Ipoint *ipt, float red, float green, float blue)
{
	// float o = ipt->orientation;
	DrawCircle(img, ipt->x, ipt->y, 3.0f, width, height, colourComponent, red, green, blue);
}
float orientationCal(float p1X, float p1Y, float p2X, float p2Y)
{
	float y = p2Y - p1Y, x = p2X - p1X;
	float result = 0.f;
	if (x != 0.0f)
	{
		const union { float flVal; unsigned int nVal; } tYSign = { y };
		const union { float flVal; unsigned int nVal; } tXSign = { x };
		if (fabsf(x) >= fabsf(y))
		{
			union { float flVal; unsigned int nVal; } tOffset = { M_PI };
			tOffset.nVal |= tYSign.nVal & 0x80000000u;
			tOffset.nVal *= tXSign.nVal >> 31;
			result = tOffset.flVal;
			const float z = y / x;
			result += (0.97239411f + -0.19194795f * z * z) * z;
		}
		else
		{
			union { float flVal; unsigned int nVal; } tOffset = { M_PI_2 };
			tOffset.nVal |= tYSign.nVal & 0x80000000u;
			result = tOffset.flVal;
			const float z = x / y;
			result -= (0.97239411f + -0.19194795f * z * z) * z;
		}
	}
	else if (y > 0.0f)
		result = M_PI_2;
	else if (y < 0.0f)
		result = -M_PI_2;
	return result;
}
float lengthCal(float p1X, float p1Y, float p2X, float p2Y)
{
	float x = (p1X - p2X) * (p1X - p2X) + (p1Y - p2Y) * (p1Y - p2Y);
	unsigned int i = *(unsigned int*)&x;
	i += 127 << 23;
	i >>= 1;
	return *(float*)&i;
}

void DrawLine(float *img, float x0, float y0, float length, float ori, int width, int height, int colourComponent, float red, float green, float blue)
{
	float sinori = sinf(ori);
	float cosori = cosf(ori);
	int i;
	float x, y;
	float rouneD = roundf(length);
	for (i = 1; i < (int)rouneD; i++)
	{
		x = cosori * i;
		y = sinori * i;
		setPixel(img, x0 + x, y0 + y, red, green, blue, width, height, colourComponent, 1);
	}
	if (fabsf(length - rouneD) > 0.0f)
	{
		x = cosori * (length - 1.0f);
		y = sinori * (length - 1.0f);
		setPixel(img, x0 + x, y0 + y, red, green, blue, width, height, colourComponent, 1);
	}
	setPixel(img, x0, y0, 0.775f, 0.775f, 0.775f, width, height, colourComponent, 1);
}

void matcherFree(IpointPair *pair)
{
	free(pair->pt1);
	free(pair->pt2);
}

void printResponseMap(FastHessian *fh) {
    for (int i = 0; i < 1; i++) {
        ResponseLayer *layer = &fh->responseMap[9];
        printf("Response Layer %d:\n", i);
        printf("  Width: %d, Height: %d, Step: %d, Filter: %d\n", layer->width, layer->height, layer->step, layer->filter);

        printf("  Responses:\n");
        for (int y = 0; y < layer->height; y++) {
            for (int x = 0; x < layer->width; x++) {
                int index = y * layer->width + x;
                printf("%f ", layer->responses[index]);
            }
            printf("\n");
        }

        printf("  Laplacians:\n");
        for (int y = 0; y < layer->height; y++) {
            for (int x = 0; x < layer->width; x++) {
                int index = y * layer->width + x;
                printf("%d ", layer->laplacian[index]);
            }
            printf("\n");
        }
    }
}

void get_output_data(int width1, int height1, int width2, int height2, unsigned char *input_data1, unsigned char *input_data2, SURFDescriptor *surf1, SURFDescriptor *surf2, float threshold) {
	IpointPair matches = getMatches(surf1, surf2, threshold);
    printf("matches.size: %d\n", matches.size);
	printf("matches.count: %d\n", matches.count);
    float *imageFloat1 = (float*)malloc(width1 * height1 * 3 * sizeof(float));
	float *imageFloat2 = (float*)malloc(width2 * height2 * 3 * sizeof(float));
	int i;
	for (i = 0; i < width1 * height1 * 3; i++)
		imageFloat1[i] = input_data1[i] / 255.0f;
	for (i = 0; i < width2 * height2 * 3; i++)
		imageFloat2[i] = input_data2[i] / 255.0f;
	for (i = 0; i < matches.count; ++i)
	{
		float red = (float)RandomFloat(0.0, 1.0, 1.0);
		float green = (float)RandomFloat(0.0, 1.0, 1.0);
		float blue = (float)RandomFloat(0.0, 1.0, 1.0);
		drawPoint(imageFloat1, width1, height1, 3, &matches.pt1[i], red, green, blue);
		drawPoint(imageFloat2, width2, height2, 3, &matches.pt2[i], red, green, blue);
		float circleOrientation = orientationCal(matches.pt1[i].x, matches.pt1[i].y, matches.pt2[i].x + width1, matches.pt2[i].y);
		float distance = lengthCal(matches.pt1[i].x, matches.pt1[i].y, matches.pt2[i].x + width1, matches.pt2[i].y);
		DrawLine(imageFloat1, matches.pt1[i].x, matches.pt1[i].y, distance, circleOrientation, width1, height1, 3, red, green, blue);
		circleOrientation = orientationCal(matches.pt1[i].x - width1, matches.pt1[i].y, matches.pt2[i].x, matches.pt2[i].y);
		distance = lengthCal(matches.pt1[i].x - width1, matches.pt1[i].y, matches.pt2[i].x, matches.pt2[i].y);
		DrawLine(imageFloat2, matches.pt1[i].x - width1, matches.pt1[i].y, distance, circleOrientation, width2, height2, 3, red, green, blue);
	}

	matcherFree(&matches);
	for (i = 0; i < width1 * height1 * 3; i++)
	{
		float value = imageFloat1[i] * 255.0f;
		if (value > 255.0f)
			input_data1[i] = 255U;
		else if (value < 0.0f)
			input_data1[i] = 0U;
		else
			input_data1[i] = (unsigned char)value;
	}
	free(imageFloat1);
	for (i = 0; i < width2 * height2 * 3; i++)
	{
		float value = imageFloat2[i] * 255.0f;
		if (value > 255.0f)
			input_data2[i] = 255U;
		else if (value < 0.0f)
			input_data2[i] = 0U;
		else
			input_data2[i] = (unsigned char)value;
	}
}
void get_output_img(unsigned char *input_data1, unsigned char *input_data2, int width1, int height1, int width2, int height2, char *combined_output) {
	// Create a new image buffer to merge two images
    int combined_width = width1 + width2;
    int combined_height = height1 > height2 ? height1 : height2;
    unsigned char *combined_image = (unsigned char*)malloc(combined_width * combined_height * 3);

    // clear images
    memset(combined_image, 0, combined_width * combined_height * 3);

    // Copy the first image to the composite image
    for (int y = 0; y < height1; y++) {
        memcpy(combined_image + y * combined_width * 3, input_data1 + y * width1 * 3, width1 * 3);
    }

    // Copy the second image to the composite image
    for (int y = 0; y < height2; y++) {
        memcpy(combined_image + y * combined_width * 3 + width1 * 3, input_data2 + y * width2 * 3, width2 * 3);
    }

	// Save the merged image
	writebin(combined_output, combined_image, combined_width * combined_height * 3);
}


void SURFFree(SURFDescriptor *surf)
{
	free(surf->integralImg);
	if (surf->hessians->interestPtsLen > 0)
		free(surf->hessians->ipts);
	for (int i = 0; i < 12; i++)
	{
		if (surf->hessians->responseMap[i].responses)
		{
			free(surf->hessians->responseMap[i].responses);
			free(surf->hessians->responseMap[i].laplacian);
		}
	}
	free(surf->hessians);
}

static void fillRGB(unsigned char* input, int width, int height) {
    // Loop over each pixel row
    for (int i = 0; i < height; i++) {
        // Loop over each pixel in the row
        for (int j = 0; j < width; j++) {
            // Calculate the base index for the pixel (i, j)
            int index = (i * width + j) * 3; // Multiply by 3 for RGB

            // Assign random values to each of the three channels
            input[index] = rand() % 256;    // Red component
            input[index + 1] = rand() % 256; // Green component
            input[index + 2] = rand() % 256; // Blue component
        }
    }
}

static int test_surf_random(
        int use_real_img,
        int width1,
        int height1,
		int width2,
		int height2,
        int format,
		int octave,
		int step,
		float threshold_surf,
		float threshold_match,
		char* input_path1,
        char* input_path2,
		char* output_path_tpu,
        bm_handle_t handle) {
    int ret;
	int layer_num;

	unsigned char* input_data1 = (unsigned char*)malloc(width1 * height1 * 3 * sizeof(unsigned char));
	unsigned char* input_data2 = (unsigned char*)malloc(width2 * height2 * 3 * sizeof(unsigned char));

	if (use_real_img) {
		readbin(input_path1, input_data1, width1, height1, 3);
		readbin(input_path2, input_data2, width2, height2, 3);
	} else {
		srand(time(NULL));
		fillRGB(input_data1, width1, height1);
		fillRGB(input_data2, width2, height2);
	}

	float *data1 = LoadToMatrixGreyWeighted(input_data1, width1, height1, format);
	float *data2 = LoadToMatrixGreyWeighted(input_data2, width2, height2, format);


	SURFDescriptor surf1, surf2;
    SURFInitialize(&surf1, width1, height1, octave, step, threshold_surf);
    SURFInitialize(&surf2, width2, height2, octave, step, threshold_surf);

    layer_num = 4 + (octave - 1) * 2;

	ret = bmcv_image_surf_response(handle, data1, &surf1, width1, height1, layer_num);
	ret = bmcv_image_surf_response(handle, data2, &surf2, width2, height2, layer_num);

	get_output_data(width1, height1, width2, height2, input_data1, input_data2, &surf1, &surf2, threshold_match);
	if (use_real_img) {
		get_output_img(input_data1, input_data2, width1, height1, width2, height2, output_path_tpu);
	}


    if(ret != BM_SUCCESS) {
        free(input_data1);
		free(input_data2);
        return ret;
    }
    free(input_data1);
	free(input_data2);
	free(data1);
	free(data2);
    return ret;
}


void* test_surf(void* args) {
	cv_surf_thread_arg_t* cv_surf_thread_arg = (cv_surf_thread_arg_t*)args;
    int loop_num = cv_surf_thread_arg->loop_num;
    int use_real_img = cv_surf_thread_arg->use_real_img;
    int width1 = cv_surf_thread_arg->width1;
    int height1 = cv_surf_thread_arg->height1;
	int width2 = cv_surf_thread_arg->width2;
	int height2 = cv_surf_thread_arg->height2;
    int format = cv_surf_thread_arg->format;
	int octave = cv_surf_thread_arg->octave;
    int step = cv_surf_thread_arg->step;
	float threshold_surf = cv_surf_thread_arg->threshold_surf;
	float threshold_match = cv_surf_thread_arg->threshold_match;
	char* input_path1 = cv_surf_thread_arg->input_path1;
    char* input_path2 = cv_surf_thread_arg->input_path2;
	char* output_path_tpu = cv_surf_thread_arg->output_path_tpu;
    bm_handle_t handle = cv_surf_thread_arg->handle;
    for (int i = 0; i < loop_num; i++) {
		if (0 != test_surf_random(use_real_img, width1, height1, width2, height2, format, octave, step, threshold_surf, threshold_match, input_path1, input_path2, output_path_tpu, handle)){
            printf("------TEST CV_SURF FAILED------\n");
            exit(-1);
        }
        printf("------TEST CV_SURF PASSED!------\n");
    }
    return NULL;
}

int main(int argc, char* args[]) {

	int thread_num = 1;

    // read user input
    printf("Enter the number of threads: ");
    if (scanf("%d", &thread_num) != 1) {
        // Invalid input, notify the user and exit the program
        fprintf(stderr, "Invalid input, please enter an integer\n");
        return 1;
    }
    // Check whether the number of input threads exceeds the maximum value
    if (thread_num > MAX_THREADS) {
    printf("Error: Maximum thread number is %d.\n", MAX_THREADS);
    return -1;
    }
    // int thread_num = 1;
    int loop = 1;
    int use_real_img = 1;
    int width1 = 1059;
    int height1 = 1080;
	int width2 = 1059;
	int height2 = 1080;
	int format = FORMAT_RGB_PLANAR;
	int octave = 1;
    int step = 2;
	float threshold_surf = 0.01f;
	float threshold_match = 0.75f;
    char *input_path1 = NULL;
	char *input_path2 = NULL;
	char *output_path_tpu = NULL;
    bm_handle_t handle;
    bm_status_t ret = bm_dev_request(&handle, 0);
    if (ret != BM_SUCCESS) {
        printf("Create bm handle failed. ret = %d\n", ret);
        return -1;
    }

    if (argc == 2 && atoi(args[1]) == -1) {
        printf("usage:\n");
        printf("%s thread_num loop use_real_img width1 height1 width2 heigth2 format step thresh1 thresh2 input_path1 input_path2 output_path(when use_real_img = 1,need to set input_path and output_path) \n", args[0]);
        printf("example:\n");
        printf("%s \n", args[0]);
        printf("%s 2\n", args[0]);
        printf("%s 1 0 512 512 600 600 \n", args[0]);
        printf("%s 1 1 840 1080 840 1080 FORMAT_RGB_packed 2 0.01 0.75 res/840x1080_rgbp.bing res/840x1080_rgbp.bin out/out_quantify.bin \n", args[0]);
        return 0;
    }

    if (argc > 1) loop = atoi(args[1]);
    if (argc > 2) use_real_img = atoi(args[2]);
    if (argc > 3) width1 = atoi(args[3]);
    if (argc > 4) height1 = atoi(args[4]);
	if (argc > 5) width2 = atoi(args[5]);
	if (argc > 6) height2 = atoi(args[6]);
    if (argc > 7) format = atoi(args[7]);
	if (argc > 8) octave = atoi(args[8]);
    if (argc > 9) step = atoi(args[9]);
	if (argc > 10) threshold_surf = atof(args[10]);
	if (argc > 11) threshold_match = atof(args[11]);
    if (argc > 12) input_path1 = args[12];
	if (argc > 13) input_path2 = args[13];
    if (argc > 14) output_path_tpu = args[14];

	pthread_t *pid = (pthread_t *)malloc(thread_num * sizeof(pthread_t));
    cv_surf_thread_arg_t *cv_surf_thread_arg = (cv_surf_thread_arg_t *)malloc(thread_num * sizeof(cv_surf_thread_arg_t));
    for (int i = 0; i < thread_num; i++) {
        cv_surf_thread_arg[i].loop_num = loop;
        cv_surf_thread_arg[i].use_real_img = use_real_img;
        cv_surf_thread_arg[i].width1 = width1;
        cv_surf_thread_arg[i].height1 = height1;
		cv_surf_thread_arg[i].width2 = width2;
        cv_surf_thread_arg[i].height2 = height2;
        cv_surf_thread_arg[i].format = format;
		cv_surf_thread_arg[i].octave = octave;
        cv_surf_thread_arg[i].step = step;
		cv_surf_thread_arg[i].threshold_surf = threshold_surf;
		cv_surf_thread_arg[i].threshold_match = threshold_match;
        cv_surf_thread_arg[i].input_path1 = input_path1;
		cv_surf_thread_arg[i].input_path2 = input_path2;
        cv_surf_thread_arg[i].output_path_tpu = output_path_tpu;
        cv_surf_thread_arg[i].handle = handle;
        if (pthread_create(&pid[i], NULL, test_surf, &cv_surf_thread_arg[i]) != 0) {
            printf("create thread failed\n");
            return -1;
        }
    }

    for (int i = 0; i < thread_num; i++) {
        int ret = pthread_join(pid[i], NULL);
        if (ret != 0) {
            printf("Thread join failed\n");
            exit(-1);
        }
    }
    bm_dev_free(handle);
	free(pid);
	free(cv_surf_thread_arg);
	return ret;
}
