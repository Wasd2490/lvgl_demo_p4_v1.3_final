# 【ESP32-P4 LVGL_V1.3】从 0 到 1 打造车LVGL 音乐播放器（Home/Music/Status 三 Tab）

> 硬件：Waveshare ESP32-P4-WIFI6-Touch-LCD-7B（7寸 1024×600 MIPI DSI 触摸屏）
> 框架：ESP-IDF v5.5.5 + LVGL 9.5.0 + BSP 3.0.1
> 作者：谁暖了冬季
> 日期：2026-10-23

---

## 一、前言

之前用 STM32 做过很多项目，但第一次玩 ESP32-P4 这块多功能板子。7 寸大屏、MIPI DSI、带触摸、带音频 codec、还能插 SD 卡，基本上是个小车载屏的尺寸了。

这篇文章记录我从"点个亮屏"到"做出三 Tab 车载音乐 UI"的全过程，包括：
- 环境搭建
- LVGL 三 Tab 界面
- ES8311 音频播放
- SD 卡挂载读歌
- 动态视频背景
- 中文字体显示
- Status 实时系统信息

踩了很多坑，希望你能少走弯路。

---

## 二、硬件准备

### 2.1 你需要买的东西

| 部件 | 说明 |
|---|---|
| ESP32-P4-WIFI6-Touch-LCD-7B | 主控板 + 7寸屏一体，约 400 元 |
| 小喇叭 | 4Ω 3W，接板载喇叭接口 |
| USB-C 线 | 必须能传数据的那种 |
| Micro SD 卡 | FAT32，8GB 以上，Class 10 |
| 读卡器 | 往 SD 卡拷素材用 |

### 2.2 接线

- USB-C 接电脑（供电 + 烧录 + 串口日志）
- 喇叭接板上的 SPK+ / SPK-
- SD 卡插板载卡槽

---

## 三、开发环境搭建（Windows）

### 3.1 装 ESP-IDF v5.5.5

1. 去乐鑫官网下 **ESP-IDF Offline Installer**（选 v5.5.5）
2. 安装路径不要有中文和空格，我装在 `C:\esp\esp-idf`
3. Python venv 自动装到 `C:\Users\<xx>\.espressif\python_env\idf5.5_py3.13_env`
4. RISC-V 工具链装到 `C:\Users\<xx>\.espressif\tools`

装完后，开始菜单会有 "ESP-IDF 5.5 PowerShell"，用那个终端就能直接用 `idf.py`。

### 3.2 装 ffmpeg

转音乐和视频素材要用，下载 `ffmpeg.exe` 放到 `C:\esp\ffmpeg.exe`。

### 3.3 创建工程

```powershell
# 在 ESP-IDF PowerShell 里
cd C:\esp
# 从微雪官方例程复制基础工程（09_lvgl_demo_v9）
# 这个例程已经帮你配好了 BSP、MIPI DSI、触摸
```

---

## 四、第一步：点亮屏幕

先别管音乐和 UI，先让屏幕亮起来显示个颜色条。

微雪官方 BSP 已经把显示、触摸、背光都封装好了，你只要：

```c
#include "bsp/esp-bsp.h"

void app_main(void)
{
    bsp_display_cfg_t cfg = {
        .lv_adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG(),
        .rotation = ESP_LV_ADAPTER_ROTATE_180,  // 我的板子要转 180 才正
        .touch_flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    lv_display_t *disp = bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();
}
```

**坑 1**：一开始屏幕是倒的，改 `ROTATE_180`。
**坑 2**：触摸坐标反了，改 `swap_xy/mirror_x/mirror_y`，我的配置是全 0。

编译烧录：

```powershell
idf.py set-target esp32p4
idf.py build
idf.py -p COM6 flash
```

---

## 五、第二步：做三 Tab UI

### 5.1 布局思路

屏幕 1024×600，我做成：
- 顶部 80px：导航栏（Home / Music / Status 三个按钮）
- 中间：内容区
- 右下角：功能按钮

### 5.2 代码结构

```c
// ui.c
enum { PAGE_HOME, PAGE_MUSIC, PAGE_STATUS, PAGE_COUNT };
static lv_obj_t *s_pages[PAGE_COUNT];
```

每个 Page 是一个全屏 obj，切换时用 `lv_obj_set_page()`。

### 5.3 粒子动画

Home 和 Status 页做了粒子背景——22 个小圆点随机飘动，用 `lv_timer` 30ms 刷新一次。

```c
#define PARTICLE_CNT 22
typedef struct {
    lv_obj_t *circle;
    float x, y, vx, vy;
} particle_t;
```

---

## 六、第三步：音频播放（踩坑最多）

### 6.1 硬件

板载 ES8311 codec + NS4150B 功放，功放使能脚是 **GPIO53**。

### 6.2 踩坑记录

**坑 1：顺序！顺序！顺序！**
codec 必须在 display start **之后**初始化。因为 ES8311 走 I2C，而 I2C 是 display 初始化时才起来的。顺序反了直接断言重启。

```c
// 正确顺序
bsp_display_start_with_config();  // 先屏幕
audio_player_init();              // 再音频
```

**坑 2：必须立体声**
`channel = 2`，单声道没声音，别问我怎么知道的。

**坑 3：write 返回值**
`esp_codec_dev_write()` 返回 `esp_err_t`（0 是成功），不是写入字节数。一开始我当成字节数用来算进度，全错了。

**坑 4：功放脚要拉高**
GPIO53 手动 `gpio_set_level(53, 1)`。

**坑 5：分块写**
一次写 4KB，不要一次写 40KB，否则和 LVGL 抢 CPU 会卡顿。

### 6.3 代码

```c
esp_codec_dev_handle_t spk = bsp_audio_codec_speaker_init();
esp_codec_dev_sample_info_t fs = {
    .sample_rate = 16000,
    .channel = 2,
    .bits_per_sample = 16,
};
esp_codec_dev_open(spk, &fs);
esp_codec_dev_set_out_vol(spk, 30);  // 音量 30，不要打满
```

---

## 七、第四步：SD 卡读歌

### 7.1 挂载

一开始我手动配 SDMMC 引脚，死活挂不上。后来发现 BSP 已经封装好了：

```c
#include "esp_vfs_fat.h"
#include "bsp/esp-bsp.h"

bsp_sdcard_mount();  // 一行搞定，别手动配引脚
```

BSP 内部已经配好了 LDO 电源和引脚（CLK=43, CMD=44, D0-D3=39-42）。

### 7.2 准备音乐

把 MP3 转成 16kHz 立体声 PCM：

```powershell
C:\esp\ffmpeg.exe -y -i "把回忆拼好给你.mp3" -ar 16000 -ac 2 -f s16le song1.pcm
```

一首 4 分钟的歌约 20MB。

### 7.3 SD 卡目录

```
E:\
└── music\
    ├── song1.pcm
    ├── song2.pcm
    ├── song3.pcm
    └── song4.pcm
```

### 7.4 播放器逻辑

开一个 FreeRTOS task，分块读 PCM 写入 codec：

```c
while (sent < size && !stop) {
    fread(buf, 1, 4096, f);
    esp_codec_dev_write(spk, buf, 4096);
    sent += 4096;
}
```

---

## 八、第五步：动态视频背景

### 8.1 思路

ESP32-P4 解码 MP4 太费劲，我用最土但最稳的办法：**提前抽帧**。

把 MP4 用 ffmpeg 抽成 30 张 RGB565 裸图，每张 1024×600 = 1.2MB。运行时按 33ms 一帧从 SD 卡读出来显示。

### 8.2 抽帧

```powershell
# 樱花视频抽 30 帧
C:\esp\ffmpeg.exe -y -i sakura.mp4 -vframes 30 -vf "scale=1024:600" -f rawvideo -pix_fmt rgb565le frame_%02d.raw
```

3 个视频分别抽到 `frames/`、`frames2/`、`frames3/`。

### 8.3 显示

```c
static lv_timer_t *video_timer;
static int frame_idx = 0;

void video_cb(lv_timer_t *t) {
    char path[64];
    snprintf(path, sizeof(path), "/sdcard/img/frames/frame_%02d.raw", frame_idx);
    FILE *f = fopen(path, "rb");
    fread(psram_buf, 1, 1024*600*2, f);
    fclose(f);
    lv_image_set_src(img, &img_dsc);  // 刷新
    frame_idx = (frame_idx + 1) % 30;
}

lv_timer_create(video_cb, 33, NULL);  // 30fps
```

**注意**：buffer 要用 `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`，不要每帧 malloc，会内存碎片。

---

## 九、第六步：中文字体

LVGL 默认只有 Montserrat 英文字体，中文显示方块。

### 9.1 用 lv_font_conv 子集化

只把需要的中文字（谁暖了冬季开发首页音乐状态）从黑体里抽出来，生成 C 文件：

```powershell
npx lv_font_conv --no-compress --format lvgl `
  --font C:\Windows\Fonts\simhei.ttf `
  --size 26 --bpp 4 `
  -o cn_font.c `
  --range 0x20-0x7F `
  --symbols "谁暖了冬季开发首页音乐状态"
```

生成的 `cn_font.c` 只有 83KB，比整字库小多了。

### 9.2 用

```c
extern const lv_font_t cn_font;

lv_obj_t *title = lv_label_create(parent);
lv_label_set_text(title, "谁暖了冬季");
lv_obj_set_style_text_font(title, &cn_font, 0);
```

---

## 十、第七步：Status 页实时信息

Status 页加了个半透明黑底卡片，每秒刷新：

```c
static void stat_timer_cb(lv_timer_t *t) {
    // CPU
    lv_label_set_text(cpu_lbl, "CPU : 240 MHz");

    // Heap
    int free_kb = heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024;
    snprintf(buf, ..., "Heap : %d KB free", free_kb);

    // SD 卡容量
    FATFS *fs; DWORD free_cl;
    f_getfree("/sdcard", &free_cl, &fs);
    uint64_t total = (uint64_t)fs->n_fatent * fs->csize * 512;

    // 运行时间
    uint32_t sec = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
}
```

---

## 十一、最终效果

- **Home**：粉紫天台少女背景 + 粒子飘 + 中央"谁暖了冬季 / LVGL开发"中文字幕
- **Music**：樱花/银河/风铃三种动态视频背景切，左侧播放控制，右侧歌单，4 首完整歌
- **Status**：实时 CPU/Heap/SD/Uptime 显示

---

## 十二、常见问题

**Q: 没声音？**
A: 检查：1) codec 在 display 之后初始化 2) channel=2 3) GPIO53 拉高 4) 喇叭接对了。

**Q: SD 卡挂不上？**
A: 用 `bsp_sdcard_mount()`，别手动配引脚。

**Q: 视频一卡一卡？**
A: SD 卡读速瓶颈。每帧 1.2MB，30fps 需要 36MB/s，Class 10 卡勉强。

**Q: 中文方块？**
A: 用 lv_font_conv 子集化字库，别用全字库（太大）。

---

## 十三、工程下载

完整工程：`lvgl_demo_p4_v1.3.zip`

烧录命令：
```powershell
python -m esptool --chip esp32p4 -p COM6 -b 460800 write_flash ^
  0x2000 build\bootloader\bootloader.bin ^
  0x8000 build\partition_table\partition-table.bin ^
  0x10000 build\lvgl_demo_v9.bin
```

---

**作者：谁暖了冬季**
**项目：ESP32-P4 LVGL Demo**
**版本：v1.3**
