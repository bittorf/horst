/* olsr scanning tool
 *
 * Copyright (C) 2005  Bruno Randolf
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "display.h"
#include "ieee80211_header.h"
#include "olsr_header.h"

#include <stdio.h>
#include <stdlib.h>
#include <curses.h>

#define COL_IP 1
#define COL_SNR 17
#define COL_SOURCE 21
#define COL_BSSID 39
#define COL_LQ 59
#define COL_OLSR 71
#define COL_TSF 82

WINDOW *dump_win;
WINDOW *dump_win_box;
WINDOW *list_win;
WINDOW *stat_win;

static void update_stat_win(struct packet_info* pkt);
static void update_list_win(void);

static int do_sort=0;

void
init_display(void)
{
	initscr();
	start_color();                  /* Start the color functionality */
	keypad(stdscr, TRUE);
	nonl();         /* tell curses not to do NL->CR/NL on output */
	cbreak();       /* take input chars one at a time, no wait for \n */
	noecho();         /* echo input - in color */
	nodelay(stdscr,TRUE);
	init_pair(1, COLOR_CYAN, COLOR_BLACK);
	init_pair(2, COLOR_GREEN, COLOR_BLACK);
	init_pair(3, COLOR_YELLOW, COLOR_BLACK);
	init_pair(4, COLOR_BLUE, COLOR_BLACK);
	init_pair(5, COLOR_WHITE, COLOR_BLACK);

	move(0,COLS/2-20);
	printw("HORST - Horsts OLSR Radio Scanning Tool " PACKAGE_VERSION " (build date: " PACKAGE_BUILDDATE ")");
	refresh();

	list_win = newwin(LINES/2-1, COLS, 1, 0);
	
	scrollok(list_win,FALSE);
	wrefresh(list_win);

	stat_win = newwin(LINES/2, 15, LINES/2, COLS-15);
	box(stat_win, 0 , 0);
	mvwprintw(stat_win,0,2," Status ");
	wattron(stat_win, COLOR_PAIR(2));
	scrollok(stat_win,FALSE);
	wrefresh(stat_win);

	dump_win_box = newwin(LINES/2, COLS-15, LINES/2, 0);
	box(dump_win_box, 0 , 0);
	mvwprintw(dump_win_box,0,1,"Sig/Noi");
	mvwprintw(dump_win_box,0,9,"SOURCE");
	mvwprintw(dump_win_box,0,27,"(BSSID)");
	mvwprintw(dump_win_box,0,47,"TYPE");
	mvwprintw(dump_win_box,0,54,"INFO");
	wrefresh(dump_win_box);
	dump_win = newwin(LINES/2-2, COLS-15-2, LINES/2+1, 1);
	wattron(dump_win, COLOR_PAIR(1));
	scrollok(dump_win,TRUE);
	
	wrefresh(dump_win);
}

void 
finish_display(int sig)
{
	printw("CLEAN");
	endwin();
}

void
handle_user_input()
{
	int key;

	key = getch();
	switch(key) {
		case ' ': case 'p': case 'P':
			paused = paused ? 0 : 1;
			update_stat_win(NULL);
			break;
		case 'c': case 'C':
			no_ctrl = no_ctrl ? 0 : 1;
			update_stat_win(NULL);
			break;
		case 'o': case 'O':
			olsr_only = olsr_only ? 0 : 1;
			update_stat_win(NULL);
			break;
		case 'q': case 'Q':
			finish_all(0);
		case 's': case 'S':
			do_sort = do_sort ? 0 : 1;
			break;
		/* not yet: 
		case 'c': case 'C':
			pause = 1;
			show_channel_win();
			break;
		*/
		case KEY_RESIZE: /* xterm window resize event */
			endwin();
			init_display();
	}
}

void
update_display(struct packet_info* pkt)
{
	if (pkt->olsr_type>0)
	if (pkt->pkt_types & PKT_TYPE_OLSR)
		wattron(dump_win,A_BOLD);
	/* print */
	wprintw(dump_win,"%03d/%03d %s ",
		pkt->prism_signal, pkt->prism_noise, 
		ether_sprintf(pkt->wlan_src));
	wprintw(dump_win,"(%s) ", ether_sprintf(pkt->wlan_bssid));
	if (pkt->pkt_types & PKT_TYPE_BEACON) {
		wprintw(dump_win,"BEACON '%s' %08x:%08x", pkt->wlan_essid,
			ntohl(*(unsigned long*)(&pkt->wlan_tsf[4])),
			ntohl(*(unsigned long*)(&pkt->wlan_tsf[0])));
	}
	else if (pkt->pkt_types & PKT_TYPE_PROBE_REQ) {
		wprintw(dump_win,"PROBE_REQUEST" );
	}
	else if (pkt->pkt_types & PKT_TYPE_OLSR) {
		wprintw(dump_win,"OLSR   %s ", ip_sprintf(pkt->ip_src));
		switch (pkt->olsr_type) {
			case HELLO_MESSAGE: wprintw(dump_win,"HELLO"); break;
			case TC_MESSAGE: wprintw(dump_win,"TC"); break;
			case MID_MESSAGE: wprintw(dump_win,"MID");break;
			case HNA_MESSAGE: wprintw(dump_win,"HNA"); break;
			case LQ_HELLO_MESSAGE: wprintw(dump_win,"LQ_HELLO"); break;
			case LQ_TC_MESSAGE: wprintw(dump_win,"LQ_TC"); break;
			default: wprintw(dump_win,"OLSR(%d)",pkt->olsr_type);
		}
	}
	else if (pkt->pkt_types & PKT_TYPE_DATA) {
		wprintw(dump_win,"IP     %s ", ip_sprintf(pkt->ip_src));
	}
	else if (pkt->pkt_types & PKT_TYPE_DATA) {
		wprintw(dump_win,"DATA");
	}
	else if (WLAN_FC_TYPE_MGMT == pkt->wlan_type) {
		switch(pkt->wlan_stype) {
			case WLAN_FC_STYPE_ASSOC_REQ:
				wprintw(dump_win,"ASSOC_REQ");
			break;
			case WLAN_FC_STYPE_ASSOC_RESP:
				wprintw(dump_win,"ASSOC_RESP");
			break;
			case WLAN_FC_STYPE_REASSOC_REQ:
				wprintw(dump_win,"REASSOC_REQ");
			break;
			case WLAN_FC_STYPE_REASSOC_RESP:
				wprintw(dump_win,"REASSOC_RESP");
			break;
			case WLAN_FC_STYPE_PROBE_REQ:
				wprintw(dump_win,"PROBE_REQ");
			break;
			case WLAN_FC_STYPE_PROBE_RESP:
				wprintw(dump_win,"PROBE_RESP");
			break;
			case WLAN_FC_STYPE_BEACON:
				wprintw(dump_win,"BEACON");
			break;
			case WLAN_FC_STYPE_ATIM:
				wprintw(dump_win,"ATIM");
			break;
			case WLAN_FC_STYPE_DISASSOC:
				wprintw(dump_win,"DISASSOC");
			break;
			case WLAN_FC_STYPE_AUTH:
				wprintw(dump_win,"AUTH");
			break;
			case WLAN_FC_STYPE_DEAUTH:
				wprintw(dump_win,"DEAUTH");
			break;
			default:
				wprintw(dump_win,"MGMT(0x%02x)",pkt->wlan_stype);
			break;
		}
	}
	else if (WLAN_FC_TYPE_CTRL == pkt->wlan_type) {
		switch(pkt->wlan_stype) {
			case WLAN_FC_STYPE_PSPOLL:
				wprintw(dump_win,"PSPOLL");
			break;
			case WLAN_FC_STYPE_RTS:
				wprintw(dump_win,"RTS");
			break;
			case WLAN_FC_STYPE_CTS:
				wprintw(dump_win,"CTS");
			break;
			case WLAN_FC_STYPE_ACK:
				wprintw(dump_win,"ACK");
			break;
			case WLAN_FC_STYPE_CFEND:
				wprintw(dump_win,"CFEND");
			break;
			case WLAN_FC_STYPE_CFENDACK:
				wprintw(dump_win,"CFENDACK");
			break;
			default:
				wprintw(dump_win,"CTRL(0x%02x)",pkt->wlan_stype);
			break;
		}
	}
	else if (WLAN_FC_TYPE_DATA == pkt->wlan_type) {
		switch(pkt->wlan_stype) {
			case WLAN_FC_STYPE_DATA:
				wprintw(dump_win,"DATA");
			break;
			case WLAN_FC_STYPE_DATA_CFACK:
				wprintw(dump_win,"DATA_CFACK");
			break;
			case WLAN_FC_STYPE_DATA_CFPOLL:
				wprintw(dump_win,"DATA_CFPOLL");
			break;
			case WLAN_FC_STYPE_DATA_CFACKPOLL:
				wprintw(dump_win,"DATA_CFACKPOLL");
			break;
			case WLAN_FC_STYPE_NULLFUNC:
				wprintw(dump_win,"NULLFUNC");
			break;
			case WLAN_FC_STYPE_CFACK:
				wprintw(dump_win,"CFACK");
			break;
			case WLAN_FC_STYPE_CFPOLL:
				wprintw(dump_win,"CFPOLL");
			break;
			case WLAN_FC_STYPE_CFACKPOLL:
				wprintw(dump_win,"CFACKPOLL");
			break;
			default:
				wprintw(dump_win,"DATA(0x%02x)",pkt->wlan_stype);
			break;
		}
	}
	else {
		wprintw(dump_win,"UNK(%x,%x)", pkt->wlan_stype, pkt->wlan_type);
	}
	
	wprintw(dump_win,"\n");
	wattroff(dump_win,A_BOLD);
	wrefresh(dump_win);

	update_stat_win(pkt);

	update_list_win();
}

static void
update_stat_win(struct packet_info* pkt)
{
	if (pkt!=NULL)
	{
		int snr = pkt->snr;
		int max_bar = LINES/2-2;
		
		snr=(snr/60.0)*max_bar; /* normalize for bar, assume max received SNR is 60 */
		if (snr>max_bar) snr=max_bar; /* cap if still bigger */
	
		wattron(stat_win, COLOR_PAIR(2));
		mvwvline(stat_win, 1, 2, ' ', max_bar-snr);
		mvwvline(stat_win, max_bar-snr+1, 2, ACS_BLOCK, snr);
		mvwvline(stat_win, 1, 3, ' ', max_bar-snr);
		mvwvline(stat_win, max_bar-snr+1, 3, ACS_BLOCK, snr);

		mvwprintw(stat_win, LINES/2-4,6,"SIG:%03d", pkt->prism_signal);
		mvwprintw(stat_win, LINES/2-3,6,"NOI:%03d", pkt->prism_noise);
		mvwprintw(stat_win, LINES/2-2,6,"SNR:%03d", pkt->snr);
	}

	wattron(stat_win, COLOR_PAIR(5));
	mvwprintw(stat_win,2,6,"q: QUIT");
	if (paused)
		mvwprintw(stat_win,3,6,"p: PAUSE");
	else
		mvwprintw(stat_win,3,6,"p: RUN  ");

	if (olsr_only) {
		mvwprintw(stat_win,4,6,"        ");
	}
	else {
		if (no_ctrl)
			mvwprintw(stat_win,4,6,"c: -CTRL");
		else
			mvwprintw(stat_win,4,6,"c: +CTRL");
	}

	if (olsr_only)
		mvwprintw(stat_win,5,6,"o: OLSR");
	else
		mvwprintw(stat_win,5,6,"o: ALL ");


	if (do_sort)
		mvwprintw(stat_win,6,6,"s: SORT ");
	else
		mvwprintw(stat_win,6,6,"s: !SORT");

	wrefresh(stat_win);
}


static void
print_list_line(int line, int i, struct packet_info* p)
{
	/* Prevents overdraw of last line */
	if (line >= LINES/2-2)
		return;

	if (nodes[i].pkt_types & PKT_TYPE_OLSR)
		wattron(list_win,A_BOLD);

        // SNR values being too big are marked as invalid.
        if (p->snr > 999)
          mvwprintw(list_win, line, COL_SNR, "INV");
        else
	  mvwprintw(list_win,line,COL_SNR,"%3d", p->snr);

	mvwprintw(list_win,line,COL_SOURCE,"%s", ether_sprintf(p->wlan_src));
	mvwprintw(list_win,line,COL_BSSID,"(%s)", ether_sprintf(nodes[i].wlan_bssid));
	if (nodes[i].pkt_types & PKT_TYPE_IP)
		mvwprintw(list_win,line,COL_IP,"%s", ip_sprintf(nodes[i].ip_src));
	if (nodes[i].pkt_types & PKT_TYPE_OLSR_LQ)
		mvwprintw(list_win,line,COL_LQ,"LQ");
	if (nodes[i].pkt_types & PKT_TYPE_OLSR_GW)
		mvwprintw(list_win,line,COL_LQ+3,"GW");
	if (nodes[i].pkt_types & PKT_TYPE_OLSR)
		mvwprintw(list_win,line,COL_LQ+6,"N:%d", nodes[i].olsr_neigh);
	mvwprintw(list_win,line,COL_OLSR,"%d/%d", nodes[i].olsr_count, nodes[i].pkt_count);
	mvwprintw(list_win,line,COL_TSF,"%08x", nodes[i].tsfh);
	wattroff(list_win,A_BOLD);
}


static void
update_list_win(void)
{
	int i;
	int s;
	int line=0;
	struct packet_info* p;
	time_t now;
	now = time(NULL);

	werase(list_win);
	wattron(list_win,COLOR_PAIR(5));
	box(list_win, 0 , 0);
	mvwprintw(list_win,0,COL_SNR,"SNR");
	mvwprintw(list_win,0,COL_SOURCE,"SOURCE");
	mvwprintw(list_win,0,COL_BSSID,"(BSSID)");
	mvwprintw(list_win,0,COL_IP,"IP");
	mvwprintw(list_win,0,COL_LQ,"LQ GW NEIGH");
	mvwprintw(list_win,0,COL_OLSR,"OLSR/COUNT");
	mvwprintw(list_win,0,COL_TSF,"TSF(High)");

	wattron(list_win,COLOR_PAIR(1));

	if (do_sort) {
		/* sort by SNR: probably the most inefficient way to do it ;) */
		for (s=100; s>0; s--) {
			for (i=0; i<MAX_NODES; i++) {
				if (nodes[i].status == 1) {
					p = &nodes[i].last_pkt;
					if ((p->snr) == s
					&& nodes[i].last_seen > now - NODE_TIMEOUT) {
						line++;
						print_list_line(line,i,p);
					}
				}
				else
					break;
			}
		}
	}
	else {
		for (i=0; i<MAX_NODES; i++) {
			if (nodes[i].status == 1
			    && nodes[i].last_seen > now - NODE_TIMEOUT) {
				p = &nodes[i].last_pkt;
				line++;
				print_list_line(line,i,p);
			}
		}
	}
	wrefresh(list_win);
}

#if 0 /* not used yet */
static void
show_channel_win()
{
	char buf[255];

	WINDOW* chan_win = newwin(3, 30, LINES/2-5, COLS/2-10);
	box(chan_win, 0 , 0);
	mvwprintw(chan_win,1,2,"enter channel number: ");
	wrefresh(chan_win);

	echo();
	nodelay(stdscr,FALSE);
	getnstr(buf,255);
	mvwprintw(chan_win,1,20,"%s",buf);
	nodelay(stdscr,TRUE);
	noecho();

	wrefresh(chan_win);
	delwin(chan_win);
	paused = 0;

	wrefresh(list_win);
}
#endif

void
dump_packet(const unsigned char* buf, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		if ((i%2) == 0)
			DEBUG(" ");
		if ((i%16) == 0)
			DEBUG("\n");
		DEBUG("%02x", buf[i]);
	}
	DEBUG("\n");
}

const char*
ether_sprintf(const unsigned char *mac)
{
	static char etherbuf[18];
	snprintf(etherbuf, sizeof(etherbuf), "%02x:%02x:%02x:%02x:%02x:%02x",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return etherbuf;
}

const char*
ip_sprintf(const unsigned int ip)
{
	static char ipbuf[18];
	unsigned char* cip = (unsigned char*)&ip;
	snprintf(ipbuf, sizeof(ipbuf), "%d.%d.%d.%d",
		cip[0], cip[1], cip[2], cip[3]);
	return ipbuf;
}