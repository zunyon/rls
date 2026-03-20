## Last Update:
## my-last-update-time "2025, 07/13 08:35"


## ================================================================================
## 共通
function __fish_rls_farg_completions
	set -l token (commandline -ct)
	set -l partial (string replace -r '^-' '' -- $token)

	## 候補とその説明の対応表
	set -l keys s l u b e a o n d O t i r m z N mm zz NN
	set -l descs \
		"short list format (no kind, one color)." \
		"long list format." \
		"deep unique word check." \
		"beginning of file name." \
		"paint unique group-name word." \
		"show all dot files." \
		"show only directories." \
		"no color." \
		"use default colors." \
		"no sort order." \
		"human-readable date." \
		"human-readable size." \
		"show aggregate results." \
		"mtime sort." \
		"size sort." \
		"alphabet sort." \
		"reverse mtime sort." \
		"reverse size sort." \
		"reverse alphabet."

	for idx in (seq (count $keys))
		set key $keys[$idx]
		set desc $descs[$idx]

		## 入力済みの文字は候補に出さない
	   if not string match -q "*$key*" "$partial"
			echo "$key	$desc"
	   end
	end
end


## ================================================================================
## Listing format
complete -c rls -s s -a '(__fish_rls_farg_completions)' -d 'short list format (no kind, one color)'
complete -c rls -s l -a '(__fish_rls_farg_completions)' -d 'long list format'
complete -c rls -s f -a '(__fish_rls_-ffarg_completions)' -d 'change format orders' -x

## -f
function __fish_rls_-ffarg_completions
	set -l token (commandline -ct)
	set -l partial (string replace -r '^-f' '' -- $token)

	## 候補とその説明の対応表
	## !! 大文字の説明
	set -l keys m o g c d p n k l e i h s t w "[" "]"
	set -l descs "mode" "owner" "group" "count entry of DIR and FILE" "date" "PATH/FILE format" "name" "kind" "linkname" "errno" "inode" "hardlinks" "size of DIR and FILE" "time" "week" "enclosing [" "enclosing ]"

	for idx in (seq (count $keys))
		set key $keys[$idx]
		set desc $descs[$idx]

		## 本来は複数表示は可能だが、入力済みの文字は候補に出さない
		if not string match -q "*$key*" "$partial"
			echo "$partial$key	$desc"
		end
	end
end


## Coloring algorithm
complete -c rls -s u -a '(__fish_rls_farg_completions)' -d 'deep unique word check'
complete -c rls -s b -a '(__fish_rls_farg_completions)' -d 'beginning of file name'
## complete -c rls -s p -d '-pxxx:paint the matched string.' -x 
complete -c rls -s p -a '(__fish_rls_-pflag_completions)' -d '-pxxx:paint the matched string.' -x
complete -c rls -s e -a '(__fish_rls_farg_completions)' -d 'paint unique group-name word'

## -p
function __fish_rls_-pflag_completions
	set -l token (commandline -ct)
	set -l partial (string replace -r '^-p' '' -- $token)

	## 使いそうなコマンド結果を登録
	set -l wami (whoami)
	set -l hname (hostname)
	set -l month (date +%b)
	set -l group (id -g)

	## 候補とその説明の対応表、だいたい -l があると良さげ
	set -l keys $wami $hname 'rwxrwxrwx' $month $group
	set -l descs "whoami" "hostname" "all bit" "month whith -l" "group"

	for idx in (seq (count $keys))
		set key $keys[$idx]
		set desc $descs[$idx]

		## 入力済みの文字は候補に出さない
		if not string match -q "*$key*" "$partial"
			echo "$partial$key	$desc"
		end
	end
end


## Output data
complete -c rls -s a -a '(__fish_rls_farg_completions)' -d 'show all dot files'
complete -c rls -s o -a '(__fish_rls_farg_completions)' -d 'show only directories'
complete -c rls -s P -d '-Pxxx:pickup only the string matched.' -x


## Color
complete -c rls -s n -a '(__fish_rls_farg_completions)' -d 'no colors'
complete -c rls -s c -a '(__fish_rls_-cfarg_completions)' -d 'set colors' -x
complete -c rls -s d -a '(__fish_rls_farg_completions)' -d 'use default colors'

## -c
function __fish_rls_-cfarg_completions
	set -l token (commandline -ct)
	set -l partial (string replace -r '^-c' '' -- $token)

	## 候補とその説明の対応表
	## !! 8 colors, 256 colors の説明、、、fore:3,000 back:4,000
	## !! : のところまでが補完対象
	set -l keys base= normal= dir= fifo= socket= device= error= paint= reset=0
	set -l descs "base" "normal file" "directories" "FIFO/pipe" "socket" "block/character device" "error" "paint matched string" "RESET"

	for idx in (seq (count $keys))
		set key $keys[$idx]
		set desc $descs[$idx]

		## 本来は複数の属性を設定できるが、入力済みの文字は候補に出さない
	   if not string match -q "*$key*" "$partial"
			echo "$partial$key	$desc"
	   end
	end
end


## Sort
complete -c rls -s m  -a '(__fish_rls_farg_completions)' -x -d 'mtime sort'
complete -c rls -s z  -a '(__fish_rls_farg_completions)' -x -d 'size sort'
complete -c rls -s N  -a '(__fish_rls_farg_completions)' -x -d 'alphabet sort'
# !! これは始めは無くて良いか
## complete -c rls -s mm  -a '(__fish_rls_farg_completions)' -x -d 'reverse mtime sort' 
## complete -c rls -s zz  -a '(__fish_rls_farg_completions)' -x -d 'reverse size sort' 
## complete -c rls -s NN  -a '(__fish_rls_farg_completions)' -x -d 'reverse alphabet sort' 
complete -c rls -s O -a '(__fish_rls_farg_completions)' -d 'no sort order'


## Additional
complete -c rls -s t -a '(__fish_rls_farg_completions)' -d 'human-readable date'
complete -c rls -s i -a '(__fish_rls_farg_completions)' -d 'human-readable size'
complete -c rls -s r -a '(__fish_rls_farg_completions)' -d 'show aggregate results'


## Other
complete -c rls -s h -l help -x -d 'show usage.'
complete -c rls -s v -l version -x -d 'show version.'
# !! rls -alr /usTAB などで、/usr/include/ が出てこない
## complete -c rls -a '-TB' -d 'enclosing string.(TB)' -x 
## complete -c rls -a '-TE' -d 'enclosing string.(TE)' -x 
## complete -c rls -a '-always' -d 'output the escape sequence characters.' -x 
## complete -c rls -a '-256' -d 'color text output.' 
