-- Written by kde-rice: its theme "{{ title }}",
-- as WezTerm's `colors`.
return {
  foreground = "{{ terminalForeground }}",
  background = "{{ terminalBackground }}",
  cursor_bg = "{{ terminalCursor }}",
  cursor_fg = "{{ terminalBackground }}",
  cursor_border = "{{ terminalCursor }}",
  selection_bg = "{{ terminalSelection }}",
  selection_fg = "{{ terminalForeground }}",
  ansi = { "{{ terminal0 }}", "{{ terminal1 }}", "{{ terminal2 }}", "{{ terminal3 }}", "{{ terminal4 }}", "{{ terminal5 }}", "{{ terminal6 }}", "{{ terminal7 }}" },
  brights = { "{{ terminal8 }}", "{{ terminal9 }}", "{{ terminal10 }}", "{{ terminal11 }}", "{{ terminal12 }}", "{{ terminal13 }}", "{{ terminal14 }}", "{{ terminal15 }}" },
  tab_bar = {
    background = "{{ bg }}",
    active_tab = { bg_color = "{{ accent }}", fg_color = "{{ accentFg }}", intensity = "Bold" },
    inactive_tab = { bg_color = "{{ surface }}", fg_color = "{{ fgDim }}" },
    inactive_tab_hover = { bg_color = "{{ surfaceHover }}", fg_color = "{{ fg }}", italic = true },
    new_tab = { bg_color = "{{ surface }}", fg_color = "{{ fgDim }}" },
    new_tab_hover = { bg_color = "{{ accent }}", fg_color = "{{ accentFg }}" },
  },
}
