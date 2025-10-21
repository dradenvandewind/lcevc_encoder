#ifndef __GST_LCEVC_ENC_H__
#define __GST_LCEVC_ENC_H__

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/video/gstvideoencoder.h>

// Use only system LCEVC headers
#include <Encoder.hpp>
#include <Parameters.hpp>
#include <Image.hpp>
#include <Surface.hpp>

#include <memory>
#include <vector>
#include <string>

G_BEGIN_DECLS

#define GST_TYPE_LCEVC_ENC (gst_lcevc_enc_get_type())
#define GST_LCEVC_ENC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_LCEVC_ENC,GstLcevcEnc))
#define GST_LCEVC_ENC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_LCEVC_ENC,GstLcevcEncClass))
#define GST_IS_LCEVC_ENC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_LCEVC_ENC))
#define GST_IS_LCEVC_ENC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_LCEVC_ENC))

typedef struct _GstLcevcEnc GstLcevcEnc;
typedef struct _GstLcevcEncClass GstLcevcEncClass;

struct _GstLcevcEnc {
    GstVideoEncoder parent;
    
    // LCEVC encoder
    lctm::Encoder *encoder;
    lctm::Parameters *params;
    
    // Configuration
    guint qp;
    guint base_qp;
    guint step_width_loq1;
    guint step_width_loq2;
    gchar *base_encoder;
    gchar *transform_type;
    gchar *priority_mode;
    gboolean temporal_enabled;
    gboolean enhancement_enabled;
    guint base_depth;
    guint enhancement_depth;
    guint fps;
    
    // State
    GstVideoCodecState *input_state;
    lctm::ImageDescription src_desc;
    gint frame_count;
    std::vector<std::unique_ptr<lctm::Image>> frame_buffer;
};

struct _GstLcevcEncClass {
    GstVideoEncoderClass parent_class;
};

GType gst_lcevc_enc_get_type(void);

// Helper function declarations
lctm::ImageFormat gst_video_format_to_image_format(GstVideoFormat format);
std::shared_ptr<lctm::Image> create_image_from_gst_frame(const std::string& name, 
                                                       GstVideoCodecFrame* frame, 
                                                       GstVideoInfo* video_info,
                                                       uint64_t timestamp);

G_END_DECLS

#endif /* __GST_LCEVC_ENC_H__ */