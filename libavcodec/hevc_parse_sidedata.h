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

#ifndef AVCODEC_HEVC_PARSE_SIDEDATA_H
#define AVCODEC_HEVC_PARSE_SIDEDATA_H

#include <stdint.h>
#include <libavutil/hdr_dynamic_vivid_metadata.h>
#include <libavutil/mastering_display_metadata.h>
#include "avcodec.h"


int av_hevc_decode_extradata(AVBufferRef **side_data_buffers, enum AVFrameSideDataType *side_data_types, int *side_data_count,AVCodecContext *avctx, AVPacket *pkt, int *is_nalff, int *nal_length_size,int *extradata_parsed);

#endif /* AVCODEC_HEVC_PARSE_SIDEDATA_H */
