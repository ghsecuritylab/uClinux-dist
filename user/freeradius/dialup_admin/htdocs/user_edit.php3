<?php
require('../conf/config.php3');
if ($edit_group == 1){
	header("Location: group_admin.php3?login=$group_to_edit");
	exit;
}
require('../lib/attrshow.php3');
require('../lib/defaults.php3');
if ($user_type != 'group'){
	if (is_file("../lib/$config[general_lib_type]/user_info.php3"))
		include("../lib/$config[general_lib_type]/user_info.php3");
}
else{
	if (is_file("../lib/$config[general_lib_type]/group_info.php3"))
		include("../lib/$config[general_lib_type]/group_info.php3");
}
if ($config[general_lib_type] == 'sql' && $config[sql_use_operators] == 'true'){
	$colspan=2;
	$show_ops = 1;
	include("../lib/operators.php3");
}
else{
	$show_ops = 0;
	$colspan=1;
}


echo <<<EOM
<html>
<head>
EOM;

if ($user_type != 'group')
	echo " <title>subscription configuration for $login ($cn)</title>\n";
else
	echo " <title>subscription configuration for $login</title>\n";
?>

<meta http-equiv="Content-Type" content="text/html; charset=<?php echo $config[general_charset]?>">
<link rel="stylesheet" href="style.css">
</head>
<body bgcolor="#80a040" background="images/greenlines1.gif" link="black" alink="black">
<center>
<table border=0 width=550 cellpadding=0 cellspacing=0>
<tr valign=top>
<td align=center><img src="images/title2.gif"></td>
</tr>
</table>
<table border=0 width=400 cellpadding=0 cellspacing=2>

<?php
if ($user_type != 'group')
	include("../html/user_toolbar.html.php3");
else
	include("../html/group_toolbar.html.php3");	

print <<<EOM
</table>

<br>
<table border=0 width=540 cellpadding=1 cellspacing=1>
<tr valign=top>
<td width=75%>&nbsp;</td>
<td bgcolor="black" width=25% align=right>
	<table border=0 width="200" cellpadding=2 cellspacing=0>
	<tr bgcolor="#907030" align=center valign=top><th>
	<font color="white">User Preferences for $login ($cn)</font>&nbsp;
	</th></tr>
	</table>
</td></tr>
<tr bgcolor="black" valign=top><td colspan=2>
	<table border=0 width=100% cellpadding=12 cellspacing=0 bgcolor="#ffffd0" valign=top>
	<tr><td>
EOM;
   
if ($change == 1){
	if (is_file("../lib/$config[general_lib_type]/change_attrs.php3"))
		include("../lib/$config[general_lib_type]/change_attrs.php3");
	if ($user_type != 'group'){
		if ($config[general_show_user_password] != 'no' && $passwd != '' 
			&& is_file("../lib/$config[general_lib_type]/change_passwd.php3"))
			include("../lib/$config[general_lib_type]/change_passwd.php3");
		if (is_file("../lib/$config[general_lib_type]/user_info.php3"))
			include("../lib/$config[general_lib_type]/user_info.php3");
	}
	else{
		if (is_file("../lib/$config[general_lib_type]/group_info.php3"))
			include("../lib/$config[general_lib_type]/group_info.php3");
	}
}
else if ($badusers == 1){
	if (is_file("../lib/add_badusers.php3"))
		include("../lib/add_badusers.php3");
}
	
?>
   <form name="edituser" method=post>
      <input type=hidden name=login value="<?php print $login ?>">
      <input type=hidden name=user_type value=<?php print $user_type ?>>
      <input type=hidden name=change value="0">
      <input type=hidden name=add value="0">
      <input type=hidden name=badusers value="0">
	<table border=1 bordercolordark=#ffffe0 bordercolorlight=#000000 width=100% cellpadding=2 cellspacing=0 bgcolor="#ffffe0" valign=top>
<?php
if ($user_type == 'group')
	echo <<<EOM
Note: The attributes contained in the groups the user belongs to<br>
are extracted after the attributes in the radcheck/radreply tables.<br>
Please take that into consideration when adding attributes in the group<br>
and selecting operators.
<br>
EOM;
if ($user_type != 'group' && $config[general_show_user_password] != 'no'){
	echo <<<EOM
<tr>
<td align=right colspan=$colspan bgcolor="#d0ddb0">
User Password (changes only)<br>
EOM;
if ($user_password_exists == 'yes')
	echo "<font size=-2>User password <font color=\"green\"><b>exists</b></font></font>\n";
else
	echo "<font size=-2>User password <font color=\"red\"><b>does not exist</b></font></font>\n";
	echo <<<EOM
</td>
<td>
<input type=password name=passwd value="" size=40>
</td>
</tr>
EOM;
}
	foreach($show_attrs as $key => $desc){
		$name = $attrmap["$key"];
		$generic = $attrmap[generic]["$key"];
		if ($name == 'none')
			continue;
		unset($vals);
		unset($selected);
		unset($ops);
		$def_added = 0;
		if ($item_vals["$key"][count]){
			for($i=0;$i<$item_vals["$key"][count];$i++){
				$vals[] = $item_vals["$key"][$i];
				$ops[] = $item_vals["$key"][operator][$i];
			}
		}
		else{
			if ($default_vals["$key"][count]){
				for($i=0;$i<$default_vals["$key"][count];$i++){
					$vals[] = $default_vals["$key"][$i];
					$ops[] = $default_vals["$key"][operator][$i];
				}
			}
			else{
				$vals[] = '';
				$ops[] = '=';
			}
			$def_added = 1;
		}
		if ($generic == 'generic' && $def_added == 0){
			for($i=0;$i<$default_vals["$key"][count];$i++){
				$vals[] = $default_vals["$key"][$i];
				$ops[] = $default_vals["$key"][operator][$i];
			}
		}	
		if ($add && $name == $add_attr){
			$vals[] = $default_vals["$key"][0];
			$ops[] = ($default_vals["$key"][operator][0] != '') ? $default_vals["$key"][operator][0] : '=';
		}

		$i = 0;
		foreach($vals as $val){
			$name1 = $name . $i;
			$val = ereg_replace('"','&quot;',$val);
			$oper_name = $name1 . '_op';
			$oper = $ops[$i];
			$selected[$oper] = 'selected';
			$i++;
			print <<<EOM
<tr>
<td align=right bgcolor="#d0ddb0">
EOM;
			$desc = addslashes($desc);
			eval("\$desc = \"$desc\";");
			$desc = stripslashes($desc);
			if ($i == 1)
				echo "$desc\n";
			else
				echo "$desc ($i)\n";
			print <<<EOM
</td>
EOM;
			if ($show_ops)
				print <<<EOM
<td>
<select name=$oper_name>
<option $selected[$op_eq] value="=">=
<option $selected[$op_set] value=":=">:=
<option $selected[$op_add] value="+=">+=
<option $selected[$op_eq2] value="==">==
<option $selected[$op_ne] value="!=">!=
<option $selected[$op_gt] value=">">&gt;
<option $selected[$op_ge] value=">=">&gt;=
<option $selected[$op_lt] value="<">&lt;
<option $selected[$op_le] value="<=">&lt;=
<option $selected[$op_regeq] value="=~">=~
<option $selected[$op_regne] value="!~">!~
<option $selected[$op_exst] value="=*">=*
<option $selected[$op_nexst] value="!*">!*
</select>
</td>
EOM;

			print <<<EOM
<td>
<input type=text name="$name1" value="$val" size=40>
</td>
</tr>
EOM;
		}
	}
?>
<tr>
<td align=right colspan=<?php print $colspan ?> bgcolor="#d0ddb0">
Add Attribute
</td>
<td>
<select name="add_attr" OnChange="this.form.add.value=1;this.form.submit()">
<?php
foreach ($show_attrs as $key => $desc){
	$name = $attrmap["$key"];
	print <<<EOM
<option value="$name">$desc
EOM;
}
?>
</select>
</td>
</tr>

<?php
if (isset($member_groups)){
	echo <<<EOM
<tr>
<input type=hidden name=edit_group value=0>
<td align=right colspan=$colspan bgcolor="#d0ddb0">
Member of
</td>
<td>
<select name="group_to_edit">
EOM;
	foreach ($member_groups as $group){
		echo "<option value=\"$group\">$group\n";
	}
	echo <<<EOM
</select>
&nbsp;&nbsp;&nbsp;
<input type=submit class=button value="Edit Group" OnClick="this.form.edit_group.value=1">
</td>
</tr>
EOM;
}
?>
	</table>
<br>
<input type=submit class=button value=Change OnClick="this.form.change.value=1">
<?php
if ($user_type != 'group'){
	echo <<<EOM
<br><br>
<input type=submit class=button value="Add to Badusers" OnClick="this.form.badusers.value=1">
<a href="help/badusers_help.html" target=bu_help onclick=window.open("help/badusers_help.html","bu_help","width=600,height=210,toolbar=no,scrollbars=no,resizable=yes") title="BADUSERS Help Page"><font color="blue">&lt;--Help</font></a>
EOM;
}
?>
</form>
	</td></tr>
</table>
</tr>
</table>
</body>
</html>
