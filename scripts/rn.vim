" Revolution-Now-specific vim configuration.
set sw=2
set shiftwidth=2
set tabstop=2

" Sourcing other vim scripts in currnet folder.
" 1: Get the absolute path of this script.
" 2: Resolve all symbolic links.
" 3: Get the folder of the resolved absolute file.
let s:path = fnamemodify(resolve(expand('<sfile>:p')), ':h')
"exec 'source ' . s:path . '/another.vim'

" This is so that the RNL syntax file gets picked up.
let &runtimepath .= ',' . s:path . '/../src/rnl'

" This makes the design doc look nicer because it typically con-
" tains snippets of code.
au BufNewFile,BufRead *design.txt set syntax=cpp11
" jsav files are not yaml, but it seems to work nicely.
au BufNewFile,BufRead *.jsav set syntax=yaml
au BufNewFile,BufRead *.rnl set filetype=rnl

"function! CloseTerminal()
"		let s:term_buf_name = bufname( "*bin/fish*" )
"    if s:term_buf_name != ""
"        let bnr = bufwinnr( s:term_buf_name )
"        if bnr > 0
"            :exe bnr . "wincmd w"
"            call feedkeys( "\<C-D>" )
"        endif
"    endif
"endfunction

"function CloseTerminal()
"  call feedkeys( ":tabn 1\<CR>\<C-W>\<C-T>\<C-W>l\<C-W>l\<C-D>\<C-W>\<C-T>" )
"  call feedkeys( ":qa\<CR>" )
"endfunction

"nnoremap Q :call CloseTerminal()<CR>

function! MyTabLine()
  let s = ''
  for i in range(tabpagenr('$'))
    " select the highlighting
    if i + 1 == tabpagenr()
      let s .= '%#TabLineSel#'
    else
      let s .= '%#TabLine#'
    endif

    " set the tab page number (for mouse clicks)
    let s .= '%' . (i + 1) . 'T'

    " the label is made by MyTabLabel()
    let s .= ' %{MyTabLabel(' . (i + 1) . ')} '
  endfor

  " after the last tab fill with TabLineFill and reset tab page nr
  let s .= '%#TabLineFill#%T'

  " right-align the label to close the current tab page
  if tabpagenr('$') > 1
    let s .= '%=%#TabLine#%999XX'
  endif

  return s
endfunction

function! MyTabLabel( n )
  let buflist = tabpagebuflist( a:n )
  let winnr = tabpagewinnr( a:n, '$' )
  for i in range( winnr )
    let path = bufname( buflist[i] )
    let ext = fnamemodify( path, ':e' )
    if ext == 'cpp' || ext == 'hpp'
      return fnamemodify( path, ':t:r' )
    endif
    if ext == 'ucl'
      return fnamemodify( 'ucl', ':t:r' )
    endif
    if ext == 'txt'
      return fnamemodify( 'docs', ':t:r' )
    endif
    if ext == 'lua'
      let path = fnamemodify( path, ':t:r' )
      return 'lua/' . path
    endif
  endfor
  let winnr = tabpagewinnr( a:n )
  return bufname( buflist[winnr - 1] )
endfunction

set tabline=%!MyTabLine()

" Automatically format the C++ source files just before saving.
autocmd BufWritePre *.hpp,*.cpp call ClangFormatAll()

" Get the folder in which this file resides.
let s:path = expand( '<sfile>:p:h' )

" We set this ycm global variable to point YCM to the conf script.  The
" reason we don't just put a .ycm_extra_conf.py in the root folder
" (which YCM would then find on its own without our help) is that we
" want to keep the folder structure organized with all scripts in the
" scripts folder.
let g:ycm_global_ycm_extra_conf = s:path . '/scripts/ycm_extra_conf.py'

" Tell the vim-templates function where to find the templates.
let g:tmpl_search_paths = [s:path . '/scripts/templates']
