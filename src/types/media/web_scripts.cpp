#include "src/types/media/web_scripts.h"

void append_media_scripts(String& html) {
  html += R"html(
  <script>
  function maybeFillTitleFromMedia(tab) {
    maybeFillTitleFromEntity(tab, '_media_entity');
  }

  function parseMediaPreviewPayload(value) {
    const out = { title: '--', subtitle: '--', state: '--' };
    if (value === undefined || value === null) return out;
    const text = String(value).trim();
    if (!text.length) return out;
    if (text.startsWith('{')) {
      try {
        const obj = JSON.parse(text);
        if (obj && typeof obj === 'object') {
          out.title = obj.media_title || obj.media_channel || obj.state || '--';
          out.subtitle = obj.media_artist || obj.media_album_name || obj.app_name || obj.source || '--';
          out.state = obj.state || '--';
          if (obj.volume_level !== undefined && obj.volume_level !== null) {
            const pct = Math.max(0, Math.min(100, Math.round(Number(obj.volume_level) * 100)));
            out.state = (out.state && out.state !== '--') ? (out.state + '  ' + pct + '%') : (pct + '%');
          }
        }
      } catch (e) {}
      return out;
    }
    out.title = text;
    out.state = text;
    return out;
  }

  function updateMediaValuePreview(tab) {
    // Media tiles stay intentionally simple in the WebUI preview:
    // only icon and configured tile title are shown.
  }

  function loadMediaFields(tab, data) {
    const prefix = tab;
    const el = document.getElementById(prefix + '_media_entity');
    if (el) el.value = data.sensor_entity || data.media_entity || '';
    maybeFillTitleFromMedia(tab);
    updateMediaValuePreview(tab);
  }

  function saveMediaFields(tab, formData) {
    const prefix = tab;
    const entity = document.getElementById(prefix + '_media_entity')?.value || '';
    formData.append('media_entity', entity);
    formData.append('sensor_entity', entity);
  }

  function resetMediaFields(tab) {
    const prefix = tab;
    const el = document.getElementById(prefix + '_media_entity');
    if (el) el.value = '';
  }
  </script>
)html";
}
