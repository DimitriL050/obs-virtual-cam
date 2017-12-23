#include "hflip.h"

bool init_flip_filter(FlipContext* ctx,int width, int height, int format)
{	
	char args[512];
	int ret = -1;

	avfilter_register_all();

	AVFilter *buffersrc = avfilter_get_by_name("buffer");
	AVFilter *buffersink = avfilter_get_by_name("buffersink");
	AVFilterInOut *outputs = avfilter_inout_alloc();
	AVFilterInOut *inputs = avfilter_inout_alloc();
	enum AVPixelFormat pix_fmts[] = { (AVPixelFormat)format, AV_PIX_FMT_NONE };
	AVBufferSinkParams *buffersink_params;

	ctx->filter_graph = avfilter_graph_alloc();
	sprintf(args,
		"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
		width, height, format, 1, 30, 1, 1);
	ret = avfilter_graph_create_filter(&ctx->buffersrc_ctx, buffersrc, "in",
		args, NULL, ctx->filter_graph);

	buffersink_params = av_buffersink_params_alloc();
	buffersink_params->pixel_fmts = pix_fmts;
	ret = avfilter_graph_create_filter(&ctx->buffersink_ctx, buffersink, "out",
		NULL, buffersink_params, ctx->filter_graph);
	av_free(buffersink_params);

	outputs->name = av_strdup("in");
	outputs->filter_ctx = ctx->buffersrc_ctx;
	outputs->pad_idx = 0;
	outputs->next = NULL;

	inputs->name = av_strdup("out");
	inputs->filter_ctx = ctx->buffersink_ctx;
	inputs->pad_idx = 0;
	inputs->next = NULL;

	if ((ret = avfilter_graph_parse_ptr(ctx->filter_graph, "hflip",
		&inputs, &outputs, NULL)) < 0)
		return false;

	if ((ret = avfilter_graph_config(ctx->filter_graph, NULL)) < 0)
		return false;

	ctx->frame_in = av_frame_alloc();
	ctx->frame_out = av_frame_alloc();
	ctx->frame_buffer_out = (unsigned char *)av_malloc(
		av_image_get_buffer_size((AVPixelFormat)format, width, height, 1));
	av_image_fill_arrays(ctx->frame_out->data, ctx->frame_out->linesize, 
		ctx->frame_buffer_out,(AVPixelFormat)format, width, height, 1);
	ctx->frame_in->width = width;
	ctx->frame_in->height = height;
	ctx->frame_in->format = format;
	return true;
}

bool release_flip_filter(FlipContext* ctx)
{
	av_frame_free(&ctx->frame_out);
	av_frame_free(&ctx->frame_in);
	avfilter_graph_free(&ctx->filter_graph);
	return true;
}

void flip_frame(FlipContext* ctx, uint8_t** src, uint32_t* linesize)
{
	for (int i = 0; i < 8; i++){
		ctx->frame_in->data[i] = src[i];
		ctx->frame_in->linesize[i] = linesize[i];
	}

	av_buffersrc_add_frame(ctx->buffersrc_ctx, ctx->frame_in);
	av_buffersink_get_frame(ctx->buffersink_ctx, ctx->frame_out);
}

void unref_flip_frame(FlipContext* ctx)
{
	for (int i = 0; i < 8; i++){
		ctx->frame_in->data[i] = NULL;
		ctx->frame_in->linesize[i] = 0;
	}
	av_frame_unref(ctx->frame_out);
}