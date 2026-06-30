#pragma once

#include "src/core/display_manager.h"

namespace popup_layout {

constexpr int kCardMargin = 4;
constexpr int kCardWidth =
    (SCREEN_WIDTH > SCREEN_HEIGHT)
        ? (SCREEN_HEIGHT - (kCardMargin * 2))
        : (SCREEN_WIDTH - (kCardMargin * 2));
constexpr int kCardHeight = SCREEN_HEIGHT - (kCardMargin * 2);
constexpr int kCardPad = 20;
constexpr int kContentWidth = kCardWidth - (kCardPad * 2);

constexpr int kDesignCardHeight = 720 - (kCardMargin * 2);
constexpr int kExtraHeight = (kCardHeight > kDesignCardHeight)
                                 ? (kCardHeight - kDesignCardHeight)
                                 : 0;

constexpr int kExtraGapBase = kExtraHeight / 2;
constexpr int kExtraGapRemainder = kExtraHeight % 2;

constexpr int extra_gap(int index) {
  return kExtraGapBase + ((index >= 0 && index < kExtraGapRemainder) ? 1 : 0);
}

constexpr int extra_gap_before_value() {
  return 0;
}

constexpr int extra_gap_before_body() {
  return extra_gap(0) / 2;
}

constexpr int kHeaderY = 0;
constexpr int kHeaderHeight = 96;
constexpr int kValueBaseY = 104;
constexpr int kValueY = kValueBaseY + extra_gap_before_value();
constexpr int kValueHeight = 74;
constexpr int kBodyBaseY = 186;
constexpr int kBodyY = kBodyBaseY + extra_gap_before_body();
constexpr int kBodyHeight = 408;
constexpr int kNavHeight = 92;
constexpr int kNavBottomInset = 6;
constexpr int kNavY = kCardHeight - kNavBottomInset - kNavHeight;

}  // namespace popup_layout
