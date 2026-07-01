#include "src/types/pixelanim/renderer.h"

#include <Arduino.h>
#include <FS.h>
#include <esp_heap_caps.h>
#include <new>

#include "src/tiles/tile_renderer_shared.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/devices/device.h"
#if defined(DEVICE_WAVESHARE_TOUCH_LCD_8)
#include "src/devices/waveshare_touch_lcd_8/device_waveshare_touch_lcd_8.h"
#endif

// ---------------------------------------------------------------------------
// .panim binary format (little-endian header, then raw frames):
//
//   off  size  field
//   0    4     magic  'P','A','N','1'  (opaque RGB565) or 'P','A','N','2' (RGBA)
//   4    2     width        (uint16)
//   6    2     height       (uint16)
//   8    2     frame_count  (uint16)
//   10   2     frame_ms     (uint16)  per-frame duration in ms (fallback)
//   12   ...   frame_count * width*height * bpp bytes
//                PAN1: bpp=2, each pixel RGB565 big-endian (opaque)
//                PAN2: bpp=4, each pixel R,G,B,A bytes (A=0 -> transparent)
//
// The art is intentionally tiny. The renderer integer-upscales each frame with
// nearest-neighbour into an ARGB8888 PSRAM buffer (crisp pixels + per-pixel
// alpha so transparent areas show the tile/dashboard behind). The on-screen
// frame is a plain blit (no runtime scaling).
// ---------------------------------------------------------------------------

namespace {

constexpr char kAnimDir[] = "/animations";
constexpr uint16_t kMaxSide = 128;
constexpr uint16_t kMaxFrames = 64;
constexpr size_t kMaxUpscaledBytes = 1536u * 1024u;  // PSRAM budget per tile (ARGB)
constexpr uint16_t kHeaderBytes = 12;

struct PixelAnimState {
  lv_obj_t* img = nullptr;
  lv_timer_t* timer = nullptr;
  uint8_t* frames = nullptr;       // PSRAM: frame_count contiguous ARGB8888 frames
  lv_image_dsc_t* dscs = nullptr;  // one descriptor per frame (distinct src ptrs)
  uint16_t frame_count = 0;
  uint16_t cur_frame = 0;
};

void pixelanim_timer_cb(lv_timer_t* t) {
  PixelAnimState* st = static_cast<PixelAnimState*>(lv_timer_get_user_data(t));
  if (!st || !st->img || !st->dscs || st->frame_count < 2) return;
#if defined(DEVICE_WAVESHARE_TOUCH_LCD_8)
  // A media-tile update briefly forces every flush onto the slow CPU-rotate
  // path (see pausePpaFor in tile_renderer.cpp). Swapping our frame into that
  // window would visibly hitch and adds more work to an already-loaded
  // pipeline. Hold the current frame instead; it resumes smoothly once the
  // cooldown clears, which reads better than an uneven stutter.
  if (DeviceWaveshareTouchLCD8::ppaCooldownActive()) return;
#endif
  st->cur_frame = (st->cur_frame + 1) % st->frame_count;
  // Distinct descriptor per frame -> distinct image-cache key -> no stale frame,
  // using only public API.
  lv_image_set_src(st->img, &st->dscs[st->cur_frame]);
}

void pixelanim_delete_cb(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
  PixelAnimState* st = static_cast<PixelAnimState*>(lv_event_get_user_data(e));
  if (!st) return;
  if (st->timer) lv_timer_delete(st->timer);
  if (st->frames) heap_caps_free(st->frames);
  delete[] st->dscs;
  delete st;
}

uint16_t read_u16le(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

// Expand a 5/6-bit channel to 8-bit.
inline uint8_t exp5(uint8_t v) { return static_cast<uint8_t>((v << 3) | (v >> 2)); }
inline uint8_t exp6(uint8_t v) { return static_cast<uint8_t>((v << 2) | (v >> 4)); }

// Decode one source pixel (PAN1: 2 bytes RGB565 BE; PAN2: 4 bytes RGBA) into an
// LVGL ARGB8888 word (0xAARRGGBB, which is B,G,R,A in little-endian memory).
inline uint32_t decode_pixel(const uint8_t* p, bool rgba) {
  if (rgba) {
    return (static_cast<uint32_t>(p[3]) << 24) | (static_cast<uint32_t>(p[0]) << 16) |
           (static_cast<uint32_t>(p[1]) << 8) | static_cast<uint32_t>(p[2]);
  }
  uint16_t c = (static_cast<uint16_t>(p[0]) << 8) | p[1];
  uint8_t r = exp5((c >> 11) & 0x1F);
  uint8_t g = exp6((c >> 5) & 0x3F);
  uint8_t b = exp5(c & 0x1F);
  return 0xFF000000u | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
}

// Loads the file, upscales every frame into a fresh PSRAM ARGB8888 buffer and
// fills `st`. Returns false (nothing allocated) on any error.
bool load_panim(const String& file_name, uint16_t avail_w, uint16_t avail_h,
                PixelAnimState& st, uint16_t& out_frame_ms) {
  if (!Device::sdReady()) return false;

  String path = String(kAnimDir) + "/" + file_name;
  File f = Device::sdFS().open(path, FILE_READ);
  if (!f) {
    Serial.printf("[PixelAnim] open fail: '%s'\n", path.c_str());
    return false;
  }

  uint8_t header[kHeaderBytes];
  if (f.read(header, kHeaderBytes) != kHeaderBytes ||
      header[0] != 'P' || header[1] != 'A' || header[2] != 'N' ||
      (header[3] != '1' && header[3] != '2')) {
    Serial.println("[PixelAnim] bad header");
    f.close();
    return false;
  }
  const bool rgba = (header[3] == '2');
  const uint8_t src_bpp = rgba ? 4 : 2;

  const uint16_t w = read_u16le(header + 4);
  const uint16_t h = read_u16le(header + 6);
  const uint16_t frames = read_u16le(header + 8);
  uint16_t frame_ms = read_u16le(header + 10);

  if (w == 0 || h == 0 || w > kMaxSide || h > kMaxSide || frames == 0 || frames > kMaxFrames) {
    Serial.printf("[PixelAnim] bad dims %ux%u frames=%u\n", w, h, frames);
    f.close();
    return false;
  }
  if (frame_ms < 20) frame_ms = 120;
  if (frame_ms > 2000) frame_ms = 2000;

  const size_t native_frame_bytes = static_cast<size_t>(w) * h * src_bpp;
  if (f.size() < static_cast<size_t>(kHeaderBytes) + native_frame_bytes * frames) {
    Serial.println("[PixelAnim] file too short for frame count");
    f.close();
    return false;
  }

  uint16_t scale = 1;
  {
    uint16_t sx = w ? avail_w / w : 1;
    uint16_t sy = h ? avail_h / h : 1;
    uint16_t s = sx < sy ? sx : sy;
    if (s >= 1) scale = s;
  }
  while (scale > 1) {
    size_t total = static_cast<size_t>(w) * scale * h * scale * 4 * frames;
    if (total <= kMaxUpscaledBytes) break;
    --scale;
  }

  const uint16_t disp_w = static_cast<uint16_t>(w * scale);
  const uint16_t disp_h = static_cast<uint16_t>(h * scale);
  const size_t frame_px = static_cast<size_t>(disp_w) * disp_h;
  const size_t frame_bytes = frame_px * 4;  // ARGB8888
  const size_t total_bytes = frame_bytes * frames;

  uint8_t* buf = static_cast<uint8_t*>(heap_caps_malloc(total_bytes, MALLOC_CAP_SPIRAM));
  if (!buf) buf = static_cast<uint8_t*>(heap_caps_malloc(total_bytes, MALLOC_CAP_8BIT));
  if (!buf) {
    Serial.printf("[PixelAnim] alloc fail (%u bytes)\n", static_cast<unsigned>(total_bytes));
    f.close();
    return false;
  }

  // Per-row scratch for the native source row (<= 512 bytes).
  uint8_t* native_row = static_cast<uint8_t*>(malloc(static_cast<size_t>(w) * src_bpp));
  if (!native_row) {
    heap_caps_free(buf);
    f.close();
    return false;
  }

  bool ok = true;
  const int row_bytes = static_cast<int>(w) * src_bpp;
  uint32_t* buf32 = reinterpret_cast<uint32_t*>(buf);
  for (uint16_t fr = 0; fr < frames && ok; ++fr) {
    uint32_t* slot = buf32 + static_cast<size_t>(fr) * frame_px;
    for (uint16_t ny = 0; ny < h && ok; ++ny) {
      if (f.read(native_row, row_bytes) != row_bytes) { ok = false; break; }
      uint32_t* dst0 = slot + static_cast<size_t>(ny) * scale * disp_w;
      for (uint16_t nx = 0; nx < w; ++nx) {
        uint32_t argb = decode_pixel(native_row + static_cast<size_t>(nx) * src_bpp, rgba);
        uint32_t* p = dst0 + static_cast<size_t>(nx) * scale;
        for (uint16_t sx = 0; sx < scale; ++sx) p[sx] = argb;
      }
      for (uint16_t sy = 1; sy < scale; ++sy) {
        memcpy(slot + (static_cast<size_t>(ny) * scale + sy) * disp_w, dst0,
               static_cast<size_t>(disp_w) * 4);
      }
    }
  }

  free(native_row);
  f.close();
  if (!ok) {
    heap_caps_free(buf);
    Serial.println("[PixelAnim] frame read failed");
    return false;
  }

  lv_image_dsc_t* dscs = new (std::nothrow) lv_image_dsc_t[frames];
  if (!dscs) {
    heap_caps_free(buf);
    Serial.println("[PixelAnim] dsc alloc fail");
    return false;
  }
  for (uint16_t i = 0; i < frames; ++i) {
    lv_image_dsc_t& d = dscs[i];
    memset(&d, 0, sizeof(d));
    d.header.magic = LV_IMAGE_HEADER_MAGIC;
    d.header.cf = LV_COLOR_FORMAT_ARGB8888;
    d.header.w = disp_w;
    d.header.h = disp_h;
    d.header.stride = disp_w * 4;
    d.data_size = frame_bytes;
    d.data = buf + static_cast<size_t>(i) * frame_bytes;
  }

  st.frames = buf;
  st.dscs = dscs;
  st.frame_count = frames;
  st.cur_frame = 0;

  out_frame_ms = frame_ms;
  Serial.printf("[PixelAnim] '%s' %ux%u x%u %s -> %ux%u (%u bytes)\n",
                file_name.c_str(), w, h, frames, rgba ? "rgba" : "rgb565", disp_w, disp_h,
                static_cast<unsigned>(total_bytes));
  return true;
}

}  // namespace

lv_obj_t* render_pixelanim_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index) {
  (void)index;
  if (!parent) return nullptr;

  lv_obj_t* card = lv_obj_create(parent);
  if (!card) return nullptr;
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_shadow_width(card, 0, 0);
  lv_obj_set_style_pad_all(card, 0, 0);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  // bg_color 0 -> fully transparent: the tile looks like empty space (no visible
  // card), and a transparent sprite shows the dashboard behind it. A real colour
  // fills a normal rounded card that the sprite's alpha blends over.
  if (tile.bg_color == 0) {
    lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(card, 0, 0);
  } else {
    lv_obj_set_style_bg_color(card, lv_color_hex(tile.bg_color), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 22, 0);
    lv_obj_set_style_clip_corner(card, true, 0);
  }
  set_tile_grid_cell(card, col, row, tile.span_w, tile.span_h);

  const bool has_title = tile.title.length() > 0;
  if (has_title) {
    lv_obj_t* title_lbl = lv_label_create(card);
    if (title_lbl) {
      set_label_style(title_lbl, lv_color_white(), FONT_TITLE);
      lv_label_set_text(title_lbl, tile.title.c_str());
      lv_obj_align(title_lbl, LV_ALIGN_TOP_LEFT, 14, 12);
    }
  }

  String file_name = tile.scene_alias;
  file_name.trim();
  if (!file_name.length()) {
    return card;  // no animation chosen yet
  }

  uint16_t avail_w = static_cast<uint16_t>(GRID_CELL_W * tile.span_w + GRID_GAP * (tile.span_w - 1));
  uint16_t avail_h = static_cast<uint16_t>(GRID_CELL_H * tile.span_h + GRID_GAP * (tile.span_h - 1));
  if (avail_w > 20) avail_w -= 16;
  if (avail_h > 20) avail_h -= (has_title ? 40 : 16);

  PixelAnimState* st = new PixelAnimState();
  uint16_t frame_ms = 120;
  if (!load_panim(file_name, avail_w, avail_h, *st, frame_ms)) {
    delete st;
    return card;
  }

  // Speed override: image_slideshow_sec doubles as frames-per-second for this
  // tile type (set by the web speed slider). 0 -> use the file's own timing.
  const uint16_t fps = tile.image_slideshow_sec;
  if (fps >= 1 && fps <= 60) frame_ms = static_cast<uint16_t>(1000 / fps);

  lv_obj_t* img = lv_img_create(card);
  if (!img) {
    heap_caps_free(st->frames);
    delete[] st->dscs;
    delete st;
    return card;
  }
  lv_image_set_src(img, &st->dscs[0]);
  lv_obj_set_size(img, st->dscs[0].header.w, st->dscs[0].header.h);
  lv_obj_align(img, LV_ALIGN_CENTER, 0, has_title ? 12 : 0);
  lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);

  st->img = img;
  st->timer = (st->frame_count > 1) ? lv_timer_create(pixelanim_timer_cb, frame_ms, st) : nullptr;

  lv_obj_add_event_cb(card, pixelanim_delete_cb, LV_EVENT_DELETE, st);
  return card;
}
