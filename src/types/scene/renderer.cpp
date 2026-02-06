#include "src/types/scene/renderer.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/tiles/tile_renderer_fonts.h"
#include "src/tiles/mdi_icons.h"
#include "src/network/ha_bridge_config.h"
#include <Arduino.h>
#include <SD.h>
#include <libs/tjpgd/tjpgd.h>

// --- Simple JPEG icon decoder ---

struct IconDecodeCtx {
  File* file;
  uint16_t* buf;
  uint16_t w;
  uint16_t h;
};

static size_t icon_jpeg_input(JDEC* jd, uint8_t* buff, size_t ndata) {
  IconDecodeCtx* ctx = static_cast<IconDecodeCtx*>(jd->device);
  if (!ctx || !ctx->file) return 0;
  if (buff) return ctx->file->read(buff, ndata);
  ctx->file->seek(ctx->file->position() + ndata);
  return ndata;
}

static int icon_jpeg_output(JDEC* jd, void* bitmap, JRECT* rect) {
  IconDecodeCtx* ctx = static_cast<IconDecodeCtx*>(jd->device);
  if (!ctx || !ctx->buf || !bitmap) return 0;
  const uint8_t* src = static_cast<const uint8_t*>(bitmap);
  const uint16_t rw = rect->right - rect->left + 1;
  for (uint16_t y = rect->top; y <= rect->bottom && y < ctx->h; ++y) {
    for (uint16_t x = rect->left; x <= rect->right && x < ctx->w; ++x) {
      size_t si = ((y - rect->top) * rw + (x - rect->left)) * 3;
      uint8_t b = src[si], g = src[si + 1], r = src[si + 2];
      uint16_t c = static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
      ctx->buf[y * ctx->w + x] = static_cast<uint16_t>((c >> 8) | (c << 8));
    }
  }
  return 1;
}

// Decode JPEG to a small RGB565_SWAPPED buffer (max 64px).
// Uses scale=0 decode (like image_popup) then simple nearest-neighbor downsample.
// Returns nullptr on failure.
static lv_image_dsc_t* decode_jpeg_icon(const String& path) {
  File f = SD.open(path, FILE_READ);
  if (!f) { Serial.printf("[Scene] ICON open fail: '%s'\n", path.c_str()); return nullptr; }

  uint8_t* work = static_cast<uint8_t*>(lv_malloc(4096));
  if (!work) { f.close(); return nullptr; }

  IconDecodeCtx ctx{};
  ctx.file = &f;
  JDEC jd;
  JRESULT rc = jd_prepare(&jd, icon_jpeg_input, work, 4096, &ctx);
  if (rc != JDR_OK) { Serial.println("[Scene] ICON prepare fail"); lv_free(work); f.close(); return nullptr; }

  // Decode at full resolution first
  ctx.w = jd.width;
  ctx.h = jd.height;
  size_t full_bytes = static_cast<size_t>(jd.width) * jd.height * 2;
  // Limit: don't decode huge images (max ~256x256 = 128KB)
  if (full_bytes > 131072) { Serial.println("[Scene] ICON too large"); lv_free(work); f.close(); return nullptr; }

  ctx.buf = static_cast<uint16_t*>(lv_malloc(full_bytes));
  if (!ctx.buf) { Serial.println("[Scene] ICON alloc fail"); lv_free(work); f.close(); return nullptr; }
  memset(ctx.buf, 0, full_bytes);

  rc = jd_decomp(&jd, icon_jpeg_output, 0);
  lv_free(work);
  f.close();
  if (rc != JDR_OK) { Serial.println("[Scene] ICON decomp fail"); lv_free(ctx.buf); return nullptr; }

  // Determine target size (max 64px on longest side, keep aspect)
  uint16_t dst_w, dst_h;
  if (jd.width >= jd.height) {
    dst_w = jd.width <= 64 ? jd.width : 64;
    dst_h = static_cast<uint16_t>((static_cast<uint32_t>(jd.height) * dst_w) / jd.width);
  } else {
    dst_h = jd.height <= 64 ? jd.height : 64;
    dst_w = static_cast<uint16_t>((static_cast<uint32_t>(jd.width) * dst_h) / jd.height);
  }
  if (dst_w == 0) dst_w = 1;
  if (dst_h == 0) dst_h = 1;

  // If already small enough, skip downscale
  uint16_t* final_buf;
  size_t final_bytes;
  if (dst_w == jd.width && dst_h == jd.height) {
    final_buf = ctx.buf;
    final_bytes = full_bytes;
  } else {
    // Nearest-neighbor downsample
    final_bytes = static_cast<size_t>(dst_w) * dst_h * 2;
    final_buf = static_cast<uint16_t*>(lv_malloc(final_bytes));
    if (!final_buf) { lv_free(ctx.buf); return nullptr; }
    for (uint16_t y = 0; y < dst_h; ++y) {
      uint16_t sy = static_cast<uint16_t>((static_cast<uint32_t>(y) * jd.height) / dst_h);
      for (uint16_t x = 0; x < dst_w; ++x) {
        uint16_t sx = static_cast<uint16_t>((static_cast<uint32_t>(x) * jd.width) / dst_w);
        final_buf[y * dst_w + x] = ctx.buf[sy * jd.width + sx];
      }
    }
    lv_free(ctx.buf);
  }

  lv_image_dsc_t* dsc = static_cast<lv_image_dsc_t*>(lv_malloc(sizeof(lv_image_dsc_t)));
  if (!dsc) { lv_free(final_buf); return nullptr; }
  memset(dsc, 0, sizeof(*dsc));
  dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
  dsc->header.cf = LV_COLOR_FORMAT_RGB565_SWAPPED;
  dsc->header.w = dst_w;
  dsc->header.h = dst_h;
  dsc->header.stride = dst_w * 2;
  dsc->data_size = final_bytes;
  dsc->data = reinterpret_cast<const uint8_t*>(final_buf);

  Serial.printf("[Scene] ICON OK %ux%u -> %ux%u\n", jd.width, jd.height, dst_w, dst_h);
  return dsc;
}

// Free icon dsc + pixel buffer on LVGL delete event
static void icon_dsc_delete_cb(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
  lv_image_dsc_t* dsc = static_cast<lv_image_dsc_t*>(lv_event_get_user_data(e));
  if (dsc) {
    if (dsc->data) lv_free(const_cast<uint8_t*>(dsc->data));
    lv_free(dsc);
  }
}

// --- Scene tile renderer ---

struct SceneEventData {
  String scene_alias;
  scene_publish_cb_t callback;
};

lv_obj_t* render_scene_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, scene_publish_cb_t scene_cb) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_style_radius(btn, 22, 0);
  lv_obj_set_style_border_width(btn, 0, 0);

  uint32_t btn_color = (tile.bg_color != 0) ? tile.bg_color : 0x353535;
  lv_obj_set_style_bg_color(btn, lv_color_hex(btn_color), LV_PART_MAIN | LV_STATE_DEFAULT);
  uint32_t pressed_color = btn_color + 0x101010;
  lv_obj_set_style_bg_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);

  set_tile_grid_cell(btn, col, row, tile.span_w, tile.span_h);

  // --- Try JPEG icon from image_path ---
  lv_obj_t* icon_img = nullptr;
  if (tile.image_path.length() > 0) {
    Serial.printf("[Scene] ICON '%s' path='%s'\n", tile.title.c_str(), tile.image_path.c_str());
    lv_image_dsc_t* dsc = decode_jpeg_icon(tile.image_path);
    if (!dsc) Serial.println("[Scene] ICON DECODE FAIL");
    if (dsc) {
      icon_img = lv_img_create(btn);
      lv_image_set_src(icon_img, dsc);
      lv_obj_set_style_radius(icon_img, 8, 0);
      lv_obj_set_style_clip_corner(icon_img, true, 0);
      // Free dsc+data when button is deleted
      lv_obj_add_event_cb(btn, icon_dsc_delete_cb, LV_EVENT_DELETE, dsc);
    }
  }

  // --- Fallback: MDI icon ---
  lv_obj_t* icon_lbl = nullptr;
  if (!icon_img) {
    String icon_name = tile.icon_name;
    bool icon_disabled = isMdiIconDisabled(icon_name);
    icon_name = normalizeMdiIconName(icon_name);
    if (!icon_disabled && !icon_name.length() && tile.scene_alias.length()) {
      String scene_entity = haBridgeConfig.findSceneEntity(tile.scene_alias);
      if (scene_entity.length()) {
        icon_name = normalizeMdiIconName(haBridgeConfig.findEntityIcon(scene_entity));
      }
    }
    String iconChar;
    if (icon_name.length() > 0 && FONT_MDI_ICONS != nullptr) {
      iconChar = getMdiChar(icon_name);
    }
    if (iconChar.length() > 0) {
      icon_lbl = lv_label_create(btn);
      if (icon_lbl) {
        set_label_style(icon_lbl, lv_color_white(), FONT_MDI_ICONS);
        lv_label_set_text(icon_lbl, iconChar.c_str());
      }
    }
  }

  bool has_icon = (icon_img != nullptr) || (icon_lbl != nullptr);
  bool has_title = tile.title.length() > 0;

  // Position icon
  if (icon_img) {
    if (has_title) {
      lv_obj_align(icon_img, LV_ALIGN_CENTER, 0, -20);
    } else {
      lv_obj_center(icon_img);
    }
  } else if (icon_lbl) {
    if (has_title) {
      lv_obj_align(icon_lbl, LV_ALIGN_CENTER, 0, -20);
    } else {
      lv_obj_center(icon_lbl);
    }
  }

  // Title
  if (has_title) {
    lv_obj_t* l = lv_label_create(btn);
    if (l) {
      set_label_style(l, lv_color_white(), FONT_TITLE);
      lv_label_set_text(l, tile.title.c_str());
      if (has_icon) {
        lv_obj_align(l, LV_ALIGN_CENTER, 0, 35);
      } else {
        lv_obj_center(l);
      }
    }
  }

  // Event-Handler für Scene-Aktivierung
  if (scene_cb && tile.scene_alias.length()) {
    SceneEventData* event_data = new SceneEventData{tile.scene_alias, scene_cb};

    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
          SceneEventData* data = static_cast<SceneEventData*>(lv_event_get_user_data(e));
          if (data && data->callback) {
            Serial.printf("[Tile] Szene aktiviert: %s\n", data->scene_alias.c_str());
            data->callback(data->scene_alias.c_str());
          }
        },
        LV_EVENT_CLICKED,
        event_data);
    lv_obj_add_event_cb(
        btn,
        [](lv_event_t* e) {
          if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
          SceneEventData* data = static_cast<SceneEventData*>(lv_event_get_user_data(e));
          delete data;
        },
        LV_EVENT_DELETE,
        event_data);
  }

  return btn;
}
