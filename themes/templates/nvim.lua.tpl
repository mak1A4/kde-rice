-- Written by kde-rice: its theme "{{ title }}".
-- Run at Neovim's start, and again in every running Neovim when the theme
-- changes. A colour scheme that is not installed leaves the one there is.
vim.o.background = "{{ mode }}"
pcall(vim.cmd.colorscheme, "{{ nvim }}")
