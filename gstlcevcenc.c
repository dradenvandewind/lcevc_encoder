#include "gstlcevcenc.h"
#include <string.h>

GST_DEBUG_CATEGORY_STATIC(gst_lcevc_enc_debug);
#define GST_CAT_DEFAULT gst_lcevc_enc_debug

/* STUB implementations for LCEVC */

// LCEVC stub function implementations
gboolean 
lcevc_encoder_init(LcevcContext **ctx, gint bitrate, gfloat quality)
{
    GST_DEBUG("LCEVC stub: Initializing encoder with bitrate=%d, quality=%.2f", 
              bitrate, quality);
    
    *ctx = g_malloc0(sizeof(LcevcContext));
    if (!*ctx) {
        return FALSE;
    }
    
    (*ctx)->bitrate = bitrate;
    (*ctx)->quality = quality;
    return TRUE;
}

void 
lcevc_encoder_cleanup(LcevcContext *ctx)
{
    GST_DEBUG("LCEVC stub: Cleaning up encoder");
    if (ctx) {
        g_free(ctx);
    }
}

void 
lcevc_encoder_configure(LcevcContext *ctx, gint width, gint height, 
                       GstVideoFormat format, gint enhancement_layers)
{
    if (!ctx) return;
    
    GST_DEBUG("LCEVC stub: Configuring encoder: %dx%d, format=%d, layers=%d",
              width, height, format, enhancement_layers);
    ctx->width = width;
    ctx->height = height;
    ctx->format = format;
    ctx->enhancement_layers = enhancement_layers;
}

gboolean 
lcevc_encode_frame(LcevcContext *ctx, GstBuffer *input_buffer, 
                  LcevcEncodedResult *result)
{
    if (!ctx || !result) return FALSE;
    
    GST_DEBUG("LCEVC stub: Encoding frame");
    
    // Create dummy data for testing
    static guint8 stub_data[] = { 0x4C, 0x43, 0x45, 0x56, 0x43 }; // "LCEVC"
    result->data = stub_data;
    result->data_size = sizeof(stub_data);
    result->keyframe = (g_random_int() % 30 == 0); // Random keyframe approx every 30 frames
    
    return TRUE;
}

/* End of LCEVC stubs */

static GstStaticPadTemplate sink_template_main_yuv_input = GST_STATIC_PAD_TEMPLATE(
    "sink_main",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(
        "video/x-raw, "
        "format = (string) { I420, YV12, Y42B, Y444, NV12, NV21 }, "
        "width = (int) [ 1, 8192 ], "
        "height = (int) [ 1, 4320 ], "
        "framerate = (fraction) [ 0, 60 ]"
    )
);

static GstStaticPadTemplate sink_template_secondary_from_dec_yuv_input = GST_STATIC_PAD_TEMPLATE(
    "sink_secondary",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(
        "video/x-raw, "
        "format = (string) { I420, YV12, Y42B, Y444, NV12, NV21 }, "
        "width = (int) [ 1, 8192 ], "
        "height = (int) [ 1, 4320 ], "
        "framerate = (fraction) [ 0, 60 ]"
    )
);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(
        "video/x-lvc1, "
        "width = (int) [ 1, 8192 ], "
        "height = (int) [ 1, 4320 ], "
        "framerate = (fraction) [ 0, 60 ], "
        "parsed = (boolean) true"
    )
);

enum {
    PROP_0,
    PROP_BITRATE,
    PROP_QUALITY,
    PROP_ENHANCEMENT_LAYERS,
    PROP_TWO_PASS
};

static void gst_lcevc_enc_finalize(GObject *object);
static void gst_lcevc_enc_set_property(GObject *object, guint prop_id,
                                     const GValue *value, GParamSpec *pspec);
static void gst_lcevc_enc_get_property(GObject *object, guint prop_id,
                                     GValue *value, GParamSpec *pspec);

static gboolean gst_lcevc_enc_start(GstVideoEncoder *enc);
static gboolean gst_lcevc_enc_stop(GstVideoEncoder *enc);
static gboolean gst_lcevc_enc_set_format(GstVideoEncoder *enc,
                                       GstVideoCodecState *state);
static GstFlowReturn gst_lcevc_enc_handle_frame(GstVideoEncoder *enc,
                                              GstVideoCodecFrame *frame);

static GstPad *gst_lcevc_enc_request_new_pad(GstElement *element,
                                            GstPadTemplate *templ,
                                            const gchar *name,
                                            const GstCaps *caps);

static void gst_lcevc_enc_release_pad(GstElement *element, GstPad *pad);

G_DEFINE_TYPE(GstLcevcEnc, gst_lcevc_enc, GST_TYPE_VIDEO_ENCODER);

static void
gst_lcevc_enc_class_init(GstLcevcEncClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstVideoEncoderClass *video_encoder_class = GST_VIDEO_ENCODER_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

    gobject_class->finalize = gst_lcevc_enc_finalize;
    gobject_class->set_property = gst_lcevc_enc_set_property;
    gobject_class->get_property = gst_lcevc_enc_get_property;

    element_class->request_new_pad = GST_DEBUG_FUNCPTR(gst_lcevc_enc_request_new_pad);
    element_class->release_pad = GST_DEBUG_FUNCPTR(gst_lcevc_enc_release_pad);

    // Configurable properties
    g_object_class_install_property(gobject_class, PROP_BITRATE,
        g_param_spec_int("bitrate", "Bitrate", "Target bitrate in kbps",
                        100, 50000, 2000,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_QUALITY,
        g_param_spec_float("quality", "Quality", "Encoding quality (0.0 - 1.0)",
                         0.0, 1.0, 0.8,
                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_ENHANCEMENT_LAYERS,
        g_param_spec_int("enhancement-layers", "Enhancement Layers",
                        "Number of LCEVC enhancement layers",
                        1, 4, 2,
                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(gobject_class, PROP_TWO_PASS,
        g_param_spec_boolean("two-pass", "Two Pass",
                           "Enable two-pass encoding",
                           FALSE,
                           G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    gst_element_class_add_static_pad_template(element_class, &sink_template_main_yuv_input);
    gst_element_class_add_static_pad_template(element_class, &sink_template_secondary_from_dec_yuv_input);
    gst_element_class_add_static_pad_template(element_class, &src_template);

    gst_element_class_set_static_metadata(element_class,
        "LCEVC Encoder",
        "Encoder/Video",
        "Encodes video using LCEVC (ISO/IEC 23094-2)",
        "Le Blond Erwan <erwanleblond@gmail.com>");

    video_encoder_class->start = GST_DEBUG_FUNCPTR(gst_lcevc_enc_start);
    video_encoder_class->stop = GST_DEBUG_FUNCPTR(gst_lcevc_enc_stop);
    video_encoder_class->set_format = GST_DEBUG_FUNCPTR(gst_lcevc_enc_set_format);
    video_encoder_class->handle_frame = GST_DEBUG_FUNCPTR(gst_lcevc_enc_handle_frame);

    GST_DEBUG_CATEGORY_INIT(gst_lcevc_enc_debug, "lcevcenc", 0, "LCEVC Encoder");
}

static void
gst_lcevc_enc_init(GstLcevcEnc *enc)
{
    enc->bitrate = 2000;
    enc->quality = 0.8;
    enc->enhancement_layers = 2;
    enc->two_pass = FALSE;
    enc->initialized = FALSE;
    enc->frames_processed = 0;
    enc->processing_time = 0;
    enc->lcevc_context = NULL;
    enc->sink_main = NULL;
    enc->sink_secondary = NULL;
}

static void
gst_lcevc_enc_finalize(GObject *object)
{
    GstLcevcEnc *enc = GST_LCEVC_ENC(object);

    if (enc->lcevc_context) {
        lcevc_encoder_cleanup(enc->lcevc_context);
        enc->lcevc_context = NULL;
    }

    G_OBJECT_CLASS(gst_lcevc_enc_parent_class)->finalize(object);
}

static void
gst_lcevc_enc_set_property(GObject *object, guint prop_id,
                         const GValue *value, GParamSpec *pspec)
{
    GstLcevcEnc *enc = GST_LCEVC_ENC(object);

    switch (prop_id) {
        case PROP_BITRATE:
            enc->bitrate = g_value_get_int(value);
            break;
        case PROP_QUALITY:
            enc->quality = g_value_get_float(value);
            break;
        case PROP_ENHANCEMENT_LAYERS:
            enc->enhancement_layers = g_value_get_int(value);
            break;
        case PROP_TWO_PASS:
            enc->two_pass = g_value_get_boolean(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
gst_lcevc_enc_get_property(GObject *object, guint prop_id,
                         GValue *value, GParamSpec *pspec)
{
    GstLcevcEnc *enc = GST_LCEVC_ENC(object);

    switch (prop_id) {
        case PROP_BITRATE:
            g_value_set_int(value, enc->bitrate);
            break;
        case PROP_QUALITY:
            g_value_set_float(value, enc->quality);
            break;
        case PROP_ENHANCEMENT_LAYERS:
            g_value_set_int(value, enc->enhancement_layers);
            break;
        case PROP_TWO_PASS:
            g_value_set_boolean(value, enc->two_pass);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static GstPad *
gst_lcevc_enc_request_new_pad(GstElement *element,
                             GstPadTemplate *templ,
                             const gchar *name,
                             const GstCaps *caps)
{
    GstLcevcEnc *enc = GST_LCEVC_ENC(element);
    GstPad *pad = NULL;

    GST_DEBUG_OBJECT(enc, "Requesting new pad: %s", templ->name_template);

    pad = GST_PAD(gst_pad_new_from_template(templ, name));
    if (!pad) {
        GST_ERROR_OBJECT(enc, "Failed to create pad from template");
        return NULL;
    }

    // Store reference to the pads
    if (g_str_has_prefix(GST_PAD_NAME(pad), "sink_main")) {
        enc->sink_main = pad;
    } else if (g_str_has_prefix(GST_PAD_NAME(pad), "sink_secondary")) {
        enc->sink_secondary = pad;
    }

    gst_element_add_pad(element, pad);

    return pad;
}

static void
gst_lcevc_enc_release_pad(GstElement *element, GstPad *pad)
{
    GstLcevcEnc *enc = GST_LCEVC_ENC(element);

    GST_DEBUG_OBJECT(enc, "Releasing pad: %s", GST_PAD_NAME(pad));

    if (pad == enc->sink_main) {
        enc->sink_main = NULL;
    } else if (pad == enc->sink_secondary) {
        enc->sink_secondary = NULL;
    }

    gst_element_remove_pad(element, pad);
}

static gboolean
gst_lcevc_enc_start(GstVideoEncoder *encoder)
{
    GstLcevcEnc *enc = GST_LCEVC_ENC(encoder);

    GST_DEBUG_OBJECT(enc, "Starting LCEVC encoder");

    // Initialize LCEVC context
    if (!lcevc_encoder_init(&enc->lcevc_context, enc->bitrate, enc->quality)) {
        GST_ERROR_OBJECT(enc, "Failed to initialize LCEVC encoder");
        return FALSE;
    }

    enc->initialized = TRUE;
    enc->frames_processed = 0;
    enc->processing_time = 0;

    return TRUE;
}

static gboolean
gst_lcevc_enc_stop(GstVideoEncoder *encoder)
{
    GstLcevcEnc *enc = GST_LCEVC_ENC(encoder);

    GST_DEBUG_OBJECT(enc, "Stopping LCEVC encoder");

    if (enc->lcevc_context) {
        lcevc_encoder_cleanup(enc->lcevc_context);
        enc->lcevc_context = NULL;
    }

    enc->initialized = FALSE;
    return TRUE;
}

static gboolean
gst_lcevc_enc_set_format(GstVideoEncoder *encoder, GstVideoCodecState *state)
{
    GstLcevcEnc *enc = GST_LCEVC_ENC(encoder);
    GstVideoInfo *info = &state->info;

    enc->base_width = GST_VIDEO_INFO_WIDTH(info);
    enc->base_height = GST_VIDEO_INFO_HEIGHT(info);
    enc->base_format = GST_VIDEO_INFO_FORMAT(info);

    GST_DEBUG_OBJECT(enc, "Setting format: %dx%d, format: %s",
                    enc->base_width, enc->base_height,
                    gst_video_format_to_string(enc->base_format));

    // Configure LCEVC encoder with format
    if (enc->initialized && enc->lcevc_context) {
        lcevc_encoder_configure(enc->lcevc_context, enc->base_width, 
                              enc->base_height, enc->base_format,
                              enc->enhancement_layers);
    }

    return TRUE;
}

static GstFlowReturn
gst_lcevc_enc_handle_frame(GstVideoEncoder *encoder, GstVideoCodecFrame *frame)
{
    GstLcevcEnc *enc = GST_LCEVC_ENC(encoder);
    GstBuffer *output_buffer;
    GstFlowReturn ret = GST_FLOW_OK;
    GstClockTime start_time, end_time;
    LcevcEncodedResult result;

    if (!enc->initialized || !enc->lcevc_context) {
        GST_ERROR_OBJECT(enc, "Encoder not initialized");
        return GST_FLOW_NOT_NEGOTIATED;
    }

    start_time = gst_util_get_timestamp();

    // Encode LCEVC frame
    if (!lcevc_encode_frame(enc->lcevc_context, frame->input_buffer, &result)) {
        GST_ERROR_OBJECT(enc, "Failed to encode frame");
        return GST_FLOW_ERROR;
    }

    // Create output buffer
    output_buffer = gst_buffer_new_allocate(NULL, result.data_size, NULL);
    if (!output_buffer) {
        GST_ERROR_OBJECT(enc, "Failed to allocate output buffer");
        return GST_FLOW_ERROR;
    }

    // Copy encoded data
    gst_buffer_fill(output_buffer, 0, result.data, result.data_size);

    // Configure output frame metadata
    frame->output_buffer = output_buffer;
    frame->dts = frame->pts;
    


    ret = gst_video_encoder_finish_frame(encoder, frame);

    end_time = gst_util_get_timestamp();
    enc->processing_time += (end_time - start_time);
    enc->frames_processed++;

    return ret;
}

/* Plugin initialization */
static gboolean
plugin_init(GstPlugin *plugin)
{
    return gst_element_register(plugin, "lcevcenc", GST_RANK_NONE, GST_TYPE_LCEVC_ENC);
}

#ifndef PACKAGE
#define PACKAGE "lcevc-encoder-package"
#endif

GST_PLUGIN_DEFINE(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    lcevcenc,
    "LCEVC Encoder",
    plugin_init,
    "1.0",
    "LGPL",
    "gst-lcevc-enc",
    "https://example.com"
)