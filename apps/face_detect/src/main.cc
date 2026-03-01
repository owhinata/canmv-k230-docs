#include <nncase/runtime/runtime_op_utility.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

#include "face_ae_roi.h"
#include "mobile_retinaface.h"
#include "mpi_sys_api.h"

// OpenCV (キャプチャ用)
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

using namespace nncase;
using namespace nncase::runtime;
using namespace nncase::runtime::detail;

/* vicap */
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/mman.h>
#include <unistd.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

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
std::atomic<bool> capture_requested(false);

void fun_sig(int sig) {
  if (sig == SIGINT) {
    printf("recive ctrl+c\n");
    quit.store(false);
  }
}

k_vo_draw_frame vo_frame = {1, 16, 16, 128, 128, 1};

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
  return nullptr;
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

static void save_frame_as_png(void *vaddr, int h, int w,
                              const char *capture_dir, int count) {
  // VICAP の RGB planar フレームを OpenCV の Mat に変換して PNG 保存
  auto *base = reinterpret_cast<uint8_t *>(vaddr);
  std::vector<cv::Mat> channels = {
      cv::Mat(h, w, CV_8UC1, base + 2 * h * w),  // B plane
      cv::Mat(h, w, CV_8UC1, base + 1 * h * w),  // G plane
      cv::Mat(h, w, CV_8UC1, base + 0 * h * w),  // R plane
  };
  cv::Mat bgr;
  cv::merge(channels, bgr);

  char filename[256];
  snprintf(filename, sizeof(filename), "%s/capture_%04d.png", capture_dir,
           count);
  cv::imwrite(filename, bgr);
  printf("Captured: %s\n", filename);
}

static void *input_thread(void *arg) {
  printf("press 'q' to exit, 'c' to capture frame\n");
  while (app_run) {
    int ch = getchar();
    if (ch == 'q') {
      app_run = false;
      break;
    } else if (ch == 'c') {
      capture_requested.store(true);
    }
  }
  return nullptr;
}

int main(int argc, char *argv[]) {
  /*Allow one frame time for the VO to release the VB block*/
  k_u32 display_ms = 1000 / 33;
  int face_count = 1;
  int ret;
  const char *capture_dir = nullptr;
  int capture_count = 0;

  if (argc < 3 || argc > 4) {
    std::cerr << "Usage: " << argv[0]
              << " <kmodel> <ae_roi> [capture_dir]" << std::endl;
    std::cerr << "  ae_roi: 0=disable, 1=enable" << std::endl;
    std::cerr << "  capture_dir: directory to save PNG captures (optional)"
              << std::endl;
    return -1;
  }
  if (argc == 4) {
    capture_dir = argv[3];
  }

  /****fixed operation for ctrl+c****/
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = fun_sig;
  sigfillset(&sa.sa_mask);
  sigaction(SIGINT, &sa, nullptr);

  pthread_t vo_thread_handle;
  pthread_t input_thread_handle;
  pthread_create(&input_thread_handle, nullptr, input_thread, nullptr);

  size_t size = CHANNEL * ISP_CHN1_HEIGHT * ISP_CHN1_WIDTH;

  MobileRetinaface model(argv[1], CHANNEL, ISP_CHN1_HEIGHT, ISP_CHN1_WIDTH);
  DetectResult box_result;
  std::vector<face_coordinate> boxes;

  ret = sample_vb_init();
  if (ret) {
    goto vb_init_error;
  }

  pthread_create(&vo_thread_handle, nullptr, sample_vo_thread, nullptr);
  ret = sample_vivcap_init();
  pthread_join(vo_thread_handle, nullptr);
  if (ret) {
    goto vicap_init_error;
  }

  {
    FaceAeRoi face_ae_roi(static_cast<k_isp_dev>(vicap_dev), ISP_CHN1_WIDTH,
                          ISP_CHN1_HEIGHT, sensor_info.width,
                          sensor_info.height);
    face_ae_roi.SetEnable(atoi(argv[2]) == 1);

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
      model.Run(reinterpret_cast<uintptr_t>(vbvaddr),
                reinterpret_cast<uintptr_t>(dump_info.v_frame.phys_addr[0]));
      // get face boxes
      box_result = model.GetResult();
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
        vo_frame.line_x_start = static_cast<uint32_t>(boxes[i].x1) *
                                ISP_CHN0_WIDTH / ISP_CHN1_WIDTH;
        vo_frame.line_y_start = static_cast<uint32_t>(boxes[i].y1) *
                                ISP_CHN0_HEIGHT / ISP_CHN1_HEIGHT;
        vo_frame.line_x_end = static_cast<uint32_t>(boxes[i].x2) *
                              ISP_CHN0_WIDTH / ISP_CHN1_WIDTH;
        vo_frame.line_y_end = static_cast<uint32_t>(boxes[i].y2) *
                              ISP_CHN0_HEIGHT / ISP_CHN1_HEIGHT;
        vo_frame.frame_num = ++j;
        kd_mpi_vo_draw_frame(&vo_frame);
      }
      face_count = boxes.size();

      face_ae_roi.Update(boxes);

      // 'c' が押されていたらキャプチャ
      if (capture_requested.load() && capture_dir != nullptr) {
        save_frame_as_png(vbvaddr, ISP_CHN1_HEIGHT, ISP_CHN1_WIDTH,
                          capture_dir, capture_count++);
        capture_requested.store(false);
      }

      kd_mpi_sys_munmap(vbvaddr, size);
      ret = kd_mpi_vicap_dump_release(vicap_dev, VICAP_CHN_ID_1, &dump_info);
      if (ret) {
        printf("sample_vicap...kd_mpi_vicap_dump_release failed.\n");
      }
    }
  }

  pthread_join(input_thread_handle, nullptr);
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
