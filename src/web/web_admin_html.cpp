#include "src/web/web_admin.h"
#include "src/web/web_admin_utils.h"
#include <WiFi.h>
#include <math.h>
#include <stdlib.h>
#include <nvs.h>
#include <nvs_flash.h>
#include "src/core/config_manager.h"
#include "src/network/ha_bridge_config.h"
#include "src/game/game_controls_config.h"
#include "src/web/web_admin_scripts.h"
#include "src/web/web_admin_styles.h"
#include "src/web/web_admin_fonts.h"
#include "src/web/web_admin_tile_helpers.h"
#include "src/tiles/tile_config.h"
#include "src/types/types_registry.h"
#include "src/core/crash_log.h"
#include "src/core/device_entities.h"
#include "src/core/firmware_version.h"
#include "src/core/i18n.h"
#include "src/devices/device.h"
#include "src/types/clock/clock_format.h"
#include "src/types/energy/energy_data.h"
#include "src/ui/screensaver_config.h"
#include <cstring>

namespace {

struct TimezoneOption {
  uint8_t group;
  const char* code;
  const char* label_en;
  const char* label_de;
};

static String buildTimezoneOptionsHtml(const char* selected_code, bool is_german) {
  static const TimezoneOption kOptions[] = {
      {0, "utc", "UTC+0 - UTC", "UTC+0 - UTC"},
      {1, "london", "UTC+0 / UTC+1 - London", "UTC+0 / UTC+1 - London"},
      {1, "berlin", "UTC+1 / UTC+2 - Berlin", "UTC+1 / UTC+2 - Berlin"},
      {1, "athens", "UTC+2 / UTC+3 - Athens", "UTC+2 / UTC+3 - Athen"},
      {1, "istanbul", "UTC+3 - Istanbul", "UTC+3 - Istanbul"},
      {1, "moscow", "UTC+3 - Moscow", "UTC+3 - Moskau"},
      {2, "honolulu", "UTC-10 - Honolulu", "UTC-10 - Honolulu"},
      {2, "los_angeles", "UTC-8 / UTC-7 - Los Angeles", "UTC-8 / UTC-7 - Los Angeles"},
      {2, "phoenix", "UTC-7 - Phoenix", "UTC-7 - Phoenix"},
      {2, "denver", "UTC-7 / UTC-6 - Denver", "UTC-7 / UTC-6 - Denver"},
      {2, "chicago", "UTC-6 / UTC-5 - Chicago", "UTC-6 / UTC-5 - Chicago"},
      {2, "new_york", "UTC-5 / UTC-4 - New York", "UTC-5 / UTC-4 - New York"},
      {2, "buenos_aires", "UTC-3 - Buenos Aires", "UTC-3 - Buenos Aires"},
      {2, "sao_paulo", "UTC-3 - Sao Paulo", "UTC-3 - Sao Paulo"},
      {3, "johannesburg", "UTC+2 - Johannesburg", "UTC+2 - Johannesburg"},
      {3, "nairobi", "UTC+3 - Nairobi", "UTC+3 - Nairobi"},
      {3, "dubai", "UTC+4 - Dubai", "UTC+4 - Dubai"},
      {4, "karachi", "UTC+5 - Karachi", "UTC+5 - Karatschi"},
      {4, "kolkata", "UTC+5:30 - Kolkata", "UTC+5:30 - Kolkata"},
      {4, "dhaka", "UTC+6 - Dhaka", "UTC+6 - Dhaka"},
      {4, "bangkok", "UTC+7 - Bangkok", "UTC+7 - Bangkok"},
      {4, "singapore", "UTC+8 - Singapore", "UTC+8 - Singapur"},
      {4, "perth", "UTC+8 - Perth", "UTC+8 - Perth"},
      {4, "tokyo", "UTC+9 - Tokyo", "UTC+9 - Tokio"},
      {5, "darwin", "UTC+9:30 - Darwin", "UTC+9:30 - Darwin"},
      {5, "sydney", "UTC+10 / UTC+11 - Sydney", "UTC+10 / UTC+11 - Sydney"},
      {5, "auckland", "UTC+12 / UTC+13 - Auckland", "UTC+12 / UTC+13 - Auckland"},
  };
  static const char* kGroupLabelsEn[] = {
      "Global",
      "Europe",
      "Americas",
      "Africa & Middle East",
      "Asia",
      "Oceania",
  };
  static const char* kGroupLabelsDe[] = {
      "Global",
      "Europa",
      "Amerika",
      "Afrika & Naher Osten",
      "Asien",
      "Ozeanien",
  };
  const char* selected = (selected_code && selected_code[0]) ? selected_code : "berlin";
  String html;
  html.reserve(2048);
  uint8_t current_group = 255;
  for (const auto& option : kOptions) {
    if (option.group != current_group) {
      if (current_group != 255) html += "</optgroup>";
      current_group = option.group;
      html += "<optgroup label=\"";
      html += is_german ? kGroupLabelsDe[current_group] : kGroupLabelsEn[current_group];
      html += "\">";
    }
    html += "<option value=\"";
    html += option.code;
    html += "\"";
    if (strcmp(selected, option.code) == 0) html += " selected";
    html += ">";
    html += is_german ? option.label_de : option.label_en;
    html += "</option>";
  }
  if (current_group != 255) html += "</optgroup>";
  return html;
}

static String buildGlobalTimeFormatOptionsHtml(uint8_t selected_format, const i18n::Strings& tr) {
  selected_format = clock_tile::normalize_time_format(selected_format);
  String html;
  html.reserve(192);
  html += "<option value=\"0\"";
  if (selected_format == clock_tile::TIME_FORMAT_AUTO) html += " selected";
  html += ">";
  html += tr.format_auto_language;
  html += "</option>";
  html += "<option value=\"1\"";
  if (selected_format == clock_tile::TIME_FORMAT_24H) html += " selected";
  html += ">";
  html += tr.format_24_hour;
  html += "</option>";
  html += "<option value=\"2\"";
  if (selected_format == clock_tile::TIME_FORMAT_12H) html += " selected";
  html += ">";
  html += tr.format_12_hour;
  html += "</option>";
  return html;
}

static String buildGlobalDateFormatOptionsHtml(uint8_t selected_format, const i18n::Strings& tr) {
  selected_format = clock_tile::normalize_date_format(selected_format);
  String html;
  html.reserve(224);
  html += "<option value=\"0\"";
  if (selected_format == clock_tile::DATE_FORMAT_AUTO) html += " selected";
  html += ">";
  html += tr.format_auto_language;
  html += "</option>";
  html += "<option value=\"1\"";
  if (selected_format == clock_tile::DATE_FORMAT_DMY) html += " selected";
  html += ">DD.MM.YYYY</option>";
  html += "<option value=\"2\"";
  if (selected_format == clock_tile::DATE_FORMAT_MDY) html += " selected";
  html += ">MM/DD/YYYY</option>";
  html += "<option value=\"3\"";
  if (selected_format == clock_tile::DATE_FORMAT_YMD) html += " selected";
  html += ">YYYY/MM/DD</option>";
  return html;
}

}  // namespace

// Helper function to generate tile tab HTML (unified for all folders)
static void appendTileTabHTML(
    String& html,
    uint16_t folder_id,
    const FolderEntry& folder,
    const TileGridConfig& grid,
    const std::vector<String>& sensorOptions,
    const std::vector<String>& energyOptions,
    const std::vector<String>& weatherOptions,
    const std::vector<SceneOption>& sceneOptions,
    const std::vector<String>& switchOptions,
    const std::vector<String>& mediaOptions,
    const std::function<String(const String&, uint8_t)>& formatSensorValue,
    const String& navigateOptionsHtml,
    bool screensaver_mode = false
) {
  const auto& tr = i18n::strings(configManager.getConfig().language);
  String tab_id = screensaver_mode ? String("screensaver")
                                   : String("folder") + String(folder_id);

  html += R"html(
      <!-- Tile Folder -->
      <div id="tab-tiles-)html";
  html += tab_id;
  html += R"html(" class="tab-content tile-tab" data-tab-id=")html";
  html += tab_id;
  html += R"html(" data-folder-id=")html";
  html += String(folder_id);
  html += R"html(" data-folder-parent=")html";
  html += String(folder.parent_id);
  html += R"html(" data-folder-name=")html";
  appendHtmlEscaped(html, folder.name);
  html += R"html(" data-folder-icon=")html";
  appendHtmlEscaped(html, folder.icon_name);
  if (screensaver_mode) html += R"html(" data-screensaver-grid="1)html";
  html += R"html(">
        <div class="tile-editor">
          <div class="tile-editor-main">
          <div class="tile-grid-scroll">
          <!-- Grid Preview -->
          <div class="tile-grid)html";
  if (screensaver_mode) {
    html += " screensaver-tile-grid";
  } else if (configManager.getConfig().tile_borders) {
    html += " tiles-bordered";
  }
  html += R"html(" id=")html";
  html += tab_id;
  html += R"html(Grid">
)html";

  if (screensaver_mode) {
    html += R"html(            <div class="screensaver-grid-image-frame">
              <img id="screensaverPreviewImage" class="screensaver-grid-image" alt="" draggable="false" hidden>
            </div>
            <div id="screensaverClock" class="screensaver-grid-clock">
              <div id="screensaverClockTime">--:--</div>
              <div id="screensaverClockDate">--.--.----</div>
              <span class="screensaver-clock-resize-handle" title="Resize"></span>
            </div>
)html";
  }

  // Generate tiles
  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    const Tile& tile = grid.tiles[i];
    String cssClass = "tile";
    String tileStyle = "";
    const TileTypeDescriptor* type_desc = get_tile_type_descriptor(tile.type);
    const char* type_css = type_desc ? type_desc->css_class : nullptr;
    uint8_t col = (tile.col < GRID_COLS) ? tile.col : 0;
    uint8_t row = (tile.row < GRID_ROWS) ? tile.row : 0;
    uint8_t span_w = (tile.span_w < 1) ? 1 : tile.span_w;
    uint8_t span_h = (tile.span_h < 1) ? 1 : tile.span_h;
    clamp_media_tile_layout(tile.type, col, row, span_w, span_h);
    if (screensaver_mode && GRID_ROWS > 1 && row < GRID_ROWS - 2) {
      row = GRID_ROWS - 2;
    }
    if (span_w > GRID_COLS - col) span_w = GRID_COLS - col;
    if (span_h > GRID_ROWS - row) span_h = GRID_ROWS - row;

    if (type_css && type_css[0]) {
      cssClass += " ";
      cssClass += type_css;
    } else if (tile.type == TILE_EMPTY) {
      cssClass += " empty";
    }

    if (tile.type != TILE_EMPTY) {
      uint32_t bg_color = tileBgColorIsSet(tile)
                              ? tileBgColorRgb(tile)
                              : (type_desc ? type_desc->default_bg_color : 0);
      if (bg_color == 0) bg_color = 0x353535;
      char colorHex[10];
      if (screensaver_mode) {
        snprintf(colorHex, sizeof(colorHex), "#%06X%02X",
                 (unsigned int)bg_color,
                 static_cast<unsigned int>(tile.background_opacity));
      } else {
        snprintf(colorHex, sizeof(colorHex), "#%06X", (unsigned int)bg_color);
      }
      tileStyle = "background:";
      tileStyle += colorHex;
    }

    tileStyle += ";grid-column:";
    tileStyle += String(static_cast<unsigned>(col + 1));
    tileStyle += " / span ";
    tileStyle += String(static_cast<unsigned>(span_w));
    tileStyle += ";grid-row:";
    tileStyle += String(static_cast<unsigned>(row + 1));
    tileStyle += " / span ";
    tileStyle += String(static_cast<unsigned>(span_h));
    tileStyle += ";";
    if (screensaver_mode && tile.type == TILE_EMPTY &&
        row < (GRID_ROWS > 1 ? GRID_ROWS - 2 : 0)) {
      tileStyle += "display:none;";
    }

    html += "<div class=\"";
    html += cssClass;
    html += "\" data-index=\"";
    html += String(i);
    html += "\" data-col=\"";
    html += String(static_cast<unsigned>(col));
    html += "\" data-row=\"";
    html += String(static_cast<unsigned>(row));
    html += "\" data-span-w=\"";
    html += String(static_cast<unsigned>(span_w));
    html += "\" data-span-h=\"";
    html += String(static_cast<unsigned>(span_h));
    html += "\" data-type=\"";
    html += String(static_cast<unsigned>(tile.type));
    if (tile.type == TILE_FOLDER) {
      html += "\" data-navigate-target=\"";
      html += String(getNavigateTargetId(tile));
    }
    html += "\" draggable=\"true\" id=\"";
    html += tab_id;
    html += "-tile-";
    html += String(i);
    html += "\" style=\"";
    html += tileStyle;
    html += "\" onclick=\"selectTile(parseInt(this.dataset.index), '";
    html += tab_id;
    html += "')\" ondblclick=\"openPreviewNavigation(this, '";
    html += tab_id;
    html += "')\">";

    if (tile.type != TILE_EMPTY) {
      // Icon (optional) - normalize icon name (lowercase, trim, remove mdi: prefix)
      String iconName = tile.icon_name;
      iconName.toLowerCase();
      iconName.trim();
      if (iconName.startsWith("mdi:")) iconName.remove(0, 4);
      else if (iconName.startsWith("mdi-")) iconName.remove(0, 4);

      bool hasIcon = iconName.length() > 0;

      if (hasIcon) {
        html += "<i class=\"mdi mdi-";
        html += iconName;  // Direkt hinzufügen (CSS-Klasse darf nicht escaped werden!)
        html += " tile-icon\"></i>";
      }

      // Title nur anzeigen wenn vorhanden
      if (tile.title.length()) {
        html += "<div class=\"tile-title\" id=\"";
        html += tab_id;
        html += "-tile-";
        html += String(i);
        html += "-title\">";
        appendHtmlEscaped(html, tile.title);
        html += "</div>";
      }
    }

    const char* preview_kind = get_tile_type_preview_kind(tile.type);
    if (preview_kind && strcmp(preview_kind, "weather") == 0) {
      html += "<div class=\"tile-ghost-icon\"><i class=\"mdi mdi-weather-partly-cloudy\"></i></div>";
    }
    if (preview_kind && strcmp(preview_kind, "media") == 0) {
      html += "<div class=\"tile-ghost-icon\"><i class=\"mdi mdi-music\"></i></div>";
    }
    if (preview_kind && strcmp(preview_kind, "sensor") == 0) {
      html += "<div class=\"tile-value\" id=\"";
      html += tab_id;
      html += "-tile-";
      html += String(i);
      html += "-value\">";

      String sensorValue = "--";
      if (tile.sensor_entity.length()) {
        sensorValue = haBridgeConfig.findSensorInitialValue(tile.sensor_entity);
        sensorValue = formatSensorValue(sensorValue, tile.sensor_decimals);
        if (sensorValue.length() == 0) {
          sensorValue = "--";
        }
      }
      appendHtmlEscaped(html, sensorValue);

      if (tile.sensor_unit.length()) {
        html += "<span class=\"tile-unit\">";
        appendHtmlEscaped(html, tile.sensor_unit);
        html += "</span>";
      }
      html += "</div>";
    }

    if (preview_kind && strcmp(preview_kind, "clock") == 0) {
      uint8_t flags = tile.sensor_decimals;
      if (flags == 0xFF) flags = 1;
      flags &= 0x03;
      if (flags == 0) flags = 1;
      if (flags & 1) {
        html += "<div class=\"tile-clock-time\">--:--</div>";
      }
      if (flags & 2) {
        html += "<div class=\"tile-clock-date\">--.--.----</div>";
      }
    }

    html += "</div>";
  }

  html += R"html(
          </div>
          </div>
          <div class="folder-footer">
            <p class="hint">)html";
  if (screensaver_mode) {
    html += tr.screensaver_hint;
  } else {
    html += tr.admin_tile_hint;
  }
  html += R"html(</p>
)html";
  if (!screensaver_mode && folder_id != 0) {
    html += R"html(            <button type="button" class="btn btn-danger btn-delete-folder" onclick="deleteFolder(')html";
    html += tab_id;
    html += R"html(')">)html";
    html += tr.admin_delete_folder_tab;
    html += R"html(</button>
)html";
  }
  html += R"html(          </div>
          </div>

          <!-- Settings Panel -->
          <div class="tile-settings" id=")html";
  html += tab_id;
  html += R"html(Settings">
)html";
  if (screensaver_mode) {
    html += R"html(            <div id="screensaverBackgroundSettings" class="screensaver-background-settings">
              <div class="tile-settings-head"><h3 style="margin-top:0;">)html";
    html += strcmp(tr.html_lang, "de") == 0 ? "Diashow" : "Slideshow";
    html += R"html(</h3></div>
              <div class="tile-settings-body">
                <div class="screensaver-fixed-type"><label>)html";
    html += tr.admin_type;
    html += R"html(</label><input value=")html";
    html += strcmp(tr.html_lang, "de") == 0 ? "Diashow" : "Slideshow";
    html += R"html(" disabled></div>
                <label class="inline-checkbox"><input id="screensaverUseWallpapers" type="checkbox"> )html";
    html += tr.screensaver_use_wallpapers;
    html += R"html(</label>
                <label class="inline-checkbox"><input id="screensaverShuffle" type="checkbox"> )html";
    html += tr.screensaver_shuffle;
    html += R"html(</label>
                <label class="inline-checkbox"><input id="screensaverTileShadow" type="checkbox"> )html";
    html += tr.screensaver_tile_shadow;
    html += R"html(</label>
                <label class="inline-checkbox"><input id="screensaverTileBorder" type="checkbox"> )html";
    html += tr.screensaver_tile_border;
    html += R"html(</label>
                <div class="screensaver-wallpaper-heading">)html";
    html += tr.screensaver_wallpapers_heading;
    html += R"html(</div>
                <div class="screensaver-storage-hint">)html";
    html += tr.screensaver_storage_hint;
    html += R"html(</div>
                <div id="screensaverWallpaperList" class="screensaver-wallpaper-list"></div>
                <div id="screensaverWallpaperControls" class="screensaver-wallpaper-controls">
                  <label>)html";
    html += tr.screensaver_duration_seconds;
    html += R"html(</label><input id="screensaverWallpaperDuration" type="number" min="3" max="3600" value="15">
                  <label>)html";
    html += tr.screensaver_zoom;
    html += R"html(</label><input id="screensaverWallpaperZoom" type="range" min="1000" max="3000" step="25" value="1000">
                  <div class="screensaver-focus-grid">
                    <label>)html";
    html += tr.screensaver_focus_x;
    html += R"html(<input id="screensaverFocusX" type="range" min="0" max="1000" value="500"></label>
                    <label>)html";
    html += tr.screensaver_focus_y;
    html += R"html(<input id="screensaverFocusY" type="range" min="0" max="1000" value="500"></label>
                  </div>
                </div>
              </div>
            </div>
            <div id="screensaverClockSettings" class="screensaver-background-settings screensaver-clock-settings hidden">
              <div class="tile-settings-head"><h3 style="margin-top:0;">)html";
    html += tr.screensaver_clock_heading;
    html += R"html(</h3></div>
              <div class="tile-settings-body">
                <div class="screensaver-fixed-type"><label>)html";
    html += tr.admin_type;
    html += R"html(</label><input value=")html";
    html += tr.tile_type_clock;
    html += R"html(" disabled></div>
                  <div class="clock-toggle-row">
                    <label class="inline-checkbox"><input id="screensaverShowTime" type="checkbox"> )html";
    html += tr.show_time;
    html += R"html(</label>
                    <label class="inline-checkbox"><input id="screensaverShowDate" type="checkbox"> )html";
    html += tr.show_date;
    html += R"html(</label>
                    <label class="inline-checkbox"><input id="screensaverShowWeekday" type="checkbox"> )html";
    html += tr.screensaver_show_weekday;
    html += R"html(</label>
                    <label class="inline-checkbox"><input id="screensaverClockShadow" type="checkbox"> )html";
    html += tr.screensaver_clock_shadow;
    html += R"html(</label>
                  </div>
                  <div class="screensaver-two-fields">
                    <label>)html";
    html += tr.time_font_size;
    html += R"html(<select id="screensaverTimeFont"><option>20</option><option>24</option><option>28</option><option>32</option><option>40</option><option selected>48</option><option>56</option><option>64</option><option>72</option><option>80</option><option>96</option></select></label>
                    <label>)html";
    html += tr.date_font_size;
    html += R"html(<select id="screensaverDateFont"><option>20</option><option>24</option><option selected>28</option><option>32</option><option>40</option><option>48</option><option>56</option><option>64</option><option>72</option></select></label>
                    <label>)html";
    html += tr.screensaver_time_alignment;
    html += R"html(<select id="screensaverTimeAlignment"><option value="0">)html";
    html += tr.alignment_left;
    html += R"html(</option><option value="1" selected>)html";
    html += tr.alignment_center;
    html += R"html(</option><option value="2">)html";
    html += tr.alignment_right;
    html += R"html(</option></select></label>
                    <label>)html";
    html += tr.screensaver_date_alignment;
    html += R"html(<select id="screensaverDateAlignment"><option value="0">)html";
    html += tr.alignment_left;
    html += R"html(</option><option value="1" selected>)html";
    html += tr.alignment_center;
    html += R"html(</option><option value="2">)html";
    html += tr.alignment_right;
    html += R"html(</option></select></label>
                    <label>)html";
    html += tr.time_format_label;
    html += R"html(<select id="screensaverTimeFormat"><option value="0">)html";
    html += tr.format_auto_localization;
    html += R"html(</option><option value="1">24 h</option><option value="2">12 h</option></select></label>
                    <label>)html";
    html += tr.date_format_label;
    html += R"html(<select id="screensaverDateFormat"><option value="0">)html";
    html += tr.format_auto_localization;
    html += R"html(</option><option value="1">DD.MM.YYYY</option><option value="2">MM/DD/YYYY</option><option value="3">YYYY/MM/DD</option></select></label>
                  </div>
              </div>
            </div>
)html";
  }
  html += R"html(
            <!-- Tile Settings (Visible only when tile selected) -->
            <div class="tile-specific-settings hidden">
            <div class="tile-settings-head">
              <h3 style="margin-top:0;">)html";
  html += tr.admin_tile_settings;
  html += R"html(</h3>

            <label>)html";
  html += tr.admin_type;
  html += R"html(</label>
            <select id=")html";
  html += tab_id;
  html += R"html(_tile_type" onchange="updateTileType(')html";
  html += tab_id;
  html += R"html(')">
            )html";
  if (screensaver_mode) {
    html += "<option value=\"0\">";
    html += tr.tile_type_empty;
    html += "</option><option value=\"1\">";
    html += tr.tile_type_sensor;
    html += "</option><option value=\"14\">";
    html += tr.tile_type_energy;
    html += "</option><option value=\"2\">";
    html += tr.tile_type_scene;
    html += "</option><option value=\"5\">";
    html += tr.tile_type_switch;
    html += "</option><option value=\"15\">";
    html += tr.tile_type_media;
    html += "</option>";
  } else {
    append_tile_type_select_options(html);
  }
  html += R"html(
            </select>
            <p class="hint hidden" id=")html";
  html += tab_id;
  html += R"html(_tile_type_hint">)html";
  html += tr.admin_folder_type_locked;
  html += R"html(</p>
            </div>
            <div class="tile-settings-body">

            <label>)html";
  html += tr.admin_title;
  html += R"html(</label>
            <input type="text" id=")html";
  html += tab_id;
  html += R"html(_tile_title" placeholder=")html";
  html += tr.admin_tile_title_placeholder;
  html += R"html(">

            <label>)html";
  html += tr.admin_icon_label;
  html += R"html(</label>
            <input type="text" id=")html";
  html += tab_id;
  html += R"html(_tile_icon" placeholder=")html";
  html += tr.admin_icon_placeholder;
  html += R"html(">
            <div style="font-size:11px;color:#8a8a8a;margin-top:4px;">
              Material Design Icons: <a href="https://pictogrammers.com/library/mdi/" target="_blank" style="color:#4db6ac;">)html";
  html += tr.admin_icon_list;
  html += R"html(</a>
            </div>

            <div class="tile-color-label-row)html";
  if (screensaver_mode) html += " has-opacity";
  html += R"html("><span>)html";
  html += tr.admin_color;
  html += R"html(</span>)html";
  if (screensaver_mode) {
    html += R"html(<span>)html";
    html += tr.screensaver_background_opacity;
    html += R"html(</span><span aria-hidden="true"></span>)html";
  }
  html += R"html(</div>
            <div class="tile-color-row)html";
  if (screensaver_mode) html += " has-opacity";
  html += R"html(">
            <input type="color" id=")html";
  html += tab_id;
  html += R"html(_tile_color" value="#2A2A2A">
)html";
  if (screensaver_mode) {
    html += R"html(              <input type="range" id="screensaver_tile_opacity" min="0" max="255" step="1" value="0">
)html";
  }
  html += R"html(              <button type="button" class="tile-color-reset-btn" title="Reset" onclick="resetTileColor(')html";
  html += tab_id;
  html += R"html(')"><i class="mdi mdi-restore"></i></button>
            </div>

            <div class="tile-layout">
              <div class="layout-field">
                <label>)html";
  html += tr.admin_column;
  html += R"html(</label>
                <input type="number" id=")html";
  html += tab_id;
  html += R"html(_tile_col" min="1" max=")html";
  html += String(GRID_COLS);
  html += R"html(" step="1" value="1">
              </div>
              <div class="layout-field">
                <label>)html";
  html += tr.admin_row;
  html += R"html(</label>
                <input type="number" id=")html";
  html += tab_id;
  html += R"html(_tile_row" min=")html";
  html += String(screensaver_mode && GRID_ROWS > 1 ? GRID_ROWS - 1 : 1);
  html += R"html(" max=")html";
  html += String(GRID_ROWS);
  html += R"html(" step="1" value="1">
              </div>
              <div class="layout-field">
                <label>)html";
  html += tr.admin_width_cells;
  html += R"html(</label>
                <input type="number" id=")html";
  html += tab_id;
  html += R"html(_tile_span_w" min="1" max=")html";
  html += String(GRID_COLS);
  html += R"html(" step="1" value="1">
              </div>
              <div class="layout-field">
                <label>)html";
  html += tr.admin_height_cells;
  html += R"html(</label>
                <input type="number" id=")html";
  html += tab_id;
  html += R"html(_tile_span_h" min="1" max=")html";
  html += String(GRID_ROWS);
  html += R"html(" step="1" value="1">
              </div>
            </div>

)html";

            TileTypeWebContext type_ctx;
            type_ctx.tab_id = &tab_id;
            type_ctx.sensor_options = &sensorOptions;
            type_ctx.energy_options = &energyOptions;
            type_ctx.weather_options = &weatherOptions;
            type_ctx.scene_options = &sceneOptions;
            type_ctx.switch_options = &switchOptions;
            type_ctx.media_options = &mediaOptions;
            type_ctx.navigate_options_html = &navigateOptionsHtml;
            append_tile_type_fields_html(html, type_ctx);

  html += R"html(
            </div><!-- /tile-settings-body -->
            <div class="tile-actions">
                <button type="button" class="btn" onclick="copyTile(')html";
  html += tab_id;
  html += R"html(')">)html";
  html += tr.admin_copy;
  html += R"html(</button>
                <button type="button" class="btn" onclick="pasteTile(')html";
  html += tab_id;
  html += R"html(')">)html";
  html += tr.admin_paste;
  html += R"html(</button>
                <button type="button" class="btn btn-danger" onclick="resetTile(')html";
  html += tab_id;
  html += R"html(')">)html";
  html += tr.admin_delete;
  html += R"html(</button>
            </div>
            </div><!-- /tile-specific-settings -->
          </div>
        </div>
      </div>
)html";
}

static String buildFolderTabButtonHtml(const FolderEntry& entry) {
  const auto& tr = i18n::strings(configManager.getConfig().language);
  String tab_id = "folder" + String(entry.id);
  String icon = String(entry.icon_name);
  String name = String(entry.name);
  icon.trim();
  icon.toLowerCase();
  if (icon.startsWith("mdi:")) icon = icon.substring(4);
  else if (icon.startsWith("mdi-")) icon = icon.substring(4);
  name.trim();
  if (!name.length()) {
    name = (entry.id == 0) ? String(tr.home) : String(tr.folder_prefix) + String(entry.id);
  }

  String html;
  html += R"html(
        <button class="tab-btn" onclick="switchTab('tab-tiles-)html";
  html += tab_id;
  html += R"html(')">)html";
  if (icon.length()) {
    html += R"html(
          <i class="mdi mdi-)html";
    html += icon;
    html += R"html(" style="font-size:24px;"></i>)html";
  }
  html += R"html(
          <span style="font-size:14px;font-weight:600;">)html";
  appendHtmlEscaped(html, name);
  html += R"html(</span>
        </button>
)html";
  return html;
}

bool buildAdminFolderTabFragments(uint16_t folder_id, String& button_html, String& tab_html, String& tab_id) {
  const FolderEntry* folder = tileConfig.getFolder(folder_id);
  if (!folder) return false;

  const HaBridgeConfigData& ha = haBridgeConfig.get();
  const auto sensorOptions = parseSensorList(ha.sensors_text);
  auto energyOptions = parseSensorList(ha.energy_text);
  energy_append_cached_entity_ids(energyOptions);
  const auto weatherOptions = parseSensorList(ha.weathers_text);
  const auto sceneOptions = parseSceneList(ha.scene_alias_text);
  const auto lightOptions = parseSensorList(ha.lights_text);
  const auto switchOptionsRaw = parseSensorList(ha.switches_text);
  const auto mediaOptions = parseSensorList(ha.media_players_text);
  std::vector<String> switchOptions;
  switchOptions.reserve(lightOptions.size() + switchOptionsRaw.size());
  auto addSwitchOption = [&](const String& entry) {
    if (!entry.length()) return;
    for (const auto& existing : switchOptions) {
      if (existing.equalsIgnoreCase(entry)) return;
    }
    switchOptions.push_back(entry);
  };
  for (const auto& opt : lightOptions) addSwitchOption(opt);
  for (const auto& opt : switchOptionsRaw) addSwitchOption(opt);
  addSwitchOption(kEntityDisplayBrightness);
  addSwitchOption(kEntityDisplayRotate);
  addSwitchOption(kEntityDisplaySleep);

  auto formatSensorValue = [](const String& raw, uint8_t decimals) -> String {
    String v = raw;
    v.trim();
    if (!v.length()) return String("--");
    String lower = v;
    lower.toLowerCase();
    if (lower == "unavailable") return String("--");
    if (decimals == 0xFF) return v;
    String normalized = v;
    normalized.replace(",", ".");
    char* end = nullptr;
    float f = strtof(normalized.c_str(), &end);
    if (!end || end == normalized.c_str()) return v;
    if (isnan(f) || isinf(f)) return v;
    uint8_t d = decimals > 6 ? 6 : decimals;
    return String(f, static_cast<unsigned int>(d));
  };

  String navigateOptionsHtml;
  for (const auto& entry : tileConfig.getFolders()) {
    if (entry.id == 0) continue;
    String label = String(entry.name);
    label.trim();
    if (!label.length()) {
      label = i18n::strings(configManager.getConfig().language).folder_prefix;
      label += String(entry.id);
    }
    navigateOptionsHtml += "<option value=\"";
    navigateOptionsHtml += String(entry.id);
    navigateOptionsHtml += "\">";
    appendHtmlEscaped(navigateOptionsHtml, label);
    navigateOptionsHtml += "</option>\n";
  }

  TileGridConfig grid{};
  tileConfig.loadFolderGrid(folder_id, grid);
  tab_id = "folder" + String(folder_id);
  button_html = buildFolderTabButtonHtml(*folder);
  tab_html = "";
  appendTileTabHTML(tab_html, folder_id, *folder, grid, sensorOptions, energyOptions, weatherOptions,
                    sceneOptions, switchOptions, mediaOptions, formatSensorValue, navigateOptionsHtml);
  return true;
}

String WebAdminServer::getAdminPage() {
  const DeviceConfig& cfg = configManager.getConfig();
  const auto& tr = i18n::strings(cfg.language);
  const bool is_german = strcmp(tr.html_lang, "de") == 0;
  const bool use_static_wifi =
      cfg.wifi_static_ip[0] || cfg.wifi_gateway[0] || cfg.wifi_subnet[0] || cfg.wifi_dns[0];
  const String admin_panel_title =
      String(Device::displayName()) + (is_german ? " Admin-Panel" : " Admin Panel");
  const String admin_heading_title = is_german ? "HomeTiles Admin-Panel" : "HomeTiles Admin Panel";
  const String admin_heading_subtitle =
      String(FW_VERSION) + "  \xC2\xB7  " + Device::displayName();
  const String current_firmware_name =
      String("hometiles_") + FW_VERSION + "_" + Device::profile().key;
  const HaBridgeConfigData& ha = haBridgeConfig.get();
  const auto sensorOptions = parseSensorList(ha.sensors_text);
  auto energyOptions = parseSensorList(ha.energy_text);
  energy_append_cached_entity_ids(energyOptions);
  const auto weatherOptions = parseSensorList(ha.weathers_text);
  const auto sceneOptions = parseSceneList(ha.scene_alias_text);
  const auto lightOptions = parseSensorList(ha.lights_text);
  const auto switchOptionsRaw = parseSensorList(ha.switches_text);
  const auto mediaOptions = parseSensorList(ha.media_players_text);
  std::vector<String> switchOptions;
  switchOptions.reserve(lightOptions.size() + switchOptionsRaw.size());
  auto addSwitchOption = [&](const String& entry) {
    if (!entry.length()) return;
    for (const auto& existing : switchOptions) {
      if (existing.equalsIgnoreCase(entry)) {
        return;
      }
    }
    switchOptions.push_back(entry);
  };
  for (const auto& opt : lightOptions) {
    addSwitchOption(opt);
  }
  for (const auto& opt : switchOptionsRaw) {
    addSwitchOption(opt);
  }
  addSwitchOption(kEntityDisplayBrightness);
  addSwitchOption(kEntityDisplayRotate);
  addSwitchOption(kEntityDisplaySleep);
  auto formatSensorValue = [](const String& raw, uint8_t decimals) -> String {
    String v = raw;
    v.trim();
    if (!v.length()) return String("--");
    String lower = v;
    lower.toLowerCase();
    if (lower == "unavailable") return String("--");
    if (decimals == 0xFF) return v;  // Keine Rundung gewünscht
    String normalized = v;
    normalized.replace(",", ".");
    char* end = nullptr;
    float f = strtof(normalized.c_str(), &end);
    if (!end || end == normalized.c_str()) return v;  // Nicht numerisch
    if (isnan(f) || isinf(f)) return v;
    uint8_t d = decimals > 6 ? 6 : decimals;
    return String(f, static_cast<unsigned int>(d));
  };

  const auto& folders = tileConfig.getFolders();
  String navigateOptionsHtml;
  for (const auto& entry : folders) {
    if (entry.id == 0) continue;
    String label = String(entry.name);
    label.trim();
    if (!label.length()) {
      label = tr.folder_prefix;
      label += String(entry.id);
    }
    navigateOptionsHtml += "<option value=\"";
    navigateOptionsHtml += String(entry.id);
    navigateOptionsHtml += "\">";
    appendHtmlEscaped(navigateOptionsHtml, label);
    navigateOptionsHtml += "</option>\n";
  }

  String html;
  html.reserve(12000);
  html += "<!DOCTYPE html>\n<html lang=\"";
  html += tr.html_lang;
  html += R"html(">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>)html";
  html += admin_panel_title;
  html += R"html(</title>
)html";

  appendAdminStyles(html);
  appendAdminScripts(html);

  html += R"html(
</head>
<body>
  <div class="wrapper">
    <div class="card">
      <div class="brand">
        <svg width="44" height="44" viewBox="0 0 48 48" xmlns="http://www.w3.org/2000/svg" aria-hidden="true">
          <rect x="4" y="4" width="17" height="17" rx="4" fill="#ffffff"/>
          <rect x="27" y="4" width="17" height="17" rx="4" fill="#ffffff"/>
          <rect x="4" y="27" width="17" height="17" rx="4" fill="#ffffff"/>
          <path d="M33 26h5v6.5h6.5v5H38V44h-5v-6.5h-6.5v-5H33z" fill="#26a69a"/>
        </svg>
        <div>
          <h1>)html";
  html += admin_heading_title;
  html += R"html(</h1>
          <div class="device">)html";
  html += admin_heading_subtitle;
  html += R"html(</div>
        </div>
        <div class="brand-links">
          <a class="brand-link" href="https://galusperes.github.io/HomeTiles/" target="_blank" rel="noopener"><i class="mdi mdi-book-open-variant"></i>Docs</a>
          <a class="brand-link" href="https://github.com/GalusPeres/HomeTiles" target="_blank" rel="noopener"><i class="mdi mdi-github"></i>GitHub</a>
        </div>
      </div>
      
      <!-- Tab Navigation -->
      <div class="tab-nav">
)html";

  for (const auto& entry : folders) {
    String tab_id = "folder" + String(entry.id);
    String icon = String(entry.icon_name);
    String name = String(entry.name);
    icon.trim();
    icon.toLowerCase();
    if (icon.startsWith("mdi:")) icon = icon.substring(4);
    else if (icon.startsWith("mdi-")) icon = icon.substring(4);
    name.trim();
    if (!name.length()) {
      name = (entry.id == 0) ? String(tr.home) : String(tr.folder_prefix) + String(entry.id);
    }

    html += R"html(
        <button class="tab-btn" onclick="switchTab('tab-tiles-)html";
    html += tab_id;
    html += R"html(')">)html";
    if (icon.length()) {
      html += R"html(
          <i class="mdi mdi-)html";
      html += icon;
      html += R"html(" style="font-size:24px;"></i>)html";
    }
    html += R"html(
          <span style="font-size:14px;font-weight:600;">)html";
    appendHtmlEscaped(html, name);
    html += R"html(</span>
        </button>
)html";
  }

  html += R"html(
        <button class="tab-btn" onclick="switchTab('tab-tiles-screensaver')">
          <i class="mdi mdi-monitor" style="font-size:24px;"></i>
          <span style="font-size:14px;font-weight:600;">Screensaver</span>
        </button>
        <button class="tab-btn" onclick="switchTab('tab-network')">
          <i class="mdi mdi-cog" style="font-size:24px;"></i>
          <span style="font-size:14px;font-weight:600;">)html";
  html += tr.tile_type_settings;
  html += R"html(</span>
        </button>
      </div>
)html";

  // Generate folder tile tabs
  for (const auto& entry : folders) {
    TileGridConfig grid{};
    tileConfig.loadFolderGrid(entry.id, grid);
    appendTileTabHTML(html, entry.id, entry, grid, sensorOptions, energyOptions, weatherOptions, sceneOptions, switchOptions, mediaOptions, formatSensorValue, navigateOptionsHtml);
  }

  FolderEntry screensaver_folder{};
  screensaver_folder.id = TileConfig::kScreensaverGridStorageId;
  screensaver_folder.parent_id = 0;
  snprintf(screensaver_folder.name, sizeof(screensaver_folder.name), "%s",
           "Screensaver");
  snprintf(screensaver_folder.icon_name, sizeof(screensaver_folder.icon_name),
           "%s", "monitor");
  appendTileTabHTML(html, TileConfig::kScreensaverGridStorageId,
                    screensaver_folder, screensaverConfig.tileGrid(),
                    sensorOptions, energyOptions, weatherOptions, sceneOptions,
                    switchOptions, mediaOptions, formatSensorValue,
                    navigateOptionsHtml, true);

#if 0  // Alte separate Slot-Vorschau; ersetzt durch den normalen Tile-Editor oben.
  html += R"html(
      <div id="tab-screensaver" class="tab-content screensaver-tab">
        <div class="screensaver-editor">
          <div class="screensaver-editor-main">
            <div class="screensaver-preview-scroll">
              <div id="screensaverPreview" class="screensaver-preview selected-background">
                <img id="screensaverPreviewImage" alt="" draggable="false">
                <div id="screensaverClock" class="screensaver-clock">
                  <div id="screensaverClockTime">--:--</div>
                  <div id="screensaverClockDate">--.--.----</div>
                </div>
                <div id="screensaverSlots" class="screensaver-slots"></div>
              </div>
            </div>
            <div class="screensaver-help">)html";
  html += is_german
              ? "Hintergrund oder Uhr anklicken und ziehen. Leere Slots sind nur im Editor sichtbar."
              : "Click and drag the background or clock. Empty slots are only visible in the editor.";
  html += R"html(</div>
          </div>

          <aside class="tile-settings screensaver-settings" id="screensaverSettings">
            <h2 id="screensaverSettingsTitle">Screensaver</h2>
            <div id="screensaverBackgroundSettings">
              <div class="screensaver-fixed-type"><label>Typ</label><input value="Screensaver" disabled></div>
              <label class="inline-checkbox"><input id="screensaverUseWallpapers" type="checkbox"> )html";
  html += is_german ? "Bilder verwenden" : "Use wallpapers";
  html += R"html(</label>
              <label class="inline-checkbox"><input id="screensaverShuffle" type="checkbox"> )html";
  html += is_german ? "Zuf&auml;llige Reihenfolge" : "Shuffle";
  html += R"html(</label>
              <div class="screensaver-wallpaper-heading">Wallpapers</div>
              <div id="screensaverWallpaperList" class="screensaver-wallpaper-list"></div>
              <div id="screensaverWallpaperControls" class="screensaver-wallpaper-controls">
                <label>)html";
  html += is_german ? "Anzeigedauer (Sekunden)" : "Duration (seconds)";
  html += R"html(</label><input id="screensaverWallpaperDuration" type="number" min="3" max="3600" value="15">
                <label>Zoom</label><input id="screensaverWallpaperZoom" type="range" min="1000" max="3000" step="25" value="1000">
                <div class="screensaver-focus-grid">
                  <label>Fokus X<input id="screensaverFocusX" type="range" min="0" max="1000" value="500"></label>
                  <label>Fokus Y<input id="screensaverFocusY" type="range" min="0" max="1000" value="500"></label>
                </div>
              </div>
              <div class="screensaver-clock-settings">
                <div class="screensaver-wallpaper-heading">Uhrzeit</div>
                <div class="clock-toggle-row">
                  <label class="inline-checkbox"><input id="screensaverShowTime" type="checkbox"> )html";
  html += is_german ? "Uhrzeit" : "Time";
  html += R"html(</label>
                  <label class="inline-checkbox"><input id="screensaverShowDate" type="checkbox"> )html";
  html += is_german ? "Datum" : "Date";
  html += R"html(</label>
                </div>
                <div class="screensaver-two-fields">
                  <label>)html";
  html += is_german ? "Zeit-Schrift" : "Time font";
  html += R"html(<select id="screensaverTimeFont"><option>20</option><option>24</option><option>28</option><option>32</option><option>40</option><option selected>48</option></select></label>
                  <label>)html";
  html += is_german ? "Datum-Schrift" : "Date font";
  html += R"html(<select id="screensaverDateFont"><option>20</option><option>24</option><option selected>28</option><option>32</option><option>40</option><option>48</option></select></label>
                  <label>)html";
  html += is_german ? "Zeitformat" : "Time format";
  html += R"html(<select id="screensaverTimeFormat"><option value="0">Auto</option><option value="1">24 h</option><option value="2">12 h</option></select></label>
                  <label>)html";
  html += is_german ? "Datumsformat" : "Date format";
  html += R"html(<select id="screensaverDateFormat"><option value="0">Auto</option><option value="1">DD.MM.YYYY</option><option value="2">MM/DD/YYYY</option><option value="3">YYYY/MM/DD</option></select></label>
                </div>
              </div>
            </div>

            <div id="screensaverSlotSettings" hidden>
              <label>Typ</label>
              <select id="screensaverSlotType"><option value="0">Empty</option><option value="1">Sensor</option><option value="2">Scene</option><option value="5">Switch</option></select>
              <label>)html";
  html += is_german ? "Titel" : "Title";
  html += R"html(</label><input id="screensaverSlotTitle" type="text">
              <label>Icon (MDI)</label><input id="screensaverSlotIcon" type="text" placeholder="thermometer, lightbulb">
              <label>)html";
  html += is_german ? "Farbe" : "Color";
  html += R"html(</label><input id="screensaverSlotColor" type="color" value="#353535">
              <label>)html";
  html += is_german ? "Hintergrund-Deckkraft" : "Background opacity";
  html += R"html(</label><input id="screensaverSlotOpacity" type="range" min="0" max="255" value="0">
              <div id="screensaverSlotEntityWrap"><label id="screensaverSlotEntityLabel">Entity</label><select id="screensaverSlotEntity"><option value=""></option></select></div>
              <div id="screensaverSensorFields" class="screensaver-two-fields">
                <label>)html";
  html += is_german ? "Einheit" : "Unit";
  html += R"html(<input id="screensaverSlotUnit" type="text"></label>
                <label>)html";
  html += is_german ? "Nachkommastellen" : "Decimals";
  html += R"html(<input id="screensaverSlotDecimals" type="number" min="-1" max="6" value="-1"></label>
              </div>
              <label id="screensaverPopupModeLabel">Popup</label><select id="screensaverPopupMode"><option value="0">Long press</option><option value="1">Short press</option></select>
              <label id="screensaverSwitchStyleLabel">Switch style</label><select id="screensaverSwitchStyle"><option value="0">Icon</option><option value="1">Toggle</option></select>
            </div>
            <div id="screensaverSaveState" class="screensaver-save-state"></div>
          </aside>
        </div>
      </div>
)html";
#endif

  html += R"html(
      <!-- Tab 3: Settings (Network/MQTT Configuration) -->
      <div id="tab-network" class="tab-content">
        <form id="admin_settings_form" action="/mqtt" method="POST">
          <div class="settings-section">
            <div class="section-title">)html";
  html += tr.display_label;
  html += R"html(</div>
            <div class="settings-grid">
              <div class="settings-full">
                <label class="settings-checkbox" for="tile_borders">
                  <input type="checkbox" id="tile_borders" name="tile_borders" onchange="setNormalTileBordersPreview(this.checked)" )html";
  if (cfg.tile_borders) {
    html += "checked";
  }
  html += R"html(>
                  <span>)html";
  html += is_german ? "Feine Kachel-Rahmen" : "Subtle tile borders";
  html += R"html(</span>
                </label>
                <div class="settings-note">)html";
  html += is_german
              ? "Gilt global fuer die normale Oberflaeche; der Screensaver besitzt eine eigene Option."
              : "Applies globally to the normal interface; the screensaver has its own option.";
  html += R"html(</div>
              </div>
            </div>
          </div>

          <div class="settings-section">
            <div class="section-title-row">
              <div class="section-title">)html";
  html += tr.admin_settings_wifi;
  html += R"html(</div>
              <div class="wifi-inline-status"><span class="wifi-inline-dot)html";
  if (WiFi.status() != WL_CONNECTED) {
    html += " off";
  }
  html += R"html("></span>)html";
  html += (WiFi.status() == WL_CONNECTED) ? tr.wifi_connected : tr.wifi_disconnected;
  if (WiFi.status() == WL_CONNECTED) {
    html += " \xC2\xB7 ";
    appendHtmlEscaped(html, WiFi.SSID());
    html += " \xC2\xB7 ";
    html += WiFi.localIP().toString();
  }
  html += R"html(</div>
            </div>
            <div class="settings-grid">
              <div>
                <label for="wifi_ssid">)html";
  html += tr.ssid_label;
  html += R"html(:</label>
                <input type="text" id="wifi_ssid" name="wifi_ssid" value=")html";
  html += cfg.wifi_ssid;
  html += R"html(">
              </div>
              <div>
                <label for="wifi_pass">)html";
  html += tr.wifi_password_label;
  html += R"html(:</label>
                <div class="password-field">
                  <input type="password" id="wifi_pass" name="wifi_pass" value=")html";
  html += cfg.wifi_pass;
  html += R"html(">
                  <button type="button" class="password-toggle" data-label-show=")html";
  html += is_german ? "Anzeigen" : "Show";
  html += R"html(" data-label-hide=")html";
  html += is_german ? "Verbergen" : "Hide";
  html += R"html(" onclick="togglePasswordVisibility('wifi_pass', this)">)html";
  html += is_german ? "Anzeigen" : "Show";
  html += R"html(</button>
                </div>
              </div>
              <div class="settings-full">
                <label class="settings-checkbox" for="wifi_use_static">
                  <input type="checkbox" id="wifi_use_static" name="wifi_use_static" onchange="toggleStaticWifiFields()" )html";
  if (use_static_wifi) {
    html += "checked";
  }
  html += R"html(>
                  <span>)html";
  html += is_german ? "Statische IP-Adresse verwenden" : "Use static IP address";
  html += R"html(</span>
                </label>
              </div>
              <div id="wifi_static_fields" class="settings-subgrid settings-full )html";
  if (!use_static_wifi) {
    html += "is-hidden";
  }
  html += R"html(">
                <div>
                <label for="wifi_static_ip">)html";
  html += tr.wifi_static_ip_label;
  html += R"html(:</label>
                <input type="text" id="wifi_static_ip" name="wifi_static_ip" value=")html";
  html += cfg.wifi_static_ip;
  html += R"html(">
                </div>
                <div>
                <label for="wifi_gateway">)html";
  html += tr.wifi_gateway_label;
  html += R"html(:</label>
                <input type="text" id="wifi_gateway" name="wifi_gateway" value=")html";
  html += cfg.wifi_gateway;
  html += R"html(">
                </div>
                <div>
                <label for="wifi_subnet">)html";
  html += tr.wifi_subnet_label;
  html += R"html(:</label>
                <input type="text" id="wifi_subnet" name="wifi_subnet" value=")html";
  html += cfg.wifi_subnet;
  html += R"html(">
                </div>
                <div>
                <label for="wifi_dns">)html";
  html += tr.wifi_dns_label;
  html += R"html(:</label>
                <input type="text" id="wifi_dns" name="wifi_dns" value=")html";
  html += cfg.wifi_dns;
  html += R"html(">
                </div>
              </div>
              <div class="settings-note settings-full">)html";
  html += tr.wifi_dhcp_hint;
  html += R"html(</div>
            </div>
          </div>

          <div class="settings-section">
            <div class="section-title">)html";
  html += tr.admin_settings_mqtt;
  html += R"html(</div>
            <div class="settings-grid">
              <div>
                <label for="mqtt_host">)html";
  html += tr.mqtt_host;
  html += R"html(:</label>
                <input type="text" id="mqtt_host" name="mqtt_host" value=")html";
  html += cfg.mqtt_host;
  html += R"html(">
              </div>
              <div>
                <label for="mqtt_port">)html";
  html += tr.mqtt_port;
  html += R"html(:</label>
                <input type="number" id="mqtt_port" name="mqtt_port" value=")html";
  html += String(cfg.mqtt_port ? cfg.mqtt_port : 1883);
  html += R"html(">
              </div>
              <div>
                <label for="mqtt_user">)html";
  html += tr.mqtt_username;
  html += R"html(:</label>
                <input type="text" id="mqtt_user" name="mqtt_user" value=")html";
  html += cfg.mqtt_user;
  html += R"html(">
              </div>
              <div>
                <label for="mqtt_pass">)html";
  html += tr.mqtt_password;
  html += R"html(:</label>
                <div class="password-field">
                  <input type="password" id="mqtt_pass" name="mqtt_pass" value=")html";
  html += cfg.mqtt_pass;
  html += R"html(">
                  <button type="button" class="password-toggle" data-label-show=")html";
  html += is_german ? "Anzeigen" : "Show";
  html += R"html(" data-label-hide=")html";
  html += is_german ? "Verbergen" : "Hide";
  html += R"html(" onclick="togglePasswordVisibility('mqtt_pass', this)">)html";
  html += is_german ? "Anzeigen" : "Show";
  html += R"html(</button>
                </div>
              </div>
              <div class="settings-full">
                <label for="mqtt_client_id">)html";
  html += tr.mqtt_client_id;
  html += R"html(:</label>
                <input type="text" id="mqtt_client_id" name="mqtt_client_id" placeholder=")html";
  html += tr.mqtt_client_id_placeholder;
  html += R"html(" value=")html";
  html += cfg.mqtt_client_id;
  html += R"html(">
                <div class="settings-note">)html";
  html += tr.mqtt_client_id_hint;
  html += R"html(</div>
              </div>
              <div>
                <label for="mqtt_base">)html";
  html += tr.mqtt_base_topic;
  html += R"html(:</label>
                <input type="text" id="mqtt_base" name="mqtt_base" value=")html";
  html += cfg.mqtt_base_topic;
  html += R"html(">
              </div>
              <div>
                <label for="ha_prefix">)html";
  html += tr.ha_prefix;
  html += R"html(:</label>
                <input type="text" id="ha_prefix" name="ha_prefix" value=")html";
  html += cfg.ha_prefix;
  html += R"html(">
              </div>
            </div>
          </div>

          <div class="settings-section">
            <div class="section-title">)html";
  html += tr.admin_settings_language;
  html += R"html(</div>
            <div class="settings-grid">
              <div>
                <label for="language">)html";
  html += tr.language_label;
  html += R"html(</label>
                <select id="language" name="language">)html";
  html += i18n::build_language_options_html(cfg.language);
  html += R"html(</select>
              </div>
              <div>
                <label for="timezone">)html";
  html += tr.timezone_label;
  html += R"html(</label>
                <select id="timezone" name="timezone">)html";
  html += buildTimezoneOptionsHtml(cfg.timezone, is_german);
  html += R"html(</select>
              </div>
              <div>
                <label for="locale_time_format">)html";
  html += tr.time_format_label;
  html += R"html(</label>
                <select id="locale_time_format" name="locale_time_format">)html";
  html += buildGlobalTimeFormatOptionsHtml(cfg.global_time_format, tr);
  html += R"html(</select>
              </div>
              <div>
                <label for="locale_date_format">)html";
  html += tr.date_format_label;
  html += R"html(</label>
                <select id="locale_date_format" name="locale_date_format">)html";
  html += buildGlobalDateFormatOptionsHtml(cfg.global_date_format, tr);
  html += R"html(</select>
              </div>
            </div>
          </div>

          <div class="settings-section">
            <div class="section-title">)html";
  html += tr.admin_settings_screenshot;
  html += R"html(</div>
            <div class="settings-grid">
              <div class="settings-full">
                <div class="settings-actions">
                  <button class="btn" type="button" onclick="createScreenshotAndDownload()">)html";
  html += tr.screenshot_create_download;
  html += R"html(</button>
                  <button class="btn btn-secondary" type="button" onclick="downloadCrashLog()">)html";
  html += is_german ? "Crash-Log herunterladen" : "Download crash log";
  html += R"html(</button>
                </div>
                <div class="settings-note">)html";
  html += tr.screenshot_saved_note;
  html += R"html(</div>)html";

  // Core-Dump nur anbieten, wenn tatsaechlich einer in der coredump-Partition
  // liegt (der Panic-Handler legt ihn bei jeder Panic automatisch ab, siehe
  // src/core/crash_log.h). crashlog.txt liegt im LittleFS und braucht den
  // eigenen Download-Button oben, weil der File Manager nur die microSD zeigt.
  if (CrashLog::hasCoreDump()) {
    const String summary = CrashLog::coreDumpSummaryLine();
    html += R"html(
                <div class="settings-note"><strong>)html";
    html += is_german ? "Gespeicherter Core-Dump" : "Stored core dump";
    html += R"html(:</strong></div>)html";
    if (summary.length()) {
      html += R"html(
                <div class="settings-note ota-version-value">)html";
      html += summary;
      html += R"html(</div>)html";
    }
    html += R"html(
                <div class="settings-actions" id="coredump_actions">
                  <button class="btn btn-secondary" type="button" onclick="window.location.href='/api/coredump'">)html";
    html += is_german ? "Core-Dump herunterladen" : "Download core dump";
    html += R"html(</button>
                  <button class="btn btn-secondary" type="button" onclick="eraseCoreDump()">)html";
    html += is_german ? "Core-Dump l&ouml;schen" : "Delete core dump";
    html += R"html(</button>
                </div>
                <div class="settings-note">)html";
    html += is_german
        ? "Der Core-Dump l&auml;sst sich am PC mit esp-coredump und dem Build-ELF zu einem vollst&auml;ndigen Stacktrace aufl&ouml;sen."
        : "Decode the core dump on a PC with esp-coredump and the build ELF to get a full stack trace.";
    html += R"html(</div>)html";
  }

  html += R"html(
              </div>
            </div>
          </div>

          <div class="settings-section">
            <div class="section-title">)html";
  html += is_german ? "Dateimanager" : "File Manager";
  html += R"html(</div>
            <div class="settings-grid file-manager">
              <div class="settings-full">
                <div class="file-manager-topbar">
                  <span id="file_manager_sd_state" class="file-manager-storage-state">)html";
  html += is_german ? "Pr&uuml;fe..." : "Checking...";
  html += R"html(</span>
                  <div class="file-manager-toolbar-group">
                    <button class="btn btn-secondary file-manager-toolbar-btn" type="button" onclick="loadFileManager()">)html";
  html += is_german ? "Aktualisieren" : "Refresh";
  html += R"html(</button>
                    <button class="btn btn-secondary file-manager-toolbar-btn file-manager-requires-sd" type="button" onclick="createFileManagerFolder()" disabled>)html";
  html += is_german ? "Neuer Ordner" : "New folder";
  html += R"html(</button>
                  </div>
                </div>
                <div class="file-manager-upload-row">
                  <button class="btn btn-secondary file-manager-toolbar-btn file-manager-requires-sd" type="button" onclick="document.getElementById('file_manager_upload').click()" disabled>)html";
  html += is_german ? "Dateien w&auml;hlen" : "Choose files";
  html += R"html(</button>
                  <button class="btn btn-secondary file-manager-toolbar-btn file-manager-requires-sd" type="button" onclick="uploadFileManagerFile()" disabled>)html";
  html += is_german ? "Hochladen" : "Upload";
  html += R"html(</button>
                  <span id="file_manager_upload_name" class="file-picker-name">)html";
  html += is_german ? "Keine Datei ausgew&auml;hlt" : "No file selected";
  html += R"html(</span>
                </div>
                <input type="file" id="file_manager_upload" multiple style="display:none" onchange="updateFileManagerUploadName(this)">
                <div class="file-manager-selection-bar">
                  <div id="file_manager_selection" class="file-manager-selection-info">)html";
  html += is_german ? "Keine Auswahl" : "No selection";
  html += R"html(</div>
                  <div class="file-manager-selection-actions">
                    <button class="btn btn-secondary file-manager-selection-btn" id="file_manager_primary_btn" type="button" onclick="openSelectedFileManagerEntry()" disabled>)html";
  html += is_german ? "&Ouml;ffnen" : "Open";
  html += R"html(</button>
                    <button class="btn btn-secondary file-manager-selection-btn" id="file_manager_rename_btn" type="button" onclick="renameSelectedFileManagerEntry()" disabled>)html";
  html += is_german ? "Umbenennen" : "Rename";
  html += R"html(</button>
                    <button class="btn btn-danger file-manager-selection-btn" id="file_manager_delete_btn" type="button" onclick="deleteSelectedFileManagerEntry()" disabled>)html";
  html += is_german ? "L&ouml;schen" : "Delete";
  html += R"html(</button>
                  </div>
                </div>
              </div>
              <div class="settings-full">
                <div id="file_manager_breadcrumb" class="file-manager-breadcrumb"></div>
                <div class="file-manager-table-wrap">
                  <table class="file-manager-table">
                    <thead>
                      <tr>
                        <th>)html";
  html += is_german ? "Name" : "Name";
  html += R"html(</th>
                        <th>)html";
  html += is_german ? "Ge&auml;ndert" : "Modified";
  html += R"html(</th>
                        <th>)html";
  html += is_german ? "Gr&ouml;&szlig;e" : "Size";
  html += R"html(</th>
                      </tr>
                    </thead>
                    <tbody id="file_manager_entries">
                      <tr><td colspan="3">)html";
  html += is_german ? "Noch nicht geladen." : "Not loaded yet.";
  html += R"html(</td></tr>
                    </tbody>
                  </table>
                </div>
                <div id="file_manager_status" class="settings-note file-manager-status"></div>
              </div>
            </div>
          </div>

          <div class="settings-section">
            <div class="section-title">)html";
  html += tr.admin_import_export;
  html += R"html(</div>
            <div class="settings-grid">
              <div class="settings-full">
                <div class="settings-actions">
                  <button type="button" class="btn" onclick="exportTilesConfig()">)html";
  html += tr.admin_export;
  html += R"html(</button>
                  <input type="file" id="settings_tile_import" accept="application/json" style="display:none" onchange="importTilesConfig('settings', this.files)">
                  <button type="button" class="btn" onclick="triggerTilesImport('settings')">)html";
  html += tr.admin_import;
  html += R"html(</button>
                </div>
                <div class="settings-note">)html";
  html += tr.admin_import_overwrite;
  html += R"html(</div>
              </div>
            </div>
          </div>

          <div class="settings-section">
            <div class="section-title">)html";
  html += tr.admin_settings_ota;
  html += R"html(</div>
            <div class="settings-grid">
              <div class="settings-full">
                <div class="settings-note"><strong>)html";
  html += tr.ota_current_version;
  html += R"html(:</strong></div>
                <div class="settings-note ota-version-value">)html";
  html += current_firmware_name;
  html += R"html(</div>
                <div class="settings-note"><strong>GitHub OTA:</strong></div>
                <div class="settings-actions ota-github-actions">
                  <button class="btn btn-go" type="button" id="ota_github_btn" onclick="checkOrInstallGithubFirmware()">)html";
  html += tr.system_check_updates_btn;
  html += R"html(</button>
                </div>
                <div id="ota_github_status" class="settings-note ota-status"></div>
                <div id="ota_github_progress" class="ota-progress is-hidden" aria-hidden="true">
                  <div id="ota_github_progress_bar" class="ota-progress-bar"></div>
                </div>
                <div class="settings-note"><strong>)html";
  html += tr.ota_firmware_file;
  html += R"html(:</strong></div>
                <input type="file" id="ota_file" accept=".bin,application/octet-stream" style="display:none" onchange="updateOtaFileName(this)">
                <div class="file-picker">
                  <button class="btn btn-secondary btn-inline" type="button" id="ota_choose_btn" onclick="document.getElementById('ota_file').click()">)html";
  html += tr.ota_choose_file;
  html += R"html(</button>
                  <button class="btn btn-go btn-inline" type="button" id="ota_upload_btn" onclick="uploadOtaFirmware()">)html";
  html += tr.ota_upload_install;
  html += R"html(</button>
                  <span id="ota_file_name" class="file-picker-name">)html";
  html += tr.ota_no_file_selected;
  html += R"html(</span>
                </div>
                <div id="ota_status" class="settings-note ota-status"></div>
                <div id="ota_progress" class="ota-progress is-hidden" aria-hidden="true">
                  <div id="ota_progress_bar" class="ota-progress-bar"></div>
                </div>
                <div class="settings-note">)html";
  html += tr.ota_update_note;
  html += R"html(</div>
              </div>
            </div>
          </div>
        </form>

        <form id="admin_restart_form" action="/restart" method="POST" onsubmit="return confirm('Geraet wirklich neu starten?');" class="admin-hidden-form"></form>
        <div class="admin-footer-actions">
          <button class="btn btn-go admin-footer-btn" type="submit" form="admin_settings_form">Speichern</button>
          <button class="btn btn-secondary admin-footer-btn" type="submit" form="admin_restart_form">Geraet neu starten</button>
        </div>
      </div>
    </div>
  </div>

  <!-- Notification Toast -->
  <div id="notification" class="notification"></div>
</body>
</html>
)html";

  html.replace(">Speichern</button>", String(">") + tr.save + "</button>");
  html.replace("return confirm('Geraet wirklich neu starten?');", String("return confirm('") + tr.restart_confirm + "');");
  html.replace(">Geraet neu starten</button>", String(">") + tr.restart_button + "</button>");

  return html;
}

String WebAdminServer::getSuccessPage() {
  const auto& tr = i18n::strings(configManager.getConfig().language);
  String html = "<!DOCTYPE html>\n<html lang=\"";
  html += tr.html_lang;
  html += R"html(">
<head>
  <meta charset="utf-8">
  <title>)html";
  html += tr.save;
  html += R"html(</title>
)html";
  appendWebFontFaceStyles(html);
  html += R"html(
  <style>
    body { font-family:'HomeTiles Inter', sans-serif; background:#0a0a0a; height:100vh; margin:0; display:flex; align-items:center; justify-content:center; }
    .box { background:#1c1c1c; border:1px solid #2a2a2a; padding:32px; border-radius:22px; box-shadow:0 20px 60px rgba(0,0,0,.5); text-align:center; }
    h1 { margin:0 0 10px; color:#ffffff; font-size:22px; }
    p { margin:0; color:#8a8a8a; }
  </style>
  <script>setTimeout(function(){window.location.href='/'},1500);</script>
</head>
<body>
  <div class="box">
    <h1>)html";
  html += tr.mqtt_saved_title;
  html += R"html(</h1>
    <p>)html";
  html += tr.mqtt_saved_message;
  html += R"html(</p>
  </div>
</body>
</html>)html";
  return html;
}

String WebAdminServer::getBridgeSuccessPage() {
  const auto& tr = i18n::strings(configManager.getConfig().language);
  String html = "<!DOCTYPE html>\n<html lang=\"";
  html += tr.html_lang;
  html += R"html(">
<head>
  <meta charset="utf-8">
  <title>)html";
  html += tr.bridge_saved_title;
  html += R"html(</title>
)html";
  appendWebFontFaceStyles(html);
  html += R"html(
  <style>
    body { font-family:'HomeTiles Inter', sans-serif; background:#0a0a0a; height:100vh; margin:0; display:flex; align-items:center; justify-content:center; }
    .box { background:#1c1c1c; border:1px solid #2a2a2a; padding:32px; border-radius:22px; box-shadow:0 20px 60px rgba(0,0,0,.5); text-align:center; }
    h1 { margin:0 0 10px; color:#ffffff; font-size:22px; }
    p { margin:0; color:#8a8a8a; }
  </style>
  <script>setTimeout(function(){window.location.href='/'},1500);</script>
</head>
<body>
  <div class="box">
    <h1>)html";
  html += tr.bridge_saved_title;
  html += R"html(</h1>
    <p>)html";
  html += tr.bridge_saved_message;
  html += R"html(</p>
  </div>
</body>
</html>)html";
  return html;
}

String WebAdminServer::getStatusJSON() {
  const DeviceConfig& cfg = configManager.getConfig();
  nvs_stats_t stats{};
  bool stats_ok = (nvs_get_stats(nullptr, &stats) == ESP_OK);

  auto get_ns_used = [](const char* ns) -> size_t {
    if (!ns) return static_cast<size_t>(-1);
    nvs_handle_t h = 0;
    if (nvs_open(ns, NVS_READONLY, &h) != ESP_OK) return static_cast<size_t>(-1);
    size_t used = 0;
    esp_err_t err = nvs_get_used_entry_count(h, &used);
    nvs_close(h);
    return (err == ESP_OK) ? used : static_cast<size_t>(-1);
  };

  size_t tiles_used = get_ns_used("tab5_tiles");
  size_t config_used = get_ns_used("tab5_config");

  String json = "{";
  json += "\"wifi_connected\":";
  json += (WiFi.status() == WL_CONNECTED) ? "true" : "false";
  json += ",\"wifi_ssid\":\"" + String(cfg.wifi_ssid) + "\"";
  json += ",\"wifi_ip\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"mqtt_host\":\"" + String(cfg.mqtt_host) + "\"";
  json += ",\"mqtt_port\":" + String(cfg.mqtt_port);
  json += ",\"mqtt_client_id\":\"" + String(cfg.mqtt_client_id) + "\"";
  json += ",\"mqtt_base\":\"" + String(cfg.mqtt_base_topic) + "\"";
  json += ",\"ha_prefix\":\"" + String(cfg.ha_prefix) + "\"";
  json += ",\"bridge_configured\":" + String(haBridgeConfig.hasData() ? "true" : "false");
  json += ",\"free_heap\":" + String(ESP.getFreeHeap());
  json += ",\"heap_total\":" + String(ESP.getHeapSize());
  json += ",\"heap_min_free\":" + String(ESP.getMinFreeHeap());
  json += ",\"psram_free\":" + String(ESP.getFreePsram());
  json += ",\"psram_total\":" + String(ESP.getPsramSize());
  json += ",\"nvs_used_entries\":" + String(stats_ok ? stats.used_entries : -1);
  json += ",\"nvs_free_entries\":" + String(stats_ok ? stats.free_entries : -1);
  json += ",\"nvs_namespace_count\":" + String(stats_ok ? stats.namespace_count : -1);
  json += ",\"nvs_tab5_tiles_used\":" + String(tiles_used);
  json += ",\"nvs_tab5_config_used\":" + String(config_used);
  json += "}";
  return json;
}
