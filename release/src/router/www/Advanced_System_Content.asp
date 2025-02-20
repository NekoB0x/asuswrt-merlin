<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - <#menu5_6_2#></title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="pwdmeter.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/detect.js"></script>
<style>
#ClientList_Block_PC{
	border:1px outset #999;
	background-color:#576D73;
	position:absolute;
	*margin-top:27px;
	margin-left:121px;
	*margin-left:-353px;
	width:345px;
	text-align:left;
	height:auto;
	overflow-y:auto;
	z-index:200;
	padding: 1px;
	display:none;
}
#ClientList_Block_PC div{
	background-color:#576D73;
	height:auto;
	*height:20px;
	line-height:20px;
	text-decoration:none;
	font-family: Lucida Console;
	padding-left:2px;
}

#ClientList_Block_PC a{
	background-color:#EFEFEF;
	color:#FFF;
	font-size:12px;
	font-family:Arial, Helvetica, sans-serif;
	text-decoration:none;
}
#ClientList_Block_PC div:hover, #ClientList_Block a:hover{
	background-color:#3366FF;
	color:#FFFFFF;
	cursor:default;
}
</style>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';
time_day = uptimeStr.substring(5,7);//Mon, 01 Aug 2011 16:25:44 +0800(1467 secs since boot....
time_mon = uptimeStr.substring(9,12);
time_time = uptimeStr.substring(18,20);
dstoffset = '<% nvram_get("time_zone_dstoff"); %>';

var isFromHTTPS = false;
if((location.href.search('https://') >= 0) || (location.href.search('HTTPS://') >= 0)){
        isFromHTTPS = true;
}

<% login_state_hook(); %>
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var http_clientlist_array = '<% nvram_get("http_clientlist"); %>';
var accounts = [<% get_all_accounts(); %>];
for(var i=0; i<accounts.length; i++){
		accounts[i] = decodeURIComponent(accounts[i]);
}
if(accounts.length == 0)
	accounts = ['<% nvram_get("http_username"); %>'];

var theUrl = "router.asus.com";
if(sw_mode == 3 || (sw_mode == 4))
	theUrl = location.hostName;

function initial(){
	show_menu();
	show_http_clientlist();

	if(reboot_schedule_support){
		document.form.reboot_date_x_Sun.checked = getDateCheck(document.form.reboot_schedule.value, 0);
		document.form.reboot_date_x_Mon.checked = getDateCheck(document.form.reboot_schedule.value, 1);
		document.form.reboot_date_x_Tue.checked = getDateCheck(document.form.reboot_schedule.value, 2);
		document.form.reboot_date_x_Wed.checked = getDateCheck(document.form.reboot_schedule.value, 3);
		document.form.reboot_date_x_Thu.checked = getDateCheck(document.form.reboot_schedule.value, 4);
		document.form.reboot_date_x_Fri.checked = getDateCheck(document.form.reboot_schedule.value, 5);
		document.form.reboot_date_x_Sat.checked = getDateCheck(document.form.reboot_schedule.value, 6);
		document.form.reboot_time_x_hour.value = getrebootTimeRange(document.form.reboot_schedule.value, 0);
		document.form.reboot_time_x_min.value = getrebootTimeRange(document.form.reboot_schedule.value, 1);
		document.getElementById('reboot_schedule_enable_tr').style.display = "";
		hide_reboot_option('<% nvram_get("reboot_schedule_enable"); %>');
	}
	else{
		document.getElementById('reboot_schedule_enable_tr').style.display = "none";
		document.getElementById('reboot_schedule_date_tr').style.display = "none";
		document.getElementById('reboot_schedule_time_tr').style.display = "none";
	}

	corrected_timezone();
	load_timezones();
	autofill_dst();
	parse_dstoffset();
	select_time_zone();
	load_dst_m_Options();
	load_dst_w_Options();
	load_dst_d_Options();
	load_dst_h_Options();
	document.form.http_passwd2.value = "";
	//Viz banned 2014.04.17 chkPass(" ", 'http_passwd');

	if(document.form.ntpd_server[0].checked == true)
		hide_ntpd_hint(0);
	else
		hide_ntpd_hint(1);

	if(svc_ready == "0")
		$('svc_hint_div').style.display = "";

	if(!HTTPS_support){
		$("https_tr").style.display = "none";
		$("https_lanport").style.display = "none";
		$("http_client_tr").style.display = "none";
		$("http_client_table").style.display = "none";
		$("http_clientlist_Block").style.display = "none";
	}
	else{
		showLANIPList();
		hide_http_lanport(document.form.http_enable.value);
		hide_https_lanport(document.form.http_enable.value);
		hide_https_wanport(document.form.http_enable.value);
	}

	if(wifi_tog_btn_support || wifi_hw_sw_support || sw_mode == 2 || sw_mode == 4){		// wifi_tog_btn && wifi_hw_sw && hide WPS button behavior under repeater mode
			document.form.btn_ez_radiotoggle[0].disabled = true;
			document.form.btn_ez_radiotoggle[1].disabled = true;
			document.getElementById('btn_ez_radiotoggle_tr').style.display = "none";
	}else{
			document.getElementById('btn_ez_radiotoggle_tr').style.display = "";
	}

	if(sw_mode != 1 || http_option == 0){
		$('misc_http_x_tr').style.display ="none";
		hideport(0);
		document.form.misc_http_x.disabled = true;
		document.form.misc_httpsport_x.disabled = true;
		document.form.misc_httpport_x.disabled = true;
		$("http_access_page").style.display = "none";
		$("https_access_page").style.display = "none";
	}
	else
		hideport(document.form.misc_http_x[0].checked);

	var http_option = '<% nvram_get("http_enable"); %>';
	if(!HTTPS_support || http_option == 0){
		$("http_lanport").style.display = "";
		$("https_lanport").style.display = "none";
		$("https_cert").style.display = "none";
	}
	else if(http_option == 1){
		$("http_lanport").style.display = "none";
		$("https_lanport").style.display = "";
		$("https_cert").style.display = "";
	}
	else if(http_option == 2){
		$("http_lanport").style.display = "";
		$("https_lanport").style.display = "";
		$("https_cert").style.display = "";
	}
	else {
		$("http_lanport").style.display = "";
		$("https_lanport").style.display = "none";
		$("https_cert").style.display = "none";
	}

	document.form.http_username.value= accounts[0];
	document.form.sshd_authkeys.value = document.form.sshd_authkeys.value.replace(/>/gm,"\r\n");

	if(based_modelid == "RT-AC66U" || based_modelid == "RT-AC56U" || based_modelid == "RT-AC68U") {
		$("jffs2log").style.display = "";

	hide_dhcp_querylog_option(document.form.log_level.value);
	}

}

var time_zone_tmp="";
var time_zone_s_tmp="";
var time_zone_e_tmp="";
var time_zone_withdst="";

function applyRule(){
	if(validForm()){
		var rule_num = $('http_clientlist_table').rows.length;
		var item_num = $('http_clientlist_table').rows[0].cells.length;
		var tmp_value = "";

		for(i=0; i<rule_num; i++){
			tmp_value += "<"
			for(j=0; j<item_num-1; j++){
				tmp_value += $('http_clientlist_table').rows[i].cells[j].innerHTML;
				if(j != item_num-2)
					tmp_value += ">";
			}
		}
		if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
			tmp_value = "";
		document.form.http_clientlist.value = tmp_value;

		if(document.form.http_clientlist.value == "" && document.form.http_client[0].checked == 1){
			alert("<#JS_fieldblank#>");
			document.form.http_client_ip_x_0.focus();
			return false;
		}

		if(document.form.http_passwd2.value.length > 0){
			document.form.http_passwd.value = document.form.http_passwd2.value;
			document.form.http_passwd.disabled = false;
		}

		if(document.form.time_zone_select.value.search("DST") >= 0 || document.form.time_zone_select.value.search("TDT") >= 0){		// DST area
				time_zone_tmp = document.form.time_zone_select.value.split("_");	//0:time_zone 1:serial number
				time_zone_s_tmp = "M"+document.form.dst_start_m.value+"."+document.form.dst_start_w.value+"."+document.form.dst_start_d.value+"/"+document.form.dst_start_h.value;
				time_zone_e_tmp = "M"+document.form.dst_end_m.value+"."+document.form.dst_end_w.value+"."+document.form.dst_end_d.value+"/"+document.form.dst_end_h.value;
				document.form.time_zone_dstoff.value=time_zone_s_tmp+","+time_zone_e_tmp;
				document.form.time_zone.value = document.form.time_zone_select.value;
		}else{
				document.form.time_zone_dstoff.value="";
				document.form.time_zone.value = document.form.time_zone_select.value;
		}

		if(document.form.misc_http_x[1].checked == true){
				document.form.misc_httpport_x.disabled = true;
				document.form.misc_httpsport_x.disabled = true;
		}
		if(document.form.misc_http_x[0].checked == true
				&& document.form.http_enable[0].selected == true){
				document.form.misc_httpport_x.disabled = true;
				document.form.misc_httpsport_x.disabled = true;
		}
		if(document.form.misc_http_x[0].checked == true
				&& document.form.http_enable[1].selected == true){
				document.form.misc_httpport_x.disabled = true;
		}

		if(document.form.https_lanport.value != '<% nvram_get("https_lanport"); %>'
				|| document.form.http_enable.value != '<% nvram_get("http_enable"); %>'
				|| document.form.misc_httpport_x.value != '<% nvram_get("misc_httpport_x"); %>'
				|| document.form.misc_httpsport_x.value != '<% nvram_get("misc_httpsport_x"); %>'
			){
			if(document.form.http_enable.value == "0"){	//HTTP
				if(isFromWAN)
					document.form.flag.value = "http://" + location.hostname + ":" + document.form.misc_httpport_x.value;
				else
					document.form.flag.value = "http://" + location.hostname + ":" + document.form.http_lanport.value;
			}
			else if(document.form.http_enable.value == "1"){	//HTTPS
				if(isFromWAN)
					document.form.flag.value = "https://" + location.hostname + ":" + document.form.misc_httpsport_x.value;
				else
					document.form.flag.value = "https://" + location.hostname + ":" + document.form.https_lanport.value;
			}
			else{	//BOTH
				if(isFromHTTPS){
					if(isFromWAN)
						document.form.flag.value = "https://" + location.hostname + ":" + document.form.misc_httpsport_x.value;
					else
						document.form.flag.value = "https://" + location.hostname + ":" + document.form.https_lanport.value;
				}else{
					if(isFromWAN)
						document.form.flag.value = "http://" + location.hostname + ":" + document.form.misc_httpport_x.value;
					else
						document.form.flag.value = "http://" + location.hostname + ":" + document.form.http_lanport.value;
				}
			}
		}

		// Set lan_port for firewall rules
		document.form.lan_port.value = document.form.http_lanport.value;

		if(reboot_schedule_support == 1){
			updateDateTime();
		}

		// Restart dnsmasq if changing state of SNTP server
		if(document.form.ntpd_server[0].checked != '<% nvram_get("ntpd_server"); %>'){
			document.form.action_script.value += ";restart_dnsmasq";
		}

		// Reboot if clearing https certificate or change JFFS log state
                if((document.form.https_crt_save[0].checked == "0" && (document.form.https_crt_save[0].checked != '<% nvram_get("https_crt_save"); %>')) ||
			(document.form.jffs2_log[0].checked != '<% nvram_get("jffs2_log"); %>')){
                        FormActions("start_apply.htm", "apply", "reboot", "<% get_default_reboot_time(); %>");
                }

		showLoading();
		document.form.submit();
	}
}

function validForm(){
	showtext($("alert_msg1"), "");
	showtext($("alert_msg2"), "");

	if((document.form.sshd_enable[0].checked) && (document.form.sshd_authkeys.value.length == 0) && (!document.form.sshd_pass[0].checked)){
		alert("You must configure at least one SSH authentication method!");
		return false;
	}

	if((document.form.sshd_enable[0].checked) && (document.form.sshd_authkeys.value.length >= 3499)){
		alert("SSH Authentication Key maximum length exceeded!");
		return false;
	}


	if(document.form.http_username.value.length == 0){
		showtext($("alert_msg1"), "<#File_Pop_content_alert_desc1#>");
		document.form.http_username.focus();
		document.form.http_username.select();
		return false;
	}
	else{
		var alert_str = validate_hostname(document.form.http_username);

		if(alert_str != ""){
			showtext($("alert_msg1"), alert_str);
			$("alert_msg1").style.display = "";
			document.form.http_username.focus();
			document.form.http_username.select();
			return false;
		}else{
			$("alert_msg1").style.display = "none";
  	}

		document.form.http_username.value = trim(document.form.http_username.value);

		if(document.form.http_username.value == "root"
				|| document.form.http_username.value == "guest"
				|| document.form.http_username.value == "anonymous"
		){
				showtext($("alert_msg1"), "<#USB_Application_account_alert#>");
				$("alert_msg1").style.display = "";
				document.form.http_username.focus();
				document.form.http_username.select();
				return false;
		}
		else if(accounts.getIndexByValue(document.form.http_username.value) > 0
				&& document.form.http_username.value != accounts[0]){
				showtext($("alert_msg1"), "<#File_Pop_content_alert_desc5#>");
				$("alert_msg1").style.display = "";
				document.form.http_username.focus();
				document.form.http_username.select();
				return false;
		}else{
				$("alert_msg1").style.display = "none";
		}
	}

	if(document.form.http_passwd2.value.length > 0 && document.form.http_passwd2.value.length < 5){
		showtext(document.getElementById("alert_msg2"),"Your password is too short");
		document.form.http_passwd2.focus();
		document.form.http_passwd2.select();
		return false;
	}

	if(document.form.http_passwd2.value != document.form.v_password2.value){
		showtext($("alert_msg2"),"*<#File_Pop_content_alert_desc7#>");
		if(document.form.http_passwd2.value.length <= 0){
			document.form.http_passwd2.focus();
			document.form.http_passwd2.select();
		}else{
			document.form.v_password2.focus();
			document.form.v_password2.select();
		}

		return false;
	}

	if(!validate_string(document.form.http_passwd2)){
		document.form.http_passwd2.focus();
		document.form.http_passwd2.select();
		return false;
	}

	if(document.form.http_passwd2.value.length > 16){
		showtext($("alert_msg2"),"Password exceeds maximum length of 16 characters");
		document.form.http_passwd2.focus();
		document.form.http_passwd2.select();
		return false;
	}

	//confirm common string combination	#JS_common_passwd#
	var is_common_string = check_common_string(document.form.http_passwd2.value, "httpd_password");
	if(document.form.http_passwd2.value.length > 0 && is_common_string){
			if(!confirm("This appears to be a common password or key and may not be secure.  Do you want to continue?")){
				document.form.http_passwd2.focus();
				document.form.http_passwd2.select();
				return false;
			}
	}

	if(!validate_ipaddr_final(document.form.log_ipaddr, 'log_ipaddr')
			|| !validate_string(document.form.ntp_server0)
			)
		return false;

	if(document.form.dst_start_m.value == document.form.dst_end_m.value
		&& document.form.dst_start_w.value == document.form.dst_end_w.value
		&& document.form.dst_start_d.value == document.form.dst_end_d.value){
			alert("<#FirewallConfig_URLActiveTime_itemhint4#>");
			return false;
	}

	if(document.form.http_passwd2.value.length > 0)
		alert("<#File_Pop_content_alert_desc10#>");

	if (document.form.http_enable.value != 1 && !validate_range(document.form.http_lanport, 1, 65535)){
			document.form.http_lanport.focus();
			return false;
		}

	if (HTTPS_support && document.form.http_enable.value != 0 && !validate_range(document.form.https_lanport, 1, 65535)){
			document.form.https_lanport.focus();
			return false;
		}

	if (document.form.misc_http_x[0].checked) {
		if (!validate_range(document.form.misc_httpport_x, 1, 65535)){
			document.form.misc_httpport_x.focus();
			return false;
		}

		if (HTTPS_support && !validate_range(document.form.misc_httpsport_x, 1, 65535)){
			document.form.misc_httpsport_x.focus();
			return false;
		}
	}
	else{
		document.form.misc_httpport_x.value = '<% nvram_get("misc_httpport_x"); %>';
		document.form.misc_httpsport_x.value = '<% nvram_get("misc_httpsport_x"); %>';
	}

	if(isPortConflict(document.form.misc_httpport_x.value)){
		alert(isPortConflict(document.form.misc_httpport_x.value));
		document.form.misc_httpport_x.focus();
		return false;
	}
	else if(isPortConflict(document.form.http_lanport.value)){
		alert(isPortConflict(document.form.http_lanport.value));
		document.form.http_lanport.focus();
		return false;
	}
	else if(document.form.https_lanport.value == document.form.http_lanport.value){
		alert("Duplicate port number with HTTP and HTTPS LAN port setting.");
		document.form.https_lanport.focus();
		return false;
        }
	else if(isPortConflict(document.form.misc_httpsport_x.value) && HTTPS_support){
		alert(isPortConflict(document.form.misc_httpsport_x.value));
		document.form.misc_httpsport_x.focus();
		return false;
	}
	else if(isPortConflict(document.form.https_lanport.value) && HTTPS_support){
		alert(isPortConflict(document.form.https_lanport.value));
		document.form.https_lanport.focus();
		return false;
	}
	else if(document.form.misc_httpsport_x.value == document.form.misc_httpport_x.value && HTTPS_support){
		alert("Duplicate port number with HTTP and HTTPS WAN port setting.");
		document.form.misc_httpsport_x.focus();
		return false;
	}
	else if(!validate_range_sp(document.form.http_autologout, 10, 999, '<% nvram_get("http_autologout"); %>'))
		return false;

	if(reboot_schedule_support == 1){
		updateDateTime();

		if(document.form.reboot_schedule_enable[1].checked == 1){
			if(!validate_timerange(document.form.reboot_time_x_hour, 0)
				|| !validate_timerange(document.form.reboot_time_x_min, 1))
				return false;
			if(document.form.reboot_schedule.value.substring(0, 7) == "0000000"){
				alert("Reboot schedule day(s) not selected!");
				document.form.reboot_date_x_Sun.focus();
				return false;
			}
		}
	}

	return true;
}

function ntp_chk(obj){
	if (obj.value == 0){
		alert("WARNING:\nSystem time must be set manually when disabling NTP updates!");
	}
}

function done_validating(action){
	refreshpage();
}

var timezones = [
	["UTC12",	"(GMT-12:00) <#TZ01#>"],
	["UTC11",	"(GMT-11:00) <#TZ02#>"],
	["UTC10",	"(GMT-10:00) <#TZ03#>"],
	["NAST9DST",	"(GMT-09:00) <#TZ04#>"],
	["PST8DST",	"(GMT-08:00) <#TZ05#>"],
	["MST7DST_1",	"(GMT-07:00) <#TZ06#>"],
	["MST7_2",	"(GMT-07:00) <#TZ07#>"],
	["MST7DST_3",	"(GMT-07:00) <#TZ08#>"],
	["CST6_2",	"(GMT-06:00) <#TZ10#>"],
	["CST6DST_3",	"(GMT-06:00) <#TZ11#>"],
	["CST6DST_3_1",	"(GMT-06:00) <#TZ12#>"],
	["UTC6DST",	"(GMT-06:00) <#TZ13#>"],
	["EST5DST",	"(GMT-05:00) <#TZ14#>"],
	["UTC5_1",	"(GMT-05:00) <#TZ15#>"],
	["UTC5_2",	"(GMT-05:00) <#TZ16#>"],
	["AST4DST",	"(GMT-04:00) <#TZ17#>"],
	["UTC4_1",	"(GMT-04:00) <#TZ18#>"],
	["UTC4_2",	"(GMT-04:00) <#TZ18_1#>"],
	["UTC4DST_2",	"(GMT-04:00) <#TZ19#>"],
	["NST3.30DST",	"(GMT-03:30) <#TZ20#>"],
	["EBST3",	"(GMT-03:00) <#TZ21#>"],	//EBST3DST_1
	["UTC3",	"(GMT-03:00) <#TZ22#>"],
	["EBST3DST_2",	"(GMT-03:00) <#TZ23#>"],
	["UTC2",	"(GMT-02:00) <#TZ24#>"],
	["EUT1DST",	"(GMT-01:00) <#TZ25#>"],
	["UTC1",	"(GMT-01:00) <#TZ26#>"],
	["GMT0",	"(GMT) <#TZ27#>"],
	["GMT0DST_1",	"(GMT) <#TZ27_2#>"],
	["GMT0DST_2",	"(GMT) <#TZ28#>"],
	["GMT0_2",	"(GMT) <#TZ28_1#>"],
	["UTC-1DST_1",	"(GMT+01:00) <#TZ29#>"],
	["UTC-1DST_1_1","(GMT+01:00) <#TZ30#>"],
	["UTC-1DST_1_2",	"(GMT+01:00) <#TZ31#>"],
	["UTC-1DST_2",	"(GMT+01:00) <#TZ32#>"],
	["MET-1DST",	"(GMT+01:00) <#TZ33#>"],
	["MET-1DST_1",	"(GMT+01:00) <#TZ34#>"],
	["MEZ-1DST",	"(GMT+01:00) <#TZ35#>"],
	["MEZ-1DST_1",	"(GMT+01:00) <#TZ36#>"],
	["UTC-1_3",	"(GMT+01:00) <#TZ37#>"],
	["UTC-2DST",	"(GMT+02:00) <#TZ38#>"],
	["UTC-2DST_3",	"(GMT+02:00) <#TZ33_1#>"],
	["EST-2",	"(GMT+02:00) <#TZ39#>"],
	["UTC-2DST_4",	"(GMT+02:00) <#TZ40#>"],
	["UTC-2DST_2",	"(GMT+02:00) <#TZ41#>"],
	["IST-2DST",	"(GMT+02:00) <#TZ42#>"],
	["EET-2DST",	"(GMT+02:00) <#TZ43_2#>"],
	["UTC-2_1",	"(GMT+02:00) <#TZ40_2#>"],
	["SAST-2",	"(GMT+02:00) <#TZ43#>"],
	["UTC-3_1",	"(GMT+03:00) <#TZ46#>"],
	["UTC-3_2",	"(GMT+03:00) <#TZ47#>"],
	["UTC-3_3",	"(GMT+03:00) <#TZ40_1#>"],
	["UTC-3_4",	"(GMT+03:00) <#TZ44#>"],
	["IST-3",	"(GMT+03:00) <#TZ48#>"],
	["UTC-3_6",	"(GMT+03:00) <#TZ48_1#>"],
	["UTC-3.30DST",	"(GMT+03:30) <#TZ49#>"],
	["UTC-4_1",	"(GMT+04:00) <#TZ50#>"],
	["UTC-4_5",	"(GMT+04:00) <#TZ50_2#>"],
	["UTC-4_7",	"(GMT+04:00) <#TZ45#>"],	//UTC-3_5
	["UTC-4_4",	"(GMT+04:00) <#TZ50_1#>"],
	["UTC-4_6",	"(GMT+04:00) <#TZ51#>"],
	["UTC-4.30",	"(GMT+04:30) <#TZ52#>"],
	["UTC-5",	"(GMT+05:00) <#TZ54#>"],
	["UTC-5_1",	"(GMT+05:00) <#TZ53#>"],
	["UTC-5.30_2",	"(GMT+05:30) <#TZ55#>"],
	["UTC-5.30_1",	"(GMT+05:30) <#TZ56#>"],
	["UTC-5.30",	"(GMT+05:30) <#TZ59#>"],
	["UTC-5.45",	"(GMT+05:45) <#TZ57#>"],
	["RFT-6",	"(GMT+06:00) <#TZ60#>"],
	["UTC-6",	"(GMT+06:00) <#TZ58#>"],
	["UTC-6.30",	"(GMT+06:30) <#TZ61#>"],
	["UTC-7",	"(GMT+07:00) <#TZ62#>"],
	["UTC-7_2",	"(GMT+07:00) <#TZ63#>"],
	["UTC-7_3",     "(GMT+07:00) <#TZ62_1#>"],	//UTC-6_2
	["CST-8",	"(GMT+08:00) <#TZ64#>"],
	["CST-8_1",	"(GMT+08:00) <#TZ65#>"],
	["SST-8",	"(GMT+08:00) <#TZ66#>"],
	["CCT-8",	"(GMT+08:00) <#TZ67#>"],
	["WAS-8",	"(GMT+08:00) <#TZ68#>"],
	["UTC-8",	"(GMT+08:00) <#TZ69#>"],
	["UTC-8_1",     "(GMT+08:00) <#TZ70#>"],
	["UTC-9_1",	"(GMT+09:00) <#TZ70_1#>"],
	["UTC-9_3",	"(GMT+09:00) <#TZ72#>"],
	["JST",		"(GMT+09:00) <#TZ71#>"],
	["CST-9.30",	"(GMT+09:30) <#TZ73#>"],
	["UTC-9.30DST",	"(GMT+09:30) <#TZ74#>"],
	["UTC-10DST_1",	"(GMT+10:00) <#TZ75#>"],
	["UTC-10_2",	"(GMT+10:00) <#TZ76#>"],
	["UTC-10_4",	"(GMT+10:00) <#TZ78#>"],
	["TST-10TDT",	"(GMT+10:00) <#TZ77#>"],
	["UTC-10_6",	"(GMT+10:00) <#TZ79#>"],
	["UTC-11",	"(GMT+11:00) <#TZ80#>"],
	["UTC-11_1",	"(GMT+11:00) <#TZ81#>"],
	["UTC-11_3",	"(GMT+11:00) <#TZ86#>"],
	["UTC-11_4",	"(GMT+11:00) <#TZ82_1#>"],
	["UTC-12",      "(GMT+12:00) <#TZ82#>"],
	["UTC-12_2",      "(GMT+12:00) <#TZ85#>"],
	["NZST-12DST",	"(GMT+12:00) <#TZ83#>"],
	["UTC-13",	"(GMT+13:00) <#TZ84#>"]];

function load_timezones(){
	free_options(document.form.time_zone_select);
	for(var i = 0; i < timezones.length; i++){
		add_option(document.form.time_zone_select,
			timezones[i][1], timezones[i][0],
			(document.form.time_zone.value == timezones[i][0]));
	}
	select_time_zone();
}

var timezone_dst_changes = {
	"AST4DST":      [ 3,2,0,2,  11,1,0,2 ],
	"CST6DST_3_1":  [ 4,1,0,2,  10,5,0,2 ],
	"CST6DST_3":    [ 4,1,0,2,  10,5,0,2 ],
	"EBST3DST_2":   [ 3,2,0,2,  11,1,0,2 ],
	"EET-2DST":     [ 3,4,0,3,  10,5,0,4 ],
	"EST5DST":      [ 3,2,0,2,  11,1,0,2 ],
	"EUT1DST":      [ 3,4,0,0,  10,5,0,1 ],
	"GMT0DST_1":    [ 3,4,0,1,  10,5,0,2 ],
	"GMT0DST_2":    [ 5,3,0,2,  3,4,0,3 ],
	"IST-2DST":     [ 3,4,5,2,  10,5,0,2 ],
	"MET-1DST_1":   [ 3,4,0,2,  10,5,0,3 ],
	"MET-1DST":     [ 3,4,0,2,  10,5,0,3 ],
	"MEZ-1DST_1":   [ 3,4,0,2,  10,5,0,3 ],
	"MEZ-1DST":     [ 3,4,0,2,  10,5,0,3 ],
	"MST7DST_1":    [ 3,2,0,2,  11,1,0,2 ],
	"MST7DST_3":    [ 4,1,0,2,  10,5,0,2 ],
	"NAST9DST":     [ 3,2,0,2,  11,1,0,2 ],
	"NST3.30DST":   [ 3,2,0,2,  11,1,0,2 ],
	"NZST-12DST":   [ 9,4,0,2,  4,1,0,3 ],
	"PST8DST":      [ 3,2,0,2,  11,1,0,2 ],
	"TST-10TDT":    [ 10,1,0,2,  4,1,0,3 ],
	"UTC-10DST_1":  [ 10,1,0,2,  4,1,0,3 ],
	"UTC-12DST":    [ 11,2,0,2,  1,3,0,3 ],
	"UTC-1DST_1_1": [ 3,4,0,2,  10,5,0,3 ],
	"UTC-1DST_1_2": [ 3,4,0,2,  10,5,0,3 ],
	"UTC-1DST_1":   [ 3,4,0,2,  10,5,0,3 ],
	"UTC-1DST_2":   [ 3,4,0,2,  10,5,0,3 ],
	"UTC-2DST_2":   [ 3,4,0,3,  10,5,0,4 ],
	"UTC2DST":      [ 3,2,0,2,  11,1,0,2 ],
	"UTC-2DST_3":   [ 3,4,0,3,  10,5,0,4 ],
	"UTC-2DST":     [ 3,4,0,3,  10,5,0,4 ],
	"UTC-2DST_4":   [ 3,4,0,3,  10,5,0,4 ],
	"UTC-3.30DST":  [ 3,4,1,0,  9,4,3,0 ],
	"UTC4DST_2":    [ 9,1,0,0,  4,1,0,0 ],
	"UTC-9.30DST":  [ 10,1,0,2,  4,1,0,3 ]
};

function autofill_dst(){
	if (document.form.time_zone_select.value.search("DST") >= 0 || document.form.time_zone_select.value.search("TDT") >= 0) {
		if (timezone_dst_changes[document.form.time_zone_select.value]) {
			document.form.dst_start_m.value = timezone_dst_changes[document.form.time_zone_select.value][0];
			document.form.dst_start_w.value = timezone_dst_changes[document.form.time_zone_select.value][1];
			document.form.dst_start_d.value = timezone_dst_changes[document.form.time_zone_select.value][2];
			document.form.dst_start_h.value = timezone_dst_changes[document.form.time_zone_select.value][3];
			document.form.dst_end_m.value = timezone_dst_changes[document.form.time_zone_select.value][4];
			document.form.dst_end_w.value = timezone_dst_changes[document.form.time_zone_select.value][5];
			document.form.dst_end_d.value = timezone_dst_changes[document.form.time_zone_select.value][6];
			document.form.dst_end_h.value = timezone_dst_changes[document.form.time_zone_select.value][7];
		}
	}
}
var dst_month = new Array("", "1st", "2nd", "3rd", "4th", "5th", "6th", "7th", "8th", "9th", "10th", "11th", "12th");
var dst_week = new Array("", "1st", "2nd", "3rd", "4th", "5th");
var dst_day = new Array("<#date_Sun_itemdesc#>", "<#date_Mon_itemdesc#>", "<#date_Tue_itemdesc#>", "<#date_Wed_itemdesc#>", "<#date_Thu_itemdesc#>", "<#date_Fri_itemdesc#>", "<#date_Sat_itemdesc#>");
var dst_hour = new Array("12AM", "1AM", "2AM", "3AM", "4AM", "5AM", "6AM", "7AM", "8AM", "9AM", "10AM", "11AM",
						 "12PM", "1PM", "2PM", "3PM", "4PM", "5PM", "6PM", "7PM", "8PM", "9PM", "10PM", "11PM");
var dstoff_start_m,dstoff_start_w,dstoff_start_d,dstoff_start_h;
var dstoff_end_m,dstoff_end_w,dstoff_end_d,dstoff_end_h;

function parse_dstoffset(){     //Mm.w.d/h,Mm.w.d/h
	if(dstoffset){
		var dstoffset_startend = dstoffset.split(",");

		if(dstoffset_startend[0] != "" && dstoffset_startend[0] != undefined){
			var dstoffset_start = trim(dstoffset_startend[0]);
			var dstoff_start = dstoffset_start.split(".");

			dstoff_start_m = parseInt(dstoff_start[0].substring(1));
			if(check_range(dstoff_start_m,1,12)){
				document.form.dst_start_m.value = dstoff_start_m;
			}

			if(dstoff_start[1] != "" && dstoff_start[1] != undefined){
				dstoff_start_w = parseInt(dstoff_start[1]);
				if(check_range(dstoff_start_w,1,5)){
					document.form.dst_start_w.value = dstoff_start_w;
				}
			}

			if(dstoff_start[2] != "" && dstoff_start[2] != undefined){
				dstoff_start_d = parseInt(dstoff_start[2].split("/")[0]);
				if(check_range(dstoff_start_d,0,6)){
					document.form.dst_start_d.value = dstoff_start_d;
				}

				dstoff_start_h = parseInt(dstoff_start[2].split("/")[1]);
				if(check_range(dstoff_start_h,0,23)){
					document.form.dst_start_h.value = dstoff_start_h;
				}
			}
		}

		if(dstoffset_startend[1] != "" && dstoffset_startend[1] != undefined){
			var dstoffset_end = trim(dstoffset_startend[1]);
			var dstoff_end = dstoffset_end.split(".");

			dstoff_end_m = parseInt(dstoff_end[0].substring(1));
			if(check_range(dstoff_end_m,1,12)){
				document.form.dst_end_m.value = dstoff_end_m;
			}

			if(dstoff_end[1] != "" && dstoff_end[1] != undefined){
				dstoff_end_w = parseInt(dstoff_end[1]);
				if(check_range(dstoff_end_w,1,5)){
					document.form.dst_end_w.value = dstoff_end_w;
				}
			}

			if(dstoff_end[2] != "" && dstoff_end[2] != undefined){
				dstoff_end_d = parseInt(dstoff_end[2].split("/")[0]);
				if(check_range(dstoff_end_d,0,6)){
					document.form.dst_end_d.value = dstoff_end_d;
				}

				dstoff_end_h = parseInt(dstoff_end[2].split("/")[1]);
				if(check_range(dstoff_end_h,0,23)){
					document.form.dst_end_h.value = dstoff_end_h;
				}
			}
		}
	}
}

function check_range(obj, first, last){
	if(obj != "NaN" && first <= obj && obj <= last)
		return true;
	else
		return false;
}

function load_dst_m_Options(){
	free_options(document.form.dst_start_m);
	free_options(document.form.dst_end_m);
	for(var i = 1; i < dst_month.length; i++){
		if(!dstoffset){		//none dst_offset
			if(i==3){
				add_option(document.form.dst_start_m, dst_month[i], i, 1);
				add_option(document.form.dst_end_m, dst_month[i], i, 0);
			}else if(i==10){
				add_option(document.form.dst_start_m, dst_month[i], i, 0);
				add_option(document.form.dst_end_m, dst_month[i], i, 1);
			}else{
				add_option(document.form.dst_start_m, dst_month[i], i, 0);
				add_option(document.form.dst_end_m, dst_month[i], i, 0);
			}
		}else{
			if(dstoff_start_m == i)
				add_option(document.form.dst_start_m, dst_month[i], i, 1);
			else
				add_option(document.form.dst_start_m, dst_month[i], i, 0);

			if(dstoff_end_m == i)
				add_option(document.form.dst_end_m, dst_month[i], i, 1);
			else
				add_option(document.form.dst_end_m, dst_month[i], i, 0);
		}

	}
}

function load_dst_w_Options(){
	free_options(document.form.dst_start_w);
	free_options(document.form.dst_end_w);
	for(var i = 1; i < dst_week.length; i++){
		if(!dstoffset){		//none dst_offset
			if(i==2){
				add_option(document.form.dst_start_w, dst_week[i], i, 1);
				add_option(document.form.dst_end_w, dst_week[i], i, 1);
			}else{
				add_option(document.form.dst_start_w, dst_week[i], i, 0);
				add_option(document.form.dst_end_w, dst_week[i], i, 0);
			}
		}else{
			if(dstoff_start_w == i)
				add_option(document.form.dst_start_w, dst_week[i], i, 1);
			else
				add_option(document.form.dst_start_w, dst_week[i], i, 0);

			if(dstoff_end_w == i)
				add_option(document.form.dst_end_w, dst_week[i], i, 1);
			else
				add_option(document.form.dst_end_w, dst_week[i], i, 0);
		}

	}
}

function load_dst_d_Options(){
	free_options(document.form.dst_start_d);
	free_options(document.form.dst_end_d);
	for(var i = 0; i < dst_day.length; i++){
		if(!dstoffset){		//none dst_offset
			if(i==0){
				add_option(document.form.dst_start_d, dst_day[i], i, 1);
				add_option(document.form.dst_end_d, dst_day[i], i, 1);
			}else{
				add_option(document.form.dst_start_d, dst_day[i], i, 0);
				add_option(document.form.dst_end_d, dst_day[i], i, 0);
			}
		}else{
			if(dstoff_start_d == i)
				add_option(document.form.dst_start_d, dst_day[i], i, 1);
			else
				add_option(document.form.dst_start_d, dst_day[i], i, 0);

			if(dstoff_end_d == i)
				add_option(document.form.dst_end_d, dst_day[i], i, 1);
			else
				add_option(document.form.dst_end_d, dst_day[i], i, 0);
		}

	}
}

function load_dst_h_Options(){
	free_options(document.form.dst_start_h);
	free_options(document.form.dst_end_h);
	for(var i = 0; i < dst_hour.length; i++){
		if(!dstoffset){		//none dst_offset
			if(i==2){
				add_option(document.form.dst_start_h, dst_hour[i], i, 1);
				add_option(document.form.dst_end_h, dst_hour[i], i, 1);
			}else{
				add_option(document.form.dst_start_h, dst_hour[i], i, 0);
				add_option(document.form.dst_end_h, dst_hour[i], i, 0);
			}
		}else{
			if(dstoff_start_h == i)
				add_option(document.form.dst_start_h, dst_hour[i], i, 1);
			else
				add_option(document.form.dst_start_h, dst_hour[i], i, 0);

			if(dstoff_end_h == i)
				add_option(document.form.dst_end_h, dst_hour[i], i, 1);
			else
				add_option(document.form.dst_end_h, dst_hour[i], i, 0);
		}
	}
}

function update_jffs2_msg(){
	$("jffs_format_msg").style.display = (document.form.jffs2_on[0].checked) ? "none" : ((document.form.jffs2_format[0].checked) ? "" : "none");
	$("jffs_log_msg").style.display = (document.form.jffs2_on[0].checked) ? "none" : ((document.form.jffs2_log[0].checked) ? "" : "none");
	$("jffs_script_msg").style.display = (document.form.jffs2_on[0].checked) ? "none" : ((document.form.jffs2_scripts[0].checked) ? "" : "none");
}

function hide_http_lanport(_value){
	var http_lanport_num = "<% nvram_get("http_lanport"); %>";
	var http_lanport_num_new = (http_lanport_num == "80") ? "" : http_lanport_num;
	var port_sep = (http_lanport_num == "80") ? "" : ":";
	$("http_lanport").style.display = (_value == "1") ? "none" : "";
	document.form.http_lanport.value = "<% nvram_get("http_lanport"); %>";
	if(sw_mode == '1'){
		$("http_access_page").innerHTML = "<#https_access_url#> ";
		$("http_access_page").innerHTML += "<a href=\"http://"+theUrl+port_sep+http_lanport_num_new+"\" target=\"_blank\" style=\"color:#FC0;text-decoration: underline; font-family:Lucida Console;\">http://"+theUrl+"<span>"+port_sep+http_lanport_num_new+"</span></a>";
		$("http_access_page").style.display = (_value == "1") ? "none" : "";
	}
	else{
		$("http_access_page").style.display = "none";
	}
}

function hide_https_lanport(_value){
	var https_lanport_num = "<% nvram_get("https_lanport"); %>";
	var https_lanport_num_new = https_lanport_num;
	var port_sep = ":";
	$("https_lanport").style.display = (_value == "0") ? "none" : "";
	document.form.https_lanport.value = "<% nvram_get("https_lanport"); %>";
	if(sw_mode == '1'){
		$("https_access_page").innerHTML = "<#https_access_url#> ";
		$("https_access_page").innerHTML += "<a href=\"https://"+theUrl+port_sep+https_lanport_num_new+"\" target=\"_blank\" style=\"color:#FC0;text-decoration: underline; font-family:Lucida Console;\">http<span>s</span>://"+theUrl+"<span>"+port_sep+https_lanport_num_new+"</span></a>";
		$("https_access_page").style.display = (_value == "0") ? "none" : "";
	}
	else{
		$("https_access_page").style.display = "none";
	}
}

function hide_https_wanport(_value){

	$("misc_http_x_tr").style.display = (_value == "0") ? "none" : "";
	$("http_port").style.display = "none";
	$("https_port").style.display = (_value == "0") ? "none" : "";
	$("https_cert").style.display = (_value == "0") ? "none" : "";
}

function hide_ntpd_hint(_value){
	var SNTPstr = "&nbsp;&nbsp;Access SNTP server via&nbsp;&nbsp;<span style=\"color:#FC0; text-decoration: underline; font-family:Lucida Console;\">"+theUrl+"</span>"
	$("ntpd_hint").innerHTML = SNTPstr;
	$("ntpd_hint").style.display = (_value == "1") ? "none" : "";
}

// show clientlist
function show_http_clientlist(){
	var http_clientlist_row = http_clientlist_array.split('&#60');
	var code = "";
	code +='<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="list_table" id="http_clientlist_table">';
	if(http_clientlist_row.length == 1)
		code +='<tr><td style="color:#FFCC00;"><#IPConnection_VSList_Norule#></td>';
	else{
		for(var i =1; i < http_clientlist_row.length; i++){
		code +='<tr id="row'+i+'">';
		code +='<td width="80%">'+ http_clientlist_row[i] +'</td>';		//Url keyword
		code +='<td width="20%">';
		code +="<input class=\"remove_btn\" type=\"button\" onclick=\"deleteRow(this);\" value=\"\"/></td>";
		}
	}
  	code +='</tr></table>';

	$("http_clientlist_Block").innerHTML = code;
}

function deleteRow(r){
	var i=r.parentNode.parentNode.rowIndex;
	$('http_clientlist_table').deleteRow(i);

	var http_clientlist_value = "";
	for(i=0; i<$('http_clientlist_table').rows.length; i++){
		http_clientlist_value += "&#60";
		http_clientlist_value += $('http_clientlist_table').rows[i].cells[0].innerHTML;
	}

	http_clientlist_array = http_clientlist_value;
	if(http_clientlist_array == "")
		show_http_clientlist();
}

function addRow(obj, upper){
	if('<% nvram_get("http_client"); %>' != "1")
		document.form.http_client[0].checked = true;

	//Viz check max-limit
	var rule_num = $('http_clientlist_table').rows.length;
	var item_num = $('http_clientlist_table').rows[0].cells.length;
	if(rule_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;
	}

	if(obj.value == ""){
		alert("<#JS_fieldblank#>");
		obj.focus();
		obj.select();
		return false;
	}
	else if(valid_IP_form(obj, 0) != true){
		return false;
	}
	else{
		//Viz check same rule
		for(i=0; i<rule_num; i++){
			for(j=0; j<item_num-1; j++){		//only 1 value column
				if(obj.value == $('http_clientlist_table').rows[i].cells[j].innerHTML){
					alert("<#JS_duplicate#>");
					return false;
				}
			}
		}

		http_clientlist_array += "&#60";
		http_clientlist_array += obj.value;
		obj.value = "";
		show_http_clientlist();
	}
}

function keyBoardListener(evt){
	var nbr = (window.evt)?event.keyCode:event.which;
	if(nbr == 13)
		addRow(document.form.http_client_ip_x_0, 4);
}

//Viz add 2012.02 LAN client ip { start

function showLANIPList(){
	var code = "";
	var show_name = "";
	var client_list_array = '<% get_client_detail_info(); %>';
	var client_list_row = client_list_array.split('<');

	for(var i = 1; i < client_list_row.length; i++){
		var client_list_col = client_list_row[i].split('>');
		if(client_list_col[1] && client_list_col[1].length > 20)
			show_name = client_list_col[1].substring(0, 16) + "..";
		else
			show_name = client_list_col[1];

		//client_list_col[]  0:type 1:device 2:ip 3:mac 4: 5: 6:
		code += '<a><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setClientIP(\''+client_list_col[2]+'\');"><strong>'+client_list_col[2]+'</strong> ';

		if(show_name && show_name.length > 0)
				code += '( '+show_name+')';
		code += ' </div></a>';
		}
	code +='<!--[if lte IE 6.5]><iframe class="hackiframe2"></iframe><![endif]-->';
	$("ClientList_Block_PC").innerHTML = code;
}

function setClientIP(ipaddr){
	document.form.http_client_ip_x_0.value = ipaddr;
	hideClients_Block();
	over_var = 0;
}

var over_var = 0;
var isMenuopen = 0;

function hideClients_Block(){
	$("pull_arrow").src = "/images/arrow-down.gif";
	$('ClientList_Block_PC').style.display='none';
	isMenuopen = 0;
}

function pullLANIPList(obj){
	if(isMenuopen == 0){
		obj.src = "/images/arrow-top.gif"
		$("ClientList_Block_PC").style.display = 'block';
		document.form.http_client_ip_x_0.focus();
		isMenuopen = 1;
	}
	else
		hideClients_Block();
}
//Viz add 2012.02 LAN client ip } end

function hideport(flag){
	$("accessfromwan_port").style.display = (flag == 1) ? "" : "none";
}

//Viz add 2012.12 show url for https [start]
function change_url(num, flag){
	var port_sep = ":";
	if(flag == 'http_lan'){
                var http_lanport_num_new = num;
		if (num == '80'){
			port_sep = "";
			http_lanport_num_new = "";
		}
                $("http_access_page").innerHTML = "<#https_access_url#> ";
                $("http_access_page").innerHTML += "<a href=\"http://"+theUrl+port_sep+http_lanport_num_new+"\" target=\"_blank\" style=\"color:#FC0;text-decoration: underline; font-family:Lucida Console;\">http://"+theUrl+"<span>"+port_sep+http_lanport_num_new+"</span></a>";
        }
	if(flag == 'https_lan'){
		var https_lanport_num_new = num;
		$("https_access_page").innerHTML = "<#https_access_url#> ";
		$("https_access_page").innerHTML += "<a href=\"https://"+theUrl+port_sep+https_lanport_num_new+"\" target=\"_blank\" style=\"color:#FC0;text-decoration: underline; font-family:Lucida Console;\">http<span>s</span>://"+theUrl+"<span>"+port_sep+https_lanport_num_new+"</span></a>";
	}
}
//Viz add 2012.12 show url for https [end]

/* password item show or not */
function pass_checked(obj){
	switchType(obj, document.form.show_pass_1.checked, true);
}

function select_time_zone(){
	var tzdst = new RegExp("^[a-zA-Z]+[0-9\-\.:]+[a-zA-Z]+", "gi"); // match "[std name][offset][dst name]"
	document.getElementById('dstzone').innerHTML = "<#General_x_SystemTime_dst#><br>";

	if(document.form.time_zone_select.value.match(tzdst)){
		document.form.time_zone_dst.value=1;
		parse_dstoffset();
		document.getElementById('dstzone').style.display = "";
		document.getElementById("dst_start").style.display="";
		document.getElementById("dst_end").style.display="";
	}else{
		document.form.time_zone_dst.value=0;
		document.getElementById('dstzone').style.display = "none";
		document.getElementById("dst_start").style.display="none";
		document.getElementById("dst_end").style.display="none";
	}
}

function clean_scorebar(obj){
	if(obj.value == "")
		document.getElementById("scorebarBorder").style.display = "none";
}

function hide_reboot_option(flag){
	document.getElementById("reboot_schedule_date_tr").style.display = (flag == 1) ? "" : "none";
	document.getElementById("reboot_schedule_time_tr").style.display = (flag == 1) ? "" : "none";
}

function hide_dhcp_querylog_option(_value){
	document.getElementById("dhcp_querylog_tr").style.display = (_value < 7) ? "none" : "";
	document.getElementById("dhcp_message").style.display = (_value < 7) ? "" : "none";
}

function getrebootTimeRange(str, pos)
{
	if (pos == 0)
		return str.substring(7,9);
	else if (pos == 1)
		return str.substring(9,11);
}

function setrebootTimeRange(rd, rh, rm)
{
	return(rd.value+rh.value+rm.value);
}

function updateDateTime()
{
	document.form.reboot_schedule.value = setDateCheck(
	document.form.reboot_date_x_Sun,
	document.form.reboot_date_x_Mon,
	document.form.reboot_date_x_Tue,
	document.form.reboot_date_x_Wed,
	document.form.reboot_date_x_Thu,
	document.form.reboot_date_x_Fri,
	document.form.reboot_date_x_Sat);
	document.form.reboot_schedule.value = setrebootTimeRange(
	document.form.reboot_schedule,
	document.form.reboot_time_x_hour,
	document.form.reboot_time_x_min)
}

</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Advanced_System_Content.asp">
<input type="hidden" name="next_page" value="Advanced_System_Content.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="flag" value="">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_script" value="restart_time;restart_upnp">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="time_zone_dst" value="<% nvram_get("time_zone_dst"); %>">
<input type="hidden" name="time_zone" value="<% nvram_get("time_zone"); %>">
<input type="hidden" name="time_zone_dstoff" value="<% nvram_get("time_zone_dstoff"); %>">
<input type="hidden" name="http_passwd" value="" disabled>
<input type="hidden" name="http_clientlist" value="<% nvram_get("http_clientlist"); %>">
<input type="hidden" name="https_crt_gen" value="<% nvram_get("https_crt_gen"); %>">
<input type="hidden" name="lan_port" value="<% nvram_get("lan_port"); %>">
<input type="hidden" name="reboot_schedule" value="<% nvram_get_x("", "reboot_schedule"); %>">
<input type="hidden" name="reboot_schedule_enable" value="<% nvram_get_x("","reboot_schedule_enable"); %>">

<table class="content" align="center" cellpadding="0" cellspacing="0">
  <tr>
	<td width="17">&nbsp;</td>

	<!--=====Beginning of Main Menu=====-->
	<td valign="top" width="202">
	  <div id="mainMenu"></div>
	  <div id="subMenu"></div>
	</td>

    <td valign="top">
	<div id="tabMenu" class="submenuBlock"></div>
		<!--===================================Beginning of Main Content===========================================-->
<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td valign="top">
<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
	<tbody>

	<tr>
  <td bgcolor="#4D595D" valign="top">
		  <div>&nbsp;</div>
		  <div class="formfonttitle"><#menu5_6_adv#> - <#menu5_6_2#></div>
		  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		  <div class="formfontdesc"><#Syste_title#></div>

	  	<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
	  	<thead>
	  	<tr>
          <td colspan="2"><#PASS_changepasswd#></td>
        </tr>
    	</thead>
        <tr>
          <th width="40%"><#Router_Login_Name#></th>
          <td>
				<div><input type="text" id="http_username" name="http_username" tabindex="1" style="height:25px;" class="input_15_table" maxlength="20"><br/><span id="alert_msg1" style="color:#FC0;margin-left:8px;"></span></div>
          </td>
        </tr>

        <tr>
          <th width="40%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(11,4)"><#PASS_new#></a></th>
          <td>
            <input type="password" readonly autocapitalization="off" name="http_passwd2" tabindex="2" onFocus=" $(this).removeAttribute('readonly');" onKeyPress="return is_string(this, event);" onkeyup="chkPass(this.value, 'http_passwd');" class="input_15_table" maxlength="64" onBlur="clean_scorebar(this);" />
            &nbsp;&nbsp;
            <div id="scorebarBorder" style="margin-left:140px; margin-top:-25px; display:none;" title="<#LANHostConfig_x_Password_itemSecur#>">
            		<div id="score"></div>
            		<div id="scorebar">&nbsp;</div>
            </div>
          </td>
        </tr>

        <tr>
          <th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(11,4)"><#PASS_retype#></a></th>
          <td>
            <input type="password" readonly autocapitalization="off" name="v_password2" tabindex="3" onFocus=" $(this).removeAttribute('readonly');" onKeyPress="return is_string(this, event);" class="input_15_table" maxlength="64" />
            <div style="margin:-25px 0px 5px 135px;"><input type="checkbox" name="show_pass_1" onclick="pass_checked(document.form.http_passwd2);pass_checked(document.form.v_password2);"><#QIS_show_pass#></div>
            <span id="alert_msg2" style="color:#FC0;margin-left:8px;"></span>

          </td>
        </tr>
      </table>

<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
                <thead>
                <tr>
          <td colspan="2">Persistent JFFS2 partition</td>
        </tr>
        </thead>
           <tr>
              <th>Enable JFFS partition</th>
              <td>
                  <input type="radio" name="jffs2_on" class="input" value="1" <% nvram_match_x("LANHostConfig", "jffs2_on", "1", "checked"); %> onchange="update_jffs2_msg();"><#checkbox_Yes#>
                  <input type="radio" name="jffs2_on" class="input" value="0" <% nvram_match_x("LANHostConfig", "jffs2_on", "0", "checked"); %> onchange="update_jffs2_msg();"><#checkbox_No#>
              </td>
          </tr>
          <tr>
              <th>Format JFFS partition at next boot</th>
              <td>
                  <input type="radio" name="jffs2_format" class="input" value="1" <% nvram_match_x("LANHostConfig", "jffs2_format", "1", "checked"); %> onchange="update_jffs2_msg();"><#checkbox_Yes#>
                  <input type="radio" name="jffs2_format" class="input" value="0" <% nvram_match_x("LANHostConfig", "jffs2_format", "0", "checked"); %> onchange="update_jffs2_msg();"><#checkbox_No#>
                  <span id="jffs_format_msg" style="display:none;padding-left:20px;">JFFS partition must be enabled</span>
              </td>
          </tr>
          <tr id="jffs2_scripts_tr">
              <th>Enable JFFS custom scripts and configs</th>
              <td>
                  <input type="radio" name="jffs2_scripts" class="input" value="1" <% nvram_match("jffs2_scripts", "1", "checked"); %> onchange="update_jffs2_msg();"><#checkbox_Yes#>
                  <input type="radio" name="jffs2_scripts" class="input" value="0" <% nvram_match("jffs2_scripts", "0", "checked"); %> onchange="update_jffs2_msg();"><#checkbox_No#>
                  <span id="jffs_script_msg" style="display:none;padding-left:20px;">JFFS partition must be enabled</span>
              </td>
          </tr>
          <tr style="display:none;" id="jffs2log">
              <th>Save syslog to JFFS</i></th>
              <td>
                  <input type="radio" name="jffs2_log" class="input" value="1" <% nvram_match("jffs2_log", "1", "checked"); %> onchange="update_jffs2_msg();"><#checkbox_Yes#>
                  <input type="radio" name="jffs2_log" class="input" value="0" <% nvram_match("jffs2_log", "0", "checked"); %> onchange="update_jffs2_msg();"><#checkbox_No#>
                  <span id="jffs_log_msg" style="display:none;padding-left:20px;">JFFS partition must be enabled</span>
              </td>
          </tr>

      </table>

      <table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable" style="margin-top:8px;">
      	<thead>
	  	<tr>
          <td colspan="2"><#t2Misc#></td>
        </tr>
    	</thead>
	<tr id="btn_ez_radiotoggle_tr">
		<th><#WPS_btn_behavior#></th>
		<td>
			<input type="radio" name="btn_ez_radiotoggle" class="input" value="1" <% nvram_match_x("", "btn_ez_radiotoggle", "1", "checked"); %>><#WPS_btn_toggle#>
			<input type="radio" name="btn_ez_radiotoggle" class="input" value="0" <% nvram_match_x("", "btn_ez_radiotoggle", "0", "checked"); %>><#WPS_btn_actWPS#>
		</td>
	</tr>
	<tr id="reboot_schedule_enable_tr">
		<th width="40%" align="right">Enable Reboot Schedule</th>
			<td width="300">
				<input type="radio" value="1" name="reboot_schedule_enable"  onClick="hide_reboot_option(1);return change_common_radio(this, 'LANHostConfig', 'reboot_schedule_enable', '1')" <% nvram_match_x("LANHostConfig","reboot_schedule_enable", "1", "checked"); %>><#checkbox_Yes#>
				<input type="radio" value="0" name="reboot_schedule_enable"  onClick="hide_reboot_option(0);return change_common_radio(this, 'LANHostConfig', 'reboot_schedule_enable', '0')" <% nvram_match_x("LANHostConfig","reboot_schedule_enable", "0", "checked"); %>><#checkbox_No#>
			</td>
	</tr>
	<tr id="reboot_schedule_date_tr">
		<th>Day(s) to Reboot</th>
			<td>
				<input type="checkbox" name="reboot_date_x_Sun" class="input" onChange="return changeDate();"><#date_Sun_itemdesc#>
				<input type="checkbox" name="reboot_date_x_Mon" class="input" onChange="return changeDate();"><#date_Mon_itemdesc#>
				<input type="checkbox" name="reboot_date_x_Tue" class="input" onChange="return changeDate();"><#date_Tue_itemdesc#>
				<input type="checkbox" name="reboot_date_x_Wed" class="input" onChange="return changeDate();"><#date_Wed_itemdesc#>
				<input type="checkbox" name="reboot_date_x_Thu" class="input" onChange="return changeDate();"><#date_Thu_itemdesc#>
				<input type="checkbox" name="reboot_date_x_Fri" class="input" onChange="return changeDate();"><#date_Fri_itemdesc#>
				<input type="checkbox" name="reboot_date_x_Sat" class="input" onChange="return changeDate();"><#date_Sat_itemdesc#>
			</td>
	</tr>
	<tr id="reboot_schedule_time_tr">
		<th>Time of Day to Reboot</th>
			<td>
				<input type="text" maxlength="2" class="input_3_table" name="reboot_time_x_hour" onKeyPress="return is_number(this,event);" onblur="validate_timerange(this, 0);" autocorrect="off" autocapitalize="off"> :
				<input type="text" maxlength="2" class="input_3_table" name="reboot_time_x_min" onKeyPress="return is_number(this,event);" onblur="validate_timerange(this, 1);" autocorrect="off" autocapitalize="off">
			</td>
	</tr>
	</table>

	<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable" style="margin-top:8px;">
        <thead>
                <tr>
          <td colspan="2">Syslog</td>
        </tr>
        </thead>
        <tr>
          <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(11,1)"><#LANHostConfig_x_ServerLogEnable_itemname#></a></th>
          <td><input type="text" maxlength="15" class="input_15_table" name="log_ipaddr" value="<% nvram_get("log_ipaddr"); %>" onKeyPress="return is_ipaddr(this, event)" ></td>
        </tr>
        <tr>
	  <th>Default router message log level</th>
	  <td>
		<select name="message_loglevel" class="input_option">
			<option value="0" <% nvram_match("message_loglevel", "0", "selected"); %>>Emergency</option>
			<option value="1" <% nvram_match("message_loglevel", "1", "selected"); %>>Alert</option>
			<option value="2" <% nvram_match("message_loglevel", "2", "selected"); %>>Critical</option>
			<option value="3" <% nvram_match("message_loglevel", "3", "selected"); %>>Error</option>
			<option value="4" <% nvram_match("message_loglevel", "4", "selected"); %>>Warning</option>
			<option value="5" <% nvram_match("message_loglevel", "5", "selected"); %>>Notice</option>
			<option value="6" <% nvram_match("message_loglevel", "6", "selected"); %>>Info</option>
			<option value="7" <% nvram_match("message_loglevel", "7", "selected"); %>>Debug</option>
		</select>
		<span>&nbsp;&nbsp;Default: Notice</span>
	  </td>
	</tr>
	<tr>
	  <th>Syslog log level</th>
	  <td>
		<select name="log_level" class="input_option">
			<option value="1" onClick="hide_dhcp_querylog_option(this.value);" <% nvram_match("log_level", "1", "selected"); %>>Emergency</option>
			<option value="2" onClick="hide_dhcp_querylog_option(this.value);" <% nvram_match("log_level", "2", "selected"); %>>Alert</option>
			<option value="3" onClick="hide_dhcp_querylog_option(this.value);" <% nvram_match("log_level", "3", "selected"); %>>Critical</option>
			<option value="4" onClick="hide_dhcp_querylog_option(this.value);" <% nvram_match("log_level", "4", "selected"); %>>Error</option>
			<option value="5" onClick="hide_dhcp_querylog_option(this.value);" <% nvram_match("log_level", "5", "selected"); %>>Warning</option>
			<option value="6" onClick="hide_dhcp_querylog_option(this.value);" <% nvram_match("log_level", "6", "selected"); %>>Notice</option>
			<option value="7" onClick="hide_dhcp_querylog_option(this.value);" <% nvram_match("log_level", "7", "selected"); %>>Info</option>
			<option value="8" onClick="hide_dhcp_querylog_option(this.value);" <% nvram_match("log_level", "8", "selected"); %>>Debug</option>
		</select>
		<span>&nbsp;&nbsp;Default: Info</span>
	  </td>
	</tr>
	<tr>
	  <th>Log DHCP queries</a></th>
	  <td>
	  <div id="dhcp_querylog_tr">
			<input type="radio" value="1" name="dhcpd_querylog" class="content_input_fd" onClick="return change_common_radio(this, 'LANHostConfig', 'dhcpd_querylog', '1')" <% nvram_match("dhcpd_querylog", "1", "checked"); %>><#checkbox_Yes#>
			<input type="radio" value="0" name="dhcpd_querylog" class="content_input_fd" onClick="return change_common_radio(this, 'LANHostConfig', 'dhcpd_querylog', '0')" <% nvram_match("dhcpd_querylog", "0", "checked"); %>><#checkbox_No#>
	  </div>
	  <span id="dhcp_message" style="display:none">Logging of DHCP queries is disabled by message priority level</span>
	  </td>
	</tr>
	<tr>
	  <th>Log NTP updates</th>
	  <td>
			<input type="radio" name="ntp_log_x" class="input" value="1" <% nvram_match_x("LANHostConfig", "ntp_log_x", "1", "checked"); %>><#checkbox_Yes#>
			<input type="radio" name="ntp_log_x" class="input" value="0" <% nvram_match_x("LANHostConfig", "ntp_log_x", "0", "checked"); %>><#checkbox_No#>
	  </td>
	</tr>
	<tr>
	  <th>Smaller log output</th>
	  <td>
			<input type="radio" name="log_small" class="input" value="1" <% nvram_match_x("LANHostConfig", "log_small", "1", "checked"); %>><#checkbox_Yes#>
			<input type="radio" name="log_small" class="input" value="0" <% nvram_match_x("LANHostConfig", "log_small", "0", "checked"); %>><#checkbox_No#>
	  </td>
	</tr>
	</table>

	<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable" style="margin-top:8px;">
        <thead>
                <tr>
          <td colspan="2">System Time</td>
        </tr>
        </thead>
	  <tr>
          <th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(11,2)"><#LANHostConfig_x_TimeZone_itemname#></a></th>
          <td>
            <select name="time_zone_select" class="input_option" onchange="select_time_zone();autofill_dst();">
            </select>
          	<div>
				<span id="dstzone" style="display:none;color:#FFFFFF;"></span>
          		<span id="dst_start" style="color:white;display:none;">
          				<b>DST start time</b>
          				<select name="dst_start_m" class="input_option" onchange=""></select>&nbsp;month &nbsp;
          				<select name="dst_start_w" class="input_option" onchange=""></select>&nbsp;week &nbsp;
          				<select name="dst_start_d" class="input_option" onchange=""></select>&nbsp;weekday &nbsp;
          				<select name="dst_start_h" class="input_option" onchange=""></select>&nbsp;hour &nbsp;
						<br>
          		</span>
          		<span id="dst_end" style="color:white;display:none;">
          			<b>DST end time</b>&nbsp;&nbsp;
    	      			<select name="dst_end_m" class="input_option" onchange=""></select>&nbsp;month &nbsp;
  	        			<select name="dst_end_w" class="input_option" onchange=""></select>&nbsp;week &nbsp;
	          			<select name="dst_end_d" class="input_option" onchange=""></select>&nbsp;weekday &nbsp;
          				<select name="dst_end_h" class="input_option" onchange=""></select>&nbsp;hour &nbsp;
						<br>
			</span>
		  		<div id="timezone_hint_div" style="display:none;"><span id="timezone_hint"></span></div>
          	</div>
            </td>
        </tr>
        <tr>
        	<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(11,3)"><#LANHostConfig_x_NTPServer_itemname#></a></th>
        	<td>
				<input type="text" maxlength="256" class="input_32_table" name="ntp_server0" value="<% nvram_get("ntp_server0"); %>" onKeyPress="return is_string(this, event);">
    	      	<a href="javascript:openLink('x_NTPServer1')"  name="x_NTPServer1_link" style=" margin-left:5px; text-decoration: underline;"><#LANHostConfig_x_NTPServer1_linkname#></a>
				<div id="svc_hint_div" style="display:none;"><span style="color:#FFCC00;"><#General_x_SystemTime_syncNTP#></span></div>
			</td>
        </tr>
	<tr>
		<th><#LANHostConfig_x_NTPServer_itemname#> (Alternate)</th>
		<td>
				<input type="text" maxlength="256" class="input_32_table" name="ntp_server1" value="<% nvram_get("ntp_server1"); %>" onKeyPress="return is_string(this,event);">
		</td>
	</tr>
	<tr id="ntp_interval">
		<th>Router NTP update interval (hours)</th>
		<td>
			<input type="text" maxlength="2" class="input_6_table" name="ntp_update" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 0, 24); ntp_chk(this);" value="<% nvram_get("ntp_update"); %>">
			<span id="ntpd_upd_hint">&nbsp;&nbsp;Default: 1 hour (0 disables NTP updates)</span>
		</td>
	</tr>
	<tr>
		<th>Enable router as local SNTP server</th>
		<td>
			<input type="radio" name="ntpd_server" class="input" value="1" onClick="hide_ntpd_hint(0);" <% nvram_match_x("LANHostConfig", "ntpd_server", "1", "checked"); %>><#checkbox_Yes#>
			<input type="radio" name="ntpd_server" class="input" value="0" onClick="hide_ntpd_hint(1);" <% nvram_match_x("LANHostConfig", "ntpd_server", "0", "checked"); %>><#checkbox_No#>
			<span id="ntpd_hint" style="display:none;">SNTPServer</span>
		</td>
	</tr>
	</table>
	<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable" style="margin-top:8px;">
        <thead>
                <tr>
          <td colspan="2">Command Line</td>
        </tr>
        </thead>
				<tr>
				  <th><#Enable_Telnet#></th>
				  <td>
				    <input type="radio" name="telnetd_enable" class="input" value="1" <% nvram_match_x("LANHostConfig", "telnetd_enable", "1", "checked"); %>><#checkbox_Yes#>
				    <input type="radio" name="telnetd_enable" class="input" value="0" <% nvram_match_x("LANHostConfig", "telnetd_enable", "0", "checked"); %>><#checkbox_No#>
				  </td>
				</tr>

				<tr>
					<th>Enable SSH</th>
					<td>
						<input type="radio" name="sshd_enable" class="input" value="1" <% nvram_match_x("LANHostConfig", "sshd_enable", "1", "checked"); %>><#checkbox_Yes#>
						<input type="radio" name="sshd_enable" class="input" value="0" <% nvram_match_x("LANHostConfig", "sshd_enable", "0", "checked"); %>><#checkbox_No#>
					</td>
				</tr>

				<tr>
					<th>Allow SSH Port Forwarding</th>
					<td>
						<input type="radio" name="sshd_forwarding" class="input" value="1" <% nvram_match_x("LANHostConfig", "sshd_forwarding", "1", "checked"); %>><#checkbox_Yes#>
						<input type="radio" name="sshd_forwarding" class="input" value="0" <% nvram_match_x("LANHostConfig", "sshd_forwarding", "0", "checked"); %>><#checkbox_No#>

					</td>
					</tr>

					<tr id="ssh_lanport">
						<th>SSH service port</th>
						<td>
							<input type="text" maxlength="5" class="input_6_table" name="sshd_port" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 1, 65535)" value="<% nvram_get("sshd_port"); %>">
						</td>
					</tr>

					<tr id="ssh_inactive">
						<th>SSH inactivity timeout</th>
						<td>
							<input type="text" maxlength="5" class="input_6_table" name="sshd_timeout" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 0, 1440)" value="<% nvram_get("sshd_timeout"); %>">
							<span id="ntpd_upd_hint">&nbsp;&nbsp;Default: 20 minutes (0 disables inactivity timeout)</span>
						</td>
					</tr>

					<tr>
						<th>Allow SSH access from WAN</th>
						<td>
							<input type="radio" name="sshd_wan" class="input" value="1" <% nvram_match_x("LANHostConfig", "sshd_wan", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="sshd_wan" class="input" value="0" <% nvram_match_x("LANHostConfig", "sshd_wan", "0", "checked"); %>><#checkbox_No#>
						</td>
					</tr>
					<tr>
						<th>Allow SSH password login</th>
						<td>
							<input type="radio" name="sshd_pass" class="input" value="1" <% nvram_match_x("LANHostConfig", "sshd_pass", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="sshd_pass" class="input" value="0" <% nvram_match_x("LANHostConfig", "sshd_pass", "0", "checked"); %>><#checkbox_No#>
						</td>
					</tr>
                                                <th>Enable SSH Brute Force Protection</th>
                                                <td>
                                                        <input type="radio" name="sshd_bfp" class="input" value="1" <% nvram_match_x("LANHostConfig", "sshd_bfp", "1", "checked"); %>><#checkbox_Yes#>
                                                        <input type="radio" name="sshd_bfp" class="input" value="0" <% nvram_match_x("LANHostConfig", "sshd_bfp", "0", "checked"); %>><#checkbox_No#>
                                                </td>
                                        </tr>

					<tr>
						<th>SSH Authentication key</th>
						<td>
							<textarea class="textarea_ssh_table_8" name="sshd_authkeys" maxlength="2999"><% nvram_clean_get("sshd_authkeys"); %></textarea>
							<span id="ssh_alert_msg"></span>
						</td>
				</tr>
	</table>
        <table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable" style="margin-top:8px;">
        <thead>
                <tr>
          <td colspan="2">Web Access</td>
        </tr>
        </thead>

		  	<tr id="https_tr">
					<th><#WLANConfig11b_AuthenticationMethod_itemname#></th>
					<td>
					<select name="http_enable" class="input_option" onchange="hide_http_lanport(this.value);hide_https_lanport(this.value);hide_https_wanport(this.value);">
							<option value="0" <% nvram_match("http_enable", "0", "selected"); %>>HTTP</option>
							<option value="1" <% nvram_match("http_enable", "1", "selected"); %>>HTTPS</option>
							<option value="2" <% nvram_match("http_enable", "2", "selected"); %>>BOTH</option>
					</select>
					</td>
		  	</tr>

			<tr id="http_lanport">
                                        <th>HTTP Lan port</th>
          <td>
                                                <input type="text" maxlength="5" class="input_6_table" name="http_lanport" value="<% nvram_get("http_lanport"); %>" onKeyPress="return is_number(this,event);" onBlur="change_url(this.value, 'http_lan');">
                                                <span id="http_access_page"></span>
                                        </td>
                        </tr>

		  	<tr id="https_lanport">
					<th>HTTPS Lan port</th>
          <td>
						<input type="text" maxlength="5" class="input_6_table" name="https_lanport" value="<% nvram_get("https_lanport"); %>" onKeyPress="return is_number(this,event);" onBlur="change_url(this.value, 'https_lan');">
						<span id="https_access_page"></span>
					</td>
		  	</tr>

			<tr id="https_cert">
					<th>Save HTTPS certificate</th>
					<td>
						<input type="radio" name="https_crt_save" class="input" value="1" <% nvram_match_x("", "https_crt_save", "1", "checked"); %>><#checkbox_Yes#>
						<input type="radio" name="https_crt_save" class="input" value="0" <% nvram_match_x("", "https_crt_save", "0", "checked"); %>><#checkbox_No#>
					</td>
			</tr>

        <tr id="misc_http_x_tr">
          	<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(8,2);"><#FirewallConfig_x_WanWebEnable_itemname#></a></th>
           	<td>
             		<input type="radio" value="1" name="misc_http_x" class="input" onClick="hideport(1);return change_common_radio(this, 'FirewallConfig', 'misc_http_x', '1')" <% nvram_match("misc_http_x", "1", "checked"); %>><#checkbox_Yes#>
             		<input type="radio" value="0" name="misc_http_x" class="input" onClick="hideport(0);return change_common_radio(this, 'FirewallConfig', 'misc_http_x', '0')" <% nvram_match("misc_http_x", "0", "checked"); %>><#checkbox_No#>
           	</td>
        </tr>

        <tr id="accessfromwan_port">
           	<th align="right"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(8,3);"><#FirewallConfig_x_WanWebPort_itemname#></a></th>
           	<td>
								<span style="margin-left:5px;" id="http_port">HTTP: <input type="text" maxlength="5" name="misc_httpport_x" class="input_6_table" value="<% nvram_get("misc_httpport_x"); %>" onKeyPress="return is_number(this,event);"/>&nbsp;&nbsp;</span>
								<span style="margin-left:5px;" id="https_port">HTTPS: <input type="text" maxlength="5" name="misc_httpsport_x" class="input_6_table" value="<% nvram_get("misc_httpsport_x"); %>" onKeyPress="return is_number(this,event);"/></span>
						</td>
        </tr>

		<tr>
			<th>Auto Logout</th>
			<td>
				<input type="text" class="input_3_table" maxlength="3" name="http_autologout" value='<% nvram_get("http_autologout"); %>'> min
				<span>(0: Disable)</span>
			</td>
		</tr>

				<tr id="http_client_tr">
				  <th><#System_login_specified_IP#></th>
				  <td>
				    <input type="radio" name="http_client" class="input" value="1" <% nvram_match_x("", "http_client", "1", "checked"); %>><#checkbox_Yes#>
				    <input type="radio" name="http_client" class="input" value="0" <% nvram_match_x("", "http_client", "0", "checked"); %>><#checkbox_No#>
				  </td>
				</tr>
      </table>

			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" id="http_client_table">
				<thead>
					<tr>
						<td colspan="4"><#System_login_specified_Iplist#>&nbsp;(<#List_limit#>&nbsp;4)</td>
					</tr>
				</thead>

			  <tr>
					<th width="80%"><#ConnectedClient#></th>
					<th width="20%"><#list_add_delete#></th>
				</tr>

				<tr>
					<!-- client info -->
					<td width="80%">
				 		<input type="text" class="input_32_table" maxlength="15" name="http_client_ip_x_0"  onKeyPress="" onClick="hideClients_Block();" onblur="if(!over_var){hideClients_Block();}">
						<img id="pull_arrow" height="14px;" src="/images/arrow-down.gif" style="position:absolute;*margin-left:-3px;*margin-top:1px;" onclick="pullLANIPList(this);" title="<#select_client#>" onmouseover="over_var=1;" onmouseout="over_var=0;">
						<div id="ClientList_Block_PC" class="ClientList_Block_PC"></div>
				 	</td>
					<td width="20%">
				  		<input class="add_btn" type="button" onClick="addRow(document.form.http_client_ip_x_0, 4);" value="">
					</td>
				</tr>
			</table>
     	<div id="http_clientlist_Block"></div>

     	<div class="apply_gen">
     		<input name="button" type="button" class="button_gen" onclick="applyRule();" value="<#CTL_apply#>"/>
     	</div>

      </td></tr>
</tbody>

</table></td>
</form>



        </tr>
      </table>
		<!--===================================Ending of Main Content===========================================-->
	</td>

    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>
</body>
</html>
