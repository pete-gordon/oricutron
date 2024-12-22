// this php file lists dsk, tap, and oricutron config files
var dskFiles = <?php $out = array();
foreach (glob('*.[dD][sS][kK]') as $filename) {
    $out[] = $filename;
}
echo json_encode($out); ?>;
var tapFiles = <?php $out = array();
foreach (glob('*.[tT][aA][pP]') as $filename) {
    $out[] = $filename;
}
echo json_encode($out); ?>;
var cfgFiles = <?php $out = array();
foreach (glob('*.[cC][fF][gG]') as $filename) {
    $out[] = $filename;
}
echo json_encode($out); ?>;