/* Written by kde-rice: its theme "{{ title }}".
 *
 * The menus of Chromium and of every Electron application (VS Code, Teams,
 * T3 Code). They draw them themselves, and take from GTK 3 only how the
 * entry under the pointer and the separators look, and the colours: the
 * size of an entry, its font and the menu's own frame, with the 12px it
 * keeps free above the first entry and below the last, are Chromium's. The
 * colours are the ones KDE writes for its Breeze theme (colors.css).
 *
 * Chromium names itself in what it asks GTK for (the class chromium), so
 * these rules are for its menus alone. The menus GTK applications draw
 * themselves keep Breeze's measures, and at the end of this file lose only
 * the outline of the entry under the pointer, for the same fill.
 *
 * Breeze outlines the entry under the pointer, and Chromium paints an entry
 * without the menu's padding and without a margin: the outline from one edge
 * of the menu to the other, up against its round corners. Here the entry
 * under the pointer keeps as far from the sides as Chromium keeps the
 * entries from the top and the bottom. It is the entry's own border,
 * transparent and left out of the fill, that holds the fill off; the corners
 * are given 12px more across than down, to be round inside that.
 *
 * No outline: Chromium has GTK paint at one pixel for a logical one and
 * stretches that to the screen's scale, which at 200 % makes steps of a round
 * outline. In a fill of the selection's colour they are not seen. */

menuitem.chromium {
    border: solid transparent;
    border-width: 0 12px;
}

menuitem.chromium:hover {
    background-color: @theme_selected_bg_color;
    background-clip: padding-box;
    border: solid transparent;
    border-width: 0 12px;
    border-radius: 17px / 5px;
    box-shadow: none;
    color: @theme_fg_color;
}

separator.chromium {
    background-color: alpha(@theme_fg_color, 0.1);
    min-height: 1px;
    margin: 0 12px;
}

/* GTK's own menus: Breeze's entry under the pointer, a fill without its
 * outline (the border stays, transparent: nothing moves) */
menu menuitem:not(.chromium):hover,
.menu menuitem:not(.chromium):hover {
    background-color: @theme_selected_bg_color;
    border-color: transparent;
}
