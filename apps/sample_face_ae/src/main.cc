#include <nncase/runtime/runtime_op_utility.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

#include "mobile_retinaface.h"
#include "mpi_sys_api.h"

using namespace nncase;
using namespace nncase::runtime;
using namespace nncase::runtime::detail;

/* vicap */
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <atomic>

#include "k_connector_comm.h"
#include "k_module.h"
#include "k_sys_comm.h"
#include "k_type.h"
#include "k_vb_comm.h"
#include "k_video_comm.h"
#include "k_vo_comm.h"
#include "mpi_connector_api.h"
#include "mpi_isp_api.h"
#include "mpi_sys_api.h"
#include "mpi_vb_api.h"
#include "mpi_vicap_api.h"
#include "mpi_vo_api.h"
#include "sys/ioctl.h"
#include "vo_test_case.h"

#define CHANNEL 3

#define ISP_CHN1_HEIGHT (720)
#define ISP_CHN1_WIDTH (1280)
#define ISP_CHN0_WIDTH (1920)
#define ISP_CHN0_HEIGHT (1080)

#define ISP_INPUT_WIDTH (1920)
#define ISP_INPUT_HEIGHT (1080)

#define LCD_WIDTH (1080)
#define LCD_HEIGHT (1920)

int sample_sys_bind_init(void);

std::atomic<bool> quit(true);

bool app_run = true;

void fun_sig(int sig) {
  if (sig == SIGINT) {
    printf("recive ctrl+c\n");
    quit.store(false);
  }
}

k_vo_draw_frame vo_frame = (k_vo_draw_frame){1, 16, 16, 128, 128, 1};

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

k_s32 sample_connector_init(void) {
  k_u32 ret = 0;
  k_s32 connector_fd;
  k_connector_type connector_type = LT9611_MIPI_4LAN_1920X1080_30FPS;
  k_connector_info connector_info;

  memset(&connector_info, 0, sizeof(k_connector_info));

  // connector get sensor info
  ret = kd_mpi_get_connector_info(connector_type, &connector_info);
  if (ret) {
    printf("sample_vicap, the sensor type not supported!\n");
    return ret;
  }

  connector_fd = kd_mpi_connector_open(connector_info.connector_name);
  if (connector_fd < 0) {
    printf("%s, connector open failed.\n", __func__);
    return K_ERR_VO_NOTREADY;
  }

  // set connect power
  kd_mpi_connector_power_set(connector_fd, K_TRUE);
  // connector init
  kd_mpi_connector_init(connector_fd, connector_info);

  return 0;
}

static k_s32 vo_layer_vdss_bind_vo_config(void) {
  layer_info info;
  k_vo_layer chn_id = K_VO_LAYER1;

  memset(&info, 0, sizeof(info));

  sample_connector_init();

  info.act_size.width = ISP_CHN0_WIDTH;
  info.act_size.height = ISP_CHN0_HEIGHT;
  info.format = PIXEL_FORMAT_YVU_PLANAR_420;
  info.func = K_ROTATION_0;
  info.global_alptha = 0xff;
  info.offset.x = 0;
  info.offset.y = 0;
  vo_creat_layer_test(chn_id, &info);

  return 0;
}

static void sample_vo_fn(void *arg) {
  usleep(10000);
  vo_layer_vdss_bind_vo_config();
  sample_sys_bind_init();
  return;
}

static void *sample_vo_thread(void *arg) {
  sample_vo_fn(arg);
  return NULL;
}

k_vicap_dev vicap_dev;
k_vicap_chn vicap_chn;
k_vicap_dev_attr dev_attr;
k_vicap_chn_attr chn_attr;
k_vicap_sensor_info sensor_info;
k_vicap_sensor_type sensor_type;
k_video_frame_info dump_info;

static void sample_vicap_unbind_vo(k_mpp_chn vicap_mpp_chn,
                                   k_mpp_chn vo_mpp_chn) {
  k_s32 ret;

  ret = kd_mpi_sys_unbind(&vicap_mpp_chn, &vo_mpp_chn);
  if (ret) {
    printf("kd_mpi_sys_unbind failed:0x%x\n", ret);
  }
  return;
}

int sample_sys_bind_init(void) {
  k_s32 ret = 0;
  k_mpp_chn vicap_mpp_chn;
  k_mpp_chn vo_mpp_chn;
  vicap_mpp_chn.mod_id = K_ID_VI;
  vicap_mpp_chn.dev_id = vicap_dev;
  vicap_mpp_chn.chn_id = vicap_chn;

  vo_mpp_chn.mod_id = K_ID_VO;
  vo_mpp_chn.dev_id = K_VO_DISPLAY_DEV_ID;
  vo_mpp_chn.chn_id = K_VO_DISPLAY_CHN_ID1;

  ret = kd_mpi_sys_bind(&vicap_mpp_chn, &vo_mpp_chn);
  if (ret) {
    printf("kd_mpi_sys_unbind failed:0x%x\n", ret);
  }
  return ret;
}

int sample_vb_init(void) {
  k_s32 ret;
  k_vb_config config;
  memset(&config, 0, sizeof(config));
  config.max_pool_cnt = 64;
  // VB for YUV420SP output
  config.comm_pool[0].blk_cnt = 5;
  config.comm_pool[0].mode = VB_REMAP_MODE_NOCACHE;
  config.comm_pool[0].blk_size = VICAP_ALIGN_UP(
      (ISP_CHN0_WIDTH * ISP_CHN0_HEIGHT * 3 / 2), VICAP_ALIGN_1K);

  // VB for RGB888 output
  config.comm_pool[1].blk_cnt = 5;
  config.comm_pool[1].mode = VB_REMAP_MODE_NOCACHE;
  config.comm_pool[1].blk_size =
      VICAP_ALIGN_UP((ISP_CHN1_HEIGHT * ISP_CHN1_WIDTH * 3), VICAP_ALIGN_1K);

  ret = kd_mpi_vb_set_config(&config);
  if (ret) {
    printf("vb_set_config failed ret:%d\n", ret);
    return ret;
  }

  k_vb_supplement_config supplement_config;
  memset(&supplement_config, 0, sizeof(supplement_config));
  supplement_config.supplement_config |= VB_SUPPLEMENT_JPEG_MASK;

  ret = kd_mpi_vb_set_supplement_config(&supplement_config);
  if (ret) {
    printf("vb_set_supplement_config failed ret:%d\n", ret);
    return ret;
  }
  ret = kd_mpi_vb_init();
  if (ret) {
    printf("vb_init failed ret:%d\n", ret);
  }
  return ret;
}

int sample_vivcap_init(void) {
  k_s32 ret = 0;
  sensor_type = OV_OV5647_MIPI_CSI0_1920X1080_30FPS_10BIT_LINEAR;

  memset(&sensor_info, 0, sizeof(k_vicap_sensor_info));
  ret = kd_mpi_vicap_get_sensor_info(sensor_type, &sensor_info);
  if (ret) {
    printf("sample_vicap, the sensor type not supported!\n");
    return ret;
  }

  memset(&dev_attr, 0, sizeof(k_vicap_dev_attr));
  dev_attr.acq_win.h_start = 0;
  dev_attr.acq_win.v_start = 0;
  dev_attr.acq_win.width = ISP_INPUT_WIDTH;
  dev_attr.acq_win.height = ISP_INPUT_HEIGHT;
  dev_attr.mode = VICAP_WORK_ONLINE_MODE;

  dev_attr.pipe_ctrl.data = 0xFFFFFFFF;
  dev_attr.pipe_ctrl.bits.af_enable = 0;
  dev_attr.pipe_ctrl.bits.ahdr_enable = 0;

  dev_attr.cpature_frame = 0;
  dev_attr.mirror = VICAP_MIRROR_VER;
  memcpy(&dev_attr.sensor_info, &sensor_info, sizeof(k_vicap_sensor_info));

  ret = kd_mpi_vicap_set_dev_attr(vicap_dev, dev_attr);
  if (ret) {
    printf("sample_vicap, kd_mpi_vicap_set_dev_attr failed.\n");
    return ret;
  }

  memset(&chn_attr, 0, sizeof(k_vicap_chn_attr));

  // set chn0 output yuv420sp
  chn_attr.out_win.h_start = 0;
  chn_attr.out_win.v_start = 0;
  chn_attr.out_win.width = ISP_CHN0_WIDTH;
  chn_attr.out_win.height = ISP_CHN0_HEIGHT;
  chn_attr.crop_win = dev_attr.acq_win;
  chn_attr.scale_win = chn_attr.out_win;
  chn_attr.crop_enable = K_FALSE;
  chn_attr.scale_enable = K_FALSE;
  chn_attr.chn_enable = K_TRUE;
  chn_attr.pix_format = PIXEL_FORMAT_YVU_PLANAR_420;
  chn_attr.buffer_num = VICAP_MAX_FRAME_COUNT;  // at least 3 buffers for isp
  chn_attr.buffer_size = VICAP_ALIGN_UP(
      (ISP_CHN0_WIDTH * ISP_CHN0_HEIGHT * 3 / 2), VICAP_ALIGN_1K);
  ;
  vicap_chn = VICAP_CHN_ID_0;

  ret = kd_mpi_vicap_set_chn_attr(vicap_dev, vicap_chn, chn_attr);
  if (ret) {
    printf("sample_vicap, kd_mpi_vicap_set_chn_attr failed.\n");
    return ret;
  }

  // set chn1 output rgb888p
  chn_attr.out_win.h_start = 0;
  chn_attr.out_win.v_start = 0;
  chn_attr.out_win.width = ISP_CHN1_WIDTH;
  chn_attr.out_win.height = ISP_CHN1_HEIGHT;

  chn_attr.crop_win = dev_attr.acq_win;
  chn_attr.scale_win = chn_attr.out_win;
  chn_attr.crop_enable = K_FALSE;
  chn_attr.scale_enable = K_FALSE;
  chn_attr.chn_enable = K_TRUE;
  chn_attr.pix_format = PIXEL_FORMAT_RGB_888_PLANAR;
  chn_attr.buffer_num = VICAP_MAX_FRAME_COUNT;  // at least 3 buffers for isp
  chn_attr.buffer_size =
      VICAP_ALIGN_UP((ISP_CHN1_HEIGHT * ISP_CHN1_WIDTH * 3), VICAP_ALIGN_1K);

  ret = kd_mpi_vicap_set_chn_attr(vicap_dev, VICAP_CHN_ID_1, chn_attr);
  if (ret) {
    printf("sample_vicap, kd_mpi_vicap_set_chn_attr failed.\n");
    return ret;
  }
  // set to header file database parse mode
  ret = kd_mpi_vicap_set_database_parse_mode(vicap_dev,
                                             VICAP_DATABASE_PARSE_XML_JSON);
  if (ret) {
    printf("sample_vicap, kd_mpi_vicap_set_database_parse_mode failed.\n");
    return ret;
  }

  ret = kd_mpi_vicap_init(vicap_dev);
  if (ret) {
    printf("sample_vicap, kd_mpi_vicap_init failed.\n");
    return ret;
  }
  ret = kd_mpi_vicap_start_stream(vicap_dev);
  if (ret) {
    printf("sample_vicap, kd_mpi_vicap_start_stream failed.\n");
    return ret;
  }
  return ret;
}

static void *exit_app(void *arg) {
  printf("press 'q' to exit application!!\n");
  while (getchar() != 'q') {
    usleep(10000);
  }
  app_run = false;
  return NULL;
}

k_u32 calc_sum(k_u32 data[], k_u8 size) {
  k_u32 sum = 0;
  for (int i = 0; i < size; i++) {
    sum += data[i];
  }
  return sum;
}

void face_location_convert_roi(std::vector<face_coordinate> boxes,
                               k_isp_ae_roi *ae_roi, k_u32 h_offset,
                               k_u32 v_offset, k_u32 scale_h, k_u32 scale_v,
                               k_u32 width, k_u32 height) {
#define CHECK_BOUNDARY(s, o, e) s = (o + s) > e ? (e - o) : (s)
  static k_u32 area[8] = {0};
  if (ae_roi == nullptr) {
    std::cout << "face location is Nullptr" << std::endl;
    return;
  }

  auto box_size = std::min<int>(boxes.size(), 8);

  if (boxes.empty()) {
    ae_roi->roiNum = 0;
    return;
  }

  ae_roi->roiNum = box_size;
  for (auto i = 0; i < box_size; i += 1) {
    boxes[i].x1 = boxes[i].x1 < 0 ? 0 : boxes[i].x1;
    boxes[i].y1 = boxes[i].y1 < 0 ? 0 : boxes[i].y1;

    ae_roi->roiWindow[i].window.hOffset =
        (k_u32)(boxes[i].x1 * width / scale_h + h_offset);
    ae_roi->roiWindow[i].window.vOffset =
        (k_u32)(boxes[i].y1 * height / scale_v + v_offset);

    ae_roi->roiWindow[i].window.width =
        ((k_u32)(boxes[i].x2 - (k_u32)boxes[i].x1) * width / scale_h);
    ae_roi->roiWindow[i].window.height =
        ((k_u32)(boxes[i].y2 - (k_u32)boxes[i].y1) * height / scale_v);

    CHECK_BOUNDARY(ae_roi->roiWindow[i].window.width,
                   ae_roi->roiWindow[i].window.hOffset, sensor_info.width);
    CHECK_BOUNDARY(ae_roi->roiWindow[i].window.height,
                   ae_roi->roiWindow[i].window.vOffset, sensor_info.height);

    area[i] =
        ae_roi->roiWindow[i].window.width * ae_roi->roiWindow[i].window.height;
  }

  k_u32 sum = calc_sum(area, ae_roi->roiNum);
  for (auto i = 0; i < ae_roi->roiNum; i++) {
    ae_roi->roiWindow[i].weight = (float)area[i] / sum;
  }
}

int main(int argc, char *argv[]) {
  /*Allow one frame time for the VO to release the VB block*/
  k_u32 display_ms = 1000 / 33;
  int face_count = 1;
  int roi_enable = 0;
  int ret;
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " <kmodel> <roi_enable>" << std::endl;
    return -1;
  }
  /****fixed operation for ctrl+c****/
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = fun_sig;
  sigfillset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);

  pthread_t vo_thread_handle;
  pthread_t exit_thread_handle;
  pthread_create(&exit_thread_handle, NULL, exit_app, NULL);

  size_t size = CHANNEL * ISP_CHN1_HEIGHT * ISP_CHN1_WIDTH;
  ;

  MobileRetinaface model(argv[1], CHANNEL, ISP_CHN1_HEIGHT, ISP_CHN1_WIDTH);
  DetectResult box_result;
  std::vector<face_coordinate> boxes;

  ret = sample_vb_init();
  if (ret) {
    goto vb_init_error;
  }

  pthread_create(&vo_thread_handle, NULL, sample_vo_thread, NULL);
  ret = sample_vivcap_init();
  pthread_join(vo_thread_handle, NULL);
  if (ret) {
    goto vicap_init_error;
  }

  roi_enable = atoi(argv[2]);
  if (roi_enable == 1) {
    kd_mpi_isp_ae_roi_set_enable((k_isp_dev)vicap_dev, (k_bool)1);
  } else {
    kd_mpi_isp_ae_roi_set_enable((k_isp_dev)vicap_dev, (k_bool)0);
  }

  while (app_run) {
    memset(&dump_info, 0, sizeof(k_video_frame_info));
    ret = kd_mpi_vicap_dump_frame(vicap_dev, VICAP_CHN_ID_1, VICAP_DUMP_YUV,
                                  &dump_info, 1000);
    if (ret) {
      quit.store(false);
      printf("sample_vicap...kd_mpi_vicap_dump_frame failed.\n");
      break;
    }

    auto vbvaddr = kd_mpi_sys_mmap(dump_info.v_frame.phys_addr[0], size);
    boxes.clear();
    // run kpu
    model.run(reinterpret_cast<uintptr_t>(vbvaddr),
              reinterpret_cast<uintptr_t>(dump_info.v_frame.phys_addr[0]));
    kd_mpi_sys_munmap(vbvaddr, size);
    // get face boxes
    box_result = model.get_result();
    boxes = box_result.boxes;

    if (boxes.size() < face_count) {
      for (size_t i = boxes.size(); i < face_count; i++) {
        vo_frame.draw_en = 0;
        vo_frame.frame_num = i + 1;
        kd_mpi_vo_draw_frame(&vo_frame);
      }
    }

    for (size_t i = 0, j = 0; i < boxes.size(); i += 1) {
      vo_frame.draw_en = 1;
      vo_frame.line_x_start =
          ((uint32_t)boxes[i].x1) * ISP_CHN0_WIDTH / ISP_CHN1_WIDTH;
      vo_frame.line_y_start =
          ((uint32_t)boxes[i].y1) * ISP_CHN0_HEIGHT / ISP_CHN1_HEIGHT;
      vo_frame.line_x_end =
          ((uint32_t)boxes[i].x2) * ISP_CHN0_WIDTH / ISP_CHN1_WIDTH;
      vo_frame.line_y_end =
          ((uint32_t)boxes[i].y2) * ISP_CHN0_HEIGHT / ISP_CHN1_HEIGHT;
      vo_frame.frame_num = ++j;
      kd_mpi_vo_draw_frame(&vo_frame);
    }
    face_count = boxes.size();

    k_isp_ae_roi user_roi;
    face_location_convert_roi(boxes, &user_roi, 0, 0, ISP_CHN1_WIDTH,
                              ISP_CHN1_HEIGHT, 1920, 1080);
    kd_mpi_isp_ae_set_roi((k_isp_dev)vicap_dev, user_roi);
    ret = kd_mpi_vicap_dump_release(vicap_dev, VICAP_CHN_ID_1, &dump_info);
    if (ret) {
      printf("sample_vicap...kd_mpi_vicap_dump_release failed.\n");
    }
  }

app_exit:
  pthread_join(exit_thread_handle, NULL);
  for (size_t i = 0; i < boxes.size(); i++) {
    vo_frame.draw_en = 0;
    vo_frame.frame_num = i + 1;
    kd_mpi_vo_draw_frame(&vo_frame);
  }
  boxes.clear();
  ret = kd_mpi_vicap_stop_stream(vicap_dev);
  if (ret) {
    printf("sample_vicap, stop stream failed.\n");
  }
  ret = kd_mpi_vicap_deinit(vicap_dev);
  if (ret) {
    printf("sample_vicap, kd_mpi_vicap_deinit failed.\n");
    return ret;
  }

  kd_mpi_vo_disable_video_layer(K_VO_LAYER1);

vicap_init_error:
  k_mpp_chn vicap_mpp_chn;
  k_mpp_chn vo_mpp_chn;
  vicap_mpp_chn.mod_id = K_ID_VI;
  vicap_mpp_chn.dev_id = vicap_dev;
  vicap_mpp_chn.chn_id = vicap_chn;

  vo_mpp_chn.mod_id = K_ID_VO;
  vo_mpp_chn.dev_id = K_VO_DISPLAY_DEV_ID;
  vo_mpp_chn.chn_id = K_VO_DISPLAY_CHN_ID1;

  sample_vicap_unbind_vo(vicap_mpp_chn, vo_mpp_chn);
  usleep(1000 * display_ms);

  ret = kd_mpi_vb_exit();
  if (ret) {
    printf("fastboot_app, kd_mpi_vb_exit failed.\n");
    return ret;
  }

vb_init_error:
  return 0;
}
