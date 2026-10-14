# Written by kde-rice: its theme "{{ title }}".
set -g status-style 'bg={{ terminalBackground }},fg={{ fgDim }}'
set -g message-style 'bg={{ surface }},fg={{ fg }},bold'
setw -g window-status-current-style 'fg={{ accent }},bold'
setw -g window-status-style 'fg={{ fgDim }},dim'
set -g pane-border-style 'fg={{ surfaceHover }}'
set -g pane-active-border-style 'fg={{ accent }}'
set -g pane-scrollbars-style 'bg={{ surface }},fg={{ surfaceActive }}'
