/*

	Copyright (C) 2008-2010 Keith Moyer, tomatovpn@keithmoyer.com

	No part of this file may be used without permission.

*/

#include "rc.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <shared.h>

// Line number as text string
#define __LINE_T__ __LINE_T_(__LINE__)
#define __LINE_T_(x) __LINE_T(x)
#define __LINE_T(x) # x

#define VPN_LOG_ERROR -1
#define VPN_LOG_NOTE 0
#define VPN_LOG_INFO 1
#define VPN_LOG_EXTRA 2
#define vpnlog(level,x...) if(nvram_get_int("vpn_debug")>=level) syslog(LOG_INFO, #level ": " __LINE_T__ ": " x)

#define CLIENT_IF_START 10
#define SERVER_IF_START 20

#define BUF_SIZE 256
#define IF_SIZE 8

extern struct nvram_tuple router_defaults[];

static int ovpn_waitfor(const char *name)
{
	int pid, n = 5;

	killall_tk_period_wait(name, 10); //wait time in seconds
	while ( (pid = pidof(name)) >= 0 && (n-- > 0) )
	{
		// Reap the zombie if it has terminated
		waitpid(pid, NULL, WNOHANG);
		sleep(1);
	}
	return (pid >= 0);
}

void start_vpnclient(int clientNum)
{
	FILE *fp;
	char iface[IF_SIZE];
	char buffer[BUF_SIZE];
	char buffer2[5120];
	char *argv[6];
	char *buf, *p;
	int argc = 0;
	enum { TLS, SECRET, CUSTOM } cryptMode = CUSTOM;
	enum { TAP, TUN } ifType = TUN;
	enum { BRIDGE, NAT, NONE } routeMode = NONE;
	int nvi, ip[4], nm[4];
	long int nvl;
	int pid;
	int userauth, useronly;
	int i;
#ifdef RTCONFIG_BCMARM
        int cpu_num = sysconf(_SC_NPROCESSORS_CONF);
#endif
#ifdef RTCONFIG_DUALWAN
	char * wans_dualwan;
	wans_dualwan = nvram_safe_get("wans_dualwan");
#endif

	sprintf(&buffer[0], "start_vpnclient%d", clientNum);
	if (getpid() != 1) {
		notify_rc(&buffer[0]);
		return;
	}

	for ( i = 1; i < 4; i++ ) {
		if (!nvram_get_int("ntp_ready")) {
			sleep(i*i);
		} else {
			i = 4;
		}
	}

	vpnlog(VPN_LOG_INFO,"VPN GUI client backend starting...");

	sprintf(&buffer[0], "vpnclient%d", clientNum);
	if ( (pid = pidof(&buffer[0])) >= 0 )
	{
		vpnlog(VPN_LOG_NOTE, "VPN Client %d already running...", clientNum);
		vpnlog(VPN_LOG_INFO,"PID: %d", pid);
		return;
	}

	sprintf(&buffer[0], "vpn_client%d_state", clientNum);
	nvram_set(&buffer[0], "1");	//initializing
	sprintf(&buffer[0], "vpn_client%d_errno", clientNum);
	nvram_set(&buffer[0], "0");

	// Determine interface
	sprintf(&buffer[0], "vpn_client%d_if", clientNum);
	if ( nvram_contains_word(&buffer[0], "tap") )
		ifType = TAP;
	else if ( nvram_contains_word(&buffer[0], "tun") )
		ifType = TUN;
	else
	{
		vpnlog(VPN_LOG_ERROR, "Invalid interface type, %.3s", nvram_safe_get(&buffer[0]));
		return;
	}

	// Build interface name
	snprintf(&iface[0], IF_SIZE, "%s%d", nvram_safe_get(&buffer[0]), clientNum+CLIENT_IF_START);

	// Determine encryption mode
	sprintf(&buffer[0], "vpn_client%d_crypt", clientNum);
	if ( nvram_contains_word(&buffer[0], "tls") )
		cryptMode = TLS;
	else if ( nvram_contains_word(&buffer[0], "secret") )
		cryptMode = SECRET;
	else if ( nvram_contains_word(&buffer[0], "custom") )
		cryptMode = CUSTOM;
	else
	{
		vpnlog(VPN_LOG_ERROR,"Invalid encryption mode, %.6s", nvram_safe_get(&buffer[0]));
		return;
	}

	// Determine if we should bridge the tunnel
	sprintf(&buffer[0], "vpn_client%d_bridge", clientNum);
	if ( ifType == TAP && nvram_get_int(&buffer[0]) == 1 )
		routeMode = BRIDGE;

	// Determine if we should NAT the tunnel
	sprintf(&buffer[0], "vpn_client%d_nat", clientNum);
	if ( (ifType == TUN || routeMode != BRIDGE) && nvram_get_int(&buffer[0]) == 1 )
		routeMode = NAT;

	// Make sure openvpn directory exists
	mkdir("/etc/openvpn", 0700);
	sprintf(&buffer[0], "/etc/openvpn/client%d", clientNum);
	mkdir(&buffer[0], 0700);

	// Make sure symbolic link exists
	sprintf(&buffer[0], "/etc/openvpn/vpnclient%d", clientNum);
	unlink(&buffer[0]);
	if ( symlink("/usr/sbin/openvpn", &buffer[0]) )
	{
		vpnlog(VPN_LOG_ERROR,"Creating symlink failed...");
		stop_vpnclient(clientNum);
		return;
	}

	// Make sure module is loaded
	modprobe("tun");
	f_wait_exists("/dev/net/tun", 5);

	// Create tap/tun interface
	sprintf(&buffer[0], "openvpn --mktun --dev %s", &iface[0]);
	for (argv[argc=0] = strtok(&buffer[0], " "); argv[argc] != NULL; argv[++argc] = strtok(NULL, " "));
	if ( _eval(argv, NULL, 0, NULL) )
	{
		vpnlog(VPN_LOG_ERROR,"Creating tunnel interface %s failed...",&iface[0]);
		stop_vpnclient(clientNum);
		return;
	}

	// Bring interface up (TAP only)
	if( ifType == TAP )
	{
		if ( routeMode == BRIDGE )
		{
			snprintf(&buffer[0], BUF_SIZE, "brctl addif %s %s", nvram_safe_get("lan_ifname"), &iface[0]);
			for (argv[argc=0] = strtok(&buffer[0], " "); argv[argc] != NULL; argv[++argc] = strtok(NULL, " "));
			if ( _eval(argv, NULL, 0, NULL) )
			{
				vpnlog(VPN_LOG_ERROR,"Adding tunnel interface to bridge failed...");
				stop_vpnclient(clientNum);
				return;
			}
		}

		snprintf(&buffer[0], BUF_SIZE, "ifconfig %s promisc up", &iface[0]);
		for (argv[argc=0] = strtok(&buffer[0], " "); argv[argc] != NULL; argv[++argc] = strtok(NULL, " "));
		if ( _eval(argv, NULL, 0, NULL) )
		{
			vpnlog(VPN_LOG_ERROR,"Bringing interface up failed...");
			stop_vpnclient(clientNum);
			return;
		}
	}

	sprintf(&buffer[0], "vpn_client%d_userauth", clientNum);
	userauth = nvram_get_int(&buffer[0]);
	sprintf(&buffer[0], "vpn_client%d_useronly", clientNum);
	useronly = userauth && nvram_get_int(&buffer[0]);

	// Build and write config file
	vpnlog(VPN_LOG_EXTRA,"Writing config file");
	sprintf(&buffer[0], "/etc/openvpn/client%d/config.ovpn", clientNum);
	fp = fopen(&buffer[0], "w");
	chmod(&buffer[0], S_IRUSR|S_IWUSR);
	fprintf(fp, "# Automatically generated configuration\n");
	fprintf(fp, "daemon\n");
	if ( cryptMode == TLS )
		fprintf(fp, "client\n");
	fprintf(fp, "dev %s\n", &iface[0]);
	fprintf(fp, "txqueuelen 1000\n");
	sprintf(&buffer[0], "vpn_client%d_proto", clientNum);
	fprintf(fp, "proto %s\n", nvram_safe_get(&buffer[0]));
	sprintf(&buffer[0], "vpn_client%d_addr", clientNum);
	fprintf(fp, "remote %s ", nvram_safe_get(&buffer[0]));
	sprintf(&buffer[0], "vpn_client%d_port", clientNum);
	fprintf(fp, "%d\n", nvram_get_int(&buffer[0]));
	if ( cryptMode == SECRET )
	{
		if ( ifType == TUN )
		{
			sprintf(&buffer[0], "vpn_client%d_local", clientNum);
			fprintf(fp, "ifconfig %s ", nvram_safe_get(&buffer[0]));
			sprintf(&buffer[0], "vpn_client%d_remote", clientNum);
			fprintf(fp, "%s\n", nvram_safe_get(&buffer[0]));
		}
		else if ( ifType == TAP )
		{
			sprintf(&buffer[0], "vpn_client%d_local", clientNum);
			fprintf(fp, "ifconfig %s ", nvram_safe_get(&buffer[0]));
			sprintf(&buffer[0], "vpn_client%d_nm", clientNum);
			fprintf(fp, "%s\n", nvram_safe_get(&buffer[0]));
		}
	}
	sprintf(&buffer[0], "vpn_client%d_retry", clientNum);
	if ( (nvi = nvram_get_int(&buffer[0])) >= 0 )
		fprintf(fp, "resolv-retry %d\n", nvi);
	else
		fprintf(fp, "resolv-retry infinite\n");
	fprintf(fp, "nobind\n");
//	fprintf(fp, "persist-key\n");
#if 0
#ifdef RTCONFIG_DUALWAN
	if (strstr(wans_dualwan, "none") != NULL)
#endif
		fprintf(fp, "persist-tun\n");	//only set if not dual wan
#endif
	sprintf(&buffer[0], "vpn_client%d_comp", clientNum);
	strlcpy(buffer2, nvram_safe_get(&buffer[0]), sizeof (buffer2));
	if (strcmp(buffer2, "-1")) {
		if (!strcmp(buffer2, "lz4") || !strcmp(buffer2, "lz4-v2") || !strcmp(buffer2, "stub") || !strcmp(buffer2, "stub-v2")) {
			fprintf(fp, "compress %s\n", buffer2);
		} else if (!strcmp(buffer2, "yes")) {
			fprintf(fp, "compress lzo\n");
		} else if (!strcmp(buffer2, "adaptive")) {
			fprintf(fp, "comp-lzo adaptive\n");
		} else if (!strcmp(buffer2, "no")) {
			fprintf(fp, "compress\n");      // Disable, but can be overriden
		}
	}

	if ( cryptMode == TLS ) {
		sprintf(&buffer[0], "vpn_client%d_ncp_enable", clientNum);
		nvi = nvram_get_int(&buffer[0]);
		sprintf(&buffer[0], "vpn_client%d_ncp_ciphers", clientNum);
		strlcpy(buffer2, nvram_safe_get(&buffer[0]), sizeof (buffer2));
		if ((nvi > 0) && (buffer2[0] != '\0')) {
			fprintf(fp, "data-ciphers %s\n", buffer2);
		} else {
			fprintf(fp, "ncp-disable\n");
			nvi = 0;
		}
	} else {
		nvi = 0;
	}

	if (nvi != 2) {
		sprintf(&buffer[0], "vpn_client%d_cipher", clientNum);
		if ( !nvram_contains_word(&buffer[0], "default") )
			fprintf(fp, "data-ciphers-fallback %s\n", nvram_safe_get(&buffer[0]));
	}

	sprintf(&buffer[0], "vpn_client%d_digest", clientNum);
	if ( !nvram_contains_word(&buffer[0], "default") )
		fprintf(fp, "auth %s\n", nvram_safe_get(&buffer[0]));
	sprintf(&buffer[0], "vpn_client%d_rgw", clientNum);
	nvi = nvram_get_int(&buffer[0]);
	if (nvi == 1)
	{
		sprintf(&buffer[0], "vpn_client%d_gw", clientNum);
		if ( ifType == TAP && nvram_safe_get(&buffer[0])[0] != '\0' )
			fprintf(fp, "route-gateway %s\n", nvram_safe_get(&buffer[0]));
		fprintf(fp, "redirect-gateway def1\n");
	}

	// For selective routing
	sprintf(&buffer[0], "/etc/openvpn/client%d/vpnrouting.sh", clientNum);
	symlink("/usr/sbin/vpnrouting.sh", &buffer[0]);
	fprintf(fp, "script-security 2\n");	// also for up/down scripts
	fprintf(fp, "route-delay 2\n");
	fprintf(fp, "route-up vpnrouting.sh\n");
	fprintf(fp, "route-pre-down vpnrouting.sh\n");

	nvi = nvram_get_int("vpn_loglevel");
	if (nvi >= 0)
		fprintf(fp, "verb %d\n", nvi);
	else
		fprintf(fp, "verb 3\n");

	if ( cryptMode == TLS )
	{
		sprintf(&buffer[0], "vpn_client%d_reneg", clientNum);
		if ( (nvl = atol(nvram_safe_get(&buffer[0]))) >= 0 )
			fprintf(fp, "reneg-sec %ld\n", nvl);

		sprintf(&buffer[0], "vpn_client%d_adns", clientNum);
		if ( nvram_get_int(&buffer[0]) >= 0 )
		{
			sprintf(&buffer[0], "/etc/openvpn/client%d/updown.sh", clientNum);
			symlink("/usr/sbin/updown-client.sh", &buffer[0]);
			fprintf(fp, "up updown.sh\n");
			fprintf(fp, "down updown.sh\n");
		}

		sprintf(&buffer[0], "vpn_client%d_hmac", clientNum);
		nvi = nvram_get_int(&buffer[0]);
		sprintf(&buffer[0], "vpn_crt_client%d_static", clientNum);
		if ( !ovpn_crt_is_empty(&buffer[0]) && nvi >= 0 )
		{
			if (nvi == 3)
				fprintf(fp, "tls-crypt static.key");
			else if (nvi == 4)
				fprintf(fp, "tls-crypt-v2 static.key");
			else
				fprintf(fp, "tls-auth static.key");

			if ( nvi < 2 )
				fprintf(fp, " %d", nvi);
			fprintf(fp, "\n");
		}

		sprintf(&buffer[0], "vpn_crt_client%d_ca", clientNum);
		if ( !ovpn_crt_is_empty(&buffer[0]) )
			fprintf(fp, "ca ca.crt\n");
		if (!useronly)
		{
			sprintf(&buffer[0], "vpn_crt_client%d_crt", clientNum);
			if ( !ovpn_crt_is_empty(&buffer[0]) )
				fprintf(fp, "cert client.crt\n");
			sprintf(&buffer[0], "vpn_crt_client%d_key", clientNum);
			if ( !ovpn_crt_is_empty(&buffer[0]) )
				fprintf(fp, "key client.key\n");
		}
		sprintf(&buffer[0], "vpn_client%d_tlsremote", clientNum);
		if (nvram_get_int(&buffer[0]))
		{
			sprintf(&buffer[0], "vpn_client%d_cn", clientNum);
			fprintf(fp, "verify-x509-name \"%s\" name\n", nvram_safe_get(&buffer[0]));
		}
		if (userauth)
			fprintf(fp, "auth-user-pass up\n");

		sprintf(&buffer[0], "vpn_crt_client%d_crl", clientNum);
                if ( !ovpn_crt_is_empty(&buffer[0]) )
                        fprintf(fp, "crl-verify crl.pem\n");
	}
	else if ( cryptMode == SECRET )
	{
		sprintf(&buffer[0], "vpn_crt_client%d_static", clientNum);
		if ( !ovpn_crt_is_empty(&buffer[0]) )
			fprintf(fp, "secret static.key\n");

	}

	// All other cryptmodes need a default up/down script set
	if (cryptMode != TLS)
	{
		sprintf(&buffer[0], "/etc/openvpn/client%d/updown.sh", clientNum);
		symlink("/usr/sbin/openvpn-event.sh", &buffer[0]);
		fprintf(fp, "up updown.sh\n");
		fprintf(fp, "down updown.sh\n");
	}

	fprintf(fp, "status-version 2\n");
	fprintf(fp, "status status 10\n"); //update status file every 10 sec

	// IPv6 currently not supported
	fprintf(fp, "pull-filter ignore \"ifconfig-ipv6\"\n");
	fprintf(fp, "pull-filter ignore \"route-ipv6\"\n");

	fprintf(fp, "\n# Custom Configuration\n");
	sprintf(&buffer[0], "vpn_client%d_custom", clientNum);
	p = buf = strdup(nvram_safe_get(&buffer[0]));
	remove_char(p, '\r');  //strip carriage returns
	fprintf(fp, "%s\n", p);
	fclose(fp);

	vpnlog(VPN_LOG_EXTRA,"Done writing config file");

	// Write certification and key files
	vpnlog(VPN_LOG_EXTRA,"Writing certs/keys");
	if ( cryptMode == TLS )
	{
		sprintf(&buffer[0], "vpn_crt_client%d_ca", clientNum);
		if ( !ovpn_crt_is_empty(&buffer[0]) )
		{
			sprintf(&buffer[0], "/etc/openvpn/client%d/ca.crt", clientNum);
			fp = fopen(&buffer[0], "w");
			chmod(&buffer[0], S_IRUSR|S_IWUSR);
			sprintf(&buffer[0], "vpn_crt_client%d_ca", clientNum);
			fprintf(fp, "%s", get_parsed_crt(&buffer[0], buffer2, sizeof(buffer2)));
			fclose(fp);
		}

		if (!useronly)
		{
			sprintf(&buffer[0], "vpn_crt_client%d_key", clientNum);
			if ( !ovpn_crt_is_empty(&buffer[0]) )
			{
				sprintf(&buffer[0], "/etc/openvpn/client%d/client.key", clientNum);
				fp = fopen(&buffer[0], "w");
				chmod(&buffer[0], S_IRUSR|S_IWUSR);
				sprintf(&buffer[0], "vpn_crt_client%d_key", clientNum);
				fprintf(fp, "%s", get_parsed_crt(&buffer[0], buffer2, sizeof(buffer2)));
				fclose(fp);
			}

			sprintf(&buffer[0], "vpn_crt_client%d_crt", clientNum);
			if ( !ovpn_crt_is_empty(&buffer[0]) )
			{
				sprintf(&buffer[0], "/etc/openvpn/client%d/client.crt", clientNum);
				fp = fopen(&buffer[0], "w");
				chmod(&buffer[0], S_IRUSR|S_IWUSR);
				sprintf(&buffer[0], "vpn_crt_client%d_crt", clientNum);
				fprintf(fp, "%s", get_parsed_crt(&buffer[0], buffer2, sizeof(buffer2)));
				fclose(fp);
			}
		}
		if (userauth)
		{
			sprintf(&buffer[0], "/etc/openvpn/client%d/up", clientNum);
			fp = fopen(&buffer[0], "w");
			chmod(&buffer[0], S_IRUSR|S_IWUSR);
			sprintf(&buffer[0], "vpn_client%d_username", clientNum);
			fprintf(fp, "%s\n", nvram_safe_get(&buffer[0]));
			sprintf(&buffer[0], "vpn_client%d_password", clientNum);
			fprintf(fp, "%s\n", nvram_safe_get(&buffer[0]));
			fclose(fp);
		}

		sprintf(&buffer[0], "vpn_crt_client%d_crl", clientNum);
		if ( !ovpn_crt_is_empty(&buffer[0]) )
		{
			sprintf(&buffer[0], "/etc/openvpn/client%d/crl.pem", clientNum);
			fp = fopen(&buffer[0], "w");
			chmod(&buffer[0], S_IRUSR|S_IWUSR);
			sprintf(&buffer[0], "vpn_crt_client%d_crl", clientNum);
			fprintf(fp, "%s", get_parsed_crt(&buffer[0], buffer2, sizeof(buffer2)));
			fclose(fp);
		}
	}
	sprintf(&buffer[0], "vpn_client%d_hmac", clientNum);
	if ( cryptMode == SECRET || (cryptMode == TLS && nvram_get_int(&buffer[0]) >= 0) )
	{
		sprintf(&buffer[0], "vpn_crt_client%d_static", clientNum);
		if ( !ovpn_crt_is_empty(&buffer[0]) )
		{
			sprintf(&buffer[0], "/etc/openvpn/client%d/static.key", clientNum);
			fp = fopen(&buffer[0], "w");
			chmod(&buffer[0], S_IRUSR|S_IWUSR);
			sprintf(&buffer[0], "vpn_crt_client%d_static", clientNum);
			fprintf(fp, "%s", get_parsed_crt(&buffer[0], buffer2, sizeof(buffer2)));
			fclose(fp);
		}
	}
	vpnlog(VPN_LOG_EXTRA,"Done writing certs/keys");

	// Handle firewall rules if appropriate
	sprintf(&buffer[0], "vpn_client%d_firewall", clientNum);
	if ( !nvram_contains_word(&buffer[0], "custom") )
	{
		// Create firewall rules
		vpnlog(VPN_LOG_EXTRA,"Creating firewall rules");
		mkdir("/etc/openvpn/fw", 0700);
		sprintf(&buffer[0], "/etc/openvpn/fw/client%d-fw.sh", clientNum);
		fp = fopen(&buffer[0], "w");
		chmod(&buffer[0], S_IRUSR|S_IWUSR|S_IXUSR);
		fprintf(fp, "#!/bin/sh\n");
		fprintf(fp, "iptables -I INPUT -i %s -j ACCEPT\n", &iface[0]);
		fprintf(fp, "iptables -I FORWARD %d -i %s -j ACCEPT\n", (nvram_match("cstats_enable", "1") ? 4 : 2), &iface[0]);
		if (nvram_match("ctf_disable", "0")) {
			fprintf(fp, "iptables -t mangle -I PREROUTING -i %s -j MARK --set-mark 0x01/0xf\n", &iface[0]);
		}
		// Setup traffic accounting
		if (nvram_match("cstats_enable", "1")) {
			ipt_account(fp, &iface[0]);
		}
		if ( routeMode == NAT )
		{
			sscanf(nvram_safe_get("lan_ipaddr"), "%d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3]);
			sscanf(nvram_safe_get("lan_netmask"), "%d.%d.%d.%d", &nm[0], &nm[1], &nm[2], &nm[3]);
//			fprintf(fp, "iptables -t nat -I POSTROUTING -s %d.%d.%d.%d/%s -o %s -j MASQUERADE\n",
//			        ip[0]&nm[0], ip[1]&nm[1], ip[2]&nm[2], ip[3]&nm[3], nvram_safe_get("lan_netmask"), &iface[0]);
			fprintf(fp, "iptables -t nat -I POSTROUTING -o %s -j MASQUERADE\n", &iface[0]);
		}
		// Disable rp_filter when in policy mode - firewall restart would re-enable it
		sprintf(&buffer[0], "vpn_client%d_rgw", clientNum);
		if (nvram_get_int(&buffer[0]) > 1) {
			fprintf(fp, "for i in /proc/sys/net/ipv4/conf/*/rp_filter ; do\n"); /* */
			fprintf(fp, "echo 0 > $i\n");
			fprintf(fp, "done\n");
		}

		fclose(fp);
		vpnlog(VPN_LOG_EXTRA,"Done creating firewall rules");

        // Run postconf custom script on it if it exists
        sprintf(&buffer[0], "openvpnclient%d.postconf", clientNum);
        sprintf(&buffer2[0], "/etc/openvpn/client%d/config.ovpn", clientNum);
        run_postconf(&buffer[0], &buffer2[0]);

		// Run the firewall rules
		vpnlog(VPN_LOG_EXTRA,"Running firewall rules");
		sprintf(&buffer[0], "/etc/openvpn/fw/client%d-fw.sh", clientNum);
		argv[0] = &buffer[0];
		argv[1] = NULL;
		_eval(argv, NULL, 0, NULL);
		vpnlog(VPN_LOG_EXTRA,"Done running firewall rules");
	}

	// Start the VPN client
	sprintf(&buffer[0], "vpn_client%d_enabled", clientNum);
	nvram_set(&buffer[0], "1");
	sprintf(&buffer[0], "vpn_client%d_off", clientNum);
	nvram_set(&buffer[0], "0");
	sprintf(&buffer[0], "vpn_clientx_enabled");
	nvram_set_int(&buffer[0], (nvram_get_int(&buffer[0])+1));
#ifdef RTCONFIG_BCMARM
        if (cpu_num > 1)
		sprintf(&buffer[0], "taskset -c %d /etc/openvpn/vpnclient%d --cd /etc/openvpn/client%d --config config.ovpn", (clientNum % 2 ? 1 : 0), clientNum, clientNum);
	else
#endif
		sprintf(&buffer[0], "/etc/openvpn/vpnclient%d --cd /etc/openvpn/client%d --config config.ovpn", clientNum, clientNum);

	vpnlog(VPN_LOG_INFO,"Starting OpenVPN: %s",&buffer[0]);
	for (argv[argc=0] = strtok(&buffer[0], " "); argv[argc] != NULL; argv[++argc] = strtok(NULL, " "));
	if ( _eval(argv, NULL, 0, &pid) )
	{
		vpnlog(VPN_LOG_ERROR,"Starting OpenVPN failed...");
		stop_vpnclient(clientNum);
		return;
	}
	vpnlog(VPN_LOG_EXTRA,"Done starting openvpn");

	// Set up cron job
	sprintf(&buffer[0], "vpn_client%d_poll", clientNum);
	if ( (nvi = nvram_get_int(&buffer[0])) > 0 )
	{
		vpnlog(VPN_LOG_EXTRA,"Adding cron job");
		argv[0] = "cru";
		argv[1] = "a";
		sprintf(&buffer[0], "CheckVPNClient%d", clientNum);
		argv[2] = &buffer[0];
		sprintf(&buffer[strlen(&buffer[0])+1], "*/%d * * * * service start_vpnclient%d", nvi, clientNum);
		argv[3] = &buffer[strlen(&buffer[0])+1];
		argv[4] = NULL;
		_eval(argv, NULL, 0, NULL);
		vpnlog(VPN_LOG_EXTRA,"Done adding cron job");
	}

#ifdef LINUX26
//	sprintf(&buffer[0], "vpn_client%d", clientNum);
//	allow_fastnat(buffer, 0);
//	try_enabling_fastnat();
#endif

	vpnlog(VPN_LOG_INFO,"VPN GUI client backend complete.");
}

void stop_vpnclient(int clientNum)
{
	int argc;
	char *argv[7];
	char buffer[BUF_SIZE];

	sprintf(&buffer[0], "stop_vpnclient%d", clientNum);
	if (getpid() != 1) {
                notify_rc(&buffer[0]);
		return;
	}

	vpnlog(VPN_LOG_INFO,"Stopping VPN GUI client backend.");

	// Remove cron job
	vpnlog(VPN_LOG_EXTRA,"Removing cron job");
	argv[0] = "cru";
	argv[1] = "d";
	sprintf(&buffer[0], "CheckVPNClient%d", clientNum);
	argv[2] = &buffer[0];
	argv[3] = NULL;
	_eval(argv, NULL, 0, NULL);
	vpnlog(VPN_LOG_EXTRA,"Done removing cron job");

	// Stop the VPN client
	vpnlog(VPN_LOG_EXTRA,"Stopping OpenVPN client.");
	sprintf(&buffer[0], "vpnclient%d", clientNum);
	if ( !ovpn_waitfor(&buffer[0]) ) {
		vpnlog(VPN_LOG_EXTRA,"OpenVPN client stopped.");
		sprintf(&buffer[0], "vpn_client%d_enabled", clientNum);
		nvram_set(&buffer[0], "0");
		sprintf(&buffer[0], "vpn_clientx_enabled");
		nvram_set_int(&buffer[0], (nvram_get_int(&buffer[0])-1));
        sprintf(&buffer[0], "vpn_client%d_off", clientNum);
		nvram_set(&buffer[0], "1");
	}

	// NVRAM setting for device type could have changed, just try to remove both
	vpnlog(VPN_LOG_EXTRA,"Removing VPN device.");
	sprintf(&buffer[0], "openvpn --rmtun --dev tap%d", clientNum+CLIENT_IF_START);
	for (argv[argc=0] = strtok(&buffer[0], " "); argv[argc] != NULL; argv[++argc] = strtok(NULL, " "));
	_eval(argv, NULL, 0, NULL);

	sprintf(&buffer[0], "openvpn --rmtun --dev tun%d", clientNum+CLIENT_IF_START);
	for (argv[argc=0] = strtok(&buffer[0], " "); argv[argc] != NULL; argv[++argc] = strtok(NULL, " "));
	_eval(argv, NULL, 0, NULL);
	vpnlog(VPN_LOG_EXTRA,"VPN device removed.");

	// Don't remove tunnel interface in case of multiple servers/clients
	//modprobe_r("tun");

	// Remove firewall rules after VPN exit
	vpnlog(VPN_LOG_EXTRA,"Removing firewall rules.");
	sprintf(&buffer[0], "/etc/openvpn/fw/client%d-fw.sh", clientNum);
	argv[0] = "sed";
	argv[1] = "-i";
	argv[2] = "s/-A/-D/g;s/-I/-D/g;s/INPUT\\ [0-9]\\ /INPUT\\ /g;s/FORWARD\\ [0-9]\\ /FORWARD\\ /g;s/PREROUTING\\ [0-9]\\ /PREROUTING\\ /g;s/POSTROUTING\\ [0-9]\\ /POSTROUTING\\ /g";
	argv[3] = &buffer[0];
	argv[4] = NULL;
	if (!_eval(argv, NULL, 0, NULL))
	{
		argv[0] = &buffer[0];
		argv[1] = NULL;
		_eval(argv, NULL, 0, NULL);
	}
	vpnlog(VPN_LOG_EXTRA,"Done removing firewall rules.");

	if ( nvram_get_int("vpn_debug") <= VPN_LOG_EXTRA )
	{
		vpnlog(VPN_LOG_EXTRA,"Removing generated files.");
		// Delete all files for this client
		sprintf(&buffer[0], "rm -rf /etc/openvpn/client%d /etc/openvpn/fw/client%d-fw.sh /etc/openvpn/vpnclient%d /etc/openvpn/dns/client%d.resolv",clientNum,clientNum,clientNum,clientNum);
		for (argv[argc=0] = strtok(&buffer[0], " "); argv[argc] != NULL; argv[++argc] = strtok(NULL, " "));
		_eval(argv, NULL, 0, NULL);

		// Attempt to remove directories.  Will fail if not empty
		rmdir("/etc/openvpn/fw");
		rmdir("/etc/openvpn/dns");
		rmdir("/etc/openvpn");
		vpnlog(VPN_LOG_EXTRA,"Done removing generated files.");
	}

#ifdef LINUX26
//	sprintf(&buffer[0], "vpn_client%d", clientNum);
//	allow_fastnat(buffer, 1);
//	try_enabling_fastnat();
#endif

	sprintf(&buffer[0], "vpn_client%d_state", clientNum);
	nvram_set(&buffer[0], "0");
	sprintf(&buffer[0], "vpn_client%d_errno", clientNum);
	nvram_set(&buffer[0], "0");

	//update_resolvconf();	//dnsmasq handled in updown.sh
	//start_dnsmasq(0);

	// Setup client routing in case some are set to be blocked when tunnel is down
	update_vpnrouting(clientNum);

	vpnlog(VPN_LOG_INFO,"VPN GUI client backend stopped.");
}

void start_vpnserver(int serverNum)
{
	FILE *fp, *ccd, *fp_client;;
	char iface[IF_SIZE];
	char buffer[BUF_SIZE];
	char buffer2[5120];
	char *argv[6], *chp, *route;
	char *buf, *p;
	int argc = 0;
	int c2c = 0;
	enum { TAP, TUN } ifType = TUN;
	enum { TLS, SECRET, CUSTOM } cryptMode = CUSTOM;
	int nvi, ip[4], nm[4];
	long int nvl;
	int pid;
	int ret;
#ifdef RTCONFIG_BCMARM
	int cpu_num = sysconf(_SC_NPROCESSORS_CONF);
#endif
	char nv1[32], nv2[32], nv3[32], fpath[128];
	int valid = 0;
	int jffs_crt = 0;
	int userauth = 0, useronly = 0;
	int i, len;

	sprintf(&buffer[0], "start_vpnserver%d", serverNum);
	if (getpid() != 1) {
		notify_rc(&buffer[0]);
		return;
	}

	for ( i = 1; i < 4; i++ ) {
		if (!nvram_get_int("ntp_ready")) {
			sleep(i*i);
		} else {
			i = 4;
		}
	}

	vpnlog(VPN_LOG_INFO,"VPN GUI server backend starting...");

	sprintf(&buffer[0], "vpnserver%d", serverNum);
	if ( (pid = pidof(&buffer[0])) >= 0 )
	{
		vpnlog(VPN_LOG_NOTE, "VPN Server %d already running...", serverNum);
		vpnlog(VPN_LOG_INFO,"PID: %d", pid);

		sprintf(&buffer[0], "vpn_server%d_state", serverNum); // re-write state
		nvram_set(&buffer[0], "2");     //running
		sprintf(&buffer[0], "vpn_server%d_errno", serverNum);
		nvram_set(&buffer[0], "0");
		return;
	}

	sprintf(&buffer[0], "vpn_server%d_state", serverNum);
	nvram_set(&buffer[0], "1");	//initializing
	sprintf(&buffer[0], "vpn_server%d_errno", serverNum);
	nvram_set(&buffer[0], "0");

	// Determine interface type
	sprintf(&buffer[0], "vpn_server%d_if", serverNum);
	if ( nvram_contains_word(&buffer[0], "tap") )
		ifType = TAP;
	else if ( nvram_contains_word(&buffer[0], "tun") )
		ifType = TUN;
	else
	{
		vpnlog(VPN_LOG_ERROR,"Invalid interface type, %.3s", nvram_safe_get(&buffer[0]));
		return;
	}

	// Build interface name
	snprintf(&iface[0], IF_SIZE, "%s%d", nvram_safe_get(&buffer[0]), serverNum+SERVER_IF_START);

	// Determine encryption mode
	sprintf(&buffer[0], "vpn_server%d_crypt", serverNum);
	if ( nvram_contains_word(&buffer[0], "tls") )
		cryptMode = TLS;
	else if ( nvram_contains_word(&buffer[0], "secret") )
		cryptMode = SECRET;
	else if ( nvram_contains_word(&buffer[0], "custom") )
		cryptMode = CUSTOM;
	else
	{
		vpnlog(VPN_LOG_ERROR,"Invalid encryption mode, %.6s", nvram_safe_get(&buffer[0]));
		return;
	}

	// Make sure openvpn directory exists
	mkdir("/etc/openvpn", 0700);
	sprintf(&buffer[0], "/etc/openvpn/server%d", serverNum);
	mkdir(&buffer[0], 0700);

	// Make sure symbolic link exists
	sprintf(&buffer[0], "/etc/openvpn/vpnserver%d", serverNum);
	unlink(&buffer[0]);
	if ( symlink("/usr/sbin/openvpn", &buffer[0]) )
	{
		vpnlog(VPN_LOG_ERROR,"Creating symlink failed...");
		stop_vpnserver(serverNum);
		return;
	}

	// Make sure module is loaded
	modprobe("tun");
	f_wait_exists("/dev/net/tun", 5);

	// Create tap/tun interface
	sprintf(&buffer[0], "openvpn --mktun --dev %s", &iface[0]);
	for (argv[argc=0] = strtok(&buffer[0], " "); argv[argc] != NULL; argv[++argc] = strtok(NULL, " "));
	if ( _eval(argv, NULL, 0, NULL) )
	{
		vpnlog(VPN_LOG_ERROR,"Creating tunnel interface %s failed...",&iface[0]);
		stop_vpnserver(serverNum);
		return;
	}


	// Add interface to LAN bridge (TAP only)
	if( ifType == TAP )
	{
		snprintf(&buffer[0], BUF_SIZE, "brctl addif %s %s", nvram_safe_get("lan_ifname"), &iface[0]);
		for (argv[argc=0] = strtok(&buffer[0], " "); argv[argc] != NULL; argv[++argc] = strtok(NULL, " "));
		if ( _eval(argv, NULL, 0, NULL) )
		{
			vpnlog(VPN_LOG_ERROR,"Adding tunnel interface to bridge failed...");
			stop_vpnserver(serverNum);
			return;
		}
	}

	// Bring interface up
	sprintf(&buffer[0], "ifconfig %s 0.0.0.0 promisc up", &iface[0]);
	for (argv[argc=0] = strtok(&buffer[0], " "); argv[argc] != NULL; argv[++argc] = strtok(NULL, " "));
	if ( _eval(argv, NULL, 0, NULL) )
	{
		vpnlog(VPN_LOG_ERROR,"Bringing up tunnel interface failed...");
		stop_vpnserver(serverNum);
		return;
	}

	// Build and write config files
	vpnlog(VPN_LOG_EXTRA,"Writing config file");
	sprintf(&buffer[0], "/etc/openvpn/server%d/config.ovpn", serverNum);
	fp = fopen(&buffer[0], "w");
	chmod(&buffer[0], S_IRUSR|S_IWUSR);
	fprintf(fp, "# Automatically generated configuration\n");
	fprintf(fp, "daemon\n");

	sprintf(&buffer[0], "/etc/openvpn/server%d/client.ovpn", serverNum);
	fp_client = fopen(&buffer[0], "w");

	if ( cryptMode == TLS )
	{
		fprintf(fp_client, "client\n");
		if ( ifType == TUN )
		{
			fprintf(fp, "topology subnet\n");
			sprintf(&buffer[0], "vpn_server%d_sn", serverNum);
			fprintf(fp, "server %s ", nvram_safe_get(&buffer[0]));
			sprintf(&buffer[0], "vpn_server%d_nm", serverNum);
			fprintf(fp, "%s\n", nvram_safe_get(&buffer[0]));
			fprintf(fp_client, "dev tun\n");
		}
		else if ( ifType == TAP )
		{
			fprintf(fp, "server-bridge");
			sprintf(&buffer[0], "vpn_server%d_dhcp", serverNum);
			if ( nvram_get_int(&buffer[0]) == 0 )
			{
				fprintf(fp, " %s ", nvram_safe_get("lan_ipaddr"));
				fprintf(fp, "%s ", nvram_safe_get("lan_netmask"));
				sprintf(&buffer[0], "vpn_server%d_r1", serverNum);
				fprintf(fp, "%s ", nvram_safe_get(&buffer[0]));
				sprintf(&buffer[0], "vpn_server%d_r2", serverNum);
				fprintf(fp, "%s", nvram_safe_get(&buffer[0]));
			}
			else
			{
				fprintf(fp, "\npush \"route 0.0.0.0 255.255.255.255 net_gateway\"");
			}

			fprintf(fp, "\n");
			fprintf(fp_client, "dev tap\n");
			fprintf(fp_client, "# Windows needs the TAP-Win32 adapter name\n");
			fprintf(fp_client, "# from the Network Connections panel\n");
			fprintf(fp_client, "# if you have more than one.  On XP SP2,\n");
			fprintf(fp_client, "# you may need to disable the firewall\n");
			fprintf(fp_client, "# for the TAP adapter.\n");
			fprintf(fp_client, ";dev-node MyTap\n");
		}
	}
	else if ( cryptMode == SECRET )
	{
		fprintf(fp_client, "mode p2p\n");

		if ( ifType == TUN )
		{
			sprintf(&buffer[0], "vpn_server%d_local", serverNum);
			fprintf(fp, "ifconfig %s ", nvram_safe_get(&buffer[0]));
			sprintf(&buffer[0], "vpn_server%d_remote", serverNum);
			fprintf(fp, "%s\n", nvram_safe_get(&buffer[0]));

			fprintf(fp_client, "dev tun\n");
			sprintf(&buffer[0], "vpn_server%d_remote", serverNum);
			fprintf(fp_client, "ifconfig %s ", nvram_safe_get(&buffer[0]));
			sprintf(&buffer[0], "vpn_server%d_local", serverNum);
			fprintf(fp_client, "%s\n", nvram_safe_get(&buffer[0]));
		}
	}
	//local binding
	fprintf(fp, "local %s\n", nvram_safe_get("wan0_ipaddr"));

	//protocol
	sprintf(&buffer[0], "vpn_server%d_proto", serverNum);
	fprintf(fp, "proto %s\n", nvram_safe_get(&buffer[0]));
	if(!strcmp(nvram_safe_get(&buffer[0]), "udp")) {
		fprintf(fp, "multihome\n");
		fprintf(fp_client, "proto %s\n", nvram_safe_get(&buffer[0]));
	} else
		fprintf(fp_client, "proto tcp-client\n");

	//port
	sprintf(&buffer[0], "vpn_server%d_port", serverNum);
	fprintf(fp, "port %d\n", nvram_get_int(&buffer[0]));

	if(nvram_get_int("ddns_enable_x"))
	{
		if (nvram_match("ddns_server_x","WWW.NAMECHEAP.COM"))
			fprintf(fp_client, "remote %s.%s %s\n", nvram_safe_get("ddns_hostname_x"), nvram_safe_get("ddns_username_x"), nvram_safe_get(&buffer[0]));
		else
			fprintf(fp_client, "remote %s %s\n",
			    (strlen(nvram_safe_get("ddns_hostname_x")) ? nvram_safe_get("ddns_hostname_x") : nvram_safe_get("wan0_ipaddr")),
			    nvram_safe_get(&buffer[0]));
	}
	else {
		fprintf(fp_client, "remote %s %s\n", nvram_safe_get("wan0_ipaddr"), nvram_safe_get(&buffer[0]));
	}

	fprintf(fp_client, "float\n");
	fprintf(fp, "dev %s\n", &iface[0]);
	fprintf(fp, "txqueuelen 1000\n");

	//cipher
	if ( cryptMode == TLS ) {
		sprintf(&buffer[0], "vpn_server%d_ncp_enable", serverNum);
		nvi = nvram_get_int(&buffer[0]);
		sprintf(&buffer[0], "vpn_server%d_ncp_ciphers", serverNum);
		strlcpy(buffer2, nvram_safe_get(&buffer[0]), sizeof (buffer2));
		if ((nvi > 0) && (buffer2[0] != '\0')) {
			fprintf(fp, "data-ciphers %s\n", buffer2);
			fprintf(fp_client, "data-ciphers %s\n", buffer2);
		} else {
			fprintf(fp, "ncp-disable\n");
			nvi = 0;
		}
	} else {
		nvi = 0;
	}
	if (nvi != 2) {
		sprintf(&buffer[0], "vpn_server%d_cipher", serverNum);
		if ( !nvram_contains_word(&buffer[0], "default") ) {
			fprintf(fp, "data-ciphers-fallback %s\n", nvram_safe_get(&buffer[0]));
			fprintf(fp_client, "data-ciphers-fallback %s\n", nvram_safe_get(&buffer[0]));
		}
 	}

	//digest
	sprintf(&buffer[0], "vpn_server%d_digest", serverNum);
	if ( !nvram_contains_word(&buffer[0], "default") ) {
		fprintf(fp, "auth %s\n", nvram_safe_get(&buffer[0]));
		fprintf(fp_client, "auth %s\n", nvram_safe_get(&buffer[0]));
	}

	//compression
	sprintf(&buffer[0], "vpn_server%d_comp", serverNum);
	strlcpy(buffer2, nvram_safe_get(&buffer[0]), sizeof (buffer2));

	if (strcmp(buffer2, "-1")) {
		if (!strcmp(buffer2, "lz4") || !strcmp(buffer2, "lz4-v2") || !strcmp(buffer2, "stub") || !strcmp(buffer2, "stub-v2")) {
			fprintf(fp, "compress %s\n", buffer2);
			fprintf(fp_client, "compress %s\n", buffer2);
		} else if (!strcmp(buffer2, "yes")) {
			fprintf(fp, "compress lzo\n");
			fprintf(fp_client, "compress lzo\n");
		} else if (!strcmp(buffer2, "adaptive")) {
			fprintf(fp, "comp-lzo adaptive\n");
			fprintf(fp_client, "comp-lzo adaptive\n");
		} else if (!strcmp(buffer2, "no")) {
			fprintf(fp, "compress\n");	// Disable, but client can override if desired
			fprintf(fp_client, "compress\n");
		}
	}

	fprintf(fp, "keepalive 15 60\n");
	fprintf(fp_client, "keepalive 15 60\n");

	if ( (nvi = nvram_get_int("vpn_loglevel")) >= 0 )
		fprintf(fp, "verb %d\n", nvi);
	else
		fprintf(fp, "verb 3\n");

	if ( cryptMode == TLS )
	{
		//TLS Renegotiation Time
		sprintf(&buffer[0], "vpn_server%d_reneg", serverNum);
		if ( (nvl = atol(nvram_safe_get(&buffer[0]))) >= 0 ) {
			fprintf(fp, "reneg-sec %ld\n", nvl);
			fprintf(fp_client, "reneg-sec %ld\n", nvl);
		}

		sprintf(&buffer[0], "vpn_server%d_plan", serverNum);
		if ( ifType == TUN && nvram_get_int(&buffer[0]) )
		{
			sscanf(nvram_safe_get("lan_ipaddr"), "%d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3]);
			sscanf(nvram_safe_get("lan_netmask"), "%d.%d.%d.%d", &nm[0], &nm[1], &nm[2], &nm[3]);
			fprintf(fp, "push \"route %d.%d.%d.%d %s\"\n", ip[0]&nm[0], ip[1]&nm[1], ip[2]&nm[2], ip[3]&nm[3],
			        nvram_safe_get("lan_netmask"));
		}

		sprintf(&buffer[0], "vpn_server%d_ccd", serverNum);
		if ( nvram_get_int(&buffer[0]) )
		{
			fprintf(fp, "client-config-dir ccd\n");

			sprintf(&buffer[0], "vpn_server%d_c2c", serverNum);
			if ( (c2c = nvram_get_int(&buffer[0])) )
				fprintf(fp, "client-to-client\n");

			sprintf(&buffer[0], "vpn_server%d_ccd_excl", serverNum);
			if ( nvram_get_int(&buffer[0]) )
				fprintf(fp, "ccd-exclusive\n");
			else
				fprintf(fp, "duplicate-cn\n");


			sprintf(&buffer[0], "/etc/openvpn/server%d/ccd", serverNum);
			mkdir(&buffer[0], 0700);
			chdir(&buffer[0]);

			sprintf(&buffer[0], "vpn_server%d_ccd_val", serverNum);
			strcpy(&buffer[0], nvram_safe_get(&buffer[0]));
			chp = strtok(&buffer[0],"<");
			while ( chp != NULL )
			{
				nvi = strlen(chp);

				chp[strcspn(chp,">")] = '\0';
				vpnlog(VPN_LOG_EXTRA,"CCD: enabled: %d", atoi(chp));
				if ( atoi(chp) == 1 )
				{
					nvi -= strlen(chp)+1;
					chp += strlen(chp)+1;

					ccd = NULL;
					route = NULL;
					if ( nvi > 0 )
					{
						chp[strcspn(chp,">")] = '\0';
						vpnlog(VPN_LOG_EXTRA,"CCD: Common name: %s", chp);
						ccd = fopen(chp, "w");
						chmod(chp, S_IRUSR|S_IWUSR);

						nvi -= strlen(chp)+1;
						chp += strlen(chp)+1;
					}
					if ( nvi > 0 && ccd != NULL && strcspn(chp,">") != strlen(chp) )
					{
						chp[strcspn(chp,">")] = ' ';
						chp[strcspn(chp,">")] = '\0';
						route = chp;
						vpnlog(VPN_LOG_EXTRA,"CCD: Route: %s", chp);
						if ( strlen(route) > 1 )
						{
							fprintf(ccd, "iroute %s\n", route);
							fprintf(fp, "route %s\n", route);
						}

						nvi -= strlen(chp)+1;
						chp += strlen(chp)+1;
					}
					if ( ccd != NULL )
						fclose(ccd);
					if ( nvi > 0 && route != NULL )
					{
						chp[strcspn(chp,">")] = '\0';
						vpnlog(VPN_LOG_EXTRA,"CCD: Push: %d", atoi(chp));
						if ( c2c && atoi(chp) == 1 && strlen(route) > 1 )
							fprintf(fp, "push \"route %s\"\n", route);

						nvi -= strlen(chp)+1;
						chp += strlen(chp)+1;
					}

					vpnlog(VPN_LOG_EXTRA,"CCD leftover: %d", nvi+1);
				}
				// Advance to next entry
				chp = strtok(NULL, "<");
			}
			// Copy any user-configured client config files
			sprintf(&buffer[0], "/jffs/configs/openvpn/ccd%d", serverNum);

			if(check_if_dir_exist(&buffer[0]))
			{
				vpnlog(VPN_LOG_EXTRA,"CCD - copying user files");
				sprintf(buffer2, "cp %s/* .", &buffer[0]); /* */
				system(buffer2);
			}

			vpnlog(VPN_LOG_EXTRA,"CCD processing complete");
		}
		else
			fprintf(fp, "duplicate-cn\n");


		sprintf(&buffer[0], "vpn_server%d_pdns", serverNum);
		if ( nvram_get_int(&buffer[0]) )
		{
			if ( nvram_safe_get("lan_domain")[0] != '\0' )
				fprintf(fp, "push \"dhcp-option DOMAIN %s\"\n", nvram_safe_get("lan_domain"));
			if ( (nvram_safe_get("wan_wins")[0] != '\0' && strcmp(nvram_safe_get("wan_wins"), "0.0.0.0") != 0) )
				fprintf(fp, "push \"dhcp-option WINS %s\"\n", nvram_safe_get("wan_wins"));
			fprintf(fp, "push \"dhcp-option DNS %s\"\n", nvram_safe_get("lan_ipaddr"));
		}

		sprintf(&buffer[0], "vpn_server%d_rgw", serverNum);
		if ( nvram_get_int(&buffer[0]) )
		{
			if ( ifType == TAP )
				fprintf(fp, "push \"route-gateway %s\"\n", nvram_safe_get("lan_ipaddr"));
			fprintf(fp, "push \"redirect-gateway def1\"\n");
		}

		sprintf(&buffer[0], "vpn_server%d_hmac", serverNum);
		nvi = nvram_get_int(&buffer[0]);
		//sprintf(&buffer[0], "vpn_crt_server%d_static", serverNum);
		//if ( !ovpn_crt_is_empty(&buffer[0]) && nvi >= 0 )
		if ( nvi >= 0 )
		{
			if (nvi == 3)
				fprintf(fp, "tls-crypt static.key");
			else if (nvi == 4)
				fprintf(fp, "tls-crypt-v2 static.key");
			else
				fprintf(fp, "tls-auth static.key");

			if ( nvi < 2 )
				fprintf(fp, " %d", nvi);
			fprintf(fp, "\n");
		}

		// Enable username/pass authentication (for Asus's PAM support)
		sprintf(&buffer[0], "vpn_server%d_userpass_auth", serverNum);
		userauth = nvram_get_int(&buffer[0]);
		sprintf(&buffer[0], "vpn_server%d_igncrt", serverNum);
		useronly = userauth && nvram_get_int(&buffer[0]);

		if ( userauth ) {
			//authentication
#ifdef RTCONFIG_BCMARM
			fprintf(fp, "plugin /usr/lib/openvpn-plugin-auth-pam.so openvpn\n");
#else
			fprintf(fp, "auth-user-pass-verify /usr/sbin/ovpnauth.sh via-file\n");
			fprintf(fp, "script-security 2\n");
#endif
			fprintf(fp_client, "auth-user-pass\n");

			//ignore client certificate, but only if user/pass auth is enabled
			//That way, existing configuration (pre-Asus OVPN)
			//will remain unchanged.
			if ( useronly ) {
				fprintf(fp, "verify-client-cert none\n");
				fprintf(fp, "username-as-common-name\n");
			}
		}

		//sprintf(&buffer[0], "vpn_crt_server%d_ca", serverNum);
		//if ( !ovpn_crt_is_empty(&buffer[0]) )
			fprintf(fp, "ca ca.crt\n");
		//sprintf(&buffer[0], "vpn_crt_server%d_dh", serverNum);
		//if ( !ovpn_crt_is_empty(&buffer[0]) )
		sprintf(&buffer[0], "vpn_crt_server%d_dh", serverNum);
		if ( !strncmp(get_parsed_crt(&buffer[0], buffer2, sizeof(buffer2)), "none", 4))
			fprintf(fp, "dh none\n");
		else
			fprintf(fp, "dh dh.pem\n");
		//sprintf(&buffer[0], "vpn_crt_server%d_crt", serverNum);
		//if ( !ovpn_crt_is_empty(&buffer[0]) )
			fprintf(fp, "cert server.crt\n");
		//sprintf(&buffer[0], "vpn_crt_server%d_key", serverNum);
		//if ( !ovpn_crt_is_empty(&buffer[0]) )
			fprintf(fp, "key server.key\n");
		sprintf(&buffer[0], "vpn_crt_server%d_crl", serverNum);
		if ( !ovpn_crt_is_empty(&buffer[0]) )
			fprintf(fp, "crl-verify crl.pem\n");

	}
	else if ( cryptMode == SECRET )
	{
		//sprintf(&buffer[0], "vpn_crt_server%d_static", serverNum);
		//if ( !ovpn_crt_is_empty(&buffer[0]) )
			fprintf(fp, "secret static.key\n");
	}

	sprintf(&buffer[0], "/etc/openvpn/server%d/updown.sh", serverNum);
	symlink("/usr/sbin/updown-server.sh", &buffer[0]);
#ifdef RTCONFIG_BCMARM
	fprintf(fp, "script-security 2\n");
#else
	if ( !userauth )
		fprintf(fp, "script-security 2\n");
#endif
	fprintf(fp, "up updown.sh\n");
	fprintf(fp, "down updown.sh\n");

	fprintf(fp, "status-version 2\n");
	fprintf(fp, "status status 10\n"); //update status file every 10 sec
	fprintf(fp, "\n# Custom Configuration\n");
	sprintf(&buffer[0], "vpn_server%d_custom", serverNum);
	p = buf = strdup(nvram_safe_get(&buffer[0]));
	remove_char(p, '\r');  //strip carriage returns
	fprintf(fp, "%s\n", p);
	fclose(fp);

	vpnlog(VPN_LOG_EXTRA,"Done writing server config file");

	// Write certification and key files
	vpnlog(VPN_LOG_EXTRA,"Writing certs/keys");
	if ( cryptMode == TLS )
	{
		//generate certification and key
		sprintf(nv1, "vpn_crt_server%d_ca", serverNum);
		sprintf(nv2, "vpn_crt_server%d_key", serverNum);
		sprintf(nv3, "vpn_crt_server%d_crt", serverNum);
		if( ovpn_crt_is_empty(nv1) ) {
			sprintf(fpath, "/tmp/genvpncert.sh");
			fp = fopen(fpath, "w");
			if(fp) {
				fprintf(fp, "#!/bin/sh\n");
				//fprintf(fp, ". /rom/easy-rsa/vars\n");
				fprintf(fp, "export OPENSSL=\"openssl\"\n");
				fprintf(fp, "export GREP=\"grep\"\n");
				fprintf(fp, "export KEY_CONFIG=\"/rom/easy-rsa/openssl.cnf\"\n");
				fprintf(fp, "export KEY_DIR=\"/etc/openvpn/server%d\"\n", serverNum);
				fprintf(fp, "export KEY_SIZE=1024\n");
				fprintf(fp, "export CA_EXPIRE=3650\n");
				fprintf(fp, "export KEY_EXPIRE=3650\n");
				fprintf(fp, "export KEY_COUNTRY=\"TW\"\n");
				fprintf(fp, "export KEY_PROVINCE=\"TW\"\n");
				fprintf(fp, "export KEY_CITY=\"Taipei\"\n");
				fprintf(fp, "export KEY_ORG=\"ASUS\"\n");
				fprintf(fp, "export KEY_EMAIL=\"me@myhost.mydomain\"\n");
				fprintf(fp, "export KEY_CN=\"%s\"\n", nvram_safe_get("productid"));
				fprintf(fp, "touch /etc/openvpn/server%d/index.txt\n", serverNum);
				fprintf(fp, "echo 01 >/etc/openvpn/server%d/serial\n", serverNum);
				fprintf(fp, "/rom/easy-rsa/pkitool --initca\n");
				fprintf(fp, "/rom/easy-rsa/pkitool --server server\n");

				//undefined common name, default use username-as-common-name
				fprintf(fp, "export KEY_CN=\"\"\n");
				fprintf(fp, "/rom/easy-rsa/pkitool client\n");

				fclose(fp);
				chmod(fpath, 0700);
				eval(fpath);
				unlink(fpath);
			}

			//set certification and key to nvram
			sprintf(fpath, "/etc/openvpn/server%d/ca.key", serverNum);
			fp = fopen(fpath, "r");
			if(fp) {
				sprintf(&buffer[0], "vpn_crt_server%d_ca_key", serverNum);
				set_crt_parsed(&buffer[0], fpath);
				fclose(fp);
			}
			sprintf(fpath, "/etc/openvpn/server%d/ca.crt", serverNum);
			fp = fopen(fpath, "r");
			if(fp) {
				set_crt_parsed(nv1, fpath);
				fclose(fp);
			}
			sprintf(fpath, "/etc/openvpn/server%d/server.key", serverNum);
			fp = fopen(fpath, "r");
			if(fp) {
				set_crt_parsed(nv2, fpath);
				fclose(fp);
			}
			sprintf(fpath, "/etc/openvpn/server%d/server.crt", serverNum);
			fp = fopen(fpath, "r");
			if(fp) {
				set_crt_parsed(nv3, fpath);
				fclose(fp);
			}
			sprintf(fpath, "/etc/openvpn/server%d/client.key", serverNum);
			fp = fopen(fpath, "r");
			if(fp) {
				sprintf(&buffer[0], "vpn_crt_server%d_client_key", serverNum);
				set_crt_parsed(&buffer[0], fpath);
				fclose(fp);
			}
			sprintf(fpath, "/etc/openvpn/server%d/client.crt", serverNum);
			fp = fopen(fpath, "r");
			if(fp) {
				sprintf(&buffer[0], "vpn_crt_server%d_client_crt", serverNum);
				set_crt_parsed(&buffer[0], fpath);
				fclose(fp);
			}
		}
		else {
				sprintf(&buffer[0], "/etc/openvpn/server%d/ca.key", serverNum);
				fp = fopen(&buffer[0], "w");
				chmod(&buffer[0], S_IRUSR|S_IWUSR);
				sprintf(&buffer[0], "vpn_crt_server%d_ca_key", serverNum);
				fprintf(fp, "%s", get_parsed_crt(&buffer[0], buffer2, sizeof(buffer2)));
				fclose(fp);

			//sprintf(&buffer[0], "vpn_crt_server%d_ca", serverNum);
			//if ( !ovpn_crt_is_empty(&buffer[0]) )
			//{
				sprintf(&buffer[0], "/etc/openvpn/server%d/ca.crt", serverNum);
				fp = fopen(&buffer[0], "w");
				chmod(&buffer[0], S_IRUSR|S_IWUSR);
				sprintf(&buffer[0], "vpn_crt_server%d_ca", serverNum);
				fprintf(fp, "%s", get_parsed_crt(&buffer[0], buffer2, sizeof(buffer2)));
				fclose(fp);
			//}

			//sprintf(&buffer[0], "vpn_crt_server%d_key", serverNum);
			//if ( !ovpn_crt_is_empty(&buffer[0]) )
			//{
				sprintf(&buffer[0], "/etc/openvpn/server%d/server.key", serverNum);
				fp = fopen(&buffer[0], "w");
				chmod(&buffer[0], S_IRUSR|S_IWUSR);
				sprintf(&buffer[0], "vpn_crt_server%d_key", serverNum);
				fprintf(fp, "%s", get_parsed_crt(&buffer[0], buffer2, sizeof(buffer2)));
				fclose(fp);
			//}

			//sprintf(&buffer[0], "vpn_crt_server%d_crt", serverNum);
			//if ( !ovpn_crt_is_empty(&buffer[0]) )
			//{
				sprintf(&buffer[0], "/etc/openvpn/server%d/server.crt", serverNum);
				fp = fopen(&buffer[0], "w");
				chmod(&buffer[0], S_IRUSR|S_IWUSR);
				sprintf(&buffer[0], "vpn_crt_server%d_crt", serverNum);
				fprintf(fp, "%s", get_parsed_crt(&buffer[0], buffer2, sizeof(buffer2)));
				fclose(fp);
			//}

			sprintf(&buffer[0], "vpn_crt_server%d_crl", serverNum);
			if ( !ovpn_crt_is_empty(&buffer[0]) )
			{
				sprintf(&buffer[0], "/etc/openvpn/server%d/crl.pem", serverNum);
				fp = fopen(&buffer[0], "w");
				chmod(&buffer[0], S_IRUSR|S_IWUSR);
				sprintf(&buffer[0], "vpn_crt_server%d_crl", serverNum);
				fprintf(fp, "%s", get_parsed_crt(&buffer[0], buffer2, sizeof(buffer2)));
				fclose(fp);
			}

		}

		sprintf(&buffer[0], "vpn_crt_server%d_ca", serverNum);
		fprintf(fp_client, "<ca>\n");
		fprintf(fp_client, "%s", get_parsed_crt(&buffer[0], buffer2, sizeof(buffer2)));
		len = strlen(buffer2);
		if ((len) && (buffer2[len-1] != '\n'))
			fprintf(fp_client, "\n");	// Append newline if missing
		fprintf(fp_client, "</ca>\n");

		// Only do this if we do not have both userauth and useronly enabled at the same time
		if ( !(userauth && useronly) ) {
			/*
			   See if stored client cert was signed with our stored CA.  If not, it means
			   the CA was changed by the user and the current client crt/key no longer match,
			   so we should not insert them in the exported client ovp file.
			*/
			fp = fopen("/tmp/test.crt", "w");
			sprintf(&buffer[0], "vpn_crt_server%d_client_crt", serverNum);
			fprintf(fp, "%s", get_parsed_crt(&buffer[0], buffer2, sizeof(buffer2)));
			fclose(fp);

			sprintf(&buffer[0], "/usr/sbin/openssl verify -CAfile /etc/openvpn/server%d/ca.crt /tmp/test.crt > /tmp/output.txt", serverNum);
			system(&buffer[0]);
			ret = f_read_string("/tmp/output.txt", &buffer[0], 64);
	                unlink("/tmp/test.crt");

			if(ret) {
				if (!strncmp(&buffer[0],"/tmp/test.crt: OK",17)) {
					valid = 1;
				} else {
					vpnlog(VPN_LOG_EXTRA,"Cannot verify client cert");
					valid = 0;
				}
			}

			fprintf(fp_client, "<cert>\n");
			sprintf(&buffer[0], "vpn_crt_server%d_client_crt", serverNum);
			if ((valid == 1) && ( !ovpn_crt_is_empty(&buffer[0]) ) ) {
				fprintf(fp_client, "%s", get_parsed_crt(&buffer[0], buffer2, sizeof(buffer2)));
				len = strlen(buffer2);
				if ((len) && (buffer2[len-1] != '\n'))
					fprintf(fp_client, "\n");	// Append newline if missing
			} else {
				fprintf(fp_client, "    paste client certificate data here\n");
			}
			fprintf(fp_client, "</cert>\n");

			fprintf(fp_client, "<key>\n");
			sprintf(&buffer[0], "vpn_crt_server%d_client_key", serverNum);
			if ((valid == 1) && ( !ovpn_crt_is_empty(&buffer[0]) ) ) {
				fprintf(fp_client, "%s", get_parsed_crt(&buffer[0], buffer2, sizeof(buffer2)));
				len = strlen(buffer2);
				if ((len) && (buffer2[len-1] != '\n'))
					fprintf(fp_client, "\n");	// Append newline if missing
			} else {
				fprintf(fp_client, "    paste client key data here\n");
			}
			fprintf(fp_client, "</key>\n");
		}

		/*
		   Process Diffie-Hellman pem
		   Update to a pre-generated dh if the current dh is below 1024 bits
		*/
		valid = 0;  //initial state invalid for dh not present
		sprintf(&buffer[0], "vpn_crt_server%d_dh", serverNum);
		if ( !ovpn_crt_is_empty(&buffer[0]) )
		{
			//write out dh.pem from nvram
			sprintf(&buffer[0], "/etc/openvpn/server%d/dh.pem", serverNum);
			fp = fopen(&buffer[0], "w");
			chmod(&buffer[0], S_IRUSR|S_IWUSR);
			sprintf(&buffer[0], "vpn_crt_server%d_dh", serverNum);
			fprintf(fp, "%s", get_parsed_crt(&buffer[0], buffer2, sizeof(buffer2)));
			if (fclose(fp) == EOF) {
				vpnlog(VPN_LOG_EXTRA,"Error writing dh.pem from nvram!");
			} else {
				vpnlog(VPN_LOG_EXTRA,"Done writing dh.pem from nvram");
			}

			// Validate dh strength
			valid = 1;      // Tentative state
			jffs_crt = 0;

			if (!strncmp(buffer2, "/jffs", 5)) {	// nvram is pointer to a jffs file
				vpnlog(VPN_LOG_EXTRA,"Found dh.pem was moved to %s", buffer2);
				if (check_if_file_exist(buffer2))
					jffs_crt = 1;
				if (jffs_crt) {
					sprintf(fpath, "/etc/openvpn/server%d/dh.pem", serverNum);
					eval("cp", buffer2, fpath);	//copy the file to the test file
				}
			}

			if (strncmp(buffer2, "none", 4)) {	// If not set to "none" then validate it
				vpnlog(VPN_LOG_EXTRA,"Start dh length check");
				sprintf(&buffer[0], "/usr/sbin/openssl dhparam -in /etc/openvpn/server%d/dh.pem -text | grep \"DH Parameters:\" > /tmp/dhtest.txt", serverNum);
				system(&buffer[0]);
				ret = f_read_string("/tmp/dhtest.txt", &buffer[0], 64);

				if(ret) {
					if (sscanf(strstr(&buffer[0],"DH Parameters"),"DH Parameters: (%d bit)", &i)) {
						if (i < 1024) {
							valid = 0;      // Not valid after all, must regenerate
							logmessage("openvpn", "WARNING: DH for server %d is too weak (%d bit, must be at least 1024 bit)", serverNum, i);
							logmessage("openvpn", "INFO: Server %d using a pre-generated 2048-bit DH PEM", serverNum);
						} else {
							vpnlog(VPN_LOG_EXTRA,"Valid dh >= 1024 bits found");
						}
					}
				} else {
					valid = 0;
					vpnlog(VPN_LOG_EXTRA,"Error testing dh length!");
				}
			}
		}

		if (valid == 0)
		{
			if (jffs_crt) {	//update jffs with new dh
				eval("cp", "/rom/dh2048.pem", buffer2);
				vpnlog(VPN_LOG_EXTRA,"Done updating jffs with new dh");
			}
			else {
				sprintf(fpath, "/etc/openvpn/server%d/dh.pem", serverNum);
				//generate dh param file
				//eval("openssl", "dhparam", "-out", fpath, "1024");

				// Provide a 2048-bit PEM, from RFC 3526.
				eval("cp", "/rom/dh2048.pem", fpath);

				//update nvram with new dh
				fp = fopen(fpath, "r");
				if(fp) {
					sprintf(&buffer[0], "vpn_crt_server%d_dh", serverNum);
					set_crt_parsed(&buffer[0], fpath);
					fclose(fp);
					vpnlog(VPN_LOG_EXTRA,"Done updating nvram with new dh");
				}
			}
		}
	}
	sprintf(&buffer[0], "vpn_server%d_hmac", serverNum);
	if ( cryptMode == SECRET || (cryptMode == TLS && nvram_get_int(&buffer[0]) >= 0) )
	{
		sprintf(&buffer[0], "vpn_crt_server%d_static", serverNum);
		if ( !ovpn_crt_is_empty(&buffer[0]) )
		{
			sprintf(&buffer[0], "/etc/openvpn/server%d/static.key", serverNum);
			fp = fopen(&buffer[0], "w");
			chmod(&buffer[0], S_IRUSR|S_IWUSR);
			sprintf(&buffer[0], "vpn_crt_server%d_static", serverNum);
			fprintf(fp, "%s", get_parsed_crt(&buffer[0], buffer2, sizeof(buffer2)));
			fclose(fp);
		}
		else
		{	//generate openvpn static key
			sprintf(fpath, "/etc/openvpn/server%d/static.key", serverNum);
			eval("openvpn", "--genkey", "secret", fpath);
			fp = fopen(fpath, "r");
			if(fp) {
				set_crt_parsed(&buffer[0], fpath);
				fclose(fp);
			}
		}

		sprintf(&buffer[0], "vpn_server%d_hmac", serverNum);
		nvi = nvram_get_int(&buffer[0]);
		sprintf(&buffer[0], "vpn_crt_server%d_static", serverNum);
		if(cryptMode == TLS)
			if (nvi == 3)
				fprintf(fp_client, "<tls-crypt>\n");
			else if (nvi == 4)
				fprintf(fp_client, "<tls-crypt-v2>\n");
			else
				fprintf(fp_client, "<tls-auth>\n");
		else if(cryptMode == SECRET)
			fprintf(fp_client, "<secret>\n");
		fprintf(fp_client, "%s", get_parsed_crt(&buffer[0], buffer2, sizeof(buffer2)));
		len = strlen(buffer2);
		if ((len) && (buffer2[len-1] != '\n'))
			fprintf(fp_client, "\n");	// Append newline if missing
		if(cryptMode == TLS) {
			if (nvi == 3)
				fprintf(fp_client, "</tls-crypt>\n");
			else if (nvi == 4)
				fprintf(fp_client, "<tls-crypt-v2>\n");
			else
				fprintf(fp_client, "</tls-auth>\n");

			if(nvi == 1)
				fprintf(fp_client, "key-direction 0\n");
			else if(nvi == 0)
				fprintf(fp_client, "key-direction 1\n");
		}
		else if(cryptMode == SECRET)
			fprintf(fp_client, "</secret>\n");
	}

	vpnlog(VPN_LOG_EXTRA,"Done writing certs/keys");
	nvram_commit();

	fprintf(fp_client, "remote-cert-tls server\n");
	fprintf(fp_client, "resolv-retry infinite\n");
	fprintf(fp_client, "nobind\n");
	fclose(fp_client);
	vpnlog(VPN_LOG_EXTRA,"Done writing client config file");

	// Handle firewall rules if appropriate
	sprintf(&buffer[0], "vpn_server%d_firewall", serverNum);
	if ( !nvram_contains_word(&buffer[0], "custom") )
	{
		// Create firewall rules
		vpnlog(VPN_LOG_EXTRA,"Creating firewall rules");
		mkdir("/etc/openvpn/fw", 0700);
		sprintf(&buffer[0], "/etc/openvpn/fw/server%d-fw.sh", serverNum);
		fp = fopen(&buffer[0], "w");
		chmod(&buffer[0], S_IRUSR|S_IWUSR|S_IXUSR);
		fprintf(fp, "#!/bin/sh\n");
		sprintf(&buffer[0], "vpn_server%d_proto", serverNum);
		strncpy(&buffer[0], nvram_safe_get(&buffer[0]), BUF_SIZE);
		fprintf(fp, "iptables -t nat -I PREROUTING -p %s ", strtok(&buffer[0], "-"));
		sprintf(&buffer[0], "vpn_server%d_port", serverNum);
		fprintf(fp, "--dport %d -j ACCEPT\n", nvram_get_int(&buffer[0]));
		sprintf(&buffer[0], "vpn_server%d_proto", serverNum);
		strncpy(&buffer[0], nvram_safe_get(&buffer[0]), BUF_SIZE);
		fprintf(fp, "iptables -I INPUT -p %s ", strtok(&buffer[0], "-"));
		sprintf(&buffer[0], "vpn_server%d_port", serverNum);
		fprintf(fp, "--dport %d -j ACCEPT\n", nvram_get_int(&buffer[0]));
		sprintf(&buffer[0], "vpn_server%d_firewall", serverNum);
		if ( !nvram_contains_word(&buffer[0], "external") )
		{
			fprintf(fp, "iptables -I INPUT -i %s -j ACCEPT\n", &iface[0]);
			fprintf(fp, "iptables -I FORWARD %d -i %s -j ACCEPT\n", (nvram_match("cstats_enable", "1") ? 4 : 2), &iface[0]);
		}
		if (nvram_match("ctf_disable", "0")) {
			fprintf(fp, "iptables -t mangle -I PREROUTING -i %s -j MARK --set-mark 0x01/0xf\n", &iface[0]);
		}
		if (nvram_match("cstats_enable", "1")) {
			ipt_account(fp, &iface[0]);
		}
		fclose(fp);
		vpnlog(VPN_LOG_EXTRA,"Done creating firewall rules");

		// Run postconf custom script on it if it exists
        sprintf(&buffer[0], "openvpnserver%d.postconf", serverNum);
        sprintf(&buffer2[0], "/etc/openvpn/server%d/config.ovpn", serverNum);
        run_postconf(&buffer[0], &buffer2[0]);

		// Run the firewall rules
		vpnlog(VPN_LOG_EXTRA,"Running firewall rules");
		sprintf(&buffer[0], "/etc/openvpn/fw/server%d-fw.sh", serverNum);
		argv[0] = &buffer[0];
		argv[1] = NULL;
		_eval(argv, NULL, 0, NULL);
		vpnlog(VPN_LOG_EXTRA,"Done running firewall rules");
	}

	// Start the VPN server
	sprintf(&buffer[0], "vpn_server%d_enabled", serverNum);
	nvram_set(&buffer[0], "1");
	sprintf(&buffer[0], "vpn_serverx_enabled");
	nvram_set_int(&buffer[0], (nvram_get_int(&buffer[0])+1));
#ifdef RTCONFIG_BCMARM
        if (cpu_num > 1)
		sprintf(&buffer[0], "taskset -c %d /etc/openvpn/vpnserver%d --cd /etc/openvpn/server%d --config config.ovpn", (serverNum % 2 ? 1 : 0), serverNum, serverNum);
	else
#endif
		sprintf(&buffer[0], "/etc/openvpn/vpnserver%d --cd /etc/openvpn/server%d --config config.ovpn", serverNum, serverNum);

	vpnlog(VPN_LOG_INFO,"Starting OpenVPN: %s",&buffer[0]);
	for (argv[argc=0] = strtok(&buffer[0], " "); argv[argc] != NULL; argv[++argc] = strtok(NULL, " "));
	if ( _eval(argv, NULL, 0, &pid) )
	{
		vpnlog(VPN_LOG_ERROR,"Starting VPN instance failed...");
		stop_vpnserver(serverNum);
		sprintf(&buffer[0], "vpn_server%d_state", serverNum);
		nvram_set(&buffer[0], "-1");	//error
		return;
	}
	vpnlog(VPN_LOG_EXTRA,"Done starting openvpn");

	// Set up cron job
	sprintf(&buffer[0], "vpn_server%d_poll", serverNum);
	if ( (nvi = nvram_get_int(&buffer[0])) > 0 )
	{
		vpnlog(VPN_LOG_EXTRA,"Adding cron job");
		argv[0] = "cru";
		argv[1] = "a";
		sprintf(&buffer[0], "CheckVPNServer%d", serverNum);
		argv[2] = &buffer[0];
		sprintf(&buffer[strlen(&buffer[0])+1], "*/%d * * * * service start_vpnserver%d", nvi, serverNum);
		argv[3] = &buffer[strlen(&buffer[0])+1];
		argv[4] = NULL;
		_eval(argv, NULL, 0, NULL);
		vpnlog(VPN_LOG_EXTRA,"Done adding cron job");
	}

	if ( cryptMode == SECRET || cryptMode == CUSTOM)
	{
		sprintf(&buffer[0], "vpn_server%d_state", serverNum);
		nvram_set(&buffer[0], "2");	//running
		sprintf(&buffer[0], "vpn_server%d_errno", serverNum);
		nvram_set(&buffer[0], "0");
	}

#ifdef LINUX26
//	sprintf(&buffer[0], "vpn_server%d", serverNum);
//	allow_fastnat(buffer, 0);
//	try_enabling_fastnat();
#endif

	vpnlog(VPN_LOG_INFO,"VPN GUI server backend complete.");
}

void stop_vpnserver(int serverNum)
{
	int argc;
	char *argv[9];
	char buffer[BUF_SIZE];

	sprintf(&buffer[0], "stop_vpnserver%d", serverNum);
	if (getpid() != 1) {
		notify_rc(&buffer[0]);
		return;
	}

	vpnlog(VPN_LOG_INFO,"Stopping VPN GUI server backend.");

	// Remove cron job
	vpnlog(VPN_LOG_EXTRA,"Removing cron job");
	argv[0] = "cru";
	argv[1] = "d";
	sprintf(&buffer[0], "CheckVPNServer%d", serverNum);
	argv[2] = &buffer[0];
	argv[3] = NULL;
	_eval(argv, NULL, 0, NULL);
	vpnlog(VPN_LOG_EXTRA,"Done removing cron job");

	// Stop the VPN server
	vpnlog(VPN_LOG_EXTRA,"Stopping OpenVPN server.");
	sprintf(&buffer[0], "vpnserver%d", serverNum);
	if ( !ovpn_waitfor(&buffer[0]) ) {
		vpnlog(VPN_LOG_EXTRA,"OpenVPN server stopped.");
		sprintf(&buffer[0], "vpn_server%d_enabled", serverNum);
		nvram_set(&buffer[0], "0");
		sprintf(&buffer[0], "vpn_serverx_enabled");
		nvram_set_int(&buffer[0], (nvram_get_int(&buffer[0])-1));
	}

	// NVRAM setting for device type could have changed, just try to remove both
	vpnlog(VPN_LOG_EXTRA,"Removing VPN device.");
	sprintf(&buffer[0], "openvpn --rmtun --dev tap%d", serverNum+SERVER_IF_START);
	for (argv[argc=0] = strtok(&buffer[0], " "); argv[argc] != NULL; argv[++argc] = strtok(NULL, " "));
	_eval(argv, NULL, 0, NULL);

	sprintf(&buffer[0], "openvpn --rmtun --dev tun%d", serverNum+SERVER_IF_START);
	for (argv[argc=0] = strtok(&buffer[0], " "); argv[argc] != NULL; argv[++argc] = strtok(NULL, " "));
	_eval(argv, NULL, 0, NULL);
	vpnlog(VPN_LOG_EXTRA,"VPN device removed.");

	// Don't remove tunnel interface in case of multiple servers/clients
	//modprobe_r("tun");

	// Remove firewall rules after VPN exit
	vpnlog(VPN_LOG_EXTRA,"Removing firewall rules.");
	sprintf(&buffer[0], "/etc/openvpn/fw/server%d-fw.sh", serverNum);
	argv[0] = "sed";
	argv[1] = "-i";
	argv[2] = "s/-A/-D/g;s/-I/-D/g;s/INPUT\\ [0-9]\\ /INPUT\\ /g;s/FORWARD\\ [0-9]\\ /FORWARD\\ /g;s/PREROUTING\\ [0-9]\\ /PREROUTING\\ /g;s/POSTROUTING\\ [0-9]\\ /POSTROUTING\\ /g";
	argv[3] = &buffer[0];
	argv[4] = NULL;
	if (!_eval(argv, NULL, 0, NULL))
	{
		argv[0] = &buffer[0];
		argv[1] = NULL;
		_eval(argv, NULL, 0, NULL);
	}
	vpnlog(VPN_LOG_EXTRA,"Done removing firewall rules.");

	if ( nvram_get_int("vpn_debug") <= VPN_LOG_EXTRA )
	{
		vpnlog(VPN_LOG_EXTRA,"Removing generated files.");
		// Delete all files for this server
		sprintf(&buffer[0], "rm -rf /etc/openvpn/server%d /etc/openvpn/fw/server%d-fw.sh /etc/openvpn/vpnserver%d",serverNum,serverNum,serverNum);
		for (argv[argc=0] = strtok(&buffer[0], " "); argv[argc] != NULL; argv[++argc] = strtok(NULL, " "));
		_eval(argv, NULL, 0, NULL);

		// Attempt to remove directories.  Will fail if not empty
		rmdir("/etc/openvpn/fw");
		rmdir("/etc/openvpn");
		vpnlog(VPN_LOG_EXTRA,"Done removing generated files.");
	}

#ifdef LINUX26
//	sprintf(&buffer[0], "vpn_server%d", serverNum);
//	allow_fastnat(buffer, 1);
//	try_enabling_fastnat();
#endif

	sprintf(&buffer[0], "vpn_server%d_state", serverNum);
	nvram_set(&buffer[0], "0");
	sprintf(&buffer[0], "vpn_server%d_errno", serverNum);
	nvram_set(&buffer[0], "0");

	vpnlog(VPN_LOG_INFO,"VPN GUI server backend stopped.");
}

void start_vpn_eas()
{
	char buffer[16], *cur;
	int nums[4], i;

	if (strlen(nvram_safe_get("vpn_serverx_eas")) == 0 && strlen(nvram_safe_get("vpn_clientx_eas")) == 0) return;
	// wait for time sync for a while
	i = 300;  // extend for dual wan environments
	while (!nvram_get_int("ntp_sync") && i--) {
		sleep(1);
	}

	// Parse and start servers
	strlcpy(&buffer[0], nvram_safe_get("vpn_serverx_eas"), sizeof(buffer));
	if ( strlen(&buffer[0]) != 0 ) vpnlog(VPN_LOG_INFO, "Starting OpenVPN servers (eas): %s", &buffer[0]);
	i = 0;
	for( cur = strtok(&buffer[0],","); cur != NULL && i < 4; cur = strtok(NULL, ",")) { nums[i++] = atoi(cur); }
	nums[i] = 0;
	for( i = 0; nums[i] > 0; i++ )
	{

		sprintf(&buffer[0], "vpnserver%d", nums[i]);
		if ( pidof(&buffer[0]) >= 0 )
		{
			vpnlog(VPN_LOG_INFO, "Stopping OpenVPN server %d (eas)", nums[i]);
			stop_vpnserver(nums[i]);
		}

		// Start server
		vpnlog(VPN_LOG_INFO, "Starting OpenVPN server %d (eas)", nums[i]);
		start_vpnserver(nums[i]);
	}

	// Parse and start clients
	strlcpy(&buffer[0], nvram_safe_get("vpn_clientx_eas"), sizeof(buffer));
	if ( strlen(&buffer[0]) != 0 ) vpnlog(VPN_LOG_INFO, "Starting clients (eas): %s", &buffer[0]);
	i = 0;
	for( cur = strtok(&buffer[0],","); cur != NULL && i < 4; cur = strtok(NULL, ",")) { nums[i++] = atoi(cur); }
	nums[i] = 0;
	for( i = 0; nums[i] > 0; i++ )
	{
		sprintf(&buffer[0], "vpnclient%d", nums[i]);
		if ( pidof(&buffer[0]) >= 0 )
		{
			vpnlog(VPN_LOG_INFO, "Stopping OpenVPN client %d (eas)", nums[i]);
			stop_vpnclient(nums[i]);
		}


		// Setup client routing in case some are set to be blocked when tunnel is down
		update_vpnrouting(nums[i]);

		// Start client
		vpnlog(VPN_LOG_INFO, "Starting OpenVPN client %d (eas)", nums[i]);
		start_vpnclient(nums[i]);
	}
}

void stop_vpn_eas()
{
	char buffer[16], *cur;
	int nums[4], i;

	// Parse and stop servers
	strlcpy(&buffer[0], nvram_safe_get("vpn_serverx_eas"), sizeof(buffer));
	if ( strlen(&buffer[0]) != 0 ) vpnlog(VPN_LOG_INFO, "Stopping OpenVPN servers (eas): %s", &buffer[0]);
	i = 0;
	for( cur = strtok(&buffer[0],","); cur != NULL && i < 4; cur = strtok(NULL, ",")) { nums[i++] = atoi(cur); }
	nums[i] = 0;
	for( i = 0; nums[i] > 0; i++ )
	{
		sprintf(&buffer[0], "vpnserver%d", nums[i]);
		if ( pidof(&buffer[0]) >= 0 )
		{
			vpnlog(VPN_LOG_INFO, "Stopping OpenVPN server %d (eas)", nums[i]);
			stop_vpnserver(nums[i]);
		}
	}

	// Parse and stop clients
	strlcpy(&buffer[0], nvram_safe_get("vpn_clientx_eas"), sizeof(buffer));
	if ( strlen(&buffer[0]) != 0 ) vpnlog(VPN_LOG_INFO, "Stopping OpenVPN clients (eas): %s", &buffer[0]);
	i = 0;
	for( cur = strtok(&buffer[0],","); cur != NULL && i < 4; cur = strtok(NULL, ",")) { nums[i++] = atoi(cur); }
	nums[i] = 0;
	for( i = 0; nums[i] > 0; i++ )
	{
		sprintf(&buffer[0], "vpnclient%d", nums[i]);
		if ( pidof(&buffer[0]) >= 0 )
		{
			vpnlog(VPN_LOG_INFO, "Stopping OpenVPN client %d (eas)", nums[i]);
			stop_vpnclient(nums[i]);
		}
	}
}

void stop_vpn_all()
{
	char buffer[16];
	int i;

	// stop servers
	for( i = 1; i <= 2; i++ )
	{
		sprintf(&buffer[0], "vpnserver%d", i);
		if ( pidof(&buffer[0]) >= 0 )
		{
			vpnlog(VPN_LOG_INFO, "Stopping OpenVPN server %d", i);
			stop_vpnserver(i);
		}
	}

	// stop clients
	for( i = 1; i <= 2; i++ )
	{
		sprintf(&buffer[0], "vpnclient%d", i);
		if ( pidof(&buffer[0]) >= 0 )
		{
			vpnlog(VPN_LOG_INFO, "Stopping OpenVPN client %d", i);
			stop_vpnclient(i);
		}
	}

	// Remove tunnel interface module
	modprobe_r("tun");
}

void run_vpn_firewall_scripts()
{
	DIR *dir;
	struct dirent *file;
	char *fn;
	char *argv[8];

	if ( chdir("/etc/openvpn/fw") )
		return;

	dir = opendir("/etc/openvpn/fw");

	vpnlog(VPN_LOG_EXTRA,"Beginning all firewall scripts...");
	while ( (file = readdir(dir)) != NULL )
	{
		fn = file->d_name;
		if ( fn[0] == '.' )
			continue;
		// Remove existing firewall rules if they exist
		vpnlog(VPN_LOG_EXTRA,"Removing existing firewall rules: %s", fn);
		argv[0] = "sed";
		argv[1] = "s/-A/-D/g;s/-I/-D/g;s/INPUT\\ [0-9]\\ /INPUT\\ /g;s/FORWARD\\ [0-9]\\ /FORWARD\\ /g;s/PREROUTING\\ [0-9]\\ /PREROUTING\\ /g;s/POSTROUTING\\ [0-9]\\ /POSTROUTING\\ /g";
		argv[2] = fn;
		argv[3] = ">";
		argv[4] = "/etc/openvpn/fw/clear-fw-tmp.sh";
		argv[5] = NULL;
		if (!_eval(argv, NULL, 0, NULL))
		{
			argv[0] = "/etc/openvpn/fw/clear-fw-tmp.sh";
			argv[1] = NULL;
			_eval(argv, NULL, 0, NULL);
		}
		unlink("/etc/openvpn/fw/clear-fw-tmp.sh");

		// Add firewall rules
		vpnlog(VPN_LOG_INFO,"Running firewall script: %s", fn);
		argv[0] = "/bin/sh";
		argv[1] = fn;
		argv[2] = NULL;
		_eval(argv, NULL, 0, NULL);
	}
	vpnlog(VPN_LOG_EXTRA,"Done with all firewall scripts...");

	closedir(dir);
}

void write_vpn_dnsmasq_config(FILE* f)
{
	char nv[16];
	char buf[24];
	char *pos, ch;
	int cur, ch2, adns;
	DIR *dir;
	struct dirent *file;
	FILE *dnsf;
	char *dnscrypt1_lanip;
	char *dnscrypt2_lanip;
	char *stubby_lanip;

	strlcpy(&buf[0], nvram_safe_get("vpn_serverx_dns"), sizeof(buf));
	for ( pos = strtok(&buf[0],","); pos != NULL; pos=strtok(NULL, ",") )
	{
		cur = atoi(pos);
		if ( cur )
		{
			vpnlog(VPN_LOG_EXTRA, "Adding server %d interface to dns config", cur);
			snprintf(&nv[0], sizeof(nv), "vpn_server%d_if", cur);
			fprintf(f, "interface=%s%d\n", nvram_safe_get(&nv[0]), SERVER_IF_START+cur);
		}
	}

	if ( (dir = opendir("/etc/openvpn/dns")) != NULL )
	{
		while ( (file = readdir(dir)) != NULL )
		{
			if ( file->d_name[0] == '.' )
				continue;

			if ( sscanf(file->d_name, "client%d.resol%c", &cur, &ch) == 2 )
			{
				vpnlog(VPN_LOG_EXTRA, "Checking ADNS settings for client %d", cur);
				snprintf(&buf[0], sizeof(buf), "vpn_client%d_adns", cur);
				adns = nvram_get_int(&buf[0]);
				if ( adns == 2 )
				{
					vpnlog(VPN_LOG_INFO, "Adding strict-order to dnsmasq config for client %d", cur);
					fprintf(f, "strict-order\n");
					break;
				}
#ifdef RTCONFIG_DNSCRYPT
				if ( adns == 4 )
				{
					if (nvram_match("dnscrypt_proxy", "1")) {
						vpnlog(VPN_LOG_INFO, "Setting DNSCRYPT server to dnsmasq config for client %d", cur);
						fprintf(f, "no-resolv\n");

#ifdef RTCONFIG_IPV6
						if (ipv6_enabled() && nvram_get_int("dnscrypt1_ipv6"))
							dnscrypt1_lanip = "::1";
						else
							dnscrypt1_lanip = "127.0.0.1";
						if (ipv6_enabled() && nvram_get_int("dnscrypt2_ipv6"))
							dnscrypt2_lanip = "::1";
						else
							dnscrypt2_lanip = "127.0.0.1";
#else
						dnscrypt1_lanip = "127.0.0.1";
						dnscrypt2_lanip = "127.0.0.1";
#endif

						if (!nvram_match("dnscrypt2_resolver", "none"))
							fprintf(f, "server=%s#%d\n", dnscrypt2_lanip, nvram_get_int("dnscrypt2_port"));
						if (!nvram_match("dnscrypt1_resolver","none"))
							fprintf(f, "server=%s#%d\n", dnscrypt1_lanip, nvram_get_int("dnscrypt1_port"));
					}
					break;
				}
#endif
#ifdef RTCONFIG_STUBBY
				if ( adns == 4 )
				{
					if (nvram_match("stubby_proxy", "1")) {
						vpnlog(VPN_LOG_INFO, "Setting STUBBY server to dnsmasq config for client %d", cur);
						fprintf(f, "no-resolv\n");

#ifdef RTCONFIG_IPV6
						if (ipv6_enabled() && nvram_get_int("stubby_ipv6") && nvram_match("stubby_noipv6", "0")) {
							stubby_lanip = "::1";
							fprintf(f, "server=%s#%d\n", stubby_lanip, nvram_get_int("stubby_port"));
						}
#endif
						if (nvram_get_int("stubby_ipv4")) {
							stubby_lanip = "127.0.0.1";
							fprintf(f, "server=%s#%d\n", stubby_lanip, nvram_get_int("stubby_port"));
						}

						// apply NextDNS parms
						if (strstr(nvram_safe_get("stubby_dns"), "NextDNS") && (strlen(nvram_safe_get("nextdns_id")) > 0)) {
//							fprintf(f, "add-cpe-id=%s\n", nvram_safe_get("nextdns_id"));
							if (nvram_get_int("nextdns_analytics") && !nvram_get_int("nextdns_analytics_x"))
								logmessage("dnsmasq", "NextDNS analytics disabled due to multiple providers");
							if (nvram_get_int("nextdns_analytics_x")) {
								fprintf(f, "add-mac\n");
								fprintf(f, "add-subnet=32,128\n");
								logmessage("dnsmasq", "NextDNS analytics enabled");
							}
						}

					}
					break;
				}
#endif
			}

			if ( sscanf(file->d_name, "client%d.con%c", &cur, &ch) == 2 )
			{
				if ( (dnsf = fopen(file->d_name, "r")) != NULL )
				{
					vpnlog(VPN_LOG_INFO, "Adding Dnsmasq config from %s", file->d_name);

					while( !feof(dnsf) )
					{
						ch2 = fgetc(dnsf);
						fputc(ch2==EOF?'\n':ch2, f);
					}

					fclose(dnsf);
				}
			}
		}
	}
	else {
		/* no openvpn dns directory */
#ifdef RTCONFIG_DNSCRYPT
		if (nvram_match("dnscrypt_proxy", "1")) {
			fprintf(f, "no-resolv\n");

#ifdef RTCONFIG_IPV6
			if (ipv6_enabled() && nvram_get_int("dnscrypt1_ipv6"))
				dnscrypt1_lanip = "::1";
			else
				dnscrypt1_lanip = "127.0.0.1";
			if (ipv6_enabled() && nvram_get_int("dnscrypt2_ipv6"))
				dnscrypt2_lanip = "::1";
			else
				dnscrypt2_lanip = "127.0.0.1";
#else
			dnscrypt1_lanip = "127.0.0.1";
			dnscrypt2_lanip = "127.0.0.1";
#endif

			if (!nvram_match("dnscrypt2_resolver", "none"))
				fprintf(f, "server=%s#%d\n", dnscrypt2_lanip, nvram_get_int("dnscrypt2_port"));
			if (!nvram_match("dnscrypt1_resolver","none"))
				fprintf(f, "server=%s#%d\n", dnscrypt1_lanip, nvram_get_int("dnscrypt1_port"));
		}
#endif
#ifdef RTCONFIG_STUBBY
		if (nvram_match("stubby_proxy", "1")) {
			fprintf(f, "no-resolv\n");

#ifdef RTCONFIG_IPV6
			if (ipv6_enabled() && nvram_get_int("stubby_ipv6") && nvram_match("stubby_noipv6", "0")) {
				stubby_lanip = "::1";
				fprintf(f, "server=%s#%d\n", stubby_lanip, nvram_get_int("stubby_port"));
			}
#endif
			if (nvram_get_int("stubby_ipv4")) {
				stubby_lanip = "127.0.0.1";
				fprintf(f, "server=%s#%d\n", stubby_lanip, nvram_get_int("stubby_port"));
			}

			// apply NextDNS parms
			if (strstr(nvram_safe_get("stubby_dns"), "NextDNS") && (strlen(nvram_safe_get("nextdns_id")) > 0)) {
//				fprintf(f, "add-cpe-id=%s\n", nvram_safe_get("nextdns_id"));
				if (nvram_get_int("nextdns_analytics") && !nvram_get_int("nextdns_analytics_x"))
					logmessage("dnsmasq", "NextDNS analytics disabled due to multiple providers");
				if (nvram_get_int("nextdns_analytics_x")) {
					fprintf(f, "add-mac\n");
					fprintf(f, "add-subnet=32,128\n");
					logmessage("dnsmasq", "NextDNS analytics enabled");
				}
			}
		}
#endif
	}
}

int get_dnslevel()
{
	DIR *dir;
	struct dirent *file;
	char *fn, ch, num, buf[24];
	FILE *dnsf;
	int strictlevel = 0, level;

	if ( chdir("/etc/openvpn/dns") )
		return 0;

	dir = opendir("/etc/openvpn/dns");

	vpnlog(VPN_LOG_EXTRA, "Getting max DNS level...");
	while ( (file = readdir(dir)) != NULL )
	{
		fn = file->d_name;

		if ( fn[0] == '.' )
			continue;

		if ( sscanf(fn, "client%c.resol%c", &num, &ch) == 2 )
		{
			snprintf(&buf[0], sizeof(buf), "vpn_client%c_adns", num);
			level = nvram_get_int(&buf[0]);

			// Only return the highest active level, so one exclusive client
			// will override a relaxed client.
			if( level > strictlevel)
				strictlevel = level;
		}
	}

	closedir(dir);

	return strictlevel;
}



int write_vpn_resolv(FILE* f)
{
	DIR *dir;
	struct dirent *file;
	char *fn, ch, num;
	FILE *dnsf;
	int ch2;

	if ( chdir("/etc/openvpn/dns") )
		return 0;

	dir = opendir("/etc/openvpn/dns");

	vpnlog(VPN_LOG_EXTRA, "Adding DNS entries...");
	while ( (file = readdir(dir)) != NULL )
	{
		fn = file->d_name;

		if ( fn[0] == '.' )
			continue;

		if ( sscanf(fn, "client%c.resol%c", &num, &ch) == 2 )
		{
			if ( (dnsf = fopen(fn, "r")) == NULL )
				continue;

			vpnlog(VPN_LOG_INFO,"Adding DNS entries from %s", fn);

			while( !feof(dnsf) )
			{
				ch2 = fgetc(dnsf);
				fputc(ch2==EOF?'\n':ch2, f);
			}

			fclose(dnsf);
		}
	}

	vpnlog(VPN_LOG_EXTRA, "Done with DNS entries...");

	closedir(dir);

	return 1;
}

void create_openvpn_passwd()
{
	unsigned char s[512];
	char *p;
	char salt[32];
	char *nv, *nvp, *b;
	char *username, *passwd;
	FILE *fp1, *fp2, *fp3;
	int id = 200;

	strcpy(salt, "$1$");
	f_read("/dev/urandom", s, 6);
	base64_encode(s, salt + 3, 6);
	salt[3 + 8] = 0;
	p = salt;
	while (*p) {
		if (*p == '+') *p = '.';
		++p;
	}

	fp1=fopen("/etc/shadow.openvpn", "w");
	fp2=fopen("/etc/passwd.openvpn", "w");
	fp3=fopen("/etc/group.openvpn", "w");
	if (fp1 && fp2 && fp3) {
		nv = nvp = strdup(nvram_safe_get("vpn_serverx_clientlist"));

		if(nv) {
			while ((b = strsep(&nvp, "<")) != NULL) {
				if((vstrsep(b, ">", &username, &passwd)!=2)) continue;
				if(strlen(username)==0||strlen(passwd)==0) continue;

				p = crypt(passwd, salt);
				fprintf(fp1, "%s:%s:0:0:99999:7:0:0:\n", username, p);
				fprintf(fp2, "%s:x:%d:%d:::\n", username, id, id);
				fprintf(fp3, "%s:x:%d:\n", username, id);
				id++;
			}
			free(nv);
 		}
 	}
	if (fp1) fclose(fp1);
	if (fp2) fclose(fp2);
	if (fp3) fclose(fp3);
	chmod("/etc/shadow.openvpn", 0600);
	chmod("/etc/group.openvpn", 0644);
	chmod("/etc/passwd.openvpn", 0644);
}


int _check_ovpn_enabled(int unit, char *config){
        char tmp[2];

        if (unit > 9) return 0;

        sprintf(tmp,"%d", unit);

        if (strstr(nvram_safe_get(config), tmp)) return 1;

        return 0;
}

int check_ovpn_server_enabled(int unit){
	return _check_ovpn_enabled(unit, "vpn_serverx_eas");
}

int check_ovpn_client_enabled(int unit){
	return _check_ovpn_enabled(unit, "vpn_clientx_eas");
}

void update_vpnrouting(int unit){
	struct stat sb;
	char tmp[8];
    char *cmd[4];
	int pid;
    int timeout = 120;
	char *error;

	snprintf(tmp, sizeof (tmp), "tun1%d", unit);
	setenv("dev", tmp, 1);
	setenv("script_type", "rmupdate", 1);

	cmd[0]="/usr/sbin/vpnrouting.sh";
	cmd[1]=NULL;
	cmd[2]=NULL;
	cmd[3]=NULL;
	_eval( cmd, NULL, timeout, (timeout ? NULL : &pid));
}

void reset_vpn_settings(int type, int unit){
	struct nvram_tuple *t;
	char prefix[]="vpn_XXX_SERVICX_", tmp[100];
	char *service;

	if (type == 1)
		service = "server";
	else if (type == 2)
		service = "client";
	else
		return;

	logmessage("openvpn","Resetting %s (unit %d) to default settings", service, unit);

	snprintf(prefix, sizeof(prefix), "vpn_%s%d_", service, unit);

	for (t = router_defaults; t->name; t++) {
		if (strncmp(t->name, prefix, 12)==0) {
			nvram_set(t->name, t->value);
		}
	}

	snprintf(prefix, sizeof(prefix), "vpn_crt_%s%d_", service, unit);

	for (t = router_defaults; t->name; t++) {
		if (strncmp(t->name, prefix, 16)==0) {
			nvram_set(t->name, t->value);
		}
	}
}
