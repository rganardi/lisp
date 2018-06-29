#!/bin/sh

for src in ./src/*.c; do
	sed -ne '/^\w\+ \w\+(.*) {/ {s/ {/;/; s/\(\(\w\+\)\( \w\+\)\?\( \|\( \*\+\)\)\(\w\|\[\|\]\)\+\([,)]\)\)/\2\3\5\7/g;p}' $src >> ${src/.c/.h}
done


#sed script explanation
#'/^\w\+ \w\+(.*) {/{		#match function definitions
#	#s/ {/;/;		#change end of line
#
#	s/\(\
#	\(\w\+\)		#(\2) match first word				\
#	\( \w\+\)\?		#(\3) match second word				\
#	\( \|\( \*\+\)\)	#(\4) match pointers				\
#	\(\w\|\[\|\]\)\+	#(\6) match variable name			\
#	\([,)]\)\)		#(\7) match trailing comma or right paren	\
#	/\2\3\5\7/g;
#	p
#}'
