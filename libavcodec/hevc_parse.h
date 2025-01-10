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

/**
 * @file
 * H.265 parser code
 */

#ifndef AVCODEC_HEVC_PARSE_H
#define AVCODEC_HEVC_PARSE_H

#include <stdint.h>
#include "libavutil/hdr_dynamic_vivid_metadata.h"

#include "hevc_ps.h"
#include "hevc_sei.h"

int ff_hevc_decode_extradata(const uint8_t *data, int size, HEVCParamSets *ps,
                             HEVCSEI *sei, int *is_nalff, int *nal_length_size,
                             int err_recognition, int apply_defdispwin, void *logctx);

int av_hevc_decode_nal_units1(const uint8_t *buf, int buf_size, HEVCParamSets *ps,
                                 HEVCSEI *sei, int is_nalff, int nal_length_size,
                             int err_recognition, int apply_defdispwin, void *logctx);

int av_hevc_decode_nal_units2(const uint8_t *buf, int buf_size, HEVCParamSets *ps,
                                 HEVCSEI *sei, int *is_nalff, int *nal_length_size,
                             int err_recognition, int apply_defdispwin, void *logctx);

int av_hevc_decode_nal_units3(const uint8_t *buf, int buf_size, AVCodecContext *avctx, int *is_nalff, int *nal_length_size,
                             int err_recognition, int apply_defdispwin, void *logctx);

int av_hevc_decode_nal_units4(const uint8_t *buf, int buf_size, AVCodecContext *avctx, int is_nalff, int nal_length_size,
                             int err_recognition, int apply_defdispwin, void *logctx);


int av_hevc_decode_nal_units5(AVDynamicHDRVivid *metadata1,const uint8_t *buf, int buf_size, AVCodecContext *avctx, int is_nalff, int nal_length_size,
                             int err_recognition, int apply_defdispwin, void *logctx);


int av_hevc_decode_nal_units6(AVBufferRef **side_data_buffers, enum AVFrameSideDataType *side_data_types, int *side_data_count,
                             const uint8_t *buf, int buf_size, AVCodecContext *avctx, int is_nalff, int nal_length_size,
                             int err_recognition, int apply_defdispwin, void *logctx);

int av_hevc_decode_sidedata(AVBufferRef **side_data_buffers, enum AVFrameSideDataType *side_data_types, int *side_data_count,
                             const uint8_t *buf, int buf_size, AVCodecContext *avctx, int is_nalff, int nal_length_size,
                             int err_recognition, int apply_defdispwin, void *logctx);

void av_print_dynamic_hdr_vivid_metadata(void *logctx,const AVDynamicHDRVivid *metadata);

#endif /* AVCODEC_HEVC_PARSE_H */
