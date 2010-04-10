/* $VER: ReadMe2Guide 0.1 $
 * Convert simple ReadMe file to AmigaGuide
 * © Stefan Haubenthal 2010
 * rx ReadMe2Guide <ReadMe.txt >Oricutron.guide
 */

say '@database ""'
say '@author "ReadMe2Guide"'
say '@node "Main" "Main"'
if ~eof(stdin) then parse pull last
prevnode="Main"
do until eof(stdin)
	parse pull curr
	if left(curr, 1)="=" then
/*	if curr=copies("=", length(last)) then*/
	do
		say '@next "'last'"'
		say '@endnode'
		say '@node "'last'" "'last'"'
		say '@prev "'prevnode'"'
		say '@toc "Main"'
		prevnode=last
	end
	if pos("@", last)>0 then
		say insert("\", last, pos("@", last)-1)
	else
		say last
	last=curr
end
say '@endnode'
