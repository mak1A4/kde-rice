# Written by kde-rice: its theme "{{ title }}" as a
# KDE colour scheme. Rewritten at each change of theme; edit the theme, not this.

[ColorEffects:Disabled]
Color=56,56,56
ColorAmount=0
ColorEffect=0
ContrastAmount=0.65
ContrastEffect=1
IntensityAmount=0.1
IntensityEffect=2

[ColorEffects:Inactive]
ChangeSelectionColor=false
Color=112,111,110
ColorAmount=0.025
ColorEffect=2
ContrastAmount=0.1
ContrastEffect=2
Enable=false
IntensityAmount=0
IntensityEffect=0

[Colors:Button]
BackgroundAlternate={{ buttonAlternate_rgb }}
BackgroundNormal={{ surface_rgb }}
DecorationFocus={{ accent_rgb }}
DecorationHover={{ accent_rgb }}
ForegroundActive={{ accent_rgb }}
ForegroundInactive={{ fgDim_rgb }}
ForegroundLink={{ link_rgb }}
ForegroundNegative={{ error_rgb }}
ForegroundNeutral={{ warning_rgb }}
ForegroundNormal={{ fg_rgb }}
ForegroundPositive={{ positive_rgb }}
ForegroundVisited={{ visited_rgb }}

[Colors:Complementary]
BackgroundAlternate={{ complementaryBg_rgb }}
BackgroundNormal={{ complementaryBg_rgb }}
DecorationFocus={{ accent_rgb }}
DecorationHover={{ accent_rgb }}
ForegroundActive={{ accent_rgb }}
ForegroundInactive={{ complementaryDim_rgb }}
ForegroundLink={{ link_rgb }}
ForegroundNegative={{ error_rgb }}
ForegroundNeutral={{ warning_rgb }}
ForegroundNormal={{ complementaryFg_rgb }}
ForegroundPositive={{ positive_rgb }}
ForegroundVisited={{ visited_rgb }}

[Colors:Header]
BackgroundAlternate={{ view_rgb }}
BackgroundNormal={{ bg_rgb }}
DecorationFocus={{ accent_rgb }}
DecorationHover={{ accent_rgb }}
ForegroundActive={{ accent_rgb }}
ForegroundInactive={{ fgDim_rgb }}
ForegroundLink={{ link_rgb }}
ForegroundNegative={{ error_rgb }}
ForegroundNeutral={{ warning_rgb }}
ForegroundNormal={{ fg_rgb }}
ForegroundPositive={{ positive_rgb }}
ForegroundVisited={{ visited_rgb }}

[Colors:Header][Inactive]
BackgroundAlternate={{ view_rgb }}
BackgroundNormal={{ bg_rgb }}
DecorationFocus={{ accent_rgb }}
DecorationHover={{ accent_rgb }}
ForegroundActive={{ accent_rgb }}
ForegroundInactive={{ fgDim_rgb }}
ForegroundLink={{ link_rgb }}
ForegroundNegative={{ error_rgb }}
ForegroundNeutral={{ warning_rgb }}
ForegroundNormal={{ fgDim_rgb }}
ForegroundPositive={{ positive_rgb }}
ForegroundVisited={{ visited_rgb }}

[Colors:Selection]
BackgroundAlternate={{ selectionAlternate_rgb }}
BackgroundNormal={{ selection_rgb }}
DecorationFocus={{ accent_rgb }}
DecorationHover={{ accent_rgb }}
ForegroundActive={{ selectionFg_rgb }}
ForegroundInactive={{ selectionInactive_rgb }}
ForegroundLink={{ selectionFg_rgb }}
ForegroundNegative={{ selectionFg_rgb }}
ForegroundNeutral={{ selectionFg_rgb }}
ForegroundNormal={{ selectionFg_rgb }}
ForegroundPositive={{ selectionFg_rgb }}
ForegroundVisited={{ selectionFg_rgb }}

[Colors:Tooltip]
BackgroundAlternate={{ bg_rgb }}
BackgroundNormal={{ surface_rgb }}
DecorationFocus={{ accent_rgb }}
DecorationHover={{ accent_rgb }}
ForegroundActive={{ accent_rgb }}
ForegroundInactive={{ fgDim_rgb }}
ForegroundLink={{ link_rgb }}
ForegroundNegative={{ error_rgb }}
ForegroundNeutral={{ warning_rgb }}
ForegroundNormal={{ fg_rgb }}
ForegroundPositive={{ positive_rgb }}
ForegroundVisited={{ visited_rgb }}

[Colors:View]
BackgroundAlternate={{ viewAlternate_rgb }}
BackgroundNormal={{ view_rgb }}
DecorationFocus={{ accent_rgb }}
DecorationHover={{ accent_rgb }}
ForegroundActive={{ accent_rgb }}
ForegroundInactive={{ fgDim_rgb }}
ForegroundLink={{ link_rgb }}
ForegroundNegative={{ error_rgb }}
ForegroundNeutral={{ warning_rgb }}
ForegroundNormal={{ fg_rgb }}
ForegroundPositive={{ positive_rgb }}
ForegroundVisited={{ visited_rgb }}

[Colors:Window]
BackgroundAlternate={{ surface_rgb }}
BackgroundNormal={{ bg_rgb }}
DecorationFocus={{ accent_rgb }}
DecorationHover={{ accent_rgb }}
ForegroundActive={{ accent_rgb }}
ForegroundInactive={{ fgDim_rgb }}
ForegroundLink={{ link_rgb }}
ForegroundNegative={{ error_rgb }}
ForegroundNeutral={{ warning_rgb }}
ForegroundNormal={{ fg_rgb }}
ForegroundPositive={{ positive_rgb }}
ForegroundVisited={{ visited_rgb }}

[General]
ColorScheme={{ scheme }}
Name={{ title }} (kde-rice)
shadeSortColumn=true

[KDE]
contrast=4

[WM]
activeBackground={{ bg_rgb }}
activeBlend={{ fg_rgb }}
activeForeground={{ fg_rgb }}
inactiveBackground={{ bg_rgb }}
inactiveBlend={{ fgDim_rgb }}
inactiveForeground={{ fgDim_rgb }}
