/* $VER: ReadMe2Guide 0.3 $
 * Convert simple ReadMe file to AmigaGuide
 * © Stefan Haubenthal 2010-2011
 * rx ReadMe2Guide <foo.txt >foo.guide
 */

say '@database ""'
say '@author "ReadMe2Guide"'
say '@node Main "Main"'
say '@toc Contents'
if ~eof(stdin) then parse pull last "0d"x
prevnode="Main"
toc=""
do until eof(stdin)
	parse pull curr "0d"x
	if length(curr)>0 & (curr=copies("=", length(last)) | curr=copies("-", length(last))) then
	do
		if prevnode~="Main" then
		do
			say '@endnode'
			say '@node 'compress(last,' "/')' "'compress(last,'"')'"'
			say '@toc Contents'
			toc=toc'@{"'compress(last,'"')'" link 'compress(last,' "/')'}'"0a"x
		end
		prevnode=last
	end
	/* escape at-sign */
	if pos("@", last)>0 then
		say insert("\", last, pos("@", last)-1)
	else
		say last
	last=curr
end
say '@endnode'
say '@node Contents'
say toc'@endnode'
