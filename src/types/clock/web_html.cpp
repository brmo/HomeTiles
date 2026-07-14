#include "src/types/clock/web_html.h"

#include <FS.h>
#include <algorithm>
#include <vector>

#include "src/core/config_manager.h"
#include "src/core/i18n.h"
#include "src/devices/device.h"
#include "src/web/web_admin_utils.h"

namespace {

// Basename eines Verzeichniseintrags (die SD-FS kann volle Pfade liefern).
String wallpaper_base_name(const String& raw) {
  int slash = raw.lastIndexOf('/');
  return (slash >= 0) ? raw.substring(slash + 1) : raw;
}

bool ends_with_jpeg(const String& name) {
  String lower = name;
  lower.toLowerCase();
  return lower.endsWith(".jpg") || lower.endsWith(".jpeg");
}

// Scannt /wallpapers auf der SD-Karte und liefert die JPEG-Dateinamen
// (basename, sortiert). Gleiches Iterationsmuster wie beim Animation-Tile:
// openNextFile() direkt, ohne isDirectory()-Gate auf dem geoeffneten Ordner
// (liefert auf der WaveshareSDMMCFS false).
std::vector<String> list_wallpapers() {
  std::vector<String> names;
  if (!Device::sdReady()) return names;

  fs::File root = Device::sdFS().open("/wallpapers");
  if (!root) return names;

  for (fs::File entry = root.openNextFile(); entry; entry = root.openNextFile()) {
    if (!entry.isDirectory()) {
      const char* raw = entry.name();
      String name = wallpaper_base_name(raw ? String(raw) : String());
      if (name.length() && ends_with_jpeg(name)) names.push_back(name);
    }
  }

  std::sort(names.begin(), names.end(), [](const String& a, const String& b) {
    return a.compareTo(b) < 0;
  });
  return names;
}

}  // namespace

void append_clock_fields_html(String& html, const String& tab_id) {
  const auto& tr = i18n::strings(configManager.getConfig().language);
  html += R"html(
            <!-- Clock Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_clock_fields" class="type-fields">
              <div class="clock-toggle-row">
              <label class="inline-checkbox">
                <input type="checkbox" id=")html";
  html += tab_id;
  html += R"html(_clock_show_time" checked>
                )html";
  html += tr.show_time;
  html += R"html(
              </label>
              <label class="inline-checkbox">
                <input type="checkbox" id=")html";
  html += tab_id;
  html += R"html(_clock_show_date">
                )html";
  html += tr.show_date;
  html += R"html(
              </label>
              </div>
              <label>)html";
  html += tr.time_font_size;
  html += R"html(</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_clock_time_font">
                <option value="40">40 (Default)</option>
                <option value="20">20</option>
                <option value="24">24</option>
                <option value="28">28</option>
                <option value="32">32</option>
                <option value="48">48</option>
              </select>
              <label>)html";
  html += tr.date_font_size;
  html += R"html(</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_clock_date_font">
                <option value="20">20 (Default)</option>
                <option value="24">24</option>
                <option value="28">28</option>
                <option value="32">32</option>
                <option value="40">40</option>
                <option value="48">48</option>
              </select>
              <label>)html";
  html += tr.time_format_label;
  html += R"html(</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_clock_time_format">
                <option value="0">)html";
  html += tr.format_auto_localization;
  html += R"html(</option>
                <option value="1">)html";
  html += tr.format_24_hour;
  html += R"html(</option>
                <option value="2">)html";
  html += tr.format_12_hour;
  html += R"html(</option>
              </select>
              <label>)html";
  html += tr.date_format_label;
  html += R"html(</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_clock_date_format">
                <option value="0">)html";
  html += tr.format_auto_localization;
  html += R"html(</option>
                <option value="1">DD.MM.YYYY</option>
                <option value="2">MM/DD/YYYY</option>
                <option value="3">YYYY/MM/DD</option>
              </select>
              <label>)html";
  html += tr.clock_wallpaper_label;
  html += R"html(</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_clock_wallpaper">
                <option value="">)html";
  html += tr.no_selection;
  html += R"html(</option>
)html";

  const std::vector<String> wallpapers = list_wallpapers();
  for (const auto& name : wallpapers) {
    html += "<option value=\"";
    appendHtmlEscaped(html, name);
    html += "\">";
    appendHtmlEscaped(html, name);
    html += "</option>";
  }

  html += R"html(
              </select>
              <div style="font-size:11px;color:#8a8a8a;margin-top:4px;">)html";
  html += tr.clock_wallpaper_hint;
  html += R"html(</div>
            </div>
)html";
}

