#include "src/types/template/web_scripts.h"

void append_template_scripts(String& html) {
  html += R"html(
  <script>
  function loadTemplateFields(tab, data) {
    const prefix = tab;
    const el = document.getElementById(prefix + '_template_value');
    if (el) el.value = data.template_value || '';
  }

  function saveTemplateFields(tab, formData) {
    const prefix = tab;
    formData.append('template_value', document.getElementById(prefix + '_template_value')?.value || '');
  }

  function resetTemplateFields(tab) {
    const prefix = tab;
    const el = document.getElementById(prefix + '_template_value');
    if (el) el.value = '';
  }
  </script>
)html";
}
