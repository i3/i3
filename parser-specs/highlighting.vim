set filetype=i3cmd
syntax case match
syntax clear

syntax keyword i3specStatement state call
highlight link i3specStatement Statement

syntax match i3specComment /#.*/
highlight link i3specComment Comment

syntax region i3specLiteral start=/'/ end=/'/
syntax keyword i3specToken string word number end
highlight link i3specLiteral String
highlight link i3specToken String

syntax match i3specState /[A-Z_]\{3,}/
highlight link i3specState PreProc

syntax match i3specSpecial /[->]/
highlight link i3specSpecial Special
