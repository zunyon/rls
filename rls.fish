## Last Update:
## my-last-update-time "2026, 03/29 14:03"

## ================================================================================
## 共通
function __fish_rls_register_options
	set -l keys s l S d u b e a o O i r
	set -l descs \
		"short list format (no kind, one color)" \
		"long list format" \
		"no sort order" \
		"use default colors" \
		"deep unique word check" \
		"beginning of file name" \
		"paint unique group-name word" \
		"show all dot files" \
		"show only directories" \
		"show only files" \
		"human-readable size" \
		"show aggregate results"

	for idx in (seq (count $keys))
		set key	 $keys[$idx]
		set desc $descs[$idx]

		complete -c rls -s $key -d "$desc"
	end
end


## ================================================================================
## Listing format
complete -c rls -s s -n "not __fish_seen_argument -s s" -d "short list format (no kind, one color)"
complete -c rls -s l -n "not __fish_seen_argument -s l" -d "long list format"
complete -c rls -s f -a '(__fish_rls_-ffarg_completions)' -d 'change format orders.' -x
complete -c rls -s J -a '(__fish_rls_-Ffarg_completions)' -d 'set jot with format order items.' -x
complete -c rls -s j -d 'JSON format' -x

## -f
function __fish_rls_-ffarg_completions
	set -l token (commandline -ct)
	set -l partial (string replace -r '^-f' '' -- $token)

	## 候補とその説明の対応表
	set -l keys m o g c C d D p n k l e i I h s S t T w W u x j "[" "]" "|" ","
	set -l descs "mode" "owner" "group" "count entry of DIR and FILE" "count (no comma)" "date" "date (English)" "path" "name" "kind" "linkname" "errno" "inode" "inode (no comma)" "hardlinks" "size" "size (no comma)" "time" "time (human-readable)" "week" "week (English)" "unique word" "extension" "jot" "enclosing [" "enclosing ]" "separator |" "separator ,"

	for idx in (seq (count $keys))
		set key $keys[$idx]
		set desc $descs[$idx]

		## 本来は複数表示は可能だが、入力済みの文字は候補に出さない
		if not string match -q "*$key*" "$partial"
			echo "$partial$key	$desc"
		end
	end
end

## 殆ど同じだから -F, -j 共通
function __fish_rls_-Ffarg_completions
	set -l token (commandline -ct)
	set -l partial (string replace -r '^-j' '' -- $token)
	set -l partial (string replace -r '^-F' '' -- $partial)

	## 候補とその説明の対応表
	set -l keys m o g c d n k l e p i h s t w u x
	set -l descs "mode" "owner" "group" "count" "date" "name" "kind" "linkname" "errno" "path" "inode" "hardlinks" "size" "time" "week" "unique" "extension"

	for idx in (seq (count $keys))
		set key $keys[$idx]
		set desc $descs[$idx]

		## 入力済みの文字は候補に出さない
		if not string match -q "*$key*" "$partial"
			echo "$partial$key	$desc"
		end
	end
end


## --------------------------------------------------------------------------------
## Sort
complete -c rls -s F -a '(__fish_rls_-Ffarg_completions)' -d 'change the sort order.' -x
complete -c rls -s S -n "not __fish_seen_argument -s S" -d "no sort order"

## -n
function __fish_rls_-nfarg_completions
	set -l token (commandline -ct)
	set -l partial (string replace -r '^-n' '' -- $token)

	## 候補とその説明の対応表
	set -l keys n n\| n\<\>
	set -l descs "[unique word]." "|unique word|." "<unique word>."

	for idx in (seq (count $keys))
		set key $keys[$idx]
		set desc $descs[$idx]

		## 入力済みの文字は候補に出さない
		if not string match -q "*$key*" "$partial"
			echo "$partial$key	$desc"
		end
	end
end


## --------------------------------------------------------------------------------
## Color
complete -c rls -s n -a '(__fish_rls_-nfarg_completions)' -d 'no colors.' -x
complete -c rls -s d -n "not __fish_seen_argument -s d" -d "use default colors."
complete -c rls -s c -a '(__fish_rls_-cfarg_completions)' -d 'set colors.' -x

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


## --------------------------------------------------------------------------------
## Coloring algorithm
complete -c rls -s u -n "not __fish_seen_argument -s u" -d "deep unique word check"
complete -c rls -s b -n "not __fish_seen_argument -s b" -d "beginning of file name"
complete -c rls -s p -a '(__fish_rls_-pflag_completions)' -d '-pxxx:paint the matched string.' -x
complete -c rls -s e -n "not __fish_seen_argument -s e" -d "paint unique group-name word"

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


## --------------------------------------------------------------------------------
## Output data
complete -c rls -s a -n "not __fish_seen_argument -s a" -d "show all dot files"
complete -c rls -s o -n "not __fish_seen_argument -s o" -d "show only directories"
complete -c rls -s O -n "not __fish_seen_argument -s O" -d "show only files"
complete -c rls -s P -d '-Pxxx:pickup only the string matched.' -x


## --------------------------------------------------------------------------------
## Additional
complete -c rls -s i -n "not __fish_seen_argument -s i" -d "human-readable size"
complete -c rls -s r -n "not __fish_seen_argument -s r" -d "show aggregate results"
complete -c rls -s R -d '-Rxxx:color the corresponding length of the aggregate results.' -x


## --------------------------------------------------------------------------------
## Other
complete -c rls -s h -l help -n "not __fish_seen_argument -s h" -d "show usage."
complete -c rls -s v -l version -n "not __fish_seen_argument -s v" -d "show version."
complete -c rls -a '--color=auto\tautomatic --color=always\talways --color=never\tnever.'
complete -c rls -o - -d 'read from stdin.' -x
