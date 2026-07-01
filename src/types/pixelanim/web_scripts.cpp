#include "src/types/pixelanim/web_scripts.h"

void append_pixelanim_scripts(String& html) {
  html += R"html(
  <script>
  function loadAnimationFields(tab, data) {
    const el = document.getElementById(tab + '_animation_file');
    if (el) {
      // The chosen file name is stored in scene_alias (see firmware web_handler).
      const val = (data && (data.animation_file !== undefined ? data.animation_file
                           : data.scene_alias)) || '';
      el.value = val;
      if (el.value !== val) el.value = '';  // saved file gone from SD -> none
    }
    // Speed (fps) is stored in image_slideshow_sec for this tile type.
    const fps = document.getElementById(tab + '_animation_fps');
    if (fps) {
      let v = data && data.image_slideshow_sec ? parseInt(data.image_slideshow_sec, 10) : 10;
      if (!(v >= 1 && v <= 30)) v = 10;
      fps.value = v;
      const lbl = document.getElementById(tab + '_animation_fps_val');
      if (lbl) lbl.textContent = v;
    }
  }

  function saveAnimationFields(tab, formData) {
    const el = document.getElementById(tab + '_animation_file');
    formData.append('animation_file', el ? (el.value || '') : '');
    const fps = document.getElementById(tab + '_animation_fps');
    formData.append('animation_fps', fps ? (fps.value || '10') : '10');
  }

  function resetAnimationFields(tab) {
    const el = document.getElementById(tab + '_animation_file');
    if (el) el.value = '';
    const fps = document.getElementById(tab + '_animation_fps');
    if (fps) fps.value = 10;
    const lbl = document.getElementById(tab + '_animation_fps_val');
    if (lbl) lbl.textContent = '10';
  }
  </script>
)html";
}
