#pragma once

namespace climate_layout {

// One shared mini-grid rectangle for every climate tile size.
inline constexpr int kCardPaddingHorizontal = 20;
inline constexpr int kCardPaddingVertical = 24;
inline constexpr int kOuterInset = 6;
inline constexpr int kContentTop = 69;
inline constexpr int kGap = 10;
inline constexpr int kControlRadius = 16;

inline constexpr int kContentTopInPaddedCard =
    kContentTop - kCardPaddingVertical;

}  // namespace climate_layout
