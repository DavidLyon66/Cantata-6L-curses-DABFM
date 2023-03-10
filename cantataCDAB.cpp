#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <syslog.h>
#include <ncurses.h>

#include "KeyStoneCOMM.h"
#include "simpleini/SimpleIni.h"

using namespace std;

vector <string> choices  {	"On/Off",
							"Station - List",
							"        - up",
							"        - Down",
							"Vol     - Up",
							"        - Down",
							"Scan",
							"Mute",
							"Presets",
							"Band",
							"Exit"
						};
		  
vector <string> playstatuslist {
							"Playing      ",
							"Searching    ",
							"Tuning       ",
							"Stop         ",
							"Sorting      ",
							"Reconfiguring"};
								
vector <wstring> stations;

string inipath("/.config/cantata-tech/cantataCDAB.ini");

void print_menu(WINDOW *menu_win, int highlight);
void print_playstatus(WINDOW *win);
void print_presets(WINDOW *win, bool setupmode=false);
bool scan_dab(WINDOW *win);
void create_inidir(WINDOW *win);

int main(int argc, char *argv[]) {
	
	wchar_t buffer[300];
	CSimpleIniA inifile;
	
	openlog("cantataCDAB", LOG_ODELAY, LOG_USER);
	
	WINDOW *menu_win, *status_win, *presets_win;
	int highlight(1),
		choice(0),
		c,
		maxY, maxX,
		dab_station, fm_station;
		
	initscr();
	getmaxyx(stdscr, maxY, maxX);
	
	clear();
	noecho();
	cbreak();	/* Line buffering disabled. pass on everything */
	scrollok(stdscr, TRUE);

	#define WIDTH 18
	#define HEIGHT 15 
	int startx = 5;
	int starty = (maxY - HEIGHT - 2) / 2;
		
	mvprintw(0, 0, "FM/DAB+ Radio Mudule Interface by Cantata.tech");
	mvprintw(maxY-1, 0, "Use arrow keys to go up and down, Press enter to select a choice");
	wrefresh(stdscr);

	menu_win = newwin(HEIGHT, WIDTH, starty, startx);
	print_menu(menu_win, highlight);
	keypad(menu_win, TRUE);
	nodelay(menu_win,TRUE);

	presets_win = newwin(3, maxX - 6, maxY - 5, startx);
	print_presets(presets_win, true);
	
	status_win = newwin(15, 40, starty, 30);
	box(status_win, 0, 0);
	wprintw(status_win, "Checking port\n");
	wrefresh(status_win);

	inipath.insert(0,getenv("HOME"));
	inifile.SetUnicode();
	int rc = inifile.LoadFile(inipath.c_str());
	if (rc < 0)
		create_inidir(status_win);
	
	if(OpenRadioPort((char*)"/dev/ttyACM0", true)) {
		
		wprintw(status_win, "Port opened\n");
		wrefresh(status_win);
	
		int totalProgram = GetTotalProgram();
		wprintw(status_win, "Total DAB programs = %d\n",totalProgram);

		for (int i=0;i<totalProgram;i++) {
			if (GetProgramName(0, i, 1, buffer)) {
				
				wstring station;
				station.append(buffer);
				stations.push_back(station);
			}
		}

		wrefresh(status_win);
		
		int v = stoi(inifile.GetValue("Player","volume","50"));
		string pm = inifile.GetValue("Player","band","DAB");
		dab_station = stoi(inifile.GetValue("Player","station_dab","0")); 
		fm_station = stoi(inifile.GetValue("Player","station_fm","94500"));

		SetVolume((int ) ((float) v / 6.25));
			
		SetStereoMode(1);
		SetApplicationType(2);
		SyncRTC(true);
		
		if (totalProgram == 0) {

			syslog(LOG_PERROR, "DAB Autosearch failed");	
			wprintw(status_win, "DAB Autosearch failed\n");
			
			wprintw(status_win, "Clear DAB Database (y/n) ?\n\n");
			wrefresh(status_win);
			
			if ('y' == wgetch(status_win)) {
				
				if (!scan_dab(status_win)) {
					syslog(LOG_PERROR, "DAB Station search failed");	
				}
				
			}
			
			if (totalProgram > 0)
				PlayStream(0, 0);		// DAB stations, just play the first station
			else
				PlayStream(1, 94500);	// No DAB stations, just play FM 94.5Mhz
			
			wrefresh(status_win);

			napms(1500);
			wclear(status_win);

		} else {
			
			wclear(status_win);
			
			if (pm == "DAB")
				PlayStream(0, dab_station);	// Ok.. play last DAB station then
			else
				PlayStream(1, fm_station);	// Ok.. play last FM station then
			
		}

		long playindex = GetPlayIndex();
		
		while(1) {
			
			c = wgetch(menu_win);
			switch(c)
			{	case KEY_UP:
					if(highlight == 1)
						highlight = choices.size();
					else
						--highlight;
					break;
				case KEY_DOWN:
					if(highlight == (int ) choices.size())
						highlight = 1;
					else 
						++highlight;
					break;
					
				case 10:
					choice = highlight;
					switch (choice) {
						case 1:
							wclear(status_win);
							mvwprintw(status_win, 2, 12, "Halting      ");
							wrefresh(status_win);
							StopStream();
							napms(150);
							break;
							
						case 2:
							wclear(status_win);
							for(decltype(stations.size()) i = 0; i < stations.size(); ++i)
								wprintw(status_win, "%ls\n", stations[i].c_str());
							
							wgetch(status_win);
							wclear(status_win);
							break;
						case 7:
							wclear(status_win);
							if (GetPlayMode() == 0)
								scan_dab(status_win);
							else {

							}
							
							wgetch(status_win);
							wclear(status_win);
							break;
						case 3:
							wclear(status_win);
							box(status_win, 0, 0);
							mvwprintw(status_win, 2, 2, "Changing to Next Channel");
							wrefresh(status_win);
							NextStream();

							PlayStream(GetPlayMode(), GetPlayIndex());
							if (GetPlayMode() == 0)
								dab_station = GetPlayIndex();
							else
								fm_station = GetPlayIndex();

							break;
						case 4:
							wclear(status_win);
							box(status_win, 0, 0);
							mvwprintw(status_win, 2, 2, "Changing to Prev Channel");
							wrefresh(status_win);
							PrevStream();

							PlayStream(GetPlayMode(), GetPlayIndex());
							if (GetPlayMode() == 0)
								dab_station = GetPlayIndex();
							else
								fm_station = GetPlayIndex();
								
							break;
						case 9:
							wclear(status_win);
							mvwprintw(status_win, 2, 2, "Preset\n\n");
							// FM
							for (int s=0;s<8;s++)
							{
								wprintw(status_win, "  %d = %ld\n",s+1, GetPreset(GetPlayMode(),s));
							}
							print_presets(presets_win);
							wrefresh(status_win);
							wgetch(status_win);
							break;						
						case 10:
							wclear(status_win);
							mvwprintw(status_win, 2, 12, "Changing Band");
							wrefresh(status_win);

							if (GetPlayMode() == 0)
								PlayStream(1, fm_station);
							else
								PlayStream(0, dab_station);

							print_presets(presets_win);
							napms(150);
							break;
							
						default:
							break;
					}
					break;
				case 'h':
					choice = 1;
					break;
				case 'b':
					choice = 10;
					break;
				case 'n':
					wclear(status_win);
					NextStream();
					napms(150);
					PlayStream(GetPlayMode(), GetPlayIndex());
					break;
				case 'p':
					wclear(status_win);
					PrevStream();
					napms(150);
					PlayStream(GetPlayMode(), GetPlayIndex());
					break;
				case '+':
					wclear(status_win);
					VolumePlus();
					mvwprintw(status_win, 10, 2, "Volume : %.0f", (float) 6.25 * GetVolume());
					napms(300);
					break;
				case '-':
					wclear(status_win);
					VolumeMinus();
					mvwprintw(status_win, 10, 2, "Volume : %.0f", (float) 6.25 * GetVolume());
					napms(300);
					break;
				case 'm':
					VolumeMute();
					break;
				case ' ':
					printw("Play");
					wprintf(L"On");
					PlayStream(0, playindex);
					playindex++;
					break;

				case 's':
					// Set a preset
					wclear(status_win);
					wprintw(status_win, "\nPress F1-F8 to save \n");
					wprintw(status_win, "this station.\n\n");
					wprintw(status_win, "Press a key 1-8 or [Esc] to continue\n");
					wrefresh(status_win);
		
					c = wgetch(status_win);
		
					if (c == '1') {
						SetPreset(GetPlayMode(),0,GetPlayIndex());
						wprintw(status_win, "Assigned to F1.\n\n");
					} else if (c == '2') {
						SetPreset(GetPlayMode(),1,GetPlayIndex());
						wprintw(status_win, "Assigned to F2.\n\n");
					} else if (c == '3') {
						SetPreset(GetPlayMode(),2,GetPlayIndex());
						wprintw(status_win, "Assigned to F3.\n\n");
					} else if (c == '4') {
						SetPreset(GetPlayMode(),3,GetPlayIndex());
						wprintw(status_win, "Assigned to F4.\n\n");
					} else if (c == '5') {
						SetPreset(GetPlayMode(),4,GetPlayIndex());
						wprintw(status_win, "Assigned to F5.\n\n");
					} else if (c == '6') {
						SetPreset(GetPlayMode(),5,GetPlayIndex());
						wprintw(status_win, "Assigned to F6.\n\n");
					} else if (c == '7') {
						SetPreset(GetPlayMode(),6,GetPlayIndex());
						wprintw(status_win, "Assigned to F7.\n\n");
					} else if (c == '8') {
						SetPreset(GetPlayMode(),7,GetPlayIndex());
						wprintw(status_win, "Assigned to F8.\n\n");
					} else
						wprintw(status_win, "Not Assigned.\n\n");
					
					print_presets(presets_win);

					wprintw(status_win, "Press a key to continue ");
					wrefresh(status_win);
					wgetch(status_win);
					wclear(status_win);
					
					break;
					
				case 'q':
					choice = 11;
					break;
					
				default:
					// Preset Handling
					long presetfreq(-1);
					if (c == KEY_F(1))
						presetfreq = GetPreset(GetPlayMode(),0);
					else if (c == KEY_F(2))
						presetfreq = GetPreset(GetPlayMode(),1);
					else if (c == KEY_F(3))
						presetfreq = GetPreset(GetPlayMode(),2);
					else if (c == KEY_F(4))
						presetfreq = GetPreset(GetPlayMode(),3);
					else if (c == KEY_F(5))
						presetfreq = GetPreset(GetPlayMode(),4);
					else if (c == KEY_F(6))
						presetfreq = GetPreset(GetPlayMode(),5);
					else if (c == KEY_F(7))
						presetfreq = GetPreset(GetPlayMode(),6);
					else if (c == KEY_F(8))
						presetfreq = GetPreset(GetPlayMode(),7);

					if (presetfreq != -1)
						PlayStream(GetPlayMode(), presetfreq);
						
					break;
			}
			print_menu(menu_win, highlight);
			if(choice == 11)	/* User did a choice come out of the infinite loop */
				break;
			
			if (GetApplicationData()) {
				GetImage(buffer);
			}
			
			print_playstatus(status_win);
			
			napms(100);

		}		
		
		inifile.SetValue("Player","volume", to_string((float) 6.25 * GetVolume()).c_str());

		if (GetPlayMode() == 0) {
			inifile.SetValue("Player","band","DAB");
			inifile.SetValue("Player","station_dab", to_string(GetPlayIndex()).c_str());
		} else {
			inifile.SetValue("Player","band","FM");
			inifile.SetValue("Player","station_fm", to_string(GetPlayIndex()).c_str());
		}
				
		inifile.SaveFile(inipath.c_str());

		CloseRadioPort();
	
	} else {
			
		wprintw(status_win, "\nFailed to connect to \n");
		wprintw(status_win, "the DAB/FM Board\n\n");
		wprintw(status_win, "Press a key to continue\n");
		wrefresh(status_win);
		
		wgetch(status_win);
		
		napms(1500);
		wclear(status_win);
	}

	endwin();
	
	closelog();

	return 0;
}

void print_menu(WINDOW *menu_win, int highlight)
{
	int x(2), y(2);	
	decltype(choices.size()) hl(highlight);

	box(menu_win, 0, 0);
	
	for(decltype(choices.size()) i = 0; i < choices.size(); ++i)
	{	if(hl == i + 1) /* High light the present choice */
		{	wattron(menu_win, A_REVERSE); 
			mvwprintw(menu_win, y, x, "%s", choices[i].c_str());
			wattroff(menu_win, A_REVERSE);
		}
		else
			mvwprintw(menu_win, y, x, "%s", choices[i].c_str());
		++y;
	}
	wrefresh(menu_win);
}

void print_playstatus(WINDOW *win)
{
	int x(2), y(2);	
	wchar_t buffer[300];
	unsigned char sec,min,hour,day,month,year;
	int bitError;

	curs_set(0);
	
	int radiostatus = GetPlayStatus();
	if (radiostatus == 255)
	{
		wclear(win);
		box(win, 0, 0);
		mvwprintw(win, y++, x, "Radio Status -1");
		return;
	}
	
	box(win, 0, 0);

	long playindex = GetPlayIndex();
	uint16_t playstatus = GetPlayStatus();
	char playmode = GetPlayMode();
	unsigned char ServiceComponentID;
	
	if(GetProgramName(playmode,playindex,1, buffer)) {

	    if (playmode==0) {
			// DAB
			mvwprintw(win, y++, x, "%ls", &buffer);
			if(GetProgramText(buffer)==0) 
				mvwprintw(win, y++, x, "%ls", &buffer);

			mvwprintw(win, y++, x, "Signal=%d,                 ", GetSignalStrength(&bitError));
			mvwprintw(win, y++, x, "Audio Sampling Rate=%d kHz, ", GetSamplingRate());
			mvwprintw(win, y++, x, "Data Rate=%d kHz, ", GetDataRate());

			uint32 ServiceID;
			uint16 EnsembleID;
			if (GetProgramInfo(playmode, &ServiceComponentID, &ServiceID, &EnsembleID)) {
				if (GetEnsembleName(playmode, 1, buffer))
					mvwprintw(win, y++, x,"%ls", buffer);
				else
					y++;
				mvwprintw(win, y++, x, "CID:%X,SID:%X,EID:%X,", ServiceComponentID, ServiceID, EnsembleID);
			}
			
	    } else if (playmode==1) {
			// FM
			float stationfreq = (float) playindex / 1000;
			mvwprintw(win, y++, x, "%ls FM - %3.2f      ", &buffer, stationfreq);
			y++;

			if(GetProgramText(buffer)==0) {
				mvwprintw(win, y++, x, "%ls", &buffer);
				y++;
			}
			
			mvwprintw(win, y++, x, "Signal=%d,                 ", GetSignalStrength(&bitError));
			if (GetStereoMode() == 1)
				mvwprintw(win, y++, x, "Stereo");
			else
				mvwprintw(win, y++, x, "Mono");
		}
			
		
	} else {

		mvwprintw(win, y++, x+10, "%ls", playstatuslist[playstatus].c_str());
		
		y++;
		mvwprintw(win, y++, x, "PlayIndex=%d, PlayMode=%d", playindex, playmode);

		if (playmode==1) {
			if (GetStereoMode() == 1)
				mvwprintw(win, y++, x, "Stereo");
			else
				mvwprintw(win, y++, x, "Mono");
		}
			
	}
	
	// GetRTC is done here to simplified the demo
	if(GetRTC(&sec,&min,&hour,&day,&month,&year)) { 

		int maxY, maxX;
		getmaxyx(stdscr, maxY, maxX);
		mvprintw(maxY-1, 0, "Realtime clock = %d:%d:%d %d/%d/%d", hour,min,sec,day,month,2000+year);
		wrefresh(stdscr);
		
		SyncRTC(false);
	}
	
	wrefresh(win);
	
	curs_set(1);

}

void print_presets(WINDOW *win, bool setupmode)
{
	box(win, 0, 0);

	if (setupmode) {
		mvwprintw(win, 1, 1, "Presets: F1 - F2 - F3 - F4 - F5 - F6 - F7 - F8 -");
		wrefresh(win);
		return;
	}
	
	if (GetPlayMode() == 0)
		mvwprintw(win, 1, 1, "Presets: F1 %d F2 %d F3 %d F4 %d F5 %d F6 %d F7 %d F8 %d", 
					GetPreset(0,0),GetPreset(0,1),GetPreset(0,2),GetPreset(0,3),
					GetPreset(0,4),GetPreset(0,5),GetPreset(0,6),GetPreset(0,7)
		);
	else if (GetPlayMode() == 1)
		mvwprintw(win, 1, 1, "Presets: F1 %d F2 %d F3 %d F4 %d F5 %d F6 %d F7 %d F8 %d", 
					GetPreset(1,0),GetPreset(1,1),GetPreset(1,2),GetPreset(1,3),
					GetPreset(1,4),GetPreset(1,5),GetPreset(1,6),GetPreset(1,7)
		);
	else
		mvwprintw(win, 1, 1, "Presets: F1 - F2 - F3 - F4 - F5 - F6 - F7 - F8 -");
		
	wrefresh(win);
}

bool scan_dab(WINDOW *win)
{
	wprintw(win, "Searching for DAB stations.....\n");
	
	wprintw(win, "Clearing database\n");
	wrefresh(win);
	ClearDatabase();

	int radiostatus(1), freq ;				
	if (DABAutoSearch(0,40)==true) {

		int totalProgram;
		while (radiostatus==1) {
			freq = GetFrequency();
			totalProgram = GetTotalProgram();
			wprintw(win, "Scanning index %d, Programs found=%d\r",freq,totalProgram);
			radiostatus = GetPlayStatus();
			wrefresh(win);
		}

		wprintw(win, "\nScan Complete\n");
		wrefresh(win);
		
	} else {
		wprintw(win, "\nDAB Station Scan Failed\n");
		wrefresh(win);
		return(false);
	}
	
	return(true);
	
}

void create_inidir(WINDOW *win) {
	
	string dircmd("mkdir -p ");
	dircmd.append(getenv("HOME"));
	dircmd.append("/.config/cantata-tech");
	
	wprintw(win, "Config file not found.\nCreating new config file\n");

	wprintw(win, "Creating directory %s\n", dircmd.c_str());
	const int direrr = system(dircmd.c_str());
	if (direrr == -1)
		wprintw(win, "Creating directory failed\n");
	else if (direrr == 0) {
		wprintw(win, "Created directory\n");

		wprintw(win, "writing %s\n", inipath.c_str());

		CSimpleIniA inifile;
		inifile.SetValue("Communications","port","/dev/ttyACM0");
		inifile.SaveFile(inipath.c_str());
	}

}
