#include "gstlcevcenc.h"
#include <string.h>
#include <memory>

// Add the correct GStreamer video encoder header
#include <gst/video/gstvideoencoder.h>

GST_DEBUG_CATEGORY_STATIC(gst_lcevc_enc_debug);
#define GST_CAT_DEFAULT gst_lcevc_enc_debug

// Properties
enum {
    PROP_0,
    PROP_QP,
    PROP_BASE_QP,
    PROP_STEP_WIDTH_LOQ1,
    PROP_STEP_WIDTH_LOQ2,
    PROP_BASE_ENCODER,
    PROP_TRANSFORM_TYPE,
    PROP_PRIORITY_MODE,
    PROP_TEMPORAL_ENABLED,
    PROP_ENHANCEMENT_ENABLED,
    PROP_BASE_DEPTH,
    PROP_ENHANCEMENT_DEPTH,
    PROP_FPS
};

// Default values
#define DEFAULT_QP 28
#define DEFAULT_BASE_QP 28
#define DEFAULT_STEP_WIDTH_LOQ1 32767
#define DEFAULT_STEP_WIDTH_LOQ2 1500
#define DEFAULT_BASE_ENCODER "hevc"
#define DEFAULT_TRANSFORM_TYPE "dds"
#define DEFAULT_PRIORITY_MODE "mode_2_0"
#define DEFAULT_TEMPORAL_ENABLED TRUE
#define DEFAULT_ENHANCEMENT_ENABLED TRUE
#define DEFAULT_BASE_DEPTH 10
#define DEFAULT_ENHANCEMENT_DEPTH 10
#define DEFAULT_FPS 30

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE(
    "sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS(
        "video/x-raw, "
        "format = (string) { I420, I422, I444, Y42B, Y444 }, "
        "width = (int) [ 16, 7680 ], "
        "height = (int) [ 16, 4320 ], "
        "framerate = (fraction) [ 0/1, 2147483647/1 ]"
    )
);

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE(
    "src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS("video/x-lcevc")
);

#define gst_lcevc_enc_parent_class parent_class
G_DEFINE_TYPE(GstLcevcEnc, gst_lcevc_enc, GST_TYPE_VIDEO_ENCODER);

// Forward declarations for static functions
static void gst_lcevc_enc_set_property(GObject *obj, guint prop_id,
    const GValue *val, GParamSpec *pspec);
static void gst_lcevc_enc_get_property(GObject *obj, guint prop_id,
    GValue *val, GParamSpec *pspec);
static void gst_lcevc_enc_finalize(GObject *obj);
static gboolean gst_lcevc_enc_start(GstVideoEncoder *enc);
static gboolean gst_lcevc_enc_stop(GstVideoEncoder *enc);
static gboolean gst_lcevc_enc_set_format(GstVideoEncoder *enc,
    GstVideoCodecState *state);
static GstFlowReturn gst_lcevc_enc_handle_frame(GstVideoEncoder *enc,
    GstVideoCodecFrame *frame);
static GstFlowReturn gst_lcevc_enc_finish(GstVideoEncoder *enc);
static gboolean gst_lcevc_enc_propose_allocation(GstVideoEncoder *encoder,
    GstQuery *query);

static void gst_lcevc_enc_class_init(GstLcevcEncClass *klass) {
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    GstVideoEncoderClass *encoder_class = GST_VIDEO_ENCODER_CLASS(klass);
    
    gobject_class->set_property = gst_lcevc_enc_set_property;
    gobject_class->get_property = gst_lcevc_enc_get_property;
    gobject_class->finalize = gst_lcevc_enc_finalize;
    
    encoder_class->start = GST_DEBUG_FUNCPTR(gst_lcevc_enc_start);
    encoder_class->stop = GST_DEBUG_FUNCPTR(gst_lcevc_enc_stop);
    encoder_class->set_format = GST_DEBUG_FUNCPTR(gst_lcevc_enc_set_format);
    encoder_class->handle_frame = GST_DEBUG_FUNCPTR(gst_lcevc_enc_handle_frame);
    encoder_class->finish = GST_DEBUG_FUNCPTR(gst_lcevc_enc_finish);
    encoder_class->propose_allocation = GST_DEBUG_FUNCPTR(gst_lcevc_enc_propose_allocation);
    
    // Install properties
    g_object_class_install_property(gobject_class, PROP_QP,
        g_param_spec_uint("qp", "QP", "Quantization parameter",
            0, 51, DEFAULT_QP, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    
    g_object_class_install_property(gobject_class, PROP_BASE_QP,
        g_param_spec_uint("base-qp", "Base QP", "Base encoder QP",
            0, 51, DEFAULT_BASE_QP, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    
    g_object_class_install_property(gobject_class, PROP_STEP_WIDTH_LOQ1,
        g_param_spec_uint("step-width-loq1", "Step Width LOQ1", 
            "Step width for level 1", 200, 32767, DEFAULT_STEP_WIDTH_LOQ1,
            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    
    g_object_class_install_property(gobject_class, PROP_STEP_WIDTH_LOQ2,
        g_param_spec_uint("step-width-loq2", "Step Width LOQ2",
            "Step width for level 2", 200, 32767, DEFAULT_STEP_WIDTH_LOQ2,
            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    
    g_object_class_install_property(gobject_class, PROP_BASE_ENCODER,
        g_param_spec_string("base-encoder", "Base Encoder",
            "Base codec (avc, hevc, vvc, evc)", DEFAULT_BASE_ENCODER,
            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    
    g_object_class_install_property(gobject_class, PROP_TRANSFORM_TYPE,
        g_param_spec_string("transform-type", "Transform Type",
            "Transform type (dd, dds)", DEFAULT_TRANSFORM_TYPE,
            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    
    g_object_class_install_property(gobject_class, PROP_PRIORITY_MODE,
        g_param_spec_string("priority-mode", "Priority Mode",
            "Priority map mode", DEFAULT_PRIORITY_MODE,
            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    
    g_object_class_install_property(gobject_class, PROP_TEMPORAL_ENABLED,
        g_param_spec_boolean("temporal-enabled", "Temporal Enabled",
            "Enable temporal prediction", DEFAULT_TEMPORAL_ENABLED,
            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    
    g_object_class_install_property(gobject_class, PROP_ENHANCEMENT_ENABLED,
        g_param_spec_boolean("enhancement-enabled", "Enhancement Enabled",
            "Enable enhancement layers", DEFAULT_ENHANCEMENT_ENABLED,
            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    
    g_object_class_install_property(gobject_class, PROP_BASE_DEPTH,
        g_param_spec_uint("base-depth", "Base Depth",
            "Base bit depth", 8, 14, DEFAULT_BASE_DEPTH,
            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    
    g_object_class_install_property(gobject_class, PROP_ENHANCEMENT_DEPTH,
        g_param_spec_uint("enhancement-depth", "Enhancement Depth",
            "Enhancement bit depth", 8, 14, DEFAULT_ENHANCEMENT_DEPTH,
            (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    
    g_object_class_install_property(gobject_class, PROP_FPS,
        g_param_spec_uint("fps", "FPS", "Frame rate",
            1, 120, DEFAULT_FPS, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
    
    gst_element_class_set_static_metadata(element_class,
        "LCEVC Encoder",
        "Codec/Encoder/Video",
        "Encodes video using LCEVC (Low Complexity Enhancement Video Coding)",
        "Erwan Le Blond <erwanleblond@gmail.com>");
    
    gst_element_class_add_static_pad_template(element_class, &sink_template);
    gst_element_class_add_static_pad_template(element_class, &src_template);
    
    GST_DEBUG_CATEGORY_INIT(gst_lcevc_enc_debug, "lcevcenc", 0, "LCEVC Encoder");
}

static void gst_lcevc_enc_init(GstLcevcEnc *enc) {
    enc->encoder = nullptr;
    enc->params = nullptr;
    enc->qp = DEFAULT_QP;
    enc->base_qp = DEFAULT_BASE_QP;
    enc->step_width_loq1 = DEFAULT_STEP_WIDTH_LOQ1;
    enc->step_width_loq2 = DEFAULT_STEP_WIDTH_LOQ2;
    enc->base_encoder = g_strdup(DEFAULT_BASE_ENCODER);
    enc->transform_type = g_strdup(DEFAULT_TRANSFORM_TYPE);
    enc->priority_mode = g_strdup(DEFAULT_PRIORITY_MODE);
    enc->temporal_enabled = DEFAULT_TEMPORAL_ENABLED;
    enc->enhancement_enabled = DEFAULT_ENHANCEMENT_ENABLED;
    enc->base_depth = DEFAULT_BASE_DEPTH;
    enc->enhancement_depth = DEFAULT_ENHANCEMENT_DEPTH;
    enc->fps = DEFAULT_FPS;
    enc->input_state = nullptr;
    enc->frame_count = 0;
}

static void gst_lcevc_enc_finalize(GObject *obj) {
    GstLcevcEnc *enc = GST_LCEVC_ENC(obj);
    
    g_free(enc->base_encoder);
    g_free(enc->transform_type);
    g_free(enc->priority_mode);
    
    if (enc->encoder) {
        delete enc->encoder;
        enc->encoder = nullptr;
    }
    
    if (enc->params) {
        delete enc->params;
        enc->params = nullptr;
    }
    
    G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void gst_lcevc_enc_set_property(GObject *obj, guint prop_id,
    const GValue *val, GParamSpec *pspec) {
    GstLcevcEnc *enc = GST_LCEVC_ENC(obj);
    
    switch (prop_id) {
        case PROP_QP:
            enc->qp = g_value_get_uint(val);
            break;
        case PROP_BASE_QP:
            enc->base_qp = g_value_get_uint(val);
            break;
        case PROP_STEP_WIDTH_LOQ1:
            enc->step_width_loq1 = g_value_get_uint(val);
            break;
        case PROP_STEP_WIDTH_LOQ2:
            enc->step_width_loq2 = g_value_get_uint(val);
            break;
        case PROP_BASE_ENCODER:
            g_free(enc->base_encoder);
            enc->base_encoder = g_value_dup_string(val);
            break;
        case PROP_TRANSFORM_TYPE:
            g_free(enc->transform_type);
            enc->transform_type = g_value_dup_string(val);
            break;
        case PROP_PRIORITY_MODE:
            g_free(enc->priority_mode);
            enc->priority_mode = g_value_dup_string(val);
            break;
        case PROP_TEMPORAL_ENABLED:
            enc->temporal_enabled = g_value_get_boolean(val);
            break;
        case PROP_ENHANCEMENT_ENABLED:
            enc->enhancement_enabled = g_value_get_boolean(val);
            break;
        case PROP_BASE_DEPTH:
            enc->base_depth = g_value_get_uint(val);
            break;
        case PROP_ENHANCEMENT_DEPTH:
            enc->enhancement_depth = g_value_get_uint(val);
            break;
        case PROP_FPS:
            enc->fps = g_value_get_uint(val);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void gst_lcevc_enc_get_property(GObject *obj, guint prop_id,
    GValue *val, GParamSpec *pspec) {
    GstLcevcEnc *enc = GST_LCEVC_ENC(obj);
    
    switch (prop_id) {
        case PROP_QP:
            g_value_set_uint(val, enc->qp);
            break;
        case PROP_BASE_QP:
            g_value_set_uint(val, enc->base_qp);
            break;
        case PROP_STEP_WIDTH_LOQ1:
            g_value_set_uint(val, enc->step_width_loq1);
            break;
        case PROP_STEP_WIDTH_LOQ2:
            g_value_set_uint(val, enc->step_width_loq2);
            break;
        case PROP_BASE_ENCODER:
            g_value_set_string(val, enc->base_encoder);
            break;
        case PROP_TRANSFORM_TYPE:
            g_value_set_string(val, enc->transform_type);
            break;
        case PROP_PRIORITY_MODE:
            g_value_set_string(val, enc->priority_mode);
            break;
        case PROP_TEMPORAL_ENABLED:
            g_value_set_boolean(val, enc->temporal_enabled);
            break;
        case PROP_ENHANCEMENT_ENABLED:
            g_value_set_boolean(val, enc->enhancement_enabled);
            break;
        case PROP_BASE_DEPTH:
            g_value_set_uint(val, enc->base_depth);
            break;
        case PROP_ENHANCEMENT_DEPTH:
            g_value_set_uint(val, enc->enhancement_depth);
            break;
        case PROP_FPS:
            g_value_set_uint(val, enc->fps);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static gboolean gst_lcevc_enc_start(GstVideoEncoder *encoder) {
    GstLcevcEnc *enc = GST_LCEVC_ENC(encoder);
    
    GST_DEBUG_OBJECT(enc, "Starting encoder");
    enc->frame_count = 0;
    enc->frame_buffer.clear();
    
    return TRUE;
}

static gboolean gst_lcevc_enc_stop(GstVideoEncoder *encoder) {
    GstLcevcEnc *enc = GST_LCEVC_ENC(encoder);
    
    GST_DEBUG_OBJECT(enc, "Stopping encoder");
    
    if (enc->encoder) {
        delete enc->encoder;
        enc->encoder = nullptr;
    }
    
    if (enc->params) {
        delete enc->params;
        enc->params = nullptr;
    }
    
    if (enc->input_state) {
        gst_video_codec_state_unref(enc->input_state);
        enc->input_state = nullptr;
    }
    
    enc->frame_buffer.clear();
    
    return TRUE;
}

static gboolean gst_lcevc_enc_propose_allocation(GstVideoEncoder *encoder,
    GstQuery *query) {
    // Propose input buffer pool - use the base class implementation
    return GST_VIDEO_ENCODER_CLASS(parent_class)->propose_allocation(encoder, query);
}

// Function to convert GstVideoFormat to ImageFormat
lctm::ImageFormat gst_video_format_to_image_format(GstVideoFormat format) {
    switch (format) {
        case GST_VIDEO_FORMAT_I420:
            return lctm::IMAGE_FORMAT_YUV420P8;
        case GST_VIDEO_FORMAT_Y42B:
            return lctm::IMAGE_FORMAT_YUV422P8;
        case GST_VIDEO_FORMAT_Y444:
            return lctm::IMAGE_FORMAT_YUV444P8;
        case GST_VIDEO_FORMAT_I420_10LE:
            return lctm::IMAGE_FORMAT_YUV420P10;
        case GST_VIDEO_FORMAT_I422_10LE:
            return lctm::IMAGE_FORMAT_YUV422P10;
        case GST_VIDEO_FORMAT_Y444_10LE:
            return lctm::IMAGE_FORMAT_YUV444P10;
        default:
            GST_WARNING("Unsupported video format: %s", gst_video_format_to_string(format));
            return lctm::IMAGE_FORMAT_NONE;
    }
}

static gboolean gst_lcevc_enc_set_format(GstVideoEncoder *encoder,
    GstVideoCodecState *state) {
    GstLcevcEnc *enc = GST_LCEVC_ENC(encoder);
    GstVideoInfo *info = &state->info;

    // Validate input format
    if (!GST_VIDEO_INFO_IS_YUV(info)) {
        GST_ERROR_OBJECT(enc, "Only YUV formats are supported");
        return FALSE;
    }
    
    GST_DEBUG_OBJECT(enc, "Setting format: %dx%d @ %d/%d fps",
        GST_VIDEO_INFO_WIDTH(info), GST_VIDEO_INFO_HEIGHT(info),
        GST_VIDEO_INFO_FPS_N(info), GST_VIDEO_INFO_FPS_D(info));
    
    if (enc->input_state)
        gst_video_codec_state_unref(enc->input_state);
    enc->input_state = gst_video_codec_state_ref(state);
    
    // Convert GStreamer format to LCEVC format
    lctm::ImageFormat image_format = gst_video_format_to_image_format(GST_VIDEO_INFO_FORMAT(info));
    if (image_format == lctm::IMAGE_FORMAT_NONE) {
        GST_ERROR_OBJECT(enc, "Unsupported format");
        return FALSE;
    }
    
    // Create image description
    enc->src_desc = lctm::ImageDescription(
        image_format,
        GST_VIDEO_INFO_WIDTH(info),
        GST_VIDEO_INFO_HEIGHT(info)
    );
    
    // Build parameters
    auto pb = lctm::Parameters::build();
    pb.set("qp", enc->qp);
    pb.set("base_encoder", std::string(enc->base_encoder));
    pb.set("encoding_transform_type", std::string(enc->transform_type));
    pb.set("priority_mode", std::string(enc->priority_mode));
    pb.set("temporal_enabled", enc->temporal_enabled);
    pb.set("enhancement", enc->enhancement_enabled);
    pb.set("base_depth", enc->base_depth);
    pb.set("cq_step_width_loq_1", enc->step_width_loq1);
    pb.set("cq_step_width_loq_0", enc->step_width_loq2);
    pb.set("fps", enc->fps);
    pb.set("width", GST_VIDEO_INFO_WIDTH(info));
    pb.set("height", GST_VIDEO_INFO_HEIGHT(info));
    
    if (enc->params)
        delete enc->params;
    enc->params = new lctm::Parameters(pb.finish());
    
    // Create encoder
    try {
        if (enc->encoder)
            delete enc->encoder;
        enc->encoder = new lctm::Encoder(enc->src_desc, *enc->params);
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(enc, "Failed to create encoder: %s", e.what());
        return FALSE;
    }
    
    // Set output caps
    GstCaps *outcaps = gst_caps_new_simple("video/x-lcevc",
        "width", G_TYPE_INT, GST_VIDEO_INFO_WIDTH(info),
        "height", G_TYPE_INT, GST_VIDEO_INFO_HEIGHT(info),
        "framerate", GST_TYPE_FRACTION, 
            GST_VIDEO_INFO_FPS_N(info), GST_VIDEO_INFO_FPS_D(info),
        nullptr);
    
    GstVideoCodecState *output_state = 
        gst_video_encoder_set_output_state(encoder, outcaps, state);
    gst_video_codec_state_unref(output_state);
    
    return TRUE;
}

// Create a simple image using the available constructors
std::shared_ptr<lctm::Image> create_simple_image(const std::string& name, 
                                                GstVideoCodecFrame* frame, 
                                                GstVideoInfo* video_info,
                                                uint64_t timestamp) {
    if (!frame || !video_info || !frame->input_buffer) {
        return nullptr;
    }

    // Convert GStreamer format to LCEVC format
    lctm::ImageFormat image_format = gst_video_format_to_image_format(video_info->finfo->format);
    if (image_format == lctm::IMAGE_FORMAT_NONE) {
        return nullptr;
    }

    // Map the GStreamer buffer
    GstMapInfo map_info;
    if (!gst_buffer_map(frame->input_buffer, &map_info, GST_MAP_READ)) {
        GST_ERROR("Failed to map GStreamer buffer");
        return nullptr;
    }

    try {
        // Create image description
        lctm::ImageDescription desc(image_format, video_info->width, video_info->height);
        
        // For now, create a simple image without planes
        // This will need to be adapted based on the actual LCEVC API
        auto image = std::make_shared<lctm::Image>();
        
        gst_buffer_unmap(frame->input_buffer, &map_info);
        return image;
        
    } catch (const std::exception& e) {
        GST_ERROR("Error creating image: %s", e.what());
        gst_buffer_unmap(frame->input_buffer, &map_info);
        return nullptr;
    }
}

static GstFlowReturn gst_lcevc_enc_handle_frame(GstVideoEncoder *encoder,
    GstVideoCodecFrame *frame) {
    GstLcevcEnc *enc = GST_LCEVC_ENC(encoder);
    
    if (!enc->encoder) {
        GST_ERROR_OBJECT(enc, "Encoder not initialized");
        gst_video_encoder_finish_frame(encoder, frame);
        return GST_FLOW_ERROR;
    }
    
    // For now, create a minimal implementation that passes through data
    // This is a placeholder until we figure out the correct LCEVC API usage
    
    try {
        // Create output buffer - for now just copy input to output
        GstBuffer *outbuf = gst_buffer_copy(frame->input_buffer);
        
        // Set output frame properties
        frame->output_buffer = outbuf;
        frame->dts = frame->pts;
        
        GST_LOG_OBJECT(enc, "Pass-through frame %d", enc->frame_count++);
        
    } catch (const std::exception &e) {
        GST_ERROR_OBJECT(enc, "Processing failed: %s", e.what());
        gst_video_encoder_finish_frame(encoder, frame);
        return GST_FLOW_ERROR;
    }
    
    return gst_video_encoder_finish_frame(encoder, frame);
}

static GstFlowReturn gst_lcevc_enc_finish(GstVideoEncoder *encoder) {
    GstLcevcEnc *enc = GST_LCEVC_ENC(encoder);
    
    GST_DEBUG_OBJECT(enc, "Finish encoding");
    
    return GST_FLOW_OK;
}

// Plugin registration
static gboolean plugin_init(GstPlugin *plugin) {
    return gst_element_register(plugin, "lcevcenc", GST_RANK_PRIMARY,
        GST_TYPE_LCEVC_ENC);
}

// Define PACKAGE before including the plugin macro
#ifndef PACKAGE
#define PACKAGE "lcevcenc"
#endif

GST_PLUGIN_DEFINE(
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    lcevcenc,
    "LCEVC Encoder Plugin",
    plugin_init,
    "1.0",
    "LGPL",
    "LCEVC",
    "http://lcevc.org/"
)
