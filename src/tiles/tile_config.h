#ifndef TILE_CONFIG_H
#define TILE_CONFIG_H

#include <Arduino.h>
#include <vector>

#include "src/devices/device.h"

static constexpr uint8_t GRID_COLS = Device::kGridCols;
static constexpr uint8_t GRID_ROWS = Device::kGridRows;
static constexpr size_t TILES_PER_GRID = GRID_COLS * GRID_ROWS;

static constexpr int GRID_GAP = Device::kGridGap;
static constexpr int GRID_PAD = Device::kGridPad;
static constexpr int GRID_CELL_W = Device::kGridCellW;
static constexpr int GRID_CELL_H = Device::kGridCellH;

enum TileType : uint8_t {
  TILE_EMPTY = 0,
  TILE_SENSOR = 1,
  TILE_SCENE = 2,
  TILE_KEY = 3,
  TILE_FOLDER = 4,
  TILE_SWITCH = 5,
  TILE_IMAGE = 6,
  TILE_SETTINGS = 7,
  TILE_BACK = 8,
  TILE_CLOCK = 9,
  TILE_TEXT = 10,
  TILE_COUNTER = 11,
  TILE_WEATHER = 12,
  TILE_RADAR = 13,
  TILE_ENERGY = 14,
  TILE_MEDIA = 15,
  TILE_PIXELANIM = 16,
  TILE_CLIMATE = 17
};

// A media tile renders its (often long) title as a horizontally scrolling band the
// full width of the tile. On the 8-inch device every flush is PPA-rotated, and a
// band wider than the safe rotate width jams the single-slot SRM engine (see
// kPpaMinRotateWidth in the Waveshare 8" driver) -> the whole UI drops onto the slow
// CPU rotate until a power cycle. Capping a media tile to a small square keeps its
// title band well under that limit (3 cells ~= 540 px on the 8" 7-col grid).
static constexpr uint8_t MEDIA_TILE_MIN_SPAN = 2;
static constexpr uint8_t MEDIA_TILE_MAX_SPAN = 3;

static inline void clamp_media_tile_span(TileType type, uint8_t& span_w, uint8_t& span_h) {
  if (type != TILE_MEDIA) return;
  const uint8_t min_w = GRID_COLS >= MEDIA_TILE_MIN_SPAN
                            ? MEDIA_TILE_MIN_SPAN
                            : GRID_COLS;
  const uint8_t min_h = GRID_ROWS >= MEDIA_TILE_MIN_SPAN
                            ? MEDIA_TILE_MIN_SPAN
                            : GRID_ROWS;
  if (span_w < min_w) span_w = min_w;
  if (span_h < min_h) span_h = min_h;
  if (span_w > MEDIA_TILE_MAX_SPAN) span_w = MEDIA_TILE_MAX_SPAN;
  if (span_h > MEDIA_TILE_MAX_SPAN) span_h = MEDIA_TILE_MAX_SPAN;
}

// Ein Media-Tile darf am rechten/unteren Rand nicht wieder unter sein
// 2x2-Minimum gekuerzt werden. In diesem Fall die Position nach innen
// verschieben; fuer andere Tile-Typen bleibt das bisherige Layout unveraendert.
static inline void clamp_media_tile_layout(TileType type,
                                           uint8_t& col, uint8_t& row,
                                           uint8_t& span_w, uint8_t& span_h) {
  clamp_media_tile_span(type, span_w, span_h);
  if (type != TILE_MEDIA) return;
  if (span_w > GRID_COLS) span_w = GRID_COLS;
  if (span_h > GRID_ROWS) span_h = GRID_ROWS;
  if (col > GRID_COLS - span_w) col = GRID_COLS - span_w;
  if (row > GRID_ROWS - span_h) row = GRID_ROWS - span_h;
}

enum TilePopupOpenMode : uint8_t {
  TILE_POPUP_OPEN_LONG_PRESS = 0,
  TILE_POPUP_OPEN_SHORT_PRESS = 1
};

enum SwitchPopupOpenModeStorage : uint8_t {
  TILE_SWITCH_POPUP_MODE_LEGACY = 0,
  TILE_SWITCH_POPUP_MODE_SHORT = 1,
  TILE_SWITCH_POPUP_MODE_LONG = 2
};

struct Tile {
  TileType type;
  String title;
  String icon_name;
  uint32_t bg_color;
  // Fuer normale Kacheln derzeit immer voll deckend. Der Screensaver nutzt
  // dasselbe Tile-Objekt und kann damit seinen Hintergrund transparent
  // zeichnen. Persistiert im bereits vorhandenen Reserved-Byte von V7.
  uint8_t background_opacity;

  uint8_t col;
  uint8_t row;
  uint8_t span_w;
  uint8_t span_h;

  String sensor_entity;
  String sensor_unit;
  uint8_t sensor_decimals;
  uint8_t sensor_value_font;
  uint8_t sensor_display_mode;
  int32_t sensor_gauge_min;
  int32_t sensor_gauge_max;
  uint16_t sensor_gauge_arc;
  uint16_t sensor_gauge_size;
  int16_t sensor_gauge_y_offset;
  int16_t sensor_value_y_offset;
  uint16_t sensor_graph_height;
  uint8_t popup_open_mode;

  String scene_alias;

  String key_macro;
  uint8_t key_code;
  uint8_t key_modifier;

  String image_path;
  uint16_t image_slideshow_sec;

  Tile()
      : type(TILE_EMPTY),
        bg_color(0),
        background_opacity(255),
        col(0),
        row(0),
        span_w(1),
        span_h(1),
        sensor_decimals(0xFF),
        sensor_value_font(0),
        sensor_display_mode(0),
        sensor_gauge_min(0),
        sensor_gauge_max(100),
        sensor_gauge_arc(100),
        sensor_gauge_size(350),
        sensor_gauge_y_offset(12),
        sensor_value_y_offset(0),
        sensor_graph_height(60),
        popup_open_mode(TILE_POPUP_OPEN_LONG_PRESS),
        key_code(0),
        key_modifier(0),
        image_slideshow_sec(10) {}
};

// Climate tile content is packed into sensor_gauge_min. Climate tiles do not
// use the sensor gauge range, so this preserves the existing V7 storage layout
// while allowing six independently configurable 4-bit slots.
enum ClimateTileContent : uint8_t {
  CLIMATE_TILE_CONTENT_AUTO = 0,
  CLIMATE_TILE_CONTENT_EMPTY = 1,
  CLIMATE_TILE_CONTENT_CURRENT_TEMPERATURE = 2,
  CLIMATE_TILE_CONTENT_CURRENT_HUMIDITY = 3,
  CLIMATE_TILE_CONTENT_TARGET_TEMPERATURE = 4,
  CLIMATE_TILE_CONTENT_TARGET_TEMPERATURE_LOW = 5,
  CLIMATE_TILE_CONTENT_TARGET_TEMPERATURE_HIGH = 6,
  CLIMATE_TILE_CONTENT_TARGET_HUMIDITY = 7,
  CLIMATE_TILE_CONTENT_HVAC_MODE = 8
};

static constexpr uint8_t CLIMATE_TILE_MAX_CONTENT_SLOTS = 6;
static constexpr uint32_t CLIMATE_TILE_CONTENT_PACKED_MASK = 0x00FFFFFFu;

// Adjustable climate values consume two cells. Their preferred orientation is
// packed into sensor_gauge_max (two bits per configured item). A magic prefix
// distinguishes the climate layout data from the legacy sensor default (100).
enum ClimateTileTargetLayout : uint8_t {
  CLIMATE_TILE_TARGET_LAYOUT_AUTO = 0,
  CLIMATE_TILE_TARGET_LAYOUT_HORIZONTAL = 1,
  CLIMATE_TILE_TARGET_LAYOUT_VERTICAL = 2
};

static constexpr uint32_t CLIMATE_TILE_LAYOUT_PACKED_MAGIC = 0x434C0000u;
static constexpr uint32_t CLIMATE_TILE_LAYOUT_PACKED_MAGIC_MASK = 0xFFFF0000u;
static constexpr uint32_t CLIMATE_TILE_LAYOUT_PACKED_VALUE_MASK = 0x00000FFFu;

static inline uint8_t climateTileSlotCapacity(const Tile& tile) {
  const uint8_t span_w = tile.span_w < 1 ? 1 : tile.span_w;
  const uint8_t span_h = tile.span_h < 1 ? 1 : tile.span_h;
  if (span_w == 1 && span_h == 1) return 1;
  if (span_w >= 2 && span_h == 1) return 2;
  if (span_w == 1) return 3;
  return CLIMATE_TILE_MAX_CONTENT_SLOTS;
}

static inline ClimateTileContent getClimateTileSlotContent(
    const Tile& tile, uint8_t slot_index) {
  if (slot_index >= CLIMATE_TILE_MAX_CONTENT_SLOTS) {
    return CLIMATE_TILE_CONTENT_AUTO;
  }
  const uint32_t packed =
      static_cast<uint32_t>(tile.sensor_gauge_min) &
      CLIMATE_TILE_CONTENT_PACKED_MASK;
  const uint8_t raw =
      static_cast<uint8_t>((packed >> (slot_index * 4)) & 0x0Fu);
  if (raw > CLIMATE_TILE_CONTENT_HVAC_MODE) {
    return CLIMATE_TILE_CONTENT_AUTO;
  }
  return static_cast<ClimateTileContent>(raw);
}

static inline void setClimateTileSlotContent(
    Tile& tile, uint8_t slot_index, ClimateTileContent content) {
  if (slot_index >= CLIMATE_TILE_MAX_CONTENT_SLOTS) return;
  uint32_t packed =
      static_cast<uint32_t>(tile.sensor_gauge_min) &
      CLIMATE_TILE_CONTENT_PACKED_MASK;
  const uint32_t shift = static_cast<uint32_t>(slot_index) * 4u;
  packed &= ~(0x0Fu << shift);
  packed |=
      (static_cast<uint32_t>(content) & 0x0Fu) << shift;
  tile.sensor_gauge_min = static_cast<int32_t>(packed);
}

static inline uint32_t getClimateTileLayoutsPacked(const Tile& tile) {
  const uint32_t stored = static_cast<uint32_t>(tile.sensor_gauge_max);
  if ((stored & CLIMATE_TILE_LAYOUT_PACKED_MAGIC_MASK) !=
      CLIMATE_TILE_LAYOUT_PACKED_MAGIC) {
    return 0;
  }
  return stored & CLIMATE_TILE_LAYOUT_PACKED_VALUE_MASK;
}

static inline ClimateTileTargetLayout getClimateTileTargetLayout(
    const Tile& tile, uint8_t slot_index) {
  if (slot_index >= CLIMATE_TILE_MAX_CONTENT_SLOTS) {
    return CLIMATE_TILE_TARGET_LAYOUT_AUTO;
  }
  const uint32_t packed = getClimateTileLayoutsPacked(tile);
  const uint8_t raw =
      static_cast<uint8_t>((packed >> (slot_index * 2)) & 0x03u);
  if (raw > CLIMATE_TILE_TARGET_LAYOUT_VERTICAL) {
    return CLIMATE_TILE_TARGET_LAYOUT_AUTO;
  }
  return static_cast<ClimateTileTargetLayout>(raw);
}

static inline void setClimateTileTargetLayout(
    Tile& tile, uint8_t slot_index, ClimateTileTargetLayout layout) {
  if (slot_index >= CLIMATE_TILE_MAX_CONTENT_SLOTS) return;
  uint32_t packed = getClimateTileLayoutsPacked(tile);
  const uint32_t shift = static_cast<uint32_t>(slot_index) * 2u;
  packed &= ~(0x03u << shift);
  packed |= (static_cast<uint32_t>(layout) & 0x03u) << shift;
  tile.sensor_gauge_max = static_cast<int32_t>(
      CLIMATE_TILE_LAYOUT_PACKED_MAGIC |
      (packed & CLIMATE_TILE_LAYOUT_PACKED_VALUE_MASK));
}

static inline uint8_t getTilePopupOpenMode(const Tile& tile) {
  if (tile.type == TILE_SWITCH) {
    return (tile.key_code == TILE_SWITCH_POPUP_MODE_LONG)
               ? TILE_POPUP_OPEN_LONG_PRESS
               : TILE_POPUP_OPEN_SHORT_PRESS;
  }
  if (tile.type != TILE_SENSOR && tile.type != TILE_WEATHER &&
      tile.type != TILE_ENERGY && tile.type != TILE_CLIMATE) {
    return TILE_POPUP_OPEN_LONG_PRESS;
  }
  return (tile.popup_open_mode == TILE_POPUP_OPEN_SHORT_PRESS)
             ? TILE_POPUP_OPEN_SHORT_PRESS
             : TILE_POPUP_OPEN_LONG_PRESS;
}

static inline void setTilePopupOpenMode(Tile& tile, uint8_t mode) {
  if (tile.type == TILE_SWITCH) {
    const uint8_t normalized =
        (mode == TILE_POPUP_OPEN_SHORT_PRESS)
            ? TILE_POPUP_OPEN_SHORT_PRESS
            : TILE_POPUP_OPEN_LONG_PRESS;
    tile.popup_open_mode = normalized;
    // Switch tiles previously had no configurable popup mode. Use the otherwise
    // unused key_code slot to distinguish legacy tiles (default to short press)
    // from an explicitly saved long-press configuration.
    tile.key_code = (normalized == TILE_POPUP_OPEN_SHORT_PRESS)
                        ? TILE_SWITCH_POPUP_MODE_SHORT
                        : TILE_SWITCH_POPUP_MODE_LONG;
    tile.key_modifier = 0;
    return;
  }
  if (tile.type != TILE_SENSOR && tile.type != TILE_WEATHER &&
      tile.type != TILE_ENERGY && tile.type != TILE_CLIMATE) return;
  tile.popup_open_mode = (mode == TILE_POPUP_OPEN_SHORT_PRESS)
                             ? TILE_POPUP_OPEN_SHORT_PRESS
                             : TILE_POPUP_OPEN_LONG_PRESS;
}

struct TileGridConfig {
  Tile tiles[TILES_PER_GRID];
};

static constexpr uint32_t TILE_BG_COLOR_RGB_MASK = 0x00FFFFFFu;
static constexpr uint32_t TILE_BG_COLOR_EXPLICIT = 0x01000000u;

static inline uint32_t makeTileBgColor(uint32_t rgb) {
  return (rgb & TILE_BG_COLOR_RGB_MASK) | TILE_BG_COLOR_EXPLICIT;
}

static inline bool tileBgColorIsSet(const Tile& tile) {
  return tile.bg_color != 0;
}

static inline uint32_t tileBgColorRgb(const Tile& tile) {
  return tile.bg_color & TILE_BG_COLOR_RGB_MASK;
}

static inline uint32_t tileBgColorOrDefault(const Tile& tile, uint32_t default_color) {
  return tileBgColorIsSet(tile) ? tileBgColorRgb(tile) : (default_color & TILE_BG_COLOR_RGB_MASK);
}

struct FolderEntry {
  uint16_t id;
  uint16_t parent_id;
  char name[32];
  char icon_name[32];
};

// Lightweight per-tile projection used by background scans (cache refresh,
// MQTT dynamic-route rebuild) that only need type + entity id, not the full
// Tile (title/icon/scene/macro/image_path). Skips ~5 of 6 per-tile String
// allocations that a full TileGridConfig load pays for every tile.
struct TileEntitySlot {
  TileType type = TILE_EMPTY;
  String sensor_entity;
};

// Read-only-Sicht auf einen Slot des PSRAM-Ordner-Entity-Caches (siehe
// TileConfig::getFolderEntitiesCached). Die entity-Zeiger zeigen in den
// Cache und bleiben nur bis zum naechsten getFolderEntitiesCached()-Aufruf
// fuer denselben Ordner gueltig -- sofort verwenden, nicht aufheben.
struct FolderEntitySlotView {
  TileType type = TILE_EMPTY;
  const char* entity = "";  // nie nullptr
};

struct FolderEntityCacheEntry;

class TileConfig {
public:
  // Eigenstaendiges Grid fuer den Screensaver. Die reservierte Storage-ID
  // ist kein Ordner und erscheint deshalb weder in der Ordnerliste noch in
  // der Navigation. Gespeichert wird trotzdem im exakt gleichen gepackten
  // LittleFS-Format wie jedes normale Kachel-Grid.
  static constexpr uint16_t kScreensaverGridStorageId = 0xFFFE;

  TileConfig();

  bool load();
  bool loadFolderGrid(uint16_t folder_id, TileGridConfig& out);
  bool loadScreensaverGrid(TileGridConfig& out);
  bool loadFolderGridEntitiesOnly(uint16_t folder_id, TileEntitySlot* out, size_t count);
  // Wie loadFolderGridEntitiesOnly(), aber ueber einen PSRAM-Cache: der
  // Flash-Read (~20ms pro Ordner, mehr als ein halber 33ms-Frame bei 30fps)
  // faellt nur beim ersten Zugriff bzw. nach einer Grid-Aenderung an. NUR vom
  // Loop-Task aufrufen (baut den Cache um); invalidateFolderEntityCache()
  // ist dagegen von jedem Task erlaubt.
  bool getFolderEntitiesCached(uint16_t folder_id, FolderEntitySlotView* out, size_t count);
  void invalidateFolderEntityCache();
  bool saveFolderGrid(uint16_t folder_id, const TileGridConfig& grid);
  bool saveScreensaverGrid(const TileGridConfig& grid);

  bool setActiveFolder(uint16_t folder_id);
  bool setActiveFolderCached(uint16_t folder_id, const TileGridConfig& grid);
  uint16_t getActiveFolderId() const { return active_folder_id; }
  const TileGridConfig& getActiveGrid() const { return active_grid; }
  TileGridConfig& getActiveGrid() { return active_grid; }

  const FolderEntry* getFolder(uint16_t folder_id) const;
  uint16_t getFolderParent(uint16_t folder_id) const;
  const std::vector<FolderEntry>& getFolders() const { return folders; }
  bool folderExists(uint16_t folder_id) const;
  bool createFolder(uint16_t parent_id, const String& name, const String& icon, uint16_t& out_id);
  bool updateFolder(uint16_t folder_id, const String& name, const String& icon);
  bool deleteFolder(uint16_t folder_id);

private:
  static constexpr uint16_t kRootFolderId = 0;
  static constexpr uint16_t kInvalidFolderId = 0xFFFF;

  TileGridConfig active_grid;
  uint16_t active_folder_id = kRootFolderId;
  std::vector<FolderEntry> folders;

  // Ordner-Entity-Cache (PSRAM): ein Eintrag pro Ordner mit Typ + Entity-ID
  // aller Kacheln. Invalidierung laeuft ueber einen globalen Generations-
  // zaehler, weil Grid-Saves auch vom Web-Task kommen -- der Schreiber
  // erhoeht nur die Zahl (kein free, kein Umbau), neu gebaut wird
  // ausschliesslich auf dem Loop-Task beim naechsten Zugriff.
  FolderEntityCacheEntry* folder_entity_cache_ = nullptr;
  size_t folder_entity_cache_count_ = 0;
  volatile uint32_t folder_entity_cache_gen_ = 1;
  FolderEntityCacheEntry* findFolderEntityCacheEntry(uint16_t folder_id);
  FolderEntityCacheEntry* storeFolderEntityCache(uint16_t folder_id,
                                                 const TileEntitySlot* slots,
                                                 uint32_t built_gen);

  bool loadFolders();
  bool saveFolders() const;
  bool loadGrid(uint16_t folder_id, TileGridConfig& grid,
                bool ensure_navigation_tile = true);
  bool saveGrid(uint16_t folder_id, const TileGridConfig& grid,
                bool ensure_navigation_tile = true);
  uint16_t nextFolderId() const;
  void ensureRootFolder();
  bool ensureSettingsTile(TileGridConfig& grid);
  bool ensureBackTile(uint16_t folder_id, TileGridConfig& grid);
};

extern TileConfig tileConfig;

#endif // TILE_CONFIG_H
