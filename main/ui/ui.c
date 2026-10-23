/**
 * @file ui.c
 * @brief 整套 Demo UI（LVGL v9）：顶部状态栏 + 三页内容 + 底部导航。
 *
 * 目标硬件：Waveshare ESP32-P4-WIFI6-Touch-LCD-7B（1024x600, MIPI DSI, GT911）。
 * BSP（display/touch/backlight）由 waveshare/esp32_p4_wifi6_touch_lcd_7b 组件完成。
 */
#include "ui.h"
#include <stdio.h>
#include <math.h>
#include "audio_player.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "ff.h"

/* 嵌入的动漫图（RGB565, 1024x600） */
extern const lv_image_dsc_t img_tab1;
extern const lv_image_dsc_t img_tab2;
extern const lv_image_dsc_t img_tab3;
extern const lv_font_t cn_font;

/* ===================== 配色 ===================== */
#define C_BG        lv_color_hex(0x101418)
#define C_CARD      lv_color_hex(0x1c222b)
#define C_CARD2     lv_color_hex(0x262e3a)
#define C_PRIMARY   lv_color_hex(0x00b4d8)
#define C_ACCENT    lv_color_hex(0xff9e00)
#define C_TEXT      lv_color_hex(0xe8eef5)
#define C_DIM       lv_color_hex(0x8a94a3)
#define C_DIVIDER   lv_color_hex(0x2f3742)

/* ===================== 页面索引 ===================== */
enum { PAGE_WIDGETS = 0, PAGE_MUSIC, PAGE_BENCH, PAGE_COUNT };

static lv_obj_t *s_pages[PAGE_COUNT];
static lv_obj_t *s_nav_btns[PAGE_COUNT];
static lv_obj_t *s_clock_lbl;

/* ===================== 小工具 ===================== */
static void switch_page(int idx)
{
    for (int i = 0; i < PAGE_COUNT; i++) {
        if (i == idx) {
            lv_obj_remove_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(s_nav_btns[i], C_CARD2, 0);
            lv_obj_set_style_text_color(s_nav_btns[i], C_PRIMARY, 0);
        } else {
            lv_obj_add_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(s_nav_btns[i], C_CARD, 0);
            lv_obj_set_style_text_color(s_nav_btns[i], C_DIM, 0);
        }
    }
}

static void nav_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target_obj(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(btn);
    switch_page(idx);
}

static void clock_cb(lv_timer_t *t)
{
    (void)t;
    uint32_t sec = lv_tick_get() / 1000;
    char buf[16];
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
             (unsigned)((sec / 3600) % 24),
             (unsigned)((sec / 60) % 60),
             (unsigned)(sec % 60));
    lv_label_set_text(s_clock_lbl, buf);
}

/* =====================================================
 *                    Widgets 页
 * ===================================================== */
static void btn_log_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        LV_LOG_USER("clicked button at %p", (void *)lv_event_get_target_obj(e));
    }
}

static void slider_val_cb(lv_event_t *e)
{
    lv_obj_t *sl  = lv_event_get_target_obj(e);
    lv_obj_t *val = (lv_obj_t *)lv_event_get_user_data(e);
    lv_label_set_text_fmt(val, "%d", (int)lv_slider_get_value(sl));
}

static void popup_ok_cb(lv_event_t *e)
{
    lv_obj_t *b = lv_event_get_target_obj(e);
    lv_obj_t *scrim = lv_obj_get_parent(b);
    lv_obj_delete(scrim);
}

static void open_popup_cb(lv_event_t *e)
{
    (void)e;
    /* 自定义半透明弹窗（避免依赖 lv_msgbox v9 的链式 API） */
    lv_obj_t *scrim = lv_obj_create(lv_screen_active());
    lv_obj_set_size(scrim, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(scrim, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scrim, LV_OPA_60, 0);
    lv_obj_set_style_border_width(scrim, 0, 0);
    lv_obj_set_style_pad_all(scrim, 0, 0);
    lv_obj_center(scrim);

    lv_obj_t *card = lv_obj_create(scrim);
    lv_obj_set_size(card, 420, 220);
    lv_obj_set_style_bg_color(card, C_CARD, 0);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_border_color(card, C_PRIMARY, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_center(card);

    lv_obj_t *t = lv_label_create(card);
    lv_label_set_text(t, "提示");
    lv_obj_set_style_text_color(t, C_PRIMARY, 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_24, 0);
    lv_obj_set_pos(t, 24, 20);

    lv_obj_t *m = lv_label_create(card);
    lv_label_set_text(m, "这是一个 LVGL v9 弹窗示例。\n点击 OK 关闭。");
    lv_obj_set_style_text_color(m, C_TEXT, 0);
    lv_obj_set_style_text_font(m, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(m, 24, 70);

    lv_obj_t *ok = lv_button_create(card);
    lv_obj_set_size(ok, 120, 44);
    lv_obj_set_style_bg_color(ok, C_PRIMARY, 0);
    lv_obj_set_style_radius(ok, 10, 0);
    lv_obj_set_pos(ok, 24, 150);
    lv_obj_t *okl = lv_label_create(ok);
    lv_label_set_text(okl, "OK");
    lv_obj_set_style_text_color(okl, lv_color_hex(0x061018), 0);
    lv_obj_center(okl);
    lv_obj_add_event_cb(ok, popup_ok_cb, LV_EVENT_CLICKED, NULL);
}

static void build_widgets_page(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_remove_style_all(cont);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(cont, 16, 0);
    lv_obj_set_style_pad_column(cont, 16, 0);

    /* ---- 卡片 1：按钮 ---- */
    lv_obj_t *c1 = lv_obj_create(cont);
    lv_obj_set_size(c1, 480, 200);
    lv_obj_set_style_bg_color(c1, C_CARD, 0);
    lv_obj_set_style_radius(c1, 14, 0);
    lv_obj_set_style_border_color(c1, C_DIVIDER, 0);
    lv_obj_set_style_border_width(c1, 1, 0);
    lv_obj_set_style_pad_all(c1, 16, 0);
    lv_obj_set_flex_flow(c1, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c1, 14, 0);

    lv_obj_t *h1 = lv_label_create(c1);
    lv_label_set_text(h1, "Buttons");
    lv_obj_set_style_text_color(h1, C_DIM, 0);
    lv_obj_set_style_text_font(h1, &lv_font_montserrat_14, 0);

    lv_obj_t *row = lv_obj_create(c1);
    lv_obj_remove_style_all(row);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(row, 14, 0);
    lv_obj_set_style_pad_all(row, 0, 0);

    lv_obj_t *b1 = lv_button_create(row);
    lv_obj_set_style_bg_color(b1, C_PRIMARY, 0);
    lv_obj_set_style_radius(b1, 10, 0);
    lv_obj_set_style_pad_hor(b1, 22, 0);
    lv_obj_set_style_pad_ver(b1, 12, 0);
    lv_obj_add_event_cb(b1, btn_log_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l1 = lv_label_create(b1);
    lv_label_set_text(l1, "Primary");
    lv_obj_set_style_text_color(l1, lv_color_hex(0x061018), 0);

    lv_obj_t *b2 = lv_button_create(row);
    lv_obj_set_style_bg_color(b2, C_CARD2, 0);
    lv_obj_set_style_radius(b2, 10, 0);
    lv_obj_set_style_pad_hor(b2, 22, 0);
    lv_obj_set_style_pad_ver(b2, 12, 0);
    lv_obj_add_event_cb(b2, btn_log_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l2 = lv_label_create(b2);
    lv_label_set_text(l2, "Ghost");
    lv_obj_set_style_text_color(l2, C_TEXT, 0);

    lv_obj_t *b3 = lv_button_create(row);
    lv_obj_set_size(b3, 64, 64);
    lv_obj_set_style_bg_color(b3, C_ACCENT, 0);
    lv_obj_set_style_radius(b3, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_event_cb(b3, btn_log_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l3 = lv_label_create(b3);
    lv_label_set_text(l3, "OK");
    lv_obj_set_style_text_color(l3, lv_color_hex(0x201400), 0);

    /* ---- 卡片 2：开关 ---- */
    lv_obj_t *c2 = lv_obj_create(cont);
    lv_obj_set_size(c2, 480, 200);
    lv_obj_set_style_bg_color(c2, C_CARD, 0);
    lv_obj_set_style_radius(c2, 14, 0);
    lv_obj_set_style_border_color(c2, C_DIVIDER, 0);
    lv_obj_set_style_border_width(c2, 1, 0);
    lv_obj_set_style_pad_all(c2, 16, 0);
    lv_obj_set_flex_flow(c2, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c2, 12, 0);

    lv_obj_t *h2 = lv_label_create(c2);
    lv_label_set_text(h2, "Switches");
    lv_obj_set_style_text_color(h2, C_DIM, 0);
    lv_obj_set_style_text_font(h2, &lv_font_montserrat_14, 0);

    lv_obj_t *sw_row = lv_obj_create(c2);
    lv_obj_remove_style_all(sw_row);
    lv_obj_set_width(sw_row, lv_pct(100));
    lv_obj_set_flex_flow(sw_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sw_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(sw_row, 0, 0);
    lv_obj_t *sw_lbl = lv_label_create(sw_row);
    lv_label_set_text(sw_lbl, "Air conditioning");
    lv_obj_set_style_text_color(sw_lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(sw_lbl, &lv_font_montserrat_16, 0);
    lv_obj_t *sw = lv_switch_create(sw_row);
    lv_obj_set_style_bg_color(sw, C_PRIMARY, LV_STATE_CHECKED);

    lv_obj_t *cb = lv_checkbox_create(c2);
    lv_obj_set_style_text_color(cb, C_TEXT, 0);
    lv_checkbox_set_text(cb, "Enable auto light");

    /* ---- 卡片 3：滑块 + 进度条 ---- */
    lv_obj_t *c3 = lv_obj_create(cont);
    lv_obj_set_size(c3, 480, 200);
    lv_obj_set_style_bg_color(c3, C_CARD, 0);
    lv_obj_set_style_radius(c3, 14, 0);
    lv_obj_set_style_border_color(c3, C_DIVIDER, 0);
    lv_obj_set_style_border_width(c3, 1, 0);
    lv_obj_set_style_pad_all(c3, 16, 0);
    lv_obj_set_flex_flow(c3, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c3, 12, 0);

    lv_obj_t *head = lv_obj_create(c3);
    lv_obj_remove_style_all(head);
    lv_obj_set_width(head, lv_pct(100));
    lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(head, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(head, 0, 0);
    lv_obj_t *sn = lv_label_create(head);
    lv_label_set_text(sn, "Volume");
    lv_obj_set_style_text_color(sn, C_TEXT, 0);
    lv_obj_set_style_text_font(sn, &lv_font_montserrat_16, 0);
    lv_obj_t *sv = lv_label_create(head);
    lv_label_set_text(sv, "50");
    lv_obj_set_style_text_color(sv, C_TEXT, 0);
    lv_obj_set_style_text_font(sv, &lv_font_montserrat_16, 0);

    lv_obj_t *sl = lv_slider_create(c3);
    lv_obj_set_width(sl, lv_pct(100));
    lv_slider_set_value(sl, 50, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(sl, C_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sl, C_CARD2, LV_PART_MAIN);
    lv_obj_add_event_cb(sl, slider_val_cb, LV_EVENT_VALUE_CHANGED, sv);

    lv_obj_t *bar = lv_bar_create(c3);
    lv_obj_set_size(bar, lv_pct(100), 14);
    lv_obj_set_style_bg_color(bar, C_CARD2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, C_ACCENT, LV_PART_INDICATOR);
    lv_bar_set_value(bar, 65, LV_ANIM_OFF);

    /* ---- 卡片 4：下拉 + 弹窗 ---- */
    lv_obj_t *c4 = lv_obj_create(cont);
    lv_obj_set_size(c4, 480, 200);
    lv_obj_set_style_bg_color(c4, C_CARD, 0);
    lv_obj_set_style_radius(c4, 14, 0);
    lv_obj_set_style_border_color(c4, C_DIVIDER, 0);
    lv_obj_set_style_border_width(c4, 1, 0);
    lv_obj_set_style_pad_all(c4, 16, 0);
    lv_obj_set_flex_flow(c4, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(c4, 12, 0);

    lv_obj_t *h4 = lv_label_create(c4);
    lv_label_set_text(h4, "Dropdown & Popup");
    lv_obj_set_style_text_color(h4, C_DIM, 0);
    lv_obj_set_style_text_font(h4, &lv_font_montserrat_14, 0);

    lv_obj_t *dd = lv_dropdown_create(c4);
    lv_dropdown_set_options(dd, "Comfort\nSport\nEco\nCustom");
    lv_obj_set_style_bg_color(dd, C_CARD2, 0);
    lv_obj_set_style_text_color(dd, C_TEXT, 0);

    lv_obj_t *bm = lv_button_create(c4);
    lv_obj_set_style_bg_color(bm, C_PRIMARY, 0);
    lv_obj_set_style_radius(bm, 10, 0);
    lv_obj_set_style_pad_hor(bm, 22, 0);
    lv_obj_set_style_pad_ver(bm, 12, 0);
    lv_obj_add_event_cb(bm, open_popup_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bml = lv_label_create(bm);
    lv_label_set_text(bml, "Show Popup");
    lv_obj_set_style_text_color(bml, lv_color_hex(0x061018), 0);

    /* ---- 卡片 5：欢迎 / 大时钟 ---- */
    lv_obj_t *c5 = lv_obj_create(cont);
    lv_obj_set_size(c5, 976, 140);
    lv_obj_set_style_bg_color(c5, C_PRIMARY, 0);
    lv_obj_set_style_radius(c5, 14, 0);
    lv_obj_set_style_border_width(c5, 0, 0);
    lv_obj_set_style_pad_all(c5, 20, 0);
    lv_obj_set_flex_flow(c5, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c5, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *wl = lv_obj_create(c5);
    lv_obj_remove_style_all(wl);
    lv_obj_set_flex_flow(wl, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(wl, 0, 0);
    lv_obj_t *hi = lv_label_create(wl);
    lv_label_set_text(hi, "Welcome");
    lv_obj_set_style_text_color(hi, lv_color_hex(0x061018), 0);
    lv_obj_set_style_text_font(hi, &lv_font_montserrat_26, 0);
    lv_obj_t *sub = lv_label_create(wl);
    lv_label_set_text(sub, "ESP32-P4 LVGL v9  Demo  |  Home");
    lv_obj_set_style_text_color(sub, lv_color_hex(0x061018), 0);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);

    lv_obj_t *clk = lv_label_create(c5);
    lv_obj_set_style_text_color(clk, lv_color_hex(0x061018), 0);
    lv_obj_set_style_text_font(clk, &lv_font_montserrat_26, 0);
    lv_label_set_text(clk, "00:00");
}

/* =====================================================
 *                    Music 页
 * ===================================================== */
typedef struct {
    const char *title;
    const char *artist;
    int duration;
} track_t;

static const track_t s_tracks[] = {
    {"Ba Hui Yi Pin Hao Gei Ni",  "Wang Er Lang",    240},
    {"Wang Bu Diao De Ni",        "Wang Er Lang",    108},
    {"Ni Yao De Quan Na Zou",     "Hu Yanbin",       230},
    {"Wu Jian Du",                "Xu Song",         152},
};
#define TRACK_COUNT (sizeof(s_tracks) / sizeof(s_tracks[0]))

static lv_obj_t *s_cover, *s_title_lbl, *s_artist_lbl, *s_progress, *s_vol_slider;
static lv_obj_t *s_music_left = NULL, *s_music_right = NULL;
static bool s_panel_collapsed = false;

/* Status 页实时信息 */
static lv_obj_t *s_stat_cpu = NULL, *s_stat_heap = NULL, *s_stat_sd = NULL;
static lv_obj_t *s_stat_wifi = NULL, *s_stat_uptime = NULL;

/* 视频背景：逐帧播放 */
static lv_obj_t *s_music_bg = NULL;
static int s_frame_idx = 0;
static int s_video_idx = 0;
static const char *s_video_dirs[] = {
    "/sdcard/img/frames",
    "/sdcard/img/frames2",
    "/sdcard/img/frames3",
};
static const int s_video_frames[] = {30, 30, 30};
#define VIDEO_COUNT (sizeof(s_video_dirs)/sizeof(s_video_dirs[0]))

static void video_frame_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_music_bg) return;
    char path[128];
    snprintf(path, sizeof(path), "%s/frame_%02d.raw", s_video_dirs[s_video_idx], s_frame_idx);
    FILE *f = fopen(path, "rb");
    if (!f) return;
    static void *buf = NULL;
    if (!buf) buf = heap_caps_malloc(1024*600*2, MALLOC_CAP_SPIRAM);
    if (buf) {
        fseek(f, 0, SEEK_SET);
        fread(buf, 1, 1024*600*2, f);
        static lv_image_dsc_t dsc;
        dsc.header.w = 1024; dsc.header.h = 600;
        dsc.header.cf = LV_COLOR_FORMAT_RGB565;
        dsc.header.stride = 2048;
        dsc.data = buf; dsc.data_size = 1024*600*2;
        lv_image_set_src(s_music_bg, &dsc);
    }
    fclose(f);
    s_frame_idx = (s_frame_idx + 1) % s_video_frames[s_video_idx];
}

static void video_next_cb(lv_event_t *e)
{
    (void)e;
    s_video_idx = (s_video_idx + 1) % VIDEO_COUNT;
    s_frame_idx = 0;
}
static lv_obj_t *s_time_cur, *s_time_tot, *s_play_lbl, *s_list;
static int  s_cur_track = 0, s_cur_pos = 0;
static bool s_playing = true;

static void fmt_mmss(char *buf, size_t n, int sec)
{
    snprintf(buf, n, "%02d:%02d", sec / 60, sec % 60);
}

static void cover_rot_anim_cb(void *var, int32_t v)
{
    lv_obj_set_style_transform_rotation((lv_obj_t *)var, v, 0);
}

static void cover_rot_start(void)
{
    if (lv_anim_get(s_cover, cover_rot_anim_cb) != NULL) return;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_cover);
    lv_anim_set_values(&a, 0, 3600);
    lv_anim_set_time(&a, 8000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, cover_rot_anim_cb);
    lv_anim_start(&a);
}

static void cover_rot_stop(void)
{
    lv_anim_del(s_cover, cover_rot_anim_cb);
}

static void refresh_track(void)
{
    const track_t *t = &s_tracks[s_cur_track];
    char cur[16], tot[16];
    lv_label_set_text(s_title_lbl, t->title);
    lv_label_set_text(s_artist_lbl, t->artist);
    fmt_mmss(cur, sizeof(cur), s_cur_pos);
    fmt_mmss(tot, sizeof(tot), t->duration);
    lv_label_set_text(s_time_cur, cur);
    lv_label_set_text(s_time_tot, tot);
    lv_bar_set_range(s_progress, 0, t->duration);
    lv_bar_set_value(s_progress, s_cur_pos, LV_ANIM_OFF);

    uint32_t n = lv_obj_get_child_count(s_list);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *item = lv_obj_get_child(s_list, i);
        lv_obj_set_style_bg_color(item, i == (uint32_t)s_cur_track ? C_CARD2
                                                                     : lv_color_hex(0x171c23), 0);
    }
}

static void apply_play_state(void)
{
    lv_label_set_text(s_play_lbl, s_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
    if (s_playing) cover_rot_start(); else cover_rot_stop();
}

static void music_tick(lv_timer_t *t)
{
    (void)t;
    /* sync from real player position */
    s_cur_track = audio_player_track_index();
    s_cur_pos = audio_player_position_sec();
    if (!audio_player_is_playing() && s_playing) {
        /* track finished, auto next */
        s_cur_track = (s_cur_track + 1) % TRACK_COUNT;
        s_cur_pos = 0;
        audio_player_play_track(s_cur_track);
    }
    refresh_track();
}

static void play_pause_cb(lv_event_t *e)
{
    (void)e;
    s_playing = !s_playing;
    if (s_playing) {
        audio_player_play();
    } else {
        audio_player_stop();
    }
    apply_play_state();
}

static void next_cb(lv_event_t *e)
{
    (void)e;
    s_cur_track = (s_cur_track + 1) % TRACK_COUNT;
    s_cur_pos = 0;
    refresh_track();
    audio_player_next();
}

static void prev_cb(lv_event_t *e)
{
    (void)e;
    s_cur_track = (s_cur_track + TRACK_COUNT - 1) % TRACK_COUNT;
    s_cur_pos = 0;
    refresh_track();
    audio_player_prev();
}

static void list_item_cb(lv_event_t *e)
{
    lv_obj_t *item = lv_event_get_target_obj(e);
    s_cur_track = (int)(intptr_t)lv_obj_get_user_data(item);
    s_cur_pos = 0;
    refresh_track();
    audio_player_play_track(s_cur_track);
    s_playing = true;
    apply_play_state();
}

static void progress_cb(lv_event_t *e)
{
    lv_obj_t *bar = lv_event_get_target_obj(e);
    s_cur_pos = (int)lv_bar_get_value(bar);
    char cur[16];
    fmt_mmss(cur, sizeof(cur), s_cur_pos);
    lv_label_set_text(s_time_cur, cur);
}

static void vol_slider_cb(lv_event_t *e)
{
    lv_obj_t *sl = lv_event_get_target_obj(e);
    int v = (int)lv_slider_get_value(sl);
    audio_player_set_volume(v);
}

/* 前向声明：壁纸切换 */
static lv_obj_t *s_page_bgs[3];
static void wall_next_cb(lv_event_t *e);

static void toggle_panel_cb(lv_event_t *e)
{
    (void)e;
    s_panel_collapsed = !s_panel_collapsed;
    if (s_music_left) lv_obj_add_flag(s_music_left, s_panel_collapsed ? LV_OBJ_FLAG_HIDDEN : LV_OBJ_FLAG_CLICKABLE);
    if (s_music_right) lv_obj_add_flag(s_music_right, s_panel_collapsed ? LV_OBJ_FLAG_HIDDEN : LV_OBJ_FLAG_CLICKABLE);
    if (s_music_left) lv_obj_clear_flag(s_music_left, s_panel_collapsed ? LV_OBJ_FLAG_CLICKABLE : LV_OBJ_FLAG_HIDDEN);
    if (s_music_right) lv_obj_clear_flag(s_music_right, s_panel_collapsed ? LV_OBJ_FLAG_CLICKABLE : LV_OBJ_FLAG_HIDDEN);
}

static void build_music_page(lv_obj_t *parent)
{
    /* Load sakura background from SD card into PSRAM */
    static lv_image_dsc_t sakura_dsc = {0};
    static const uint8_t *sakura_buf = NULL;
    if (!sakura_buf) {
        FILE *f = fopen("/sdcard/img/sakura.raw", "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            size_t sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            void *buf = heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
            if (buf && fread(buf, 1, sz, f) == sz) {
                sakura_buf = buf;
                sakura_dsc.header.w = 1024;
                sakura_dsc.header.h = 600;
                sakura_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
                sakura_dsc.header.stride = 2048;
                sakura_dsc.data = buf;
                sakura_dsc.data_size = sz;
            }
            fclose(f);
        }
    }
    lv_obj_t *bg = lv_image_create(parent);
    if (sakura_dsc.data) lv_image_set_src(bg, &sakura_dsc);
    lv_obj_set_size(bg, lv_pct(100), lv_pct(100));
    lv_obj_align(bg, LV_ALIGN_TOP_LEFT, 0, 0);
    s_page_bgs[1] = bg;
    s_music_bg = bg;
    /* 启动视频帧 timer（30fps） */
    /* 启动视频帧 timer（30fps） */
    lv_timer_create(video_frame_cb, 33, NULL);

    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* ---- 左卡片 ---- */
    lv_obj_t *left = lv_obj_create(root);
    s_music_left = left;
    lv_obj_set_size(left, 660, 448);
    lv_obj_set_style_bg_color(left, lv_color_hex(0x1c222b), 0);
    lv_obj_set_style_bg_opa(left, LV_OPA_80, 0);
    lv_obj_set_style_radius(left, 14, 0);
    lv_obj_set_style_border_color(left, C_DIVIDER, 0);
    lv_obj_set_style_border_width(left, 1, 0);
    lv_obj_set_style_pad_all(left, 16, 0);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(left, 10, 0);

    s_cover = lv_obj_create(left);
    lv_obj_set_size(s_cover, 150, 150);
    lv_obj_set_style_bg_color(s_cover, lv_color_hex(0x1a1f28), 0);
    lv_obj_set_style_bg_grad_color(s_cover, C_PRIMARY, 0);
    lv_obj_set_style_bg_grad_dir(s_cover, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_radius(s_cover, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(s_cover, C_DIVIDER, 0);
    lv_obj_set_style_border_width(s_cover, 2, 0);
    lv_obj_set_style_pad_all(s_cover, 0, 0);
    lv_obj_set_style_transform_pivot_x(s_cover, 75, 0);
    lv_obj_set_style_transform_pivot_y(s_cover, 75, 0);

    lv_obj_t *hole = lv_obj_create(s_cover);
    lv_obj_set_size(hole, 34, 34);
    lv_obj_set_style_bg_color(hole, C_BG, 0);
    lv_obj_set_style_radius(hole, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(hole, C_ACCENT, 0);
    lv_obj_set_style_border_width(hole, 3, 0);
    lv_obj_center(hole);

    s_title_lbl = lv_label_create(left);
    lv_label_set_text(s_title_lbl, "-");
    lv_obj_set_style_text_color(s_title_lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(s_title_lbl, &lv_font_montserrat_26, 0);

    s_artist_lbl = lv_label_create(left);
    lv_label_set_text(s_artist_lbl, "-");
    lv_obj_set_style_text_color(s_artist_lbl, C_DIM, 0);
    lv_obj_set_style_text_font(s_artist_lbl, &lv_font_montserrat_14, 0);

    s_progress = lv_bar_create(left);
    lv_obj_set_width(s_progress, 600);
    lv_obj_set_height(s_progress, 10);
    lv_obj_set_style_bg_color(s_progress, C_CARD2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_progress, C_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_progress, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_add_flag(s_progress, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_progress, progress_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *time_row = lv_obj_create(left);
    lv_obj_remove_style_all(time_row);
    lv_obj_set_size(time_row, 600, 22);
    lv_obj_set_flex_flow(time_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(time_row, 0, 0);
    s_time_cur = lv_label_create(time_row);
    lv_obj_set_style_text_color(s_time_cur, C_DIM, 0);
    lv_obj_set_style_text_font(s_time_cur, &lv_font_montserrat_14, 0);
    s_time_tot = lv_label_create(time_row);
    lv_obj_set_style_text_color(s_time_tot, C_DIM, 0);
    lv_obj_set_style_text_font(s_time_tot, &lv_font_montserrat_14, 0);

    /* Volume slider */
    lv_obj_t *vol_row = lv_obj_create(left);
    lv_obj_remove_style_all(vol_row);
    lv_obj_set_size(vol_row, 600, 26);
    lv_obj_set_flex_flow(vol_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(vol_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(vol_row, 0, 0);
    lv_obj_set_style_pad_column(vol_row, 10, 0);
    lv_obj_t *vol_ic = lv_label_create(vol_row);
    lv_obj_set_style_text_color(vol_ic, C_DIM, 0);
    lv_label_set_text(vol_ic, LV_SYMBOL_AUDIO);
    s_vol_slider = lv_slider_create(vol_row);
    lv_obj_set_width(s_vol_slider, 400);
    lv_obj_set_height(s_vol_slider, 8);
    lv_slider_set_range(s_vol_slider, 0, 100);
    lv_slider_set_value(s_vol_slider, 30, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_vol_slider, C_CARD2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_vol_slider, C_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_vol_slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_add_event_cb(s_vol_slider, vol_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *ctrl = lv_obj_create(left);
    lv_obj_remove_style_all(ctrl);
    lv_obj_set_size(ctrl, 600, 70);
    lv_obj_set_flex_flow(ctrl, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ctrl, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(ctrl, 24, 0);
    lv_obj_set_style_pad_all(ctrl, 0, 0);

    lv_obj_t *prev = lv_button_create(ctrl);
    lv_obj_set_size(prev, 60, 60);
    lv_obj_set_style_bg_color(prev, C_CARD2, 0);
    lv_obj_set_style_radius(prev, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_event_cb(prev, prev_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *pl = lv_label_create(prev);
    lv_label_set_text(pl, LV_SYMBOL_PREV);
    lv_obj_set_style_text_color(pl, C_TEXT, 0);
    lv_obj_center(pl);

    lv_obj_t *play = lv_button_create(ctrl);
    lv_obj_set_size(play, 72, 72);
    lv_obj_set_style_bg_color(play, C_PRIMARY, 0);
    lv_obj_set_style_radius(play, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_event_cb(play, play_pause_cb, LV_EVENT_CLICKED, NULL);
    s_play_lbl = lv_label_create(play);
    lv_obj_set_style_text_color(s_play_lbl, lv_color_hex(0x061018), 0);
    lv_obj_set_style_text_font(s_play_lbl, &lv_font_montserrat_26, 0);
    lv_obj_center(s_play_lbl);

    lv_obj_t *next = lv_button_create(ctrl);
    lv_obj_set_size(next, 60, 60);
    lv_obj_set_style_bg_color(next, C_CARD2, 0);
    lv_obj_set_style_radius(next, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_event_cb(next, next_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *nl = lv_label_create(next);
    lv_label_set_text(nl, LV_SYMBOL_NEXT);
    lv_obj_set_style_text_color(nl, C_TEXT, 0);
    lv_obj_center(nl);

    /* ---- 右卡片：播放列表 ---- */
    lv_obj_t *right = lv_obj_create(root);
    s_music_right = right;
    lv_obj_set_size(right, 292, 448);
    lv_obj_set_style_bg_color(right, lv_color_hex(0x1c222b), 0);
    lv_obj_set_style_bg_opa(right, LV_OPA_80, 0);
    lv_obj_set_style_radius(right, 14, 0);
    lv_obj_set_style_border_color(right, C_DIVIDER, 0);
    lv_obj_set_style_border_width(right, 1, 0);
    lv_obj_set_style_pad_all(right, 16, 0);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(right, 10, 0);

    lv_obj_t *hd = lv_label_create(right);
    lv_label_set_text(hd, "Playlist");
    lv_obj_set_style_text_color(hd, C_TEXT, 0);
    lv_obj_set_style_text_font(hd, &lv_font_montserrat_24, 0);

    s_list = lv_list_create(right);
    lv_obj_set_size(s_list, 252, 340);
    lv_obj_set_style_bg_color(s_list, lv_color_hex(0x171c23), 0);
    lv_obj_set_style_radius(s_list, 8, 0);
    lv_obj_set_style_pad_row(s_list, 4, 0);

    for (uint32_t i = 0; i < TRACK_COUNT; i++) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%d. %s", i + 1, s_tracks[i].title);
        lv_obj_t *item = lv_list_add_button(s_list, NULL, buf);
        lv_obj_set_user_data(item, (void *)(intptr_t)i);
        lv_obj_add_event_cb(item, list_item_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_set_style_bg_color(item, lv_color_hex(0x171c23), 0);
    }

    refresh_track();
    apply_play_state();
    lv_timer_create(music_tick, 1000, NULL);

    /* 最后加按钮，确保在最上层 */
    lv_obj_t *wb = lv_button_create(parent);
    lv_obj_set_size(wb, 56, 56);
    lv_obj_set_style_bg_color(wb, lv_color_hex(0xff6b9d), 0);
    lv_obj_set_style_radius(wb, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(wb, 0, 0);
    lv_obj_align(wb, LV_ALIGN_BOTTOM_RIGHT, -20, -60);
    lv_obj_add_event_cb(wb, video_next_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *wl = lv_label_create(wb);
    lv_label_set_text(wl, LV_SYMBOL_VIDEO);
    lv_obj_set_style_text_color(wl, lv_color_white(), 0);
    lv_obj_center(wl);

    /* 收缩/展开面板按钮 */
    lv_obj_t *tb = lv_button_create(parent);
    lv_obj_set_size(tb, 56, 56);
    lv_obj_set_style_bg_color(tb, lv_color_hex(0x444444), 0);
    lv_obj_set_style_radius(tb, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(tb, 0, 0);
    lv_obj_align(tb, LV_ALIGN_BOTTOM_RIGHT, -90, -60);
    lv_obj_add_event_cb(tb, toggle_panel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *tl = lv_label_create(tb);
    lv_label_set_text(tl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(tl, lv_color_white(), 0);
    lv_obj_center(tl);
}

/* =====================================================
 *                    Benchmark 页
 * ===================================================== */
#define BALL_CNT 8

static lv_obj_t *s_zone, *s_balls[BALL_CNT], *s_arc;
static lv_obj_t *s_fps_lbl, *s_avg_lbl, *s_min_lbl, *s_status_lbl;
static bool s_running = false;
static uint32_t s_frame_cnt = 0, s_window_tick = 0;
static float s_fps_avg = 0.0f, s_fps_min = 9999.0f;

static const uint32_t s_ball_hex[BALL_CNT] = {
    0x00b4d8, 0xff9e00, 0xef476f, 0x80db40,
    0x9b5de0, 0x4895ff, 0xffd166, 0xf87575,
};

static void bench_anim_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_running) return;
    int32_t zw = (int32_t)lv_obj_get_width(s_zone) - 40;
    int32_t zh = (int32_t)lv_obj_get_height(s_zone) - 40;
    for (int i = 0; i < BALL_CNT; i++) {
        int32_t a1 = (lv_tick_get() / (20 + i * 6) * (2 + i % 3)) % 360;
        int32_t a2 = (lv_tick_get() / (25 + i * 5) * (3 + i % 4) + 120) % 360;
        int32_t sx = lv_trigo_sin(a1);
        int32_t sy = lv_trigo_sin(a2);
        lv_obj_set_pos(s_balls[i],
                       (zw * (sx + 1024)) / 2048,
                       (zh * (sy + 1024)) / 2048);
    }
    lv_arc_set_rotation(s_arc, (lv_tick_get() / 8) % 360);
}

static void fps_sample_cb(lv_timer_t *t)
{
    (void)t;
    s_frame_cnt++;
    uint32_t now = lv_tick_get();
    if (s_window_tick == 0) { s_window_tick = now; s_frame_cnt = 0; return; }
    uint32_t el = now - s_window_tick;
    if (el >= 1000) {
        float fps = (float)s_frame_cnt * 1000.0f / (float)el;
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f FPS", fps);
        lv_label_set_text(s_fps_lbl, buf);
        s_fps_avg = (s_fps_avg == 0.0f) ? fps : s_fps_avg * 0.8f + fps * 0.2f;
        if (fps < s_fps_min) s_fps_min = fps;
        snprintf(buf, sizeof(buf), "avg %.1f", s_fps_avg);
        lv_label_set_text(s_avg_lbl, buf);
        snprintf(buf, sizeof(buf), "min %.1f", s_fps_min);
        lv_label_set_text(s_min_lbl, buf);
        s_window_tick = now;
        s_frame_cnt = 0;
    }
}

static void start_bench_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target_obj(e);
    s_running = !s_running;
    if (s_running) {
        s_fps_avg = 0.0f; s_fps_min = 9999.0f;
        s_window_tick = 0; s_frame_cnt = 0;
        lv_label_set_text(lv_obj_get_child(btn, 0), LV_SYMBOL_STOP "  Stop");
        lv_label_set_text(s_status_lbl, "Running...");
    } else {
        lv_label_set_text(lv_obj_get_child(btn, 0), LV_SYMBOL_PLAY "  Run Benchmark");
        lv_label_set_text(s_status_lbl, "Stopped.");
    }
}

static void build_bench_page(lv_obj_t *parent)
{
    lv_obj_t *root = lv_obj_create(parent);
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(root, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(root, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(root, 12, 0);

    lv_obj_t *info = lv_obj_create(root);
    lv_obj_set_size(info, 992, 80);
    lv_obj_set_style_bg_color(info, C_CARD, 0);
    lv_obj_set_style_radius(info, 14, 0);
    lv_obj_set_style_border_color(info, C_DIVIDER, 0);
    lv_obj_set_style_border_width(info, 1, 0);
    lv_obj_set_style_pad_all(info, 16, 0);
    lv_obj_set_flex_flow(info, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(info, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    s_fps_lbl = lv_label_create(info);
    lv_obj_set_style_text_font(s_fps_lbl, &lv_font_montserrat_26, 0);
    lv_obj_set_style_text_color(s_fps_lbl, C_PRIMARY, 0);
    lv_label_set_text(s_fps_lbl, "--.- FPS");

    s_avg_lbl = lv_label_create(info);
    lv_obj_set_style_text_color(s_avg_lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(s_avg_lbl, &lv_font_montserrat_16, 0);
    lv_label_set_text(s_avg_lbl, "avg --.-");

    s_min_lbl = lv_label_create(info);
    lv_obj_set_style_text_color(s_min_lbl, C_TEXT, 0);
    lv_obj_set_style_text_font(s_min_lbl, &lv_font_montserrat_16, 0);
    lv_label_set_text(s_min_lbl, "min --.-");

    s_status_lbl = lv_label_create(info);
    lv_obj_set_style_text_color(s_status_lbl, C_DIM, 0);
    lv_obj_set_style_text_font(s_status_lbl, &lv_font_montserrat_14, 0);
    lv_label_set_text(s_status_lbl, "Idle.");

    lv_obj_t *btn = lv_button_create(info);
    lv_obj_set_style_bg_color(btn, C_PRIMARY, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_pad_hor(btn, 22, 0);
    lv_obj_set_style_pad_ver(btn, 12, 0);
    lv_obj_add_event_cb(btn, start_bench_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bl = lv_label_create(btn);
    lv_label_set_text(bl, LV_SYMBOL_PLAY "  Run Benchmark");
    lv_obj_set_style_text_color(bl, lv_color_hex(0x061018), 0);

    s_zone = lv_obj_create(root);
    lv_obj_set_size(s_zone, 992, 360);
    lv_obj_set_style_bg_color(s_zone, C_CARD2, 0);
    lv_obj_set_style_radius(s_zone, 14, 0);
    lv_obj_set_style_pad_all(s_zone, 0, 0);
    lv_obj_set_style_clip_corner(s_zone, true, 0);

    for (int i = 0; i < BALL_CNT; i++) {
        s_balls[i] = lv_obj_create(s_zone);
        lv_obj_set_size(s_balls[i], 36, 36);
        lv_obj_set_style_bg_color(s_balls[i], lv_color_hex(s_ball_hex[i]), 0);
        lv_obj_set_style_radius(s_balls[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s_balls[i], 0, 0);
        lv_obj_set_pos(s_balls[i], 10 + i * 40, 10 + i * 30);
    }

    s_arc = lv_arc_create(s_zone);
    lv_obj_set_size(s_arc, 90, 90);
    lv_obj_center(s_arc);
    lv_arc_set_bg_angles(s_arc, 0, 360);
    lv_arc_set_angles(s_arc, 0, 120);
    lv_obj_set_style_arc_color(s_arc, C_CARD, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, C_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 8, LV_PART_INDICATOR);
    lv_obj_remove_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);

    lv_timer_create(fps_sample_cb, 16, NULL);
    lv_timer_create(bench_anim_cb, 16, NULL);

    /* ---- 系统信息卡片 ---- */
    lv_obj_t *sys = lv_obj_create(root);
    lv_obj_set_size(sys, 992, 90);
    lv_obj_set_style_bg_color(sys, C_CARD, 0);
    lv_obj_set_style_radius(sys, 14, 0);
    lv_obj_set_style_border_color(sys, C_DIVIDER, 0);
    lv_obj_set_style_border_width(sys, 1, 0);
    lv_obj_set_style_pad_all(sys, 16, 0);
    lv_obj_set_flex_flow(sys, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sys, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *info1 = lv_obj_create(sys);
    lv_obj_remove_style_all(info1);
    lv_obj_set_flex_flow(info1, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(info1, 0, 0);
    lv_obj_t *l1 = lv_label_create(info1);
    lv_label_set_text(l1, LV_SYMBOL_SETTINGS "  Chip");
    lv_obj_set_style_text_color(l1, C_DIM, 0);
    lv_obj_set_style_text_font(l1, &lv_font_montserrat_14, 0);
    lv_obj_t *v1 = lv_label_create(info1);
    lv_label_set_text(v1, "ESP32-P4  RISC-V");
    lv_obj_set_style_text_color(v1, C_TEXT, 0);
    lv_obj_set_style_text_font(v1, &lv_font_montserrat_16, 0);

    lv_obj_t *info2 = lv_obj_create(sys);
    lv_obj_remove_style_all(info2);
    lv_obj_set_flex_flow(info2, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(info2, 0, 0);
    lv_obj_t *l2 = lv_label_create(info2);
    lv_label_set_text(l2, LV_SYMBOL_DRIVE "  Free Heap");
    lv_obj_set_style_text_color(l2, C_DIM, 0);
    lv_obj_set_style_text_font(l2, &lv_font_montserrat_14, 0);
    lv_obj_t *v2 = lv_label_create(info2);
    lv_label_set_text(v2, "--- KB");
    lv_obj_set_style_text_color(v2, C_TEXT, 0);
    lv_obj_set_style_text_font(v2, &lv_font_montserrat_16, 0);

    lv_obj_t *info3 = lv_obj_create(sys);
    lv_obj_remove_style_all(info3);
    lv_obj_set_flex_flow(info3, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(info3, 0, 0);
    lv_obj_t *l3 = lv_label_create(info3);
    lv_label_set_text(l3, LV_SYMBOL_FILE "  LVGL");
    lv_obj_set_style_text_color(l3, C_DIM, 0);
    lv_obj_set_style_text_font(l3, &lv_font_montserrat_14, 0);
    lv_obj_t *v3 = lv_label_create(info3);
    lv_label_set_text(v3, "v9.5.0");
    lv_obj_set_style_text_color(v3, C_TEXT, 0);
    lv_obj_set_style_text_font(v3, &lv_font_montserrat_16, 0);

    lv_obj_t *info4 = lv_obj_create(sys);
    lv_obj_remove_style_all(info4);
    lv_obj_set_flex_flow(info4, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(info4, 0, 0);
    lv_obj_t *l4 = lv_label_create(info4);
    lv_label_set_text(l4, LV_SYMBOL_AUDIO "  Audio");
    lv_obj_set_style_text_color(l4, C_DIM, 0);
    lv_obj_set_style_text_font(l4, &lv_font_montserrat_14, 0);
    lv_obj_t *v4 = lv_label_create(info4);
    lv_label_set_text(v4, "ES8311  16kHz");
    lv_obj_set_style_text_color(v4, C_TEXT, 0);
    lv_obj_set_style_text_font(v4, &lv_font_montserrat_16, 0);

    /* refresh heap every 2s */
    static lv_obj_t *s_heap_lbl = NULL;
    s_heap_lbl = v2;
    (void)s_heap_lbl;
}

/* =====================================================
 *              粒子图形库页（通用）
 * ===================================================== */
#define PARTICLE_CNT 22
typedef struct {
    lv_obj_t *dot;
    float x, y, vx, vy;
} particle_t;
static particle_t s_particles[PARTICLE_CNT * 2];
static bool s_particle_timer_started = false;

static void gallery_fab_cb(lv_event_t *e)
{
    int slot = (int)(intptr_t)lv_event_get_user_data(e);
    for (int i = 0; i < PARTICLE_CNT; i++) {
        particle_t *p = &s_particles[slot + i];
        p->vx = ((float)lv_rand(0, 200) - 100.0f) / 100.0f;
        p->vy = ((float)lv_rand(0, 200) - 100.0f) / 100.0f;
    }
}

static void particle_timer_cb(lv_timer_t *t)
{
    (void)t;
    for (int i = 0; i < PARTICLE_CNT * 2; i++) {
        if (!s_particles[i].dot) continue;
        s_particles[i].x += s_particles[i].vx;
        s_particles[i].y += s_particles[i].vy;
        float w = 992.0f, h = 448.0f;
        if (s_particles[i].x < 0 || s_particles[i].x > w) s_particles[i].vx *= -1;
        if (s_particles[i].y < 0 || s_particles[i].y > h) s_particles[i].vy *= -1;
        if (s_particles[i].x < 0) s_particles[i].x = 0;
        if (s_particles[i].x > w) s_particles[i].x = w;
        if (s_particles[i].y < 0) s_particles[i].y = 0;
        if (s_particles[i].y > h) s_particles[i].y = h;
        lv_obj_set_pos(s_particles[i].dot, (int)s_particles[i].x, (int)s_particles[i].y);
    }
}

/**
 * 在 parent 上绘制：背景图 + 漂浮粒子 + 标题。
 * @param img      背景图
 * @param accent   粒子颜色
 * @param slot     粒子槽位起始索引（0 或 PARTICLE_CNT）
 */
static lv_obj_t *s_page_bgs[3] = {NULL, NULL, NULL};
static lv_obj_t *s_wall_label = NULL;
static int s_wall_idx = 0;
static const char *s_wall_paths[] = {
    "/sdcard/img/wall1.raw",
    "/sdcard/img/wall2.raw",
    "/sdcard/img/wall3.raw",
    "/sdcard/img/sakura.raw",
};
static const char *s_wall_names[] = {
    "Wall 1 - Angel",
    "Wall 2 - Test1",
    "Wall 3 - Sakura",
    "Wall 4 - Moon",
};
#define WALL_COUNT (sizeof(s_wall_paths)/sizeof(s_wall_paths[0]))

static bool load_wallpaper(lv_obj_t *bg, int idx)
{
    FILE *f = fopen(s_wall_paths[idx], "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *buf = heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
    if (!buf || fread(buf, 1, sz, f) != sz) { fclose(f); return false; }
    static lv_image_dsc_t dsc;
    dsc.header.w = 1024; dsc.header.h = 600;
    dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    dsc.header.stride = 2048;
    dsc.data = buf; dsc.data_size = sz;
    lv_image_set_src(bg, &dsc);
    fclose(f);
    return true;
}

static void wall_next_cb(lv_event_t *e)
{
    int slot = (int)(intptr_t)lv_event_get_user_data(e);
    if (slot < 0 || slot >= 3 || !s_page_bgs[slot]) return;
    s_wall_idx = (s_wall_idx + 1) % WALL_COUNT;
    if (s_wall_label) lv_label_set_text(s_wall_label, "Loading...");
    if (load_wallpaper(s_page_bgs[slot], s_wall_idx)) {
        if (s_wall_label) lv_label_set_text(s_wall_label, s_wall_names[s_wall_idx]);
    } else {
        if (s_wall_label) lv_label_set_text(s_wall_label, "SD not mounted");
    }
}

/* Status 页：每秒刷新系统信息 */
static void stat_timer_cb(lv_timer_t *t)
{
    if (!s_stat_cpu) return;
    char buf[64];

    /* CPU 频率 */
    snprintf(buf, sizeof(buf), "CPU  :  240 MHz (P4 dual-core RISC-V)");
    lv_label_set_text(s_stat_cpu, buf);

    /* Heap */
    int free_kb = (int)heap_caps_get_free_size(MALLOC_CAP_DEFAULT) / 1024;
    int min_kb  = (int)heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT) / 1024;
    snprintf(buf, sizeof(buf), "Heap :  %d KB free  (min %d KB)", free_kb, min_kb);
    lv_label_set_text(s_stat_heap, buf);

    /* SD 卡容量 */
    FATFS *fs = NULL; DWORD free_cl;
    if (f_getfree("/sdcard", &free_cl, &fs) == FR_OK && fs) {
        uint64_t total = (uint64_t)fs->n_fatent * fs->csize * 512ULL;
        uint64_t frees = (uint64_t)free_cl * fs->csize * 512ULL;
        snprintf(buf, sizeof(buf), "SD   :  %lu MB free / %lu MB",
                 (unsigned long)(frees/1024/1024), (unsigned long)(total/1024/1024));
    } else {
        snprintf(buf, sizeof(buf), "SD   :  not mounted");
    }
    lv_label_set_text(s_stat_sd, buf);

    /* WiFi */
    snprintf(buf, sizeof(buf), "WiFi :  ESP32-C6 (SDIO/SPI)");
    lv_label_set_text(s_stat_wifi, buf);
    lv_obj_set_style_text_color(s_stat_wifi, lv_color_hex(0xffd166), 0);

    /* Uptime */
    uint32_t sec = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
    snprintf(buf, sizeof(buf), "Up   :  %02lu:%02lu:%02lu",
             (unsigned long)(sec/3600), (unsigned long)((sec%3600)/60), (unsigned long)(sec%60));
    lv_label_set_text(s_stat_uptime, buf);
}

static void build_gallery_page(lv_obj_t *parent, const lv_image_dsc_t *img,
                               uint32_t accent, int slot)
{
    int page_idx = slot;
    if (slot == PARTICLE_CNT) page_idx = 2;
    lv_obj_t *bg = lv_image_create(parent);
    lv_image_set_src(bg, img);
    lv_obj_set_size(bg, lv_pct(100), lv_pct(100));
    lv_obj_align(bg, LV_ALIGN_TOP_LEFT, 0, 0);
    if (page_idx >= 0 && page_idx < 3) s_page_bgs[page_idx] = bg;

    for (int i = 0; i < PARTICLE_CNT; i++) {
        particle_t *p = &s_particles[slot + i];
        p->dot = lv_obj_create(parent);
        int sz = 4 + (lv_rand(0, 100) % 5);
        lv_obj_set_size(p->dot, sz, sz);
        lv_obj_set_style_radius(p->dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(p->dot, lv_color_hex(accent), 0);
        lv_obj_set_style_bg_opa(p->dot, LV_OPA_80, 0);
        lv_obj_set_style_border_width(p->dot, 0, 0);
        p->x = (float)lv_rand(0, 992);
        p->y = (float)lv_rand(0, 448);
        p->vx = ((float)lv_rand(0, 100) - 50.0f) / 400.0f;
        p->vy = ((float)lv_rand(0, 100) - 50.0f) / 400.0f;
        lv_obj_set_pos(p->dot, (int)p->x, (int)p->y);
    }
    if (!s_particle_timer_started) {
        lv_timer_create(particle_timer_cb, 30, NULL);
        s_particle_timer_started = true;
    }

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, slot == 0 ? "Particle Gallery - V1" : "Particle Gallery - V2");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_26, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_opa(title, LV_OPA_90, 0);
    lv_obj_set_pos(title, 24, 20);

    lv_obj_t *hint = lv_label_create(parent);
    lv_label_set_text(hint, slot == 0 ? "Cyberpunk - Rainy Neon" : "Seaside - Summer Sky");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(hint, lv_color_white(), 0);
    lv_obj_set_style_text_opa(hint, LV_OPA_70, 0);
    lv_obj_set_pos(hint, 24, 60);

    /* 所有页右下角：壁纸切换按钮 */
    lv_obj_t *wb = lv_button_create(parent);
    lv_obj_set_size(wb, 56, 56);
    lv_obj_set_style_bg_color(wb, lv_color_hex(accent), 0);
    lv_obj_set_style_radius(wb, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(wb, 0, 0);
    lv_obj_align(wb, LV_ALIGN_BOTTOM_RIGHT, -20, -60);
    lv_obj_add_event_cb(wb, wall_next_cb, LV_EVENT_CLICKED, (void *)(intptr_t)page_idx);
    lv_obj_t *wl = lv_label_create(wb);
    lv_label_set_text(wl, LV_SYMBOL_IMAGE);
    lv_obj_set_style_text_color(wl, lv_color_white(), 0);
    lv_obj_center(wl);

    /* Home 页加状态 label */
    if (page_idx == 0) {
        s_wall_label = lv_label_create(parent);
        lv_label_set_text(s_wall_label, "Wallpaper: tap icon to switch");
        lv_obj_set_style_text_font(s_wall_label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_wall_label, lv_color_white(), 0);
        lv_obj_set_style_text_opa(s_wall_label, LV_OPA_80, 0);
        lv_obj_set_pos(s_wall_label, 24, 90);

        /* 中央大标题（带半透明背景） */
        lv_obj_t *card = lv_obj_create(parent);
        lv_obj_remove_style_all(card);
        lv_obj_set_size(card, 460, 110);
        lv_obj_center(card);
        lv_obj_set_style_bg_color(card, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(card, LV_OPA_40, 0);
        lv_obj_set_style_radius(card, 16, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *title = lv_label_create(card);
        lv_label_set_text(title, "谁暖了冬季");
        lv_obj_set_style_text_font(title, &cn_font, 0);
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
        lv_obj_center(title);
        lv_obj_align(title, LV_ALIGN_CENTER, 0, -20);

        lv_obj_t *sub = lv_label_create(card);
        lv_label_set_text(sub, "LVGL开发");
        lv_obj_set_style_text_font(sub, &cn_font, 0);
        lv_obj_set_style_text_color(sub, lv_color_hex(0xff9ecb), 0);
        lv_obj_align(sub, LV_ALIGN_CENTER, 0, 20);
    }

    /* Status 页：实时系统信息面板 */
    if (page_idx == 2) {
        lv_obj_t *panel = lv_obj_create(parent);
        lv_obj_remove_style_all(panel);
        lv_obj_set_size(panel, 380, 200);
        lv_obj_align(panel, LV_ALIGN_CENTER, 0, 20);
        lv_obj_set_style_bg_color(panel, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(panel, LV_OPA_50, 0);
        lv_obj_set_style_radius(panel, 16, 0);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *title = lv_label_create(panel);
        lv_label_set_text(title, "System Status");
        lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
        lv_obj_set_style_text_color(title, lv_color_hex(0xffd166), 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 15);

        s_stat_cpu   = lv_label_create(panel);
        s_stat_heap  = lv_label_create(panel);
        s_stat_sd    = lv_label_create(panel);
        s_stat_wifi  = lv_label_create(panel);
        s_stat_uptime = lv_label_create(panel);

        lv_obj_set_style_text_font(s_stat_cpu, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_font(s_stat_heap, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_font(s_stat_sd, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_font(s_stat_wifi, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_font(s_stat_uptime, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(s_stat_cpu, lv_color_white(), 0);
        lv_obj_set_style_text_color(s_stat_heap, lv_color_white(), 0);
        lv_obj_set_style_text_color(s_stat_sd, lv_color_white(), 0);
        lv_obj_set_style_text_color(s_stat_wifi, lv_color_white(), 0);
        lv_obj_set_style_text_color(s_stat_uptime, lv_color_hex(0x9ecbff), 0);

        lv_obj_align(s_stat_cpu,    LV_ALIGN_TOP_LEFT, 20, 55);
        lv_obj_align(s_stat_heap,   LV_ALIGN_TOP_LEFT, 20, 85);
        lv_obj_align(s_stat_sd,     LV_ALIGN_TOP_LEFT, 20, 115);
        lv_obj_align(s_stat_wifi,   LV_ALIGN_TOP_LEFT, 20, 145);
        lv_obj_align(s_stat_uptime, LV_ALIGN_TOP_LEFT, 20, 175);

        lv_timer_create(stat_timer_cb, 1000, NULL);
    }
}

/* =====================================================
 *                    主入口
 * ===================================================== */
void ui_init(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, C_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* 顶部导航栏 80（ROTATE_180 下物理屏幕底部） */
    lv_obj_t *nav = lv_obj_create(scr);
    lv_obj_remove_style_all(nav);
    lv_obj_set_size(nav, 1024, 80);
    lv_obj_align(nav, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(nav, lv_color_hex(0x0a0d11), 0);
    lv_obj_set_style_border_color(nav, C_DIVIDER, 0);
    lv_obj_set_style_border_width(nav, 1, 0);
    lv_obj_set_style_border_side(nav, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    static const char *labels[PAGE_COUNT] = {
        LV_SYMBOL_HOME      "  Home",
        LV_SYMBOL_AUDIO     "  Music",
        LV_SYMBOL_SETTINGS  "  Status",
    };
    for (int i = 0; i < PAGE_COUNT; i++) {
        s_nav_btns[i] = lv_button_create(nav);
        lv_obj_set_size(s_nav_btns[i], 280, 56);
        lv_obj_set_style_radius(s_nav_btns[i], 10, 0);
        lv_obj_set_user_data(s_nav_btns[i], (void *)(intptr_t)i);
        lv_obj_add_event_cb(s_nav_btns[i], nav_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *lbl = lv_label_create(s_nav_btns[i]);
        lv_label_set_text(lbl, labels[i]);
        lv_obj_center(lbl);
    }

    /* 中部内容区 */
    lv_obj_t *content = lv_obj_create(scr);
    lv_obj_remove_style_all(content);
    lv_obj_set_size(content, 1024, 600 - 40 - 80);
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_remove_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < PAGE_COUNT; i++) {
        s_pages[i] = lv_obj_create(content);
        lv_obj_remove_style_all(s_pages[i]);
        lv_obj_set_size(s_pages[i], 992, 448);
        lv_obj_align(s_pages[i], LV_ALIGN_TOP_LEFT, 16, 16);
        lv_obj_remove_flag(s_pages[i], LV_OBJ_FLAG_SCROLLABLE);
    }
    build_gallery_page(s_pages[PAGE_WIDGETS], &img_tab1, 0x00b4d8, 0);
    build_music_page(s_pages[PAGE_MUSIC]);
    build_gallery_page(s_pages[PAGE_BENCH], &img_tab3, 0xffd166, PARTICLE_CNT);

    /* 底部状态栏 40（ROTATE_180 下物理屏幕顶部） */
    lv_obj_t *status = lv_obj_create(scr);
    lv_obj_remove_style_all(status);
    lv_obj_set_size(status, 1024, 40);
    lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(status, lv_color_hex(0x0a0d11), 0);
    lv_obj_set_flex_flow(status, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(status, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(status, 16, 0);

    lv_obj_t *brand = lv_label_create(status);
    lv_obj_set_style_text_font(brand, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(brand, C_DIM, 0);
    lv_label_set_text(brand, "ESP32-P4  LVGL v9 Demo");

    s_clock_lbl = lv_label_create(status);
    lv_obj_set_style_text_font(s_clock_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_clock_lbl, C_TEXT, 0);
    lv_label_set_text(s_clock_lbl, "00:00:00");

    lv_obj_t *icons = lv_obj_create(status);
    lv_obj_remove_style_all(icons);
    lv_obj_set_flex_flow(icons, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(icons, 12, 0);
    lv_obj_set_style_pad_all(icons, 0, 0);
    lv_obj_t *ic1 = lv_label_create(icons);
    lv_obj_set_style_text_color(ic1, C_PRIMARY, 0);
    lv_label_set_text(ic1, LV_SYMBOL_WIFI);
    lv_obj_t *ic2 = lv_label_create(icons);
    lv_obj_set_style_text_color(ic2, C_ACCENT, 0);
    lv_label_set_text(ic2, LV_SYMBOL_BATTERY_FULL);

    switch_page(PAGE_WIDGETS);
    lv_timer_create(clock_cb, 1000, NULL);
}
