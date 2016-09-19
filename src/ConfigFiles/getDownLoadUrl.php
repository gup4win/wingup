<?php
/*
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
*/

$lastestVersion = 1.23; // X.YZ
$lastestVersionStr = "1.2.3"; // X.Y.Z
$DLURL = "http://download.my-software.com/my-software/1.2.3/mySoftware.1.2.3.Installer.exe";
$curentVersion = $_GET["version"];
$param = $_GET["param"]; // optional

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
	echo 
"<?xml version=\"1.0\"?>
<GUP>
	<NeedToBeUpdated>yes</NeedToBeUpdated>
	<Version>$lastestVersionStr</Version>
	<Location>$DLURL</Location>
</GUP>"
;
}
?>
