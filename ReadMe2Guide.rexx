/* $VER: ReadMe2Guide 0.2 $
 * Convert simple ReadMe file to AmigaGuide
 * © Stefan Haubenthal 2010
 * rx ReadMe2Guide <ReadMe.txt >Oricutron.guide
 */

say '@database ""'
say '@author "ReadMe2Guide"'
say '@node Main "Main"'
if ~eof(stdin) then parse pull last "0d"x
prevnode="Main"
do until eof(stdin)
	parse pull curr "0d"x
	if length(curr)>0 & (curr=copies("=", length(last)) | curr=copies("-", length(last))) then
	do
		if prevnode~="Main" then
		do
			/*say '@next 'compress(last,' "/')''*/
			say '@endnode'
			say '@node 'compress(last,' "/')' "'compress(last,'"')'"'
		end
		prevnode=last
	end
	if pos("@", last)>0 then
		say insert("\", last, pos("@", last)-1)
	else
		say last
	last=curr
end
say '@endnode'
