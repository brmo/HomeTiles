#include "src/types/scene/web_scripts.h"

void append_scene_scripts(String& html) {
  html += R"html(
  <script>
  function maybeFillTitleFromScene(tab) {
    const prefix = tab;
    const typeSel = document.getElementById(prefix + '_tile_type');
    const titleInput = document.getElementById(prefix + '_tile_title');
    const sceneSel = document.getElementById(prefix + '_scene_alias');
    if (!typeSel || !titleInput || !sceneSel) return;
    if (typeSel.value !== '2') return;
    if (titleInput.value && titleInput.value.trim().length) return;
    const opt = sceneSel.selectedOptions && sceneSel.selectedOptions[0];
    if (!opt) return;
    const label = opt.textContent || opt.innerText || '';
    const title = label.split(' - ')[0] || opt.value || '';
    if (title.trim().length) titleInput.value = title.trim();
  }

  let sceneIconListLoaded = false;
  let sceneIconList = [];

  function refreshSceneIconSelect(tab, force) {
    if (!force && sceneIconListLoaded) {
      populateSceneIconSelect(tab, sceneIconList);
      return;
    }
    fetch('/api/sd_icons')
      .then(r => r.json())
      .then(list => {
        sceneIconList = Array.isArray(list) ? list : [];
        sceneIconListLoaded = true;
        populateSceneIconSelect(tab, sceneIconList);
      })
      .catch(() => {});
  }

  function populateSceneIconSelect(tab, list) {
    const sel = document.getElementById(tab + '_scene_icon_image');
    if (!sel) return;
    const hidden = document.getElementById(tab + '_scene_image_path');
    const savedPath = hidden ? hidden.value : '';
    sel.innerHTML = '<option value="">Kein Bild</option>';
    (list || []).forEach(p => {
      const opt = document.createElement('option');
      opt.value = p;
      opt.textContent = p;
      sel.appendChild(opt);
    });
    if (savedPath && list.includes(savedPath)) {
      sel.value = savedPath;
    }
  }

  function onSceneIconSelected(selectEl, tab) {
    const hidden = document.getElementById(tab + '_scene_image_path');
    if (hidden) hidden.value = selectEl.value;
    updateDraft(tab);
    scheduleAutoSave(tab);
  }

  function uploadSceneIcon(tab) {
    const fileInput = document.getElementById(tab + '_scene_icon_file');
    if (!fileInput || !fileInput.files.length) { alert('Keine Datei ausgewaehlt'); return; }
    const file = fileInput.files[0];
    const fd = new FormData();
    fd.append('file', file);
    fetch('/api/upload_icon', { method: 'POST', body: fd })
      .then(r => r.json())
      .then(res => {
        if (res.ok) {
          refreshSceneIconSelect(tab, true);
          setTimeout(() => {
            const hidden = document.getElementById(tab + '_scene_image_path');
            if (hidden) hidden.value = res.path;
            const sel = document.getElementById(tab + '_scene_icon_image');
            if (sel) sel.value = res.path;
            updateDraft(tab);
            scheduleAutoSave(tab);
          }, 500);
        } else {
          alert('Upload fehlgeschlagen: ' + (res.error || ''));
        }
      })
      .catch(e => alert('Upload Fehler: ' + e));
  }

  function loadSceneFields(tab, data) {
    const prefix = tab;
    const sceneEl = document.getElementById(prefix + '_scene_alias');
    if (sceneEl) sceneEl.value = data.scene_alias || '';
    const hiddenPath = document.getElementById(prefix + '_scene_image_path');
    if (hiddenPath) hiddenPath.value = data.image_path || '';
    refreshSceneIconSelect(tab, false);
    maybeFillTitleFromScene(tab);
  }

  function saveSceneFields(tab, formData) {
    const prefix = tab;
    formData.append('scene_alias', document.getElementById(prefix + '_scene_alias')?.value || '');
    formData.append('image_path', document.getElementById(prefix + '_scene_image_path')?.value || '');
  }

  function resetSceneFields(tab) {
    const prefix = tab;
    const sceneEl = document.getElementById(prefix + '_scene_alias');
    if (sceneEl) sceneEl.value = '';
    const hiddenPath = document.getElementById(prefix + '_scene_image_path');
    if (hiddenPath) hiddenPath.value = '';
    const sel = document.getElementById(prefix + '_scene_icon_image');
    if (sel) sel.value = '';
  }
  </script>
)html";
}
