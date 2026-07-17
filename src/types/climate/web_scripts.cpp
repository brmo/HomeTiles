#include "src/types/climate/web_scripts.h"

void append_climate_scripts(String& html) {
  html += R"html(
  <script>
  function loadClimateFields(tab, data) {
    const entity = document.getElementById(tab + '_climate_entity');
    if (entity) entity.value = data.sensor_entity || data.climate_entity || '';
    const popup = document.getElementById(tab + '_climate_popup_open_mode');
    if (popup) popup.value = (data.popup_open_mode !== undefined)
      ? String(data.popup_open_mode) : '1';
    maybeFillTitleFromEntity(tab, '_climate_entity');
  }

  function parseClimatePreviewPayload(value) {
    const out = { current: '--', unit: '°C', mode: '', action: '' };
    if (value === undefined || value === null) return out;
    const text = String(value).trim();
    if (!text.length || !text.startsWith('{')) return out;
    try {
      const obj = JSON.parse(text);
      if (!obj || typeof obj !== 'object') return out;
      const attrs = obj.attributes && typeof obj.attributes === 'object'
        ? obj.attributes : obj;
      const current = attrs.current_temperature;
      if (current !== undefined && current !== null && Number.isFinite(Number(current))) {
        out.current = Number(current).toFixed(1).replace(/\.0$/, '');
      }
      out.unit = attrs.temperature_unit || attrs.unit_of_measurement || '°C';
      out.mode = String(obj.hvac_mode || obj.state || attrs.hvac_mode || '');
      out.action = String(obj.hvac_action || attrs.hvac_action || '');
    } catch (e) {}
    return out;
  }

  function saveClimateFields(tab, formData) {
    formData.append('climate_entity',
      document.getElementById(tab + '_climate_entity')?.value || '');
    formData.append('popup_open_mode',
      document.getElementById(tab + '_climate_popup_open_mode')?.value || '1');
  }

  function resetClimateFields(tab) {
    const entity = document.getElementById(tab + '_climate_entity');
    if (entity) entity.value = '';
    const popup = document.getElementById(tab + '_climate_popup_open_mode');
    if (popup) popup.value = '1';
  }
  </script>
)html";
}
