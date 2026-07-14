#include "src/ui/image_screensaver.h"

#include <Arduino.h>
#include <FS.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <libs/tjpgd/tjpgd.h>
#if defined(CONFIG_IDF_TARGET_ESP32P4)
#include <driver/jpeg_decode.h>
#include <soc/soc_caps.h>
#endif
#include <string.h>
#include <time.h>

// lvgl.h exportiert lv_image_cache_drop() in 9.5 nicht mehr, die Deklaration
// liegt nur noch im Instanz-Header (siehe tile_renderer.cpp).
#include <misc/cache/instance/lv_image_cache.h>

#include "src/core/dma2d_arbiter.h"
#include "src/devices/device.h"
#include "src/fonts/ui_fonts.h"
#include "src/tiles/tile_renderer_shared.h"
#include "src/types/clock/clock_format.h"
#include "src/ui/popup_layout.h"

namespace {

constexpr char kWallpaperDir[] = "/wallpapers";
constexpr size_t kMaxFileBytes = 8U * 1024U * 1024U;

// Obergrenze fuer den voll dekodierten Zwischenpuffer: 2048x2048 RGB565 sind
// 8 MB PSRAM. Groessere Bilder laufen ueber den TJpgDec-Pfad, der per
// 1/2..1/8-Skalierung unter diese Grenze kommt.
constexpr uint32_t kMaxDecodePixels = 2048U * 2048U;

// Nach den Decode-Puffern muss genug PSRAM fuer den Rest der UI uebrig
// bleiben (Cover-Worker, Popups, LVGL-Zwischenpuffer).
constexpr size_t kPsramReserveBytes = 4U * 1024U * 1024U;
constexpr size_t kPpaBufferAlignment = 64;
constexpr uint16_t kCardRadius = 22;

struct ScreensaverState {
  lv_obj_t* overlay = nullptr;
  lv_obj_t* time_label = nullptr;
  lv_obj_t* date_label = nullptr;
  lv_timer_t* timer = nullptr;
  uint8_t time_format = clock_tile::TIME_FORMAT_24H;
  uint8_t date_format = clock_tile::DATE_FORMAT_DMY;
};

ScreensaverState* g_state = nullptr;

// Ein-Slot-Cache: das dekodierte Wallpaper bleibt in PSRAM (~2 MB), damit
// das Oeffnen nach dem ersten Mal sofort geht. Ersetzt wird der Slot erst,
// wenn ein anderes Bild (oder eine andere Zielgroesse) gebraucht wird.
// Achtung: gleicher Dateiname mit neuem Inhalt wird bis zum Neustart nicht
// erkannt — der Cache ist nur ueber den Namen gekeyt.
String g_cache_name;
uint16_t g_cache_w = 0;
uint16_t g_cache_h = 0;
lv_image_dsc_t* g_cache_dsc = nullptr;

// Preload: ein paar Sekunden nach dem Kachel-Aufbau wird das Bild einmal
// vordekodiert, damit schon der erste Tap ohne SD-/Decode-Wartezeit oeffnet.
String g_preload_name;
lv_timer_t* g_preload_timer = nullptr;

void* alloc_prefer_psram(size_t bytes) {
  if (!bytes) return nullptr;
  void* p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!p) p = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
  return p;
}

void* alloc_aligned_prefer_psram(size_t bytes) {
  if (!bytes) return nullptr;
  void* p = heap_caps_aligned_alloc(
      kPpaBufferAlignment, bytes,
      MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  if (!p) {
    p = heap_caps_aligned_alloc(
        kPpaBufferAlignment, bytes,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  }
  // Die Vorschau ist optional. Scheitert nur ihre strengere Ausrichtung,
  // muss der bewaehrte LVGL-Pfad trotzdem weiterhin ein Bild bekommen.
  if (!p) p = alloc_prefer_psram(bytes);
  return p;
}

bool psram_budget_ok(size_t needed) {
  const size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
  const size_t free_total = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  return largest >= needed && free_total >= needed + kPsramReserveBytes;
}

void free_screensaver_dsc(lv_image_dsc_t*& dsc) {
  if (!dsc) return;
  // LVGL cached Dekodier-Ergebnisse per Pointer-Adresse; ohne Drop kann ein
  // spaeterer malloc auf derselben Adresse das alte Bild treffen (gleiche
  // Falle wie beim Media-Cover, siehe free_media_cover_dsc).
  lv_image_cache_drop(dsc);
  if (dsc->data) {
    free(const_cast<uint8_t*>(dsc->data));
  }
  free(dsc);
  dsc = nullptr;
}

bool is_jpeg(const uint8_t* data, size_t len) {
  return data && len >= 3 && data[0] == 0xFF && data[1] == 0xD8;
}

// --- Datei von SD in einen PSRAM-Puffer lesen (chunked) ---
uint8_t* read_wallpaper_file(const String& file_name, size_t& out_len) {
  out_len = 0;
  if (!Device::sdReady()) {
    Serial.println("[Screensaver] microSD nicht bereit");
    return nullptr;
  }
  const String path = String(kWallpaperDir) + "/" + file_name;
  fs::File f = Device::sdFS().open(path, FILE_READ);
  if (!f) {
    Serial.printf("[Screensaver] open fail: '%s'\n", path.c_str());
    return nullptr;
  }
  const size_t len = f.size();
  if (len < 32 || len > kMaxFileBytes) {
    Serial.printf("[Screensaver] Dateigroesse ungueltig: %u Bytes\n",
                  static_cast<unsigned>(len));
    f.close();
    return nullptr;
  }
  uint8_t* buf = static_cast<uint8_t*>(alloc_prefer_psram(len));
  if (!buf) {
    Serial.println("[Screensaver] Dateipuffer-Alloc fehlgeschlagen");
    f.close();
    return nullptr;
  }
  size_t pos = 0;
  while (pos < len) {
    size_t chunk = len - pos;
    if (chunk > 64U * 1024U) chunk = 64U * 1024U;
    const size_t got = f.read(buf + pos, chunk);
    if (got == 0) break;
    pos += got;
  }
  f.close();
  if (pos != len) {
    Serial.printf("[Screensaver] Kurz gelesen: %u/%u Bytes\n",
                  static_cast<unsigned>(pos), static_cast<unsigned>(len));
    free(buf);
    return nullptr;
  }
  out_len = len;
  return buf;
}

#if defined(CONFIG_IDF_TARGET_ESP32P4) && SOC_JPEG_DECODE_SUPPORTED
// HW-Decode wie beim Media-Cover (tile_renderer.cpp): 16-aligned Dimensionen,
// RGB-Order = big-endian RGB565 = LV_COLOR_FORMAT_RGB565_SWAPPED. Nicht
// passende Bilder fallen auf TJpgDec zurueck. Rueckgabe: malloc-Puffer mit
// out_w*out_h Pixeln (Aufrufer free()t).
uint16_t* hw_decode_jpeg(const uint8_t* data, size_t len,
                         uint16_t& out_w, uint16_t& out_h) {
  if (len > UINT32_MAX) return nullptr;

  jpeg_decode_picture_info_t info{};
  esp_err_t err = jpeg_decoder_get_info(data, static_cast<uint32_t>(len), &info);
  if (err != ESP_OK || info.width == 0 || info.height == 0) return nullptr;
  if ((info.width & 15U) != 0 || (info.height & 15U) != 0) return nullptr;

  const uint32_t pixels = static_cast<uint32_t>(info.width) * info.height;
  if (pixels > kMaxDecodePixels) return nullptr;

  const size_t requested_bytes = static_cast<size_t>(pixels) * sizeof(uint16_t);
  if (!psram_budget_ok(requested_bytes)) {
    Serial.printf("[Screensaver] PSRAM-Budget zu klein fuer %u Bytes\n",
                  static_cast<unsigned>(requested_bytes));
    return nullptr;
  }

  jpeg_decode_memory_alloc_cfg_t mem_cfg{};
  mem_cfg.buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER;
  size_t allocated_bytes = 0;
  uint16_t* decoded = static_cast<uint16_t*>(
      jpeg_alloc_decoder_mem(requested_bytes, &mem_cfg, &allocated_bytes));
  if (!decoded || allocated_bytes < requested_bytes) {
    free(decoded);
    Serial.printf("[Screensaver] HW JPEG PSRAM-Puffer fehlt: %u Bytes\n",
                  static_cast<unsigned>(requested_bytes));
    return nullptr;
  }

  // Persistente Engine wie beim Media-Cover: Engine-Auf-/Abbau pro Bild
  // acquired/released den 2D-DMA-Pool, den sich der JPEG-Block mit der PPA
  // teilt — dieses Churn stand auf dem Tab5 im Verdacht, Transaktionen zu
  // verlieren ("PPA VERKLEMMT").
  static jpeg_decoder_handle_t s_engine = nullptr;
  if (!s_engine) {
    jpeg_decode_engine_cfg_t engine_cfg{};
    engine_cfg.intr_priority = 0;
    // Vollbilder brauchen laenger als ein 240er-Cover.
    engine_cfg.timeout_ms = 500;
    err = jpeg_new_decoder_engine(&engine_cfg, &s_engine);
    if (err != ESP_OK || !s_engine) {
      s_engine = nullptr;
      free(decoded);
      Serial.printf("[Screensaver] HW JPEG Engine nicht verfuegbar: %s\n",
                    esp_err_to_name(err));
      return nullptr;
    }
  }

  jpeg_decode_cfg_t decode_cfg{};
  decode_cfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB565;
  decode_cfg.rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_RGB;
  decode_cfg.conv_std = JPEG_YUV_RGB_CONV_STD_BT601;
  uint32_t decoded_bytes = 0;
  {
    // Nie parallel zu einer laufenden PPA-Rotation dekodieren (geteilter
    // 2D-DMA-Pool, siehe dma2d_arbiter.h).
    Dma2dArbiterGuard dma2d_guard(2000);
    if (!dma2d_guard.locked()) {
      Serial.println("[Screensaver] 2D-DMA-Arbiter Timeout, decode ungeschuetzt");
    }
    err = jpeg_decoder_process(s_engine, &decode_cfg, data,
                               static_cast<uint32_t>(len),
                               reinterpret_cast<uint8_t*>(decoded),
                               static_cast<uint32_t>(allocated_bytes),
                               &decoded_bytes);
  }
  if (err != ESP_OK || decoded_bytes < requested_bytes) {
    free(decoded);
    Serial.printf("[Screensaver] HW JPEG decode fehlgeschlagen: %s bytes=%u/%u\n",
                  esp_err_to_name(err),
                  static_cast<unsigned>(decoded_bytes),
                  static_cast<unsigned>(requested_bytes));
    // Wie beim Media-Cover: Engine nach Fehlschlag verwerfen statt mit
    // moeglicherweise haengendem Zustand weiterzuarbeiten.
    jpeg_del_decoder_engine(s_engine);
    s_engine = nullptr;
    return nullptr;
  }

  out_w = static_cast<uint16_t>(info.width);
  out_h = static_cast<uint16_t>(info.height);
  return decoded;
}
#endif

// --- TJpgDec-Fallback (nicht-16-aligned oder sehr grosse Bilder) ---
struct SwJpegCtx {
  const uint8_t* data = nullptr;
  size_t len = 0;
  size_t pos = 0;
  uint16_t* pixels = nullptr;
  uint16_t w = 0;
  uint16_t h = 0;
};

size_t sw_jpeg_input(JDEC* jd, uint8_t* buff, size_t ndata) {
  SwJpegCtx* ctx = static_cast<SwJpegCtx*>(jd->device);
  if (!ctx || !ctx->data || ctx->pos >= ctx->len) return 0;
  size_t remain = ctx->len - ctx->pos;
  size_t take = (ndata < remain) ? ndata : remain;
  if (buff && take) {
    memcpy(buff, ctx->data + ctx->pos, take);
  }
  ctx->pos += take;
  return take;
}

int sw_jpeg_output(JDEC* jd, void* bitmap, JRECT* rect) {
  SwJpegCtx* ctx = static_cast<SwJpegCtx*>(jd->device);
  if (!ctx || !ctx->pixels || !bitmap) return 0;

  // Gleiche Byte-Reihenfolge wie beim Media-Cover: Ergebnis ist
  // LV_COLOR_FORMAT_RGB565_SWAPPED.
  const uint8_t* src = static_cast<const uint8_t*>(bitmap);
  const uint16_t rw = rect->right - rect->left + 1;
  for (uint16_t y = rect->top; y <= rect->bottom && y < ctx->h; ++y) {
    for (uint16_t x = rect->left; x <= rect->right && x < ctx->w; ++x) {
      const size_t si = ((y - rect->top) * rw + (x - rect->left)) * 3;
      const uint8_t b = src[si];
      const uint8_t g = src[si + 1];
      const uint8_t r = src[si + 2];
      const uint16_t c = static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
      ctx->pixels[static_cast<size_t>(y) * ctx->w + x] =
          static_cast<uint16_t>((c >> 8) | (c << 8));
    }
  }
  return 1;
}

uint16_t* sw_decode_jpeg(const uint8_t* data, size_t len,
                         uint16_t& out_w, uint16_t& out_h) {
  uint8_t* work = static_cast<uint8_t*>(heap_caps_malloc(4096, MALLOC_CAP_8BIT));
  if (!work) return nullptr;

  SwJpegCtx ctx{};
  ctx.data = data;
  ctx.len = len;

  JDEC jd;
  JRESULT rc = jd_prepare(&jd, sw_jpeg_input, work, 4096, &ctx);
  if (rc != JDR_OK || jd.width == 0 || jd.height == 0 ||
      jd.width > UINT16_MAX || jd.height > UINT16_MAX) {
    Serial.printf("[Screensaver] SW JPEG prepare fehlgeschlagen: %d\n",
                  static_cast<int>(rc));
    free(work);
    return nullptr;
  }

  // TJpgDec kann beim Dekodieren 1/2, 1/4, 1/8 skalieren — genug, um auch
  // grosse Bilder unter das Pixel-Budget zu bringen.
  uint8_t scale = 0;
  while (scale < 3 &&
         (static_cast<uint32_t>(jd.width >> scale) *
          static_cast<uint32_t>(jd.height >> scale)) > kMaxDecodePixels) {
    ++scale;
  }
  const uint16_t w = static_cast<uint16_t>(jd.width >> scale);
  const uint16_t h = static_cast<uint16_t>(jd.height >> scale);
  const uint32_t pixels = static_cast<uint32_t>(w) * h;
  if (w == 0 || h == 0 || pixels > kMaxDecodePixels) {
    Serial.printf("[Screensaver] JPEG zu gross: %ux%u\n",
                  static_cast<unsigned>(jd.width),
                  static_cast<unsigned>(jd.height));
    free(work);
    return nullptr;
  }

  const size_t bytes = static_cast<size_t>(pixels) * sizeof(uint16_t);
  if (!psram_budget_ok(bytes)) {
    Serial.printf("[Screensaver] PSRAM-Budget zu klein fuer %u Bytes\n",
                  static_cast<unsigned>(bytes));
    free(work);
    return nullptr;
  }
  uint16_t* out = static_cast<uint16_t*>(alloc_prefer_psram(bytes));
  if (!out) {
    free(work);
    return nullptr;
  }
  memset(out, 0, bytes);

  ctx.pixels = out;
  ctx.w = w;
  ctx.h = h;
  rc = jd_decomp(&jd, sw_jpeg_output, scale);
  free(work);
  if (rc != JDR_OK) {
    Serial.printf("[Screensaver] SW JPEG decode fehlgeschlagen: %d\n",
                  static_cast<int>(rc));
    free(out);
    return nullptr;
  }

  out_w = w;
  out_h = h;
  return out;
}

// Anteil eines Pixels innerhalb einer abgerundeten Ecke, mit 4x4
// Supersampling. 16 = voll sichtbar, 0 = komplett Rahmenfarbe.
uint8_t rounded_pixel_coverage(uint16_t x, uint16_t y,
                               uint16_t w, uint16_t h,
                               uint16_t radius) {
  if (radius == 0 || w < radius * 2 || h < radius * 2) return 16;
  const bool at_left = x < radius;
  const bool at_right = x >= w - radius;
  const bool at_top = y < radius;
  const bool at_bottom = y >= h - radius;
  if ((!at_left && !at_right) || (!at_top && !at_bottom)) return 16;

  const uint16_t edge_x = at_left ? x : static_cast<uint16_t>(w - 1 - x);
  const uint16_t edge_y = at_top ? y : static_cast<uint16_t>(h - 1 - y);
  constexpr int32_t kSamples = 4;
  constexpr int32_t kUnitsPerPixel = kSamples * 2;
  const int32_t center = static_cast<int32_t>(radius) * kUnitsPerPixel;
  const int32_t radius_sq = center * center;
  uint8_t covered = 0;
  for (int32_t sample_y = 0; sample_y < kSamples; ++sample_y) {
    const int32_t py = static_cast<int32_t>(edge_y) * kUnitsPerPixel +
                       sample_y * 2 + 1;
    const int32_t dy = center - py;
    for (int32_t sample_x = 0; sample_x < kSamples; ++sample_x) {
      const int32_t px = static_cast<int32_t>(edge_x) * kUnitsPerPixel +
                         sample_x * 2 + 1;
      const int32_t dx = center - px;
      if (dx * dx + dy * dy <= radius_sq) ++covered;
    }
  }
  return covered;
}

uint16_t blend_swapped_rgb565_with_black(uint16_t swapped, uint8_t coverage) {
  if (coverage == 0) return 0;
  if (coverage >= 16) return swapped;
  const uint16_t color =
      static_cast<uint16_t>((swapped >> 8) | (swapped << 8));
  const uint16_t red =
      static_cast<uint16_t>((((color >> 11) & 0x1F) * coverage + 8) / 16);
  const uint16_t green =
      static_cast<uint16_t>((((color >> 5) & 0x3F) * coverage + 8) / 16);
  const uint16_t blue =
      static_cast<uint16_t>(((color & 0x1F) * coverage + 8) / 16);
  const uint16_t blended =
      static_cast<uint16_t>((red << 11) | (green << 5) | blue);
  return static_cast<uint16_t>((blended >> 8) | (blended << 8));
}

// Fertiges Bildschirmbild erzeugen: schwarzer 4-px-Rahmen, bereits
// gerundete Bildecken und innen ein zentrierter Center-Crop. PPA und LVGL
// verwenden exakt denselben Puffer, sodass beim Oeffnen nichts nachtraeglich
// geclippt oder in mehreren Streifen gezeichnet werden muss.
lv_image_dsc_t* make_cover_dsc(const uint16_t* src, uint16_t src_w,
                               uint16_t src_h, uint16_t target_w,
                               uint16_t target_h) {
  if (!src || src_w == 0 || src_h == 0 || target_w == 0 || target_h == 0) {
    return nullptr;
  }
  constexpr uint16_t margin = popup_layout::kCardMargin;
  if (target_w <= margin * 2 || target_h <= margin * 2) return nullptr;
  const uint16_t image_w = static_cast<uint16_t>(target_w - margin * 2);
  const uint16_t image_h = static_cast<uint16_t>(target_h - margin * 2);

  uint32_t crop_w = src_w;
  uint32_t crop_h =
      static_cast<uint32_t>((static_cast<uint64_t>(crop_w) * image_h) / image_w);
  if (crop_h > src_h || crop_h == 0) {
    crop_h = src_h;
    crop_w =
        static_cast<uint32_t>((static_cast<uint64_t>(crop_h) * image_w) / image_h);
    if (crop_w > src_w) crop_w = src_w;
  }
  if (crop_w == 0 || crop_h == 0) return nullptr;
  const uint32_t x0 = (src_w - crop_w) / 2;
  const uint32_t y0 = (src_h - crop_h) / 2;

  const size_t bytes = static_cast<size_t>(target_w) * target_h * sizeof(uint16_t);
  // Der fertige Cache darf auf dem 8-Zoll-Geraet direkt als PPA-Eingabe
  // dienen. 64-Byte-Ausrichtung erfuellt zugleich die Cache-/DMA-Anforderung;
  // fuer LVGL und die anderen Targets aendert sich das Datenformat nicht.
  uint16_t* out = static_cast<uint16_t*>(alloc_aligned_prefer_psram(bytes));
  if (!out) return nullptr;
  memset(out, 0, bytes);

  for (uint16_t y = 0; y < image_h; ++y) {
    const uint32_t sy = y0 + (static_cast<uint32_t>(y) * crop_h) / image_h;
    const uint16_t* src_row = src + static_cast<size_t>(sy) * src_w;
    uint16_t* dst_row =
        out + static_cast<size_t>(y + margin) * target_w + margin;
    for (uint16_t x = 0; x < image_w; ++x) {
      const uint32_t sx = x0 + (static_cast<uint32_t>(x) * crop_w) / image_w;
      const uint8_t coverage =
          rounded_pixel_coverage(x, y, image_w, image_h, kCardRadius);
      dst_row[x] = blend_swapped_rgb565_with_black(src_row[sx], coverage);
    }
  }

  lv_image_dsc_t* dsc = static_cast<lv_image_dsc_t*>(malloc(sizeof(lv_image_dsc_t)));
  if (!dsc) {
    free(out);
    return nullptr;
  }
  memset(dsc, 0, sizeof(*dsc));
  dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
  dsc->header.cf = LV_COLOR_FORMAT_RGB565_SWAPPED;
  dsc->header.w = target_w;
  dsc->header.h = target_h;
  dsc->header.stride = target_w * 2;
  dsc->data_size = bytes;
  dsc->data = reinterpret_cast<const uint8_t*>(out);
  return dsc;
}

lv_image_dsc_t* decode_wallpaper_to_size(const String& file_name,
                                         uint16_t target_w, uint16_t target_h) {
  size_t len = 0;
  uint8_t* file = read_wallpaper_file(file_name, len);
  if (!file) return nullptr;
  if (!is_jpeg(file, len)) {
    Serial.println("[Screensaver] Datei ist kein JPEG");
    free(file);
    return nullptr;
  }

  const uint32_t started_ms = millis();
  uint16_t w = 0;
  uint16_t h = 0;
  uint16_t* pixels = nullptr;
  bool hw = false;
#if defined(CONFIG_IDF_TARGET_ESP32P4) && SOC_JPEG_DECODE_SUPPORTED
  pixels = hw_decode_jpeg(file, len, w, h);
  hw = pixels != nullptr;
#endif
  if (!pixels) {
    pixels = sw_decode_jpeg(file, len, w, h);
  }
  free(file);
  if (!pixels) return nullptr;

  lv_image_dsc_t* dsc = make_cover_dsc(pixels, w, h, target_w, target_h);
  free(pixels);
  if (dsc) {
    Serial.printf("[Screensaver] %s-Decode %ux%u -> %ux%u in %u ms\n",
                  hw ? "HW" : "SW",
                  static_cast<unsigned>(w), static_cast<unsigned>(h),
                  static_cast<unsigned>(target_w), static_cast<unsigned>(target_h),
                  static_cast<unsigned>(millis() - started_ms));
  }
  return dsc;
}

void card_dims(uint16_t& w, uint16_t& h) {
  w = static_cast<uint16_t>(SCREEN_WIDTH - 2 * popup_layout::kCardMargin);
  h = static_cast<uint16_t>(SCREEN_HEIGHT - 2 * popup_layout::kCardMargin);
}

lv_image_dsc_t* get_or_decode_cached(const String& name, uint16_t w, uint16_t h) {
  if (g_cache_dsc && g_cache_name == name && g_cache_w == w && g_cache_h == h) {
    return g_cache_dsc;
  }
  lv_image_dsc_t* dsc = decode_wallpaper_to_size(name, w, h);
  if (!dsc) return nullptr;
  // Alten Slot erst nach erfolgreichem Decode ersetzen; Aufrufer stellen
  // sicher, dass kein Overlay den alten Puffer mehr anzeigt.
  free_screensaver_dsc(g_cache_dsc);
  g_cache_dsc = dsc;
  g_cache_name = name;
  g_cache_w = w;
  g_cache_h = h;
  return dsc;
}

void preload_timer_cb(lv_timer_t*) {
  // repeat_count=1: LVGL loescht den Timer nach diesem Aufruf selbst.
  g_preload_timer = nullptr;
  // Offenes Overlay zeigt evtl. gerade den Cache-Puffer — nicht ersetzen.
  if (g_state || !g_preload_name.length()) return;
  get_or_decode_cached(g_preload_name,
                       static_cast<uint16_t>(SCREEN_WIDTH),
                       static_cast<uint16_t>(SCREEN_HEIGHT));
}

// --- Uhr/Datum (gleiche Formate wie das Clock-Tile) ---
void update_labels(ScreensaverState* st) {
  if (!st) return;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) return;
  if (st->time_label) {
    char buf[16];
    if (st->time_format == clock_tile::TIME_FORMAT_12H) {
      int hour12 = timeinfo.tm_hour % 12;
      if (hour12 == 0) hour12 = 12;
      snprintf(buf, sizeof(buf), "%d:%02d %s", hour12, timeinfo.tm_min,
               timeinfo.tm_hour < 12 ? "AM" : "PM");
    } else {
      snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    }
    lv_label_set_text(st->time_label, buf);
  }
  if (st->date_label) {
    char buf[16];
    switch (st->date_format) {
      case clock_tile::DATE_FORMAT_MDY:
        snprintf(buf, sizeof(buf), "%02d/%02d/%04d",
                 timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_year + 1900);
        break;
      case clock_tile::DATE_FORMAT_YMD:
        snprintf(buf, sizeof(buf), "%04d/%02d/%02d",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
        break;
      default:
        snprintf(buf, sizeof(buf), "%02d.%02d.%04d",
                 timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
        break;
    }
    lv_label_set_text(st->date_label, buf);
  }
}

void screensaver_timer_cb(lv_timer_t* timer) {
  update_labels(static_cast<ScreensaverState*>(lv_timer_get_user_data(timer)));
}

void on_screensaver_clicked(lv_event_t*) {
  hide_image_screensaver();
}

// Sicherheitsnetz fuer den Fall, dass das Overlay von aussen geloescht wird
// (nicht ueber hide_image_screensaver): dann den State hier abbauen. Der
// Bildpuffer gehoert dem Cache und bleibt fuer das naechste Oeffnen stehen.
void on_overlay_delete(lv_event_t* e) {
  ScreensaverState* st = static_cast<ScreensaverState*>(lv_event_get_user_data(e));
  if (!st || g_state != st) return;
  g_state = nullptr;
  if (st->timer) {
    lv_timer_delete(st->timer);
    st->timer = nullptr;
  }
  delete st;
}

}  // namespace

void preload_image_screensaver(const String& file_name) {
  if (!file_name.length()) return;
  g_preload_name = file_name;
  if (g_preload_timer) {
    lv_timer_delete(g_preload_timer);
    g_preload_timer = nullptr;
  }
  // Nicht sofort dekodieren: der Kachel-Aufbau (Boot, Ordnerwechsel) soll
  // nicht um die Decode-Zeit ruckeln. Ein paar Sekunden spaeter ist Ruhe.
  g_preload_timer = lv_timer_create(preload_timer_cb, 4000, nullptr);
  if (g_preload_timer) lv_timer_set_repeat_count(g_preload_timer, 1);
}

void show_image_screensaver(const ImageScreensaverInit& init) {
  if (!init.file_name.length()) return;
  hide_image_screensaver();

  // Bildschirmfuellend wie die Settings-Ansicht, nur mit dem ueblichen
  // Popup-Rand und Eckenradius — nicht das quadratische Popup-Format.
  uint16_t card_w = 0;
  uint16_t card_h = 0;
  card_dims(card_w, card_h);
  const uint16_t image_w = static_cast<uint16_t>(SCREEN_WIDTH);
  const uint16_t image_h = static_cast<uint16_t>(SCREEN_HEIGHT);

  const bool cache_hit = g_cache_dsc && g_cache_name == init.file_name &&
                         g_cache_w == image_w && g_cache_h == image_h;
  lv_image_dsc_t* dsc =
      get_or_decode_cached(init.file_name, image_w, image_h);
  if (!dsc) return;

  ScreensaverState* st = new ScreensaverState();
  st->time_format = init.time_format;
  st->date_format = init.date_format;

  lv_obj_t* overlay = lv_obj_create(lv_layer_top());
  st->overlay = overlay;
  lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(overlay, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(overlay, 0, 0);
  lv_obj_set_style_pad_all(overlay, 0, 0);
  lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(overlay, on_screensaver_clicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(overlay, on_overlay_delete, LV_EVENT_DELETE, st);

  // Der Cache ist bereits das fertige Bildschirmbild inklusive schwarzem
  // Rahmen und gerundeten Ecken. Deshalb hier kein zusaetzliches LVGL-Clipping.
  lv_obj_t* img = lv_image_create(overlay);
  lv_image_set_src(img, dsc);
  lv_obj_center(img);
  lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);

  // Transparente Interaktions-/Layoutflaeche ueber dem vorbereiteten Bild.
  lv_obj_t* card = lv_obj_create(overlay);
  lv_obj_set_size(card, card_w, card_h);
  lv_obj_center(card);
  lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_pad_all(card, 0, 0);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(card, on_screensaver_clicked, LV_EVENT_CLICKED, nullptr);

  if (init.show_time || init.show_date) {
    lv_obj_t* pill = lv_obj_create(card);
    lv_obj_remove_style_all(pill);
    lv_obj_set_size(pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(pill, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_40, 0);
    lv_obj_set_style_radius(pill, 24, 0);
    lv_obj_set_style_pad_hor(pill, 32, 0);
    lv_obj_set_style_pad_ver(pill, 20, 0);
    lv_obj_set_style_pad_gap(pill, 8, 0);
    lv_obj_set_flex_flow(pill, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_center(pill);
    lv_obj_remove_flag(pill, LV_OBJ_FLAG_CLICKABLE);

    if (init.show_time) {
      st->time_label = lv_label_create(pill);
      set_label_style(st->time_label, lv_color_white(), &ui_font_48);
      lv_label_set_text(st->time_label, "");
    }
    if (init.show_date) {
      st->date_label = lv_label_create(pill);
      set_label_style(st->date_label, lv_color_hex(0xE0E0E0), &ui_font_28);
      lv_label_set_text(st->date_label, "");
    }
  }

  update_labels(st);
  st->timer = lv_timer_create(screensaver_timer_cb, 1000, st);
  g_state = st;

  // Das fertige Vollbild enthaelt Rahmen und Rundungen bereits. Deshalb reicht
  // auf Geraeten mit direktem Framebuffer genau ein beschleunigter Lauf ohne
  // Teilstreifen. Welcher Hardwarepfad das ist, entscheidet das aktive Geraet;
  // der Screensaver kennt weder PPA noch einen konkreten Displaytreiber.
  if (cache_hit) {
    const uint32_t preview_started_ms = millis();
    const bool preview_ok = Device::displayTryFullFramePreview(
        0, 0, image_w, image_h,
        reinterpret_cast<const uint16_t*>(dsc->data), dsc->data_size, true);
    Serial.printf("[Screensaver] Hardware-Preview %s in %u ms\n",
                  preview_ok ? "OK" : "uebersprungen",
                  static_cast<unsigned>(millis() - preview_started_ms));
  } else {
    Serial.println("[Screensaver] Hardware-Preview uebersprungen (kein Cache-Treffer)");
  }
}

void hide_image_screensaver() {
  ScreensaverState* st = g_state;
  if (!st) return;
  // Vor dem Delete nullen: on_overlay_delete erkennt daran, dass der Abbau
  // schon hier laeuft, und fasst den State nicht noch einmal an.
  g_state = nullptr;
  if (st->timer) {
    lv_timer_delete(st->timer);
    st->timer = nullptr;
  }
  if (st->overlay) {
    // Aus dem Klick-Event heraus nie synchron loeschen; HIDDEN verhindert,
    // dass zwischen jetzt und dem Async-Delete noch gerendert wird.
    lv_obj_add_flag(st->overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_delete_async(st->overlay);
  }
  // Der Bildpuffer bleibt absichtlich im Cache (g_cache_dsc) — das naechste
  // Oeffnen desselben Bilds ist damit sofort da.
  delete st;
}
