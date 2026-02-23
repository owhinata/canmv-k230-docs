/* Copyright (c) 2023, Canaan Bright Sight Co., Ltd
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "vo_test_case.h"

#include <stdio.h>
#include <string.h>

#include "k_vo_comm.h"
#include "mpi_vo_api.h"

k_u32 vo_creat_osd_test(k_vo_osd osd, osd_info *info) {
  k_vo_video_osd_attr attr;

  // set attr
  attr.global_alptha = info->global_alptha;

  if (info->format == PIXEL_FORMAT_ABGR_8888 ||
      info->format == PIXEL_FORMAT_ARGB_8888) {
    info->size = info->act_size.width * info->act_size.height * 4;
    info->stride = info->act_size.width * 4 / 8;
  } else if (info->format == PIXEL_FORMAT_RGB_565 ||
             info->format == PIXEL_FORMAT_BGR_565) {
    info->size = info->act_size.width * info->act_size.height * 2;
    info->stride = info->act_size.width * 2 / 8;
  } else if (info->format == PIXEL_FORMAT_RGB_888 ||
             info->format == PIXEL_FORMAT_BGR_888) {
    info->size = info->act_size.width * info->act_size.height * 3;
    info->stride = info->act_size.width * 3 / 8;
  } else if (info->format == PIXEL_FORMAT_ARGB_4444 ||
             info->format == PIXEL_FORMAT_ABGR_4444) {
    info->size = info->act_size.width * info->act_size.height * 2;
    info->stride = info->act_size.width * 2 / 8;
  } else if (info->format == PIXEL_FORMAT_ARGB_1555 ||
             info->format == PIXEL_FORMAT_ABGR_1555) {
    info->size = info->act_size.width * info->act_size.height * 2;
    info->stride = info->act_size.width * 2 / 8;
  } else {
    printf("set osd pixel format failed  \n");
  }

  attr.stride = info->stride;
  attr.pixel_format = info->format;
  attr.display_rect = info->offset;
  attr.img_size = info->act_size;
  kd_mpi_vo_set_video_osd_attr(osd, &attr);

  kd_mpi_vo_osd_enable(osd);

  return 0;
}

int vo_creat_layer_test(k_vo_layer chn_id, layer_info *info) {
  k_vo_video_layer_attr attr;

  // check layer
  if ((chn_id >= K_MAX_VO_LAYER_NUM) ||
      ((info->func & K_VO_SCALER_ENABLE) && (chn_id != K_VO_LAYER0)) ||
      ((info->func != 0) && (chn_id == K_VO_LAYER2))) {
    printf("input layer num failed \n");
    return -1;
  }

  memset(&attr, 0, sizeof(attr));

  // set offset
  attr.display_rect = info->offset;
  // set act
  attr.img_size = info->act_size;
  // sget size
  info->size = info->act_size.height * info->act_size.width * 3 / 2;
  // set pixel format
  attr.pixel_format = info->format;
  if (info->format != PIXEL_FORMAT_YVU_PLANAR_420) {
    printf("input pix format failed \n");
    return -1;
  }
  // set stride
  attr.stride =
      (info->act_size.width / 8 - 1) + ((info->act_size.height - 1) << 16);
  // set function
  attr.func = info->func;
  // set scaler attr
  attr.scaler_attr = info->attr;

  // set video layer atrr
  kd_mpi_vo_set_video_layer_attr(chn_id, &attr);

  // enable layer
  kd_mpi_vo_enable_video_layer(chn_id);

  return 0;
}
