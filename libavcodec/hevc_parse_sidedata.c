/*
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "bytestream.h"
#include "hevc_parse_sidedata.h"
#include "h2645_parse.h"
#include "hevc.h"
#include "hevc_parse.h"

//#include ""

#include "hevcdec.h"

static int hevc_decode_nal_units(const uint8_t *buf, int buf_size, HEVCParamSets *ps,
                                 HEVCSEI *sei, int is_nalff, int nal_length_size,
                                 int err_recognition, int apply_defdispwin, void *logctx)
{
    int i;
    int ret = 0;
    H2645Packet pkt = { 0 };

    ret = ff_h2645_packet_split(&pkt, buf, buf_size, logctx, is_nalff,
                                nal_length_size, AV_CODEC_ID_HEVC, 1, 0);
    if (ret < 0) {
        goto done;
    }

    for (i = 0; i < pkt.nb_nals; i++) {
        H2645NAL *nal = &pkt.nals[i];
        if (nal->nuh_layer_id > 0)
            continue;

        /* ignore everything except parameter sets and VCL NALUs */
        switch (nal->type) {
        case HEVC_NAL_VPS:
            ret = ff_hevc_decode_nal_vps(&nal->gb, logctx, ps);
            if (ret < 0)
                goto done;
            break;
        case HEVC_NAL_SPS:
            ret = ff_hevc_decode_nal_sps(&nal->gb, logctx, ps, apply_defdispwin);
            if (ret < 0)
                goto done;
            break;
        case HEVC_NAL_PPS:
            ret = ff_hevc_decode_nal_pps(&nal->gb, logctx, ps);
            if (ret < 0)
                goto done;
            break;
        case HEVC_NAL_SEI_PREFIX:
        case HEVC_NAL_SEI_SUFFIX:
            ret = ff_hevc_decode_nal_sei(&nal->gb, logctx, sei, ps, nal->type);
            if (ret < 0)
                goto done;
            break;
        default:
            av_log(logctx, AV_LOG_VERBOSE, "Ignoring NAL type %d in extradata\n", nal->type);
            break;
        }
    }

done:
    ff_h2645_packet_uninit(&pkt);
    if (err_recognition & AV_EF_EXPLODE)
        return ret;

    return 0;
}

static int add_side_data(AVBufferRef **side_data_buffers, enum AVFrameSideDataType *side_data_types, int *side_data_count,
                         size_t data_size, void *src_data, enum AVFrameSideDataType type, void *logctx) {
    AVBufferRef *buffer_ref = av_buffer_alloc(data_size);
    if (!buffer_ref) {
        av_log(logctx, AV_LOG_ERROR, "Failed to allocate buffer\n");
        return -1;
    }
    // memset(buffer_ref->data, 0, data_size);
    memcpy(buffer_ref->data, src_data, data_size);

    side_data_buffers[*side_data_count] = buffer_ref;
    side_data_types[*side_data_count] = type;
    (*side_data_count)++;
    return 0;
}


int av_hevc_decode_extradata(AVBufferRef **side_data_buffers, enum AVFrameSideDataType *side_data_types, int *side_data_count,AVCodecContext *avctx, AVPacket *pkt,int *is_nalff, int *nal_length_size,int *extradata_parsed){
    av_log(avctx, AV_LOG_ERROR, "av_hevc_decode_extradata \n");
    if (!side_data_buffers || !side_data_types || !side_data_count || !avctx || !pkt) {
        av_log(avctx, AV_LOG_ERROR, "Invalid input parameters\n");
        return AVERROR(EINVAL);
    }
    int err_recognition = 0;
    int apply_defdispwin = 1;
    int ret = 0;
    HEVCContext *s = (HEVCContext *)avctx->priv_data;
    if (!*extradata_parsed) {
        ret = ff_hevc_decode_extradata(avctx->extradata, avctx->extradata_size,&s->ps, &s->sei, is_nalff, nal_length_size,
                                 err_recognition, apply_defdispwin, avctx);
        int i = 0;
        for (i = 0; i < FF_ARRAY_ELEMS(s->ps.pps_list); i++) {
            if (s->ps.pps_list[i]) {
                av_log(avctx, AV_LOG_DEBUG, "av_hevc_decode_extradata  pps_list[%d] != NULL \n",i);
            }
        }

        for (i = 0; i < FF_ARRAY_ELEMS(s->ps.sps_list); i++) {
            if (s->ps.sps_list[i]) {
                av_log(avctx, AV_LOG_DEBUG, "av_hevc_decode_extradata  sps_list[%d] != NULL \n",i);
            }
        }
        for (i = 0; i < FF_ARRAY_ELEMS(s->ps.vps_list); i++) {
            if (s->ps.vps_list[i]) {
                av_log(avctx, AV_LOG_DEBUG, "av_hevc_decode_extradata  vps_list[%d] != NULL \n",i);
            }
        }

        if (ret < 0) {
            return ret;
        }
    }

    *extradata_parsed = 1;

    ret = hevc_decode_nal_units(pkt->data, pkt->size, &s->ps, &s->sei, *is_nalff, *nal_length_size,
                                     err_recognition, apply_defdispwin, avctx);

    if (s->sei.common.dynamic_hdr_vivid.info) {
        ret = add_side_data(side_data_buffers, side_data_types, side_data_count,
                            sizeof(AVDynamicHDRVivid), s->sei.common.dynamic_hdr_vivid.info->data,
                            AV_FRAME_DATA_DYNAMIC_HDR_VIVID, avctx);
        if (ret < 0) return ret;
    }

    if (s->sei.mastering_display.present) {
        AVMasteringDisplayMetadata metadata = {0};
        const int mapping[3] = {2, 0, 1};
        const int chroma_den = 50000;
        const int luma_den = 10000;

        for (int i = 0; i < 3; i++) {
            const int j = mapping[i];
            metadata.display_primaries[i][0].num = s->sei.mastering_display.display_primaries[j][0];
            metadata.display_primaries[i][0].den = chroma_den;
            metadata.display_primaries[i][1].num = s->sei.mastering_display.display_primaries[j][1];
            metadata.display_primaries[i][1].den = chroma_den;
        }
        metadata.white_point[0].num = s->sei.mastering_display.white_point[0];
        metadata.white_point[0].den = chroma_den;
        metadata.white_point[1].num = s->sei.mastering_display.white_point[1];
        metadata.white_point[1].den = chroma_den;

        metadata.max_luminance.num = s->sei.mastering_display.max_luminance;
        metadata.max_luminance.den = luma_den;
        metadata.min_luminance.num = s->sei.mastering_display.min_luminance;
        metadata.min_luminance.den = luma_den;
        metadata.has_luminance = 1;
        metadata.has_primaries = 1;

        ret = add_side_data(side_data_buffers, side_data_types, side_data_count,
                            sizeof(AVMasteringDisplayMetadata), &metadata,
                            AV_FRAME_DATA_MASTERING_DISPLAY_METADATA, avctx);
        if (ret < 0) return ret;
    }

    if (s->sei.content_light.present) {
        AVContentLightMetadata metadata = {0};

        metadata.MaxCLL = s->sei.content_light.max_content_light_level;
        metadata.MaxFALL = s->sei.content_light.max_pic_average_light_level;

        ret = add_side_data(side_data_buffers, side_data_types, side_data_count,
                            sizeof(AVContentLightMetadata), &metadata,
                            AV_FRAME_DATA_CONTENT_LIGHT_LEVEL, avctx);
        if (ret < 0) return ret;
    }

    return ret;
}
