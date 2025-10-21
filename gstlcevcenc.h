#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideoencoder.h>

G_BEGIN_DECLS

#define GST_TYPE_LCEVC_ENC (gst_lcevc_enc_get_type())
#define GST_LCEVC_ENC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_LCEVC_ENC, GstLcevcEnc))
#define GST_LCEVC_ENC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_LCEVC_ENC, GstLcevcEncClass))
#define GST_IS_LCEVC_ENC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_LCEVC_ENC))
#define GST_IS_LCEVC_ENC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_LCEVC_ENC))

// Complete LCEVC structure definitions
typedef struct _LcevcContext {
    gint width;
    gint height;
    GstVideoFormat format;
    gint enhancement_layers;
    gint bitrate;
    gfloat quality;
} LcevcContext;

typedef struct _LcevcEncodedResult {
    guint8 *data;
    gsize data_size;
    gboolean keyframe;
} LcevcEncodedResult;

typedef struct _GstLcevcEnc GstLcevcEnc;
typedef struct _GstLcevcEncClass GstLcevcEncClass;

struct _GstLcevcEnc {
    GstVideoEncoder parent;
    
    // Encoding parameters
    gint base_width;
    gint base_height;
    gint enhancement_layers;
    GstVideoFormat base_format;
    
    // Encoder state
    gboolean initialized;
    LcevcContext *lcevc_context;
    
    // Configuration
    gint bitrate;
    gfloat quality;
    gboolean two_pass;
    
    // Pads
    GstPad *sink_main;
    GstPad *sink_secondary;
    
    // Statistics
    guint64 frames_processed;
    GstClockTime processing_time;
};

struct _GstLcevcEncClass {
    GstVideoEncoderClass parent_class;
};

// LCEVC stub function declarations
gboolean lcevc_encoder_init(LcevcContext **ctx, gint bitrate, gfloat quality);
void lcevc_encoder_cleanup(LcevcContext *ctx);
void lcevc_encoder_configure(LcevcContext *ctx, gint width, gint height, 
                           GstVideoFormat format, gint enhancement_layers);
gboolean lcevc_encode_frame(LcevcContext *ctx, GstBuffer *input_buffer, 
                          LcevcEncodedResult *result);

GType gst_lcevc_enc_get_type(void);

G_END_DECLS