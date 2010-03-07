<!--
    This file is part of GUP.

    GUP is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    GUP is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with GUP.  If not, see <http://www.gnu.org/licenses/>.
-->

<?php
$lastestVersion = 4.5;
$lastestVersionStr = "4.5";
$DLURL = "http://sourceforge.net/project/downloading.php?group_id=95717&filename=npp.4.5.Installer.exe";
$curentVersion = $_GET["version"];

function curlhttp($url) 
{ // Permet d'envoyer une requète http et de recevoir le résultat avec CURL
	if (!$ch=curl_init($url)) 
	{
		return FALSE;
	}
	curl_setopt($ch, CURLOPT_HEADER, 0);
	curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1); // Permet de renvoyer le résultat dans une variable
	curl_setopt($ch, CURLOPT_TIMEOUT, 10); // Time out après 10 secondes
	$result=curl_exec($ch);
	curl_close($ch);
	return $result;
}

if ($curentVersion >= $lastestVersion)
{
	echo 
"<?xml version=\"1.0\"?>
<GUP>
	<NeedToBeUpdated>no</NeedToBeUpdated>
</GUP>
";
}
else
{
	$result = curlhttp($DLURL);
	preg_match('/http:\/\/.+\/.+.exe/', $result, $strippedResult);
	
	echo 
"<?xml version=\"1.0\"?>
<GUP>
	<NeedToBeUpdated>yes</NeedToBeUpdated>
	<Version>$lastestVersionStr</Version>
	<Location>$strippedResult[0]</Location>
</GUP>"
;
}



?>

