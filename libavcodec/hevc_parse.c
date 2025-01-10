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
#include "h2645_parse.h"
#include "hevc.h"
#include "hevc_parse.h"

#include "libavutil/hdr_dynamic_vivid_metadata.h"
#include "libavutil/mastering_display_metadata.h"

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

int ff_hevc_decode_extradata(const uint8_t *data, int size, HEVCParamSets *ps,
                             HEVCSEI *sei, int *is_nalff, int *nal_length_size,
                             int err_recognition, int apply_defdispwin, void *logctx)
{
    int ret = 0;
    GetByteContext gb;

    bytestream2_init(&gb, data, size);

    if (size > 3 && (data[0] || data[1] || data[2] > 1)) {
        /* It seems the extradata is encoded as hvcC format.
         * Temporarily, we support configurationVersion==0 until 14496-15 3rd
         * is finalized. When finalized, configurationVersion will be 1 and we
         * can recognize hvcC by checking if avctx->extradata[0]==1 or not. */
        int i, j, num_arrays, nal_len_size;

        *is_nalff = 1;

        bytestream2_skip(&gb, 21);
        nal_len_size = (bytestream2_get_byte(&gb) & 3) + 1;
        num_arrays   = bytestream2_get_byte(&gb);

        /* nal units in the hvcC always have length coded with 2 bytes,
         * so put a fake nal_length_size = 2 while parsing them */
        *nal_length_size = 2;

        /* Decode nal units from hvcC. */
        for (i = 0; i < num_arrays; i++) {
            int type = bytestream2_get_byte(&gb) & 0x3f;
            int cnt  = bytestream2_get_be16(&gb);

            for (j = 0; j < cnt; j++) {
                // +2 for the nal size field
                int nalsize = bytestream2_peek_be16(&gb) + 2;
                if (bytestream2_get_bytes_left(&gb) < nalsize) {
                    av_log(logctx, AV_LOG_ERROR,
                           "Invalid NAL unit size in extradata.\n");
                    return AVERROR_INVALIDDATA;
                }

                ret = hevc_decode_nal_units(gb.buffer, nalsize, ps, sei, *is_nalff,
                                            *nal_length_size, err_recognition, apply_defdispwin,
                                            logctx);
                if (ret < 0) {
                    av_log(logctx, AV_LOG_ERROR,
                           "Decoding nal unit %d %d from hvcC failed\n",
                           type, i);
                    return ret;
                }
                bytestream2_skip(&gb, nalsize);
            }
        }

        /* Now store right nal length size, that will be used to parse
         * all other nals */
        *nal_length_size = nal_len_size;
    } else {
        *is_nalff = 0;
        ret = hevc_decode_nal_units(data, size, ps, sei, *is_nalff, *nal_length_size,
                                    err_recognition, apply_defdispwin, logctx);
        if (ret < 0)
            return ret;
    }

    return ret;
}

static int hevc_decode_nal_units1(const uint8_t *buf, int buf_size, HEVCParamSets *ps,
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
    av_log(logctx, AV_LOG_ERROR, " pkt.nb_nals =  %d in extradata\n",  pkt.nb_nals);


    for (i = 0; i < pkt.nb_nals; i++) {

        H2645NAL *nal = &pkt.nals[i];
        if (nal->nuh_layer_id > 0)
            continue;
        av_log(logctx, AV_LOG_ERROR, " nal->type =  %d in extradata\n",  nal->type);
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

int av_hevc_decode_nal_units(const uint8_t *buf, int buf_size, int is_nalff, int nal_length_size,
                             int err_recognition, int apply_defdispwin, void *logctx) {
    HEVCParamSets ps;
    HEVCSEI sei;
    memset(&ps, 0, sizeof(ps));
    memset(&sei, 0, sizeof(sei));

    return hevc_decode_nal_units1(buf, buf_size, &ps, &sei, is_nalff, nal_length_size,
                                 err_recognition, apply_defdispwin, logctx);
}

int av_hevc_decode_nal_units1(const uint8_t *buf, int buf_size, HEVCParamSets *ps,
                                 HEVCSEI *sei, int is_nalff, int nal_length_size,
                             int err_recognition, int apply_defdispwin, void *logctx) {
    // HEVCParamSets ps;
    // HEVCSEI sei;
    // memset(&ps, 0, sizeof(ps));
    // memset(&sei, 0, sizeof(sei));

    return hevc_decode_nal_units1(buf, buf_size, ps, sei, is_nalff, nal_length_size,
                                 err_recognition, apply_defdispwin, logctx);
}


int av_hevc_decode_nal_units2(const uint8_t *buf, int buf_size, HEVCParamSets *ps,
                                 HEVCSEI *sei, int *is_nalff, int *nal_length_size,
                             int err_recognition, int apply_defdispwin, void *logctx) {
    // HEVCParamSets ps;
    // HEVCSEI sei;
    // memset(&ps, 0, sizeof(ps));
    // memset(&sei, 0, sizeof(sei));

    return ff_hevc_decode_extradata(buf, buf_size, ps, sei, is_nalff, nal_length_size,
                                 err_recognition, apply_defdispwin, logctx);
}


int av_hevc_decode_nal_units3(const uint8_t *buf, int buf_size, AVCodecContext *avctx, int *is_nalff, int *nal_length_size,
                             int err_recognition, int apply_defdispwin, void *logctx){
    // HEVCParamSets ps;
    // HEVCSEI sei;
    // memset(&ps, 0, sizeof(ps));
    // memset(&sei, 0, sizeof(sei));
    av_log(logctx, AV_LOG_ERROR, "av_hevc_decode_nal_units3 \n");
    HEVCContext *s = (HEVCContext *)avctx->priv_data;
    int ret = ff_hevc_decode_extradata(buf, buf_size,&s->ps, &s->sei, is_nalff, nal_length_size,
                                 err_recognition, apply_defdispwin, logctx);
    int i = 0;
    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.pps_list); i++) {
        if (s->ps.pps_list[i]) {
            av_log(logctx, AV_LOG_ERROR, "av_hevc_decode_nal_units3  pps_list[%d] != NULL \n",i);
        }
    }

    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.sps_list); i++) {
        if (s->ps.sps_list[i]) {
            av_log(logctx, AV_LOG_ERROR, "av_hevc_decode_nal_units3  sps_list[%d] != NULL \n",i);
        }
    }
    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.vps_list); i++) {
        if (s->ps.vps_list[i]) {
            av_log(logctx, AV_LOG_ERROR, "av_hevc_decode_nal_units3  vps_list[%d] != NULL \n",i);
        }
    }

    av_log(logctx, AV_LOG_ERROR, "av_hevc_decode_nal_units3 ret = %d \n",ret);
    return ret;
}

static void print_int(void *logctx,const char *key, long long int val) {
    av_log(logctx, AV_LOG_ERROR, "print_dynamic_hdr_vivid %s  val = %lld \n",key,val);
}
static void print_q(void *logctx,const char *key, AVRational q,const char *sep) {
    av_log(logctx, AV_LOG_ERROR, "print_dynamic_hdr_vivid %s --- %d%c%d\n",key, q.num, sep, q.den);
}

void av_print_dynamic_hdr_vivid_metadata( void *logctx,const AVDynamicHDRVivid *metadata)
{
    if (!metadata)
        return;
    print_int(logctx,"system_start_code", metadata->system_start_code);
    print_int(logctx,"num_windows", metadata->num_windows);

    for (int n = 0; n < metadata->num_windows; n++) {
        const AVHDRVividColorTransformParams *params = &metadata->params[n];

        print_q(logctx,"minimum_maxrgb", params->minimum_maxrgb, '/');
        print_q(logctx,"average_maxrgb", params->average_maxrgb, '/');
        print_q(logctx,"variance_maxrgb", params->variance_maxrgb, '/');
        print_q(logctx,"maximum_maxrgb", params->maximum_maxrgb, '/');
    }

    for (int n = 0; n < metadata->num_windows; n++) {
        const AVHDRVividColorTransformParams *params = &metadata->params[n];

        print_int(logctx,"tone_mapping_mode_flag", params->tone_mapping_mode_flag);
        print_int(logctx,"tone_mapping_param_num", params->tone_mapping_param_num);
        if (params->tone_mapping_mode_flag) {
            for (int i = 0; i < params->tone_mapping_param_num; i++) {
                const AVHDRVividColorToneMappingParams *tm_params = &params->tm_params[i];

                print_q(logctx,"targeted_system_display_maximum_luminance",
                        tm_params->targeted_system_display_maximum_luminance, '/');
                print_int(logctx,"base_enable_flag", tm_params->base_enable_flag);
                if (tm_params->base_enable_flag) {
                    print_q(logctx,"base_param_m_p", tm_params->base_param_m_p, '/');
                    print_q(logctx,"base_param_m_m", tm_params->base_param_m_m, '/');
                    print_q(logctx,"base_param_m_a", tm_params->base_param_m_a, '/');
                    print_q(logctx,"base_param_m_b", tm_params->base_param_m_b, '/');
                    print_q(logctx,"base_param_m_n", tm_params->base_param_m_n, '/');

                    print_int(logctx,"base_param_k1", tm_params->base_param_k1);
                    print_int(logctx,"base_param_k2", tm_params->base_param_k2);
                    print_int(logctx,"base_param_k3", tm_params->base_param_k3);
                    print_int(logctx,"base_param_Delta_enable_mode",
                              tm_params->base_param_Delta_enable_mode);
                    print_q(logctx,"base_param_Delta", tm_params->base_param_Delta, '/');
                }
                print_int(logctx,"3Spline_enable_flag", tm_params->three_Spline_enable_flag);
                if (tm_params->three_Spline_enable_flag) {
                    print_int(logctx,"3Spline_num", tm_params->three_Spline_num);
                    print_int(logctx,"3Spline_TH_mode", tm_params->three_Spline_TH_mode);

                    for (int j = 0; j < tm_params->three_Spline_num; j++) {
                        print_q(logctx,"3Spline_TH_enable_MB", tm_params->three_Spline_TH_enable_MB, '/');
                        print_q(logctx,"3Spline_TH_enable", tm_params->three_Spline_TH_enable, '/');
                        print_q(logctx,"3Spline_TH_Delta1", tm_params->three_Spline_TH_Delta1, '/');
                        print_q(logctx,"3Spline_TH_Delta2", tm_params->three_Spline_TH_Delta2, '/');
                        print_q(logctx,"3Spline_enable_Strength", tm_params->three_Spline_enable_Strength, '/');
                    }
                }
            }
        }

        print_int(logctx,"color_saturation_mapping_flag", params->color_saturation_mapping_flag);
        if (params->color_saturation_mapping_flag) {
            print_int(logctx,"color_saturation_num", params->color_saturation_num);
            for (int i = 0; i < params->color_saturation_num; i++) {
                print_q(logctx,"color_saturation_gain", params->color_saturation_gain[i], '/');
            }
        }
    }
}

int av_hevc_decode_nal_units4(const uint8_t *buf, int buf_size, AVCodecContext *avctx, int is_nalff, int nal_length_size,
                             int err_recognition, int apply_defdispwin, void *logctx){
    // HEVCParamSets ps;
    // HEVCSEI sei;
    // memset(&ps, 0, sizeof(ps));
    // memset(&sei, 0, sizeof(sei));

    av_log(logctx, AV_LOG_ERROR, "av_hevc_decode_nal_units4 \n");
    int i = 0;
    HEVCContext *s = (HEVCContext *)avctx->priv_data;
    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.pps_list); i++) {
        if (s->ps.pps_list[i]) {
            av_log(logctx, AV_LOG_ERROR, "av_hevc_decode_nal_units4  pps_list[%d] != NULL \n",i);
        }
    }

    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.sps_list); i++) {
        if (s->ps.sps_list[i]) {
            av_log(logctx, AV_LOG_ERROR, "av_hevc_decode_nal_units4  sps_list[%d] != NULL \n",i);
        }
    }
    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.vps_list); i++) {
        if (s->ps.vps_list[i]) {
            av_log(logctx, AV_LOG_ERROR, "av_hevc_decode_nal_units4  vps_list[%d] != NULL \n",i);
        }
    }
    int ret = hevc_decode_nal_units1(buf, buf_size,&s->ps, &s->sei, is_nalff, nal_length_size,
                                 err_recognition, apply_defdispwin, logctx);
    if (s->sei.common.dynamic_hdr_vivid.info) {
        AVDynamicHDRVivid *metadata1 = (AVDynamicHDRVivid *)s->sei.common.dynamic_hdr_vivid.info->data;
        av_log(logctx, AV_LOG_ERROR, " metadata available %d \n",metadata1->system_start_code);
        // av_hevc_decode_nal_units8(logctx,metadata1);

    }else {
        av_log(logctx, AV_LOG_ERROR, "No metadata available\n");
    }

    return ret;
}

int av_hevc_decode_nal_units5(AVDynamicHDRVivid *metadata1,const uint8_t *buf, int buf_size, AVCodecContext *avctx, int is_nalff, int nal_length_size,
                             int err_recognition, int apply_defdispwin, void *logctx) {
    av_log(logctx, AV_LOG_ERROR, "av_hevc_decode_nal_units5 \n");
    int i = 0;
    HEVCContext *s = (HEVCContext *)avctx->priv_data;

    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.pps_list); i++) {
        if (s->ps.pps_list[i]) {
            av_log(logctx, AV_LOG_ERROR, "av_hevc_decode_nal_units5  pps_list[%d] != NULL \n", i);
        }
    }

    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.sps_list); i++) {
        if (s->ps.sps_list[i]) {
            av_log(logctx, AV_LOG_ERROR, "av_hevc_decode_nal_units5  sps_list[%d] != NULL \n", i);
        }
    }

    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.vps_list); i++) {
        if (s->ps.vps_list[i]) {
            av_log(logctx, AV_LOG_ERROR, "av_hevc_decode_nal_units5  vps_list[%d] != NULL \n", i);
        }
    }

    int ret = hevc_decode_nal_units1(buf, buf_size, &s->ps, &s->sei, is_nalff, nal_length_size,
                                     err_recognition, apply_defdispwin, logctx);

    if (s->sei.common.dynamic_hdr_vivid.info) {
        // 初始化传入的 metadata1 结构体
        memset(metadata1, 0, sizeof(AVDynamicHDRVivid));

        // 将数据从 s->sei.common.dynamic_hdr_vivid.info->data 复制到 metadata1
        memcpy(metadata1, s->sei.common.dynamic_hdr_vivid.info->data, sizeof(AVDynamicHDRVivid));

        av_log(logctx, AV_LOG_ERROR, " metadata available %d \n", metadata1->system_start_code);
        // av_hevc_decode_nal_units8(logctx, metadata1);
    } else {
        av_log(logctx, AV_LOG_ERROR, "No metadata available\n");
        return -1; // 失败
    }

    return ret; // 成功
}


int av_hevc_decode_nal_units6(AVBufferRef **side_data_buffers, enum AVFrameSideDataType *side_data_types, int *side_data_count,const uint8_t *buf, int buf_size, AVCodecContext *avctx, int is_nalff, int nal_length_size,
                             int err_recognition, int apply_defdispwin, void *logctx){
    av_log(logctx, AV_LOG_ERROR, "av_hevc_decode_nal_units5 \n");
    int i = 0;
    HEVCContext *s = (HEVCContext *)avctx->priv_data;

    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.pps_list); i++) {
        if (s->ps.pps_list[i]) {
            av_log(logctx, AV_LOG_ERROR, "av_hevc_decode_nal_units5  pps_list[%d] != NULL \n", i);
        }
    }

    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.sps_list); i++) {
        if (s->ps.sps_list[i]) {
            av_log(logctx, AV_LOG_ERROR, "av_hevc_decode_nal_units5  sps_list[%d] != NULL \n", i);
        }
    }

    for (i = 0; i < FF_ARRAY_ELEMS(s->ps.vps_list); i++) {
        if (s->ps.vps_list[i]) {
            av_log(logctx, AV_LOG_ERROR, "av_hevc_decode_nal_units5  vps_list[%d] != NULL \n", i);
        }
    }

    int ret = hevc_decode_nal_units1(buf, buf_size, &s->ps, &s->sei, is_nalff, nal_length_size,
                                     err_recognition, apply_defdispwin, logctx);

    if (s->sei.common.dynamic_hdr_vivid.info) {
        // 初始化传入的 metadata1 结构体
        AVBufferRef *buffer_ref = av_buffer_alloc(sizeof(AVDynamicHDRVivid));
        if (!buffer_ref) {
            av_log(logctx, AV_LOG_ERROR, "Failed to allocate buffer\n");
            return -1;
        }
        memset(buffer_ref->data, 0, sizeof(AVDynamicHDRVivid));
        memcpy(buffer_ref->data, s->sei.common.dynamic_hdr_vivid.info, sizeof(AVDynamicHDRVivid));
        side_data_buffers[*side_data_count] = buffer_ref;
        side_data_types[*side_data_count] = AV_FRAME_DATA_DYNAMIC_HDR_VIVID;
        (*side_data_count)++;
        // av_log(logctx, AV_LOG_ERROR, " metadata available %d \n", metadata1->system_start_code);
        // print_dynamic_hdr_vivid(logctx, metadata1);
    }else if (s->sei.mastering_display.present) {
        AVBufferRef *buffer_ref = av_buffer_alloc(sizeof(AVMasteringDisplayMetadata));
        if (!buffer_ref) {
            av_log(logctx, AV_LOG_ERROR, "Failed to allocate buffer\n");
            return -1;
        }
        memset(buffer_ref->data, 0, sizeof(AVMasteringDisplayMetadata));
        AVMasteringDisplayMetadata *metadata = (AVMasteringDisplayMetadata *)buffer_ref->data;
        const int mapping[3] = {2, 0, 1};
        const int chroma_den = 50000;
        const int luma_den = 10000;

        for (i = 0; i < 3; i++) {
            const int j = mapping[i];
            metadata->display_primaries[i][0].num = s->sei.mastering_display.display_primaries[j][0];
            metadata->display_primaries[i][0].den = chroma_den;
            metadata->display_primaries[i][1].num = s->sei.mastering_display.display_primaries[j][1];
            metadata->display_primaries[i][1].den = chroma_den;
        }
        metadata->white_point[0].num = s->sei.mastering_display.white_point[0];
        metadata->white_point[0].den = chroma_den;
        metadata->white_point[1].num = s->sei.mastering_display.white_point[1];
        metadata->white_point[1].den = chroma_den;

        metadata->max_luminance.num = s->sei.mastering_display.max_luminance;
        metadata->max_luminance.den = luma_den;
        metadata->min_luminance.num = s->sei.mastering_display.min_luminance;
        metadata->min_luminance.den = luma_den;
        metadata->has_luminance = 1;
        metadata->has_primaries = 1;

        // memcpy(buffer_ref->data, s->sei.mastering_display, sizeof(AVMasteringDisplayMetadata));
        side_data_buffers[*side_data_count] = buffer_ref;
        side_data_types[*side_data_count] = AV_FRAME_DATA_MASTERING_DISPLAY_METADATA;
        (*side_data_count)++;
    }else if (s->sei.content_light.present) {
        AVBufferRef *buffer_ref = av_buffer_alloc(sizeof(AVContentLightMetadata));
        if (!buffer_ref) {
            av_log(logctx, AV_LOG_ERROR, "Failed to allocate buffer\n");
            return -1;
        }
        memset(buffer_ref->data, 0, sizeof(AVContentLightMetadata));
        AVContentLightMetadata *metadata = (AVContentLightMetadata *)buffer_ref->data;

        metadata->MaxCLL = s->sei.content_light.max_content_light_level;
        metadata->MaxFALL = s->sei.content_light.max_pic_average_light_level;

        // memcpy(buffer_ref->data, s->sei.mastering_display, sizeof(AVMasteringDisplayMetadata));
        side_data_buffers[*side_data_count] = buffer_ref;
        side_data_types[*side_data_count] = AV_FRAME_DATA_CONTENT_LIGHT_LEVEL;
        (*side_data_count)++;
    }else {
        av_log(logctx, AV_LOG_ERROR, "No metadata available\n");
        return -1; // 失败
    }

    return ret; // 成功
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

int av_hevc_decode_sidedata(AVBufferRef **side_data_buffers, enum AVFrameSideDataType *side_data_types, int *side_data_count,
                             const uint8_t *buf, int buf_size, AVCodecContext *avctx, int is_nalff, int nal_length_size,
                             int err_recognition, int apply_defdispwin, void *logctx) {
    HEVCContext *s = (HEVCContext *)avctx->priv_data;
    int ret = hevc_decode_nal_units1(buf, buf_size, &s->ps, &s->sei, is_nalff, nal_length_size,
                                     err_recognition, apply_defdispwin, logctx);

    if (s->sei.common.dynamic_hdr_vivid.info) {
        ret = add_side_data(side_data_buffers, side_data_types, side_data_count,
                            sizeof(AVDynamicHDRVivid), s->sei.common.dynamic_hdr_vivid.info->data,
                            AV_FRAME_DATA_DYNAMIC_HDR_VIVID, logctx);
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
                            AV_FRAME_DATA_MASTERING_DISPLAY_METADATA, logctx);
        if (ret < 0) return ret;
    }
    if (s->sei.content_light.present) {
        AVContentLightMetadata metadata = {0};

        metadata.MaxCLL = s->sei.content_light.max_content_light_level;
        metadata.MaxFALL = s->sei.content_light.max_pic_average_light_level;

        ret = add_side_data(side_data_buffers, side_data_types, side_data_count,
                            sizeof(AVContentLightMetadata), &metadata,
                            AV_FRAME_DATA_CONTENT_LIGHT_LEVEL, logctx);
        if (ret < 0) return ret;
    }

    return ret; // 成功
}
