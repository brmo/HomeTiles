#include "src/types/climate/web_styles.h"

void append_climate_styles(String& html) {
  html += R"html(
  <style>
    .tile.climate {
      display:grid;
      grid-template-rows:auto 1fr;
      grid-template-columns:1fr;
    }
    .tile.climate .tile-title {
      grid-column:1;
      grid-row:1;
      justify-self:end;
      align-self:start;
      width:auto;
      max-width:70%;
      text-align:right;
      overflow:hidden;
      text-overflow:ellipsis;
      white-space:nowrap;
    }
    .tile.climate .tile-icon {
      position:absolute;
      top:8px;
      left:6px;
    }
    .tile.climate .tile-value {
      position:absolute;
      left:0;
      right:0;
      top:50%;
      transform:translateY(calc(-50% + var(--value-dy, 14px)));
      margin:0;
      color:#fff;
      font-size:var(--fs28, 14px);
      font-weight:normal;
      text-align:center;
      line-height:1;
    }
    .tile.climate .climate-slots {
      position:absolute;
      left:var(--climate-margin-x, 5px);
      right:var(--climate-margin-x, 5px);
      top:var(--climate-slots-top, 35px);
      bottom:var(--climate-slots-bottom, 7px);
      display:grid;
      grid-template-columns:repeat(var(--climate-columns, 1), minmax(0, 1fr));
      grid-auto-rows:var(--climate-slot-h, 31px);
      align-content:space-between;
      column-gap:var(--climate-column-gap, 5px);
      row-gap:var(--climate-column-gap, 5px);
      pointer-events:none;
    }
    .tile.climate .climate-slot {
      min-width:0;
      min-height:var(--climate-slot-h, 31px);
      display:flex;
      align-items:center;
      justify-content:center;
      color:#fff;
      line-height:1;
      overflow:hidden;
    }
    .tile.climate .climate-slot strong {
      min-width:0;
      overflow:hidden;
      text-overflow:ellipsis;
      white-space:nowrap;
      font-size:var(--fs28, 14px);
      font-weight:400;
      text-align:center;
    }
    .tile.climate .climate-slot-mode strong {
      font-size:var(--fs20, 10px);
    }
    .tile.climate .climate-slot-control {
      display:grid;
      border:0;
      border-radius:var(--climate-control-radius, 7px);
      background:#3a3a3a;
    }
    .tile.climate .climate-slot-control span {
      font-size:var(--fs24, 12px);
      text-align:center;
    }
    .tile.climate .climate-slot-control small {
      min-width:0;
      overflow:hidden;
      color:#e0e0e0;
      font-size:var(--fs20, 10px);
      font-weight:400;
      line-height:1;
      text-align:center;
      text-overflow:ellipsis;
      white-space:nowrap;
    }
    .tile.climate .climate-slot-control-horizontal {
      grid-template-columns:
        var(--climate-control-caption-w, 48px)
        var(--climate-control-button-w, 20px)
        minmax(0, 1fr)
        var(--climate-control-button-w, 20px);
      grid-template-rows:1fr;
      padding:0 var(--climate-control-side-pad, 4px);
    }
    .tile.climate .climate-slot-control-horizontal small {
      grid-column:1;
    }
    .tile.climate .climate-slot-control-horizontal .climate-minus {
      grid-column:2;
    }
    .tile.climate .climate-slot-control-horizontal strong {
      grid-column:3;
      font-size:var(--fs28, 14px);
    }
    .tile.climate .climate-slot-control-horizontal .climate-plus {
      grid-column:4;
    }
    .tile.climate .climate-slot-control-vertical {
      width:100%;
      grid-template-columns:1fr 1fr;
      grid-template-rows:repeat(3, minmax(0, 1fr));
      padding:
        var(--climate-control-v-pad-top, 4px) 0
        var(--climate-control-v-pad-bottom, 1px);
    }
    .tile.climate .climate-slot-control-vertical small {
      grid-column:1 / span 2;
      grid-row:1;
      align-self:start;
    }
    .tile.climate .climate-slot-control-vertical strong {
      grid-column:1 / span 2;
      grid-row:2;
      align-self:center;
      font-size:var(--fs28, 14px);
    }
    .tile.climate .climate-slot-control-vertical .climate-minus {
      grid-column:1;
      grid-row:3;
    }
    .tile.climate .climate-slot-control-vertical .climate-plus {
      grid-column:2;
      grid-row:3;
    }
    .tile.climate .climate-slot-control-large {
      grid-template-columns:1fr 1fr;
      grid-template-rows:repeat(3, minmax(0, 1fr));
      padding:12px 18px;
    }
    .tile.climate .climate-slot-control-large small {
      grid-column:1 / span 2;
      grid-row:1;
      align-self:end;
    }
    .tile.climate .climate-slot-control-large strong {
      grid-column:1 / span 2;
      grid-row:2;
      align-self:center;
      font-size:var(--fs28, 14px);
    }
    .tile.climate .climate-slot-control-large .climate-minus {
      grid-column:1;
      grid-row:3;
    }
    .tile.climate .climate-slot-control-large .climate-plus {
      grid-column:2;
      grid-row:3;
    }
    .climate-content-config {
      margin-top:14px;
      padding-top:14px;
      border-top:1px solid #2d2d2d;
    }
    .climate-content-heading {
      display:block;
      margin:0 0 8px;
    }
    .climate-grid-description {
      margin:0 0 10px;
    }
    .climate-mini-editor-shell {
      padding:10px;
      border:1px solid var(--line);
      border-radius:18px;
      background:#000;
    }
    .climate-content-grid {
      --climate-editor-cell-w:128px;
      --climate-editor-cell-h:72px;
      --climate-editor-gap:7px;
      position:relative;
      display:grid;
      grid-template-columns:
        repeat(var(--climate-editor-columns, 1),
          var(--climate-editor-cell-w));
      grid-template-rows:
        repeat(var(--climate-editor-rows, 1),
          var(--climate-editor-cell-h));
      gap:var(--climate-editor-gap);
      width:fit-content;
      min-height:var(--climate-editor-cell-h);
      margin:0 auto;
      isolation:isolate;
    }
    .climate-mini-cell {
      z-index:1;
      min-width:0;
      min-height:0;
      padding:0;
      border:2px dashed rgba(38,166,154,.35);
      border-radius:11px;
      background:rgba(38,166,154,.06);
      color:rgba(255,255,255,.5);
      font-size:22px;
      cursor:pointer;
    }
    .climate-mini-cell:hover {
      border-color:rgba(38,166,154,.75);
      background:rgba(38,166,154,.12);
      color:#fff;
    }
    .climate-mini-cell.occupied {
      visibility:hidden;
      pointer-events:none;
    }
    .climate-mini-cell.hidden {
      display:none;
    }
    .climate-mini-tile.tile {
      z-index:2;
      position:relative;
      min-width:0;
      min-height:0;
      padding:7px 8px;
      background:#2a2a2a;
      cursor:grab;
      user-select:none;
      touch-action:none;
    }
    .climate-mini-tile.tile.hidden {
      display:none;
    }
    .climate-mini-tile.tile.dragging {
      opacity:.12;
    }
    .climate-mini-tile.tile.invalid-drop {
      border-color:#ef4444;
      box-shadow:0 0 0 2px rgba(239,68,68,.18) inset;
    }
    .climate-mini-preview {
      position:absolute;
      inset:7px 8px;
      display:flex;
      flex-direction:column;
      align-items:center;
      justify-content:center;
      gap:5px;
      min-width:0;
      color:#fff;
      text-align:center;
      pointer-events:none;
    }
    .climate-mini-preview small,
    .climate-mini-preview strong {
      display:block;
      max-width:100%;
      overflow:hidden;
      text-overflow:ellipsis;
      white-space:nowrap;
    }
    .climate-mini-preview small {
      color:#d8d8d8;
      font-size:11px;
      font-weight:400;
    }
    .climate-mini-preview strong {
      font-size:16px;
      font-weight:400;
    }
    .climate-mini-preview .climate-mini-control {
      width:100%;
      display:grid;
      grid-template-columns:24px minmax(0,1fr) 24px;
      align-items:center;
      column-gap:4px;
    }
    .climate-mini-preview .climate-mini-control span {
      font-size:16px;
    }
    .climate-mini-tile.tile.active {
      z-index:4;
    }
    .climate-mini-tile.tile .tile-resize-handle {
      z-index:5;
    }
    .climate-slot-storage,
    .climate-layout-storage {
      display:none !important;
    }
    .climate-selected-fields {
      margin-top:8px;
    }
    .climate-selected-fields label {
      display:block;
      margin-bottom:6px;
    }
    .climate-content-hint {
      margin-top:8px;
    }
    @media (max-width:1100px) {
      .climate-content-grid {
        --climate-editor-cell-w:112px;
        --climate-editor-cell-h:64px;
      }
    }
  </style>
)html";
}
