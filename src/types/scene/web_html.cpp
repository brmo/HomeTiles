#include "src/types/scene/web_html.h"

void append_scene_fields_html(String& html, const String& tab_id, const std::vector<SceneOption>& sceneOptions) {
  html += R"html(
            <!-- Scene Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_scene_fields" class="type-fields">
              <label>Szene</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_scene_alias">
                <option value="">Keine Auswahl</option>
)html";

  for (const auto& opt : sceneOptions) {
    html += "<option value=\"";
    appendHtmlEscaped(html, opt.alias);
    html += "\">";
    String label = humanizeIdentifier(opt.alias, false) + " - " + opt.entity;
    appendHtmlEscaped(html, label);
    html += "</option>";
  }

  html += R"html(
              </select>
              <label style="margin-top:10px">Icon-Bild (JPEG/PNG auf SD)</label>
              <small style="display:block;opacity:0.7;margin-top:4px">Empfohlen: 64x64 px</small>
              <select id=")html";
  html += tab_id;
  html += R"html(_scene_icon_image" onchange="onSceneIconSelected(this, ')html";
  html += tab_id;
  html += R"html(')">
                <option value="">Kein Bild</option>
              </select>
              <input type="hidden" id=")html";
  html += tab_id;
  html += R"html(_scene_image_path" value="">
              <div style="margin-top:8px">
                <input type="file" id=")html";
  html += tab_id;
  html += R"html(_scene_icon_file" accept=".jpg,.jpeg,.png">
                <button type="button" onclick="uploadSceneIcon(')html";
  html += tab_id;
  html += R"html(')">Hochladen</button>
              </div>
            </div>
)html";
}
