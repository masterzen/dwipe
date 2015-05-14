/*
 *  gui.c: An ncurses GUI for dwipe. 
 *
 *  Copyright Darik Horn <dajhorn-dban@vanadac.com>.
 *  
 *  This program is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free Software
 *  Foundation, version 2.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  this program; if not, write to the Free Software Foundation, Inc., 675 Mass
 *  Ave, Cambridge, MA 02139, USA.
 *
 */


/* RATIONALE:
 *
 *   This entire GUI is a non-portable task-specific thunk.
 *
 *   The alternatives are, however, no better. The CDK is large and clumsy,
 *   and things like ncurses libmenu are not worth the storage overhead.
 *
 */

#include "dwipe.h"
#include "context.h"
#include "method.h"
#include "prng.h"
#include "options.h"
#include "gui.h"
#include "pass.h"


#define DWIPE_GUI_PANE        8

/* Header window: width, height, x coordinate, y coordinate. */
#define DWIPE_GUI_HEADER_W  COLS
#define DWIPE_GUI_HEADER_H  1
#define DWIPE_GUI_HEADER_X  0
#define DWIPE_GUI_HEADER_Y  0

/* Footer window: width, height, x coordinate, y coordinate. */
#define DWIPE_GUI_FOOTER_W  COLS
#define DWIPE_GUI_FOOTER_H  1
#define DWIPE_GUI_FOOTER_X  0
#define DWIPE_GUI_FOOTER_Y  (LINES -1)

/* Options window: width, height, x coordinate, y coordinate. */
#define DWIPE_GUI_OPTIONS_W   44
#define DWIPE_GUI_OPTIONS_H   8
#define DWIPE_GUI_OPTIONS_Y   1
#define DWIPE_GUI_OPTIONS_X   0

/* Options fields, relative to their window. */
#define DWIPE_GUI_OPTIONS_TAB        10
#define DWIPE_GUI_OPTIONS_ENTROPY_Y  1
#define DWIPE_GUI_OPTIONS_ENTROPY_X  1

#define DWIPE_GUI_OPTIONS_PRNG_Y     2
#define DWIPE_GUI_OPTIONS_PRNG_X     1

#define DWIPE_GUI_OPTIONS_METHOD_Y   3
#define DWIPE_GUI_OPTIONS_METHOD_X   1

#define DWIPE_GUI_OPTIONS_VERIFY_Y   4
#define DWIPE_GUI_OPTIONS_VERIFY_X   1

#define DWIPE_GUI_OPTIONS_FINGERPRINT_Y 5
#define DWIPE_GUI_OPTIONS_FINGERPRINT_X 1

#define DWIPE_GUI_OPTIONS_ROUNDS_Y   6
#define DWIPE_GUI_OPTIONS_ROUNDS_X   1

/* Stats window: width, height, x coordinate, y coordinate. */
#define DWIPE_GUI_STATS_W 36
#define DWIPE_GUI_STATS_H 8
#define DWIPE_GUI_STATS_Y 1
#define DWIPE_GUI_STATS_X 44

/* Stats fields, relative to their window. */
#define DWIPE_GUI_STATS_RUNTIME_Y     1
#define DWIPE_GUI_STATS_RUNTIME_X     1
#define DWIPE_GUI_STATS_ETA_Y         2
#define DWIPE_GUI_STATS_ETA_X         1
#define DWIPE_GUI_STATS_LOAD_Y        3
#define DWIPE_GUI_STATS_LOAD_X        1
#define DWIPE_GUI_STATS_THROUGHPUT_Y  4
#define DWIPE_GUI_STATS_THROUGHPUT_X  1
#define DWIPE_GUI_STATS_ERRORS_Y      5
#define DWIPE_GUI_STATS_ERRORS_X      1
#define DWIPE_GUI_STATS_TAB           16


/* Select window: width, height, x coordinate, y coordinate. */
#define DWIPE_GUI_MAIN_W  COLS
#define DWIPE_GUI_MAIN_H  ( LINES - DWIPE_GUI_MAIN_Y -1 )
#define DWIPE_GUI_MAIN_Y  9
#define DWIPE_GUI_MAIN_X  0

/* Configuration window: width, height, x coordinate, y coordinate. */
#define DWIPE_GUI_CONFIG_W 40
#define DWIPE_GUI_CONFIG_H 15
#define DWIPE_GUI_CONFIG_Y (LINES - DWIPE_GUI_CONFIG_H) / 2
#define DWIPE_GUI_CONFIG_X (COLS - DWIPE_GUI_CONFIG_W) / 2

/* Verification settings window: width, height, x coordinate, y coordinate */
#define DWIPE_GUI_VERIFY_H 20
#define DWIPE_GUI_VERIFY_W 80
#define DWIPE_GUI_VERIFY_Y (LINES - DWIPE_GUI_VERIFY_H) / 2
#define DWIPE_GUI_VERIFY_X (COLS - DWIPE_GUI_VERIFY_W) / 2

/* PRNG settings window: width, height, x coordinate, y coordinate */
#define DWIPE_GUI_PRNG_H 17
#define DWIPE_GUI_PRNG_W 75
#define DWIPE_GUI_PRNG_Y (LINES - DWIPE_GUI_PRNG_H) / 2
#define DWIPE_GUI_PRNG_X (COLS - DWIPE_GUI_PRNG_W) / 2

/* Method selection window: width, height, x coordinate, y coordinate */
#define DWIPE_GUI_METHOD_H 22
#define DWIPE_GUI_METHOD_W 85
#define DWIPE_GUI_METHOD_Y (LINES - DWIPE_GUI_METHOD_H) / 2
#define DWIPE_GUI_METHOD_X (COLS - DWIPE_GUI_METHOD_W) / 2

/* Round count window: width, height, x coordinate, y coordinate */
#define DWIPE_GUI_ROUNDS_H 12
#define DWIPE_GUI_ROUNDS_W 76
#define DWIPE_GUI_ROUNDS_Y (LINES - DWIPE_GUI_ROUNDS_H) / 2
#define DWIPE_GUI_ROUNDS_X (COLS - DWIPE_GUI_ROUNDS_W) / 2


/* Window pointers. */
WINDOW* footer_window;
WINDOW* header_window;
WINDOW* main_window;
WINDOW* options_window;
WINDOW* stats_window;
WINDOW* config_window;


/* Options window title. */
const char* options_title = " Options ";

/* Statistics window title. */
const char* stats_title = " Statistics ";

/* Footer labels. */
const char* dwipe_buttons1 = " C=Configuration, J=Up K=Down Space=Select, F10=Start ";
const char* dwipe_buttons2 = " J=Up K=Down Space=Select Escape=Cancel";



void dwipe_gui_title( WINDOW* w, const char* s, const int row )
{
/**
 * Prints the string 's' centered on the first line of the window 'w'.
 *
 */

	/* The number of lines in the window. (Not used.) */
	int wy;

	/* The number of columns in the window. */
	int wx;

	/* Get the window dimensions. */
	getmaxyx( w, wy, wx );

	/* Print the title. */
	mvwprintw( w, row, ( wx - strlen( s ) ) / 2, "%s", s );
} /* dwipe_gui_title */


void dwipe_gui_init( void )
{
/**
 * Initializes the ncurses gui.
 *
 */

	/* Initialize the screen. */
	initscr();

	/* Disable TTY line buffering. */
	cbreak();

	/* Disable TTY echo. */
	noecho();

	/* Enable most special keys. */
	keypad( stdscr, TRUE );

	/* Lower escape key delay */
	set_escdelay(25);

	if( has_colors() )
	{
		/* Initialize color capabilities. */
		start_color();

		if( can_change_color() )
		{
			/* Redefine cyan to gray. */
			init_color( COLOR_CYAN, 128, 128, 128 );
		}

		/* Set white on blue as the emphasis color. */
		init_pair( 1, COLOR_WHITE, COLOR_BLUE );

		/* Set gray (or cyan) on blue as the normal color. */
		init_pair( 2, COLOR_CYAN, COLOR_BLUE );

		/* Set red on blue as the hilite color. */
		init_pair( 3, COLOR_RED, COLOR_BLUE );

		/* Set blue on white as the color for the header and footer windows. */
		init_pair( 4, COLOR_BLUE, COLOR_WHITE );

		/* Set white on green for success messages. */
		init_pair( 5, COLOR_WHITE, COLOR_GREEN );

		/* Set white on green for failure messages. */
		init_pair( 6, COLOR_WHITE, COLOR_RED );

		/* White on black for configuration menu */
		init_pair(7, COLOR_WHITE, COLOR_BLACK);

		/* Set the background style. */
		wbkgdset( stdscr, COLOR_PAIR(1) | ' ' );

	}

	/* Clear the screen. */
	wclear( stdscr );


	/* Create the header window. */
	header_window = newwin( DWIPE_GUI_HEADER_H, DWIPE_GUI_HEADER_W, DWIPE_GUI_HEADER_Y, DWIPE_GUI_HEADER_X );

	if( has_colors() )
	{
		/* Set the background style of the header window. */
		wbkgdset( header_window, COLOR_PAIR(4) | ' ' );
	}

	/* Clear the header window. */
	wclear( header_window );

	/* Print the product banner. */
	dwipe_gui_title( header_window, dwipe_options.banner, 0 );

	/* Create the footer window. */
	footer_window = newwin( DWIPE_GUI_FOOTER_H, DWIPE_GUI_FOOTER_W, DWIPE_GUI_FOOTER_Y, DWIPE_GUI_FOOTER_X );

	if( has_colors() )
	{
		/* Set the background style of the footer window. */
		wbkgdset( footer_window, COLOR_PAIR(4) | ' ' );
	}

	/* Clear the footer window. */
	wclear( footer_window );

	/* Create the options window. */
	options_window = newwin( DWIPE_GUI_OPTIONS_H, DWIPE_GUI_OPTIONS_W, DWIPE_GUI_OPTIONS_Y, DWIPE_GUI_OPTIONS_X );

	if( has_colors() )
	{
		/* Set the background style of the options window. */
		wbkgdset( options_window, COLOR_PAIR(1) | ' ' );

		/* Apply the color change to the options window. */
		wattron( options_window, COLOR_PAIR(1) );
	}

	/* Clear the options window. */
	wclear( options_window );

	/* Add a border. */
	box( options_window, 0, 0 );

	/* Create the stats window. */
	stats_window = newwin( DWIPE_GUI_STATS_H, DWIPE_GUI_STATS_W, DWIPE_GUI_STATS_Y, DWIPE_GUI_STATS_X );

	if( has_colors() )
	{
		/* Set the background style of the stats window. */
		wbkgdset( stats_window, COLOR_PAIR(1) | ' ' );

		/* Apply the color change to the stats window. */
		wattron( stats_window, COLOR_PAIR(1) );
	}

	/* Clear the new window. */
	wclear( stats_window );

	/* Add a border. */
	box( stats_window, 0, 0 );

	/* Add a title. */
	dwipe_gui_title( stats_window, stats_title, 0 );

	/* Print field labels. */
	mvwprintw( stats_window, DWIPE_GUI_STATS_RUNTIME_Y,    DWIPE_GUI_STATS_RUNTIME_X,    "Runtime:       " );
	mvwprintw( stats_window, DWIPE_GUI_STATS_ETA_Y,        DWIPE_GUI_STATS_ETA_X,        "Remaining:     " );
	mvwprintw( stats_window, DWIPE_GUI_STATS_LOAD_Y,       DWIPE_GUI_STATS_LOAD_X,       "Load Averages: " );
	mvwprintw( stats_window, DWIPE_GUI_STATS_THROUGHPUT_Y, DWIPE_GUI_STATS_THROUGHPUT_X, "Throughput:    " );
	mvwprintw( stats_window, DWIPE_GUI_STATS_ERRORS_Y,     DWIPE_GUI_STATS_ERRORS_X,     "Errors:        " );


	/* Create the main window. */
	main_window = newwin( DWIPE_GUI_MAIN_H, DWIPE_GUI_MAIN_W, DWIPE_GUI_MAIN_Y, DWIPE_GUI_MAIN_X );

	if( has_colors() )
	{
		/* Set the background style. */
		wbkgdset( main_window, COLOR_PAIR(1) | ' ' );

      /* Apply the color change. */
      wattron( main_window, COLOR_PAIR(1) );
	}

	/* Clear the main window. */
	werase( main_window );

	/* Add a border. */
	box( main_window, 0, 0 );

	/* Refresh the screen. */
	wrefresh( stdscr );
	wrefresh( header_window );
	wrefresh( footer_window );
	wrefresh( options_window );
	wrefresh( stats_window );
	wrefresh( main_window );

	/* Hide the cursor. */
	curs_set( 0 );

} /* dwipe_gui_init */


void dwipe_gui_free( void )
{
/**
 * Releases the ncurses gui.
 *
 */

	/* Free ncurses resources. */
	delwin( footer_window  );
	delwin( header_window  );
	delwin( main_window    );
	delwin( options_window );
	delwin( stats_window   );
	delwin( config_window  );
	endwin();

} /* dwipe_gui_free */


void dwipe_gui_select( int count, dwipe_context_t* c )
{
/**
 * The primary user interface.  Allows the user to
 * change options and specify the devices to be wiped.
 *
 * @parameter  count       The number of contexts in the array.
 * @parameter  c           An array of device contexts.
 *
 * @modifies   c[].select  Sets the select flag according to user input.
 * @modifies   options     Sets program options according to to user input.
 *
 */

	/* Widget labels. */
	const char* select_title = " Disks and Partitions ";

	/* The number of lines available in the window. */
	int wlines;

	/* The number of columns available in the window. */
	int wcols;

	/* The number of selection elements that we can show in the window. */
	int slots;

	/* The index of the element that is visible in the first slot. */
	int offset = 0;

	/* The selection focus. */
	int focus = 0;

	/* A generic loop variable. */
	int i;

	/* User input buffer. */
	int keystroke;

	/* The current working line. */
	int yy;

	/* The number of currently selected elements. */
	int selected = 0;

	/* There is one slot per line. */
	getmaxyx( main_window, wlines, wcols );

	/* Less two lines for the box and two lines for padding. */
	slots = wlines - 4;



	do
	{
		/* Clear the main window. */
		werase( main_window );

		/* Update the footer window. */
		werase( footer_window );
		dwipe_gui_title( footer_window, dwipe_buttons1, 0 );
		wrefresh( footer_window );

		/* Update the options window. */
		dwipe_gui_options();

		/* Initialize the line offset. */
		yy = 2;

		for( i = 0; i < slots && i < count ; i++ )
		{
			/* Move to the next line. */
			mvwprintw( main_window, yy++, 1, " " );

			if( i + offset == focus )
			{
				if( c[focus].select == DWIPE_SELECT_TRUE || c[focus].select == DWIPE_SELECT_FALSE )
				{
					/* Print the 'enabled' cursor. */
					waddch( main_window, ACS_RARROW );
				}

				else
				{
					/* Print the 'disabled' cursor. */
					waddch( main_window, ACS_DIAMOND );
				}
			}

			else
			{
				/* Print whitespace. */
				waddch( main_window, ' '  );
			}


			switch( c[i+offset].select )
			{
				case DWIPE_SELECT_TRUE:

					/* Print a selected element. */
					wprintw( main_window, " [wipe] %s", c[i+offset].label );
					break;

				case DWIPE_SELECT_FALSE:

					/* Print an element that is not selected. */
					wprintw( main_window, " [    ] %s", c[i+offset].label );
					break;

				case DWIPE_SELECT_TRUE_PARENT:

					/* This element will be wiped when its parent is wiped. */
					wprintw( main_window, " [****] %s", c[i+offset].label );
					break;

				case DWIPE_SELECT_FALSE_CHILD:

					/* We can't wipe this element because it has a child that is being wiped. */
					wprintw( main_window, " [----] %s", c[i+offset].label );
					break;

				case DWIPE_SELECT_DISABLED:

					/* We don't know how to wipe this device. (Iomega Zip drives.) */
					wprintw( main_window, " [????] %s", "Unrecognized Device" );
					break;

				default:
					
					/* TODO: Handle the sanity error. */
					break;


			} /* switch select */

		} /* for */

		if( offset > 0 )
		{
			mvwprintw( main_window, 1, wcols -8, " More " );
			waddch( main_window, ACS_UARROW );
		}

		if( count - offset > slots )
		{
			mvwprintw( main_window, wlines -2, wcols -8, " More " );
			waddch( main_window, ACS_DARROW );
		}

		/* Draw a border around the menu window. */
		box( main_window, 0, 0 );

		/* Print a title. */
		dwipe_gui_title( main_window, select_title, 0 );

		/* Refresh the window. */
		wrefresh( main_window );

		/* Get user input. */
		keystroke = getch();

		switch( keystroke )
		{
			case KEY_DOWN:
			case 'j':
			case 'J':

				/* Increment the focus. */
				focus += 1;

				if( focus >= count )
				{
					/* The focus is already at the last element. */
					focus = count -1;
					break;
				}

				if( focus - offset >= slots )
				{
					/* The next element is offscreen. Scroll down. */
					offset += 1;
					break;
				}

				break;

			case KEY_UP:
			case 'k':
			case 'K':

				/* Decrement the focus. */
				focus -= 1;

				if( focus < 0 )
				{
					/* The focus is already at the last element. */
					focus = 0;
					break;
				}

				if( focus < offset )
				{
					/* The next element is offscreen. Scroll up. */
					offset -= 1;
					break;
				}

				break;

			case KEY_ENTER:
			case 10:
			case ' ':

				/* TODO: This block should be made into a function. */

				if( c[focus].select == DWIPE_SELECT_TRUE )
				{
					/* Reverse the selection of this element. */
					c[focus].select = DWIPE_SELECT_FALSE;

					if( c[focus].device_part == 0 )
					{
						/* Sub-deselect all partitions and slices within this disk. */
						for( i = 0 ; i < count ; i++ )
						{
							if
							(
								   c[i].device_type   == c[focus].device_type
								&& c[i].device_host   == c[focus].device_host
								&& c[i].device_bus    == c[focus].device_bus
								&& c[i].device_target == c[focus].device_target
								&& c[i].device_lun    == c[focus].device_lun
								&& c[i].device_part   > 0
							)
							{
								c[i].select = DWIPE_SELECT_FALSE;
							}

						} /* for all contexts */

					} /* if sub-deselect */

					else
					{
						/* The number of selected partitions or slices within this disk. */
						int j = 0;

						for( i = 0 ; i < count ; i++ )
						{
							if
							(
								   c[i].device_type   == c[focus].device_type
								&& c[i].device_host   == c[focus].device_host
								&& c[i].device_bus    == c[focus].device_bus
								&& c[i].device_target == c[focus].device_target
								&& c[i].device_lun    == c[focus].device_lun
								&& c[i].device_part   > 0
								&& c[i].select        == DWIPE_SELECT_TRUE
							)
							{
								/* Increment the counter. */
								j += 1;
							}

						} /* for all contexts */

						if( j == 0 )
						{
							/* Find the parent disk of this partition or slice. */
							for( i = 0 ; i < count ; i++ )
							{
								if
								(
									   c[i].device_type   == c[focus].device_type
									&& c[i].device_host   == c[focus].device_host
									&& c[i].device_bus    == c[focus].device_bus
									&& c[i].device_target == c[focus].device_target
									&& c[i].device_lun    == c[focus].device_lun
									&& c[i].device_part   == 0
								)
								{
									/* Enable the disk element. */
									c[i].select = DWIPE_SELECT_FALSE;
								}

							} /* for all contexts */

						} /* if */

					} /* else super-enable */

					break;

				} /* if DWIPE_SELECT_TRUE */

				if( c[focus].select == DWIPE_SELECT_FALSE )
				{
					/* Reverse the selection. */
					c[focus].select = DWIPE_SELECT_TRUE;

					if( c[focus].device_part == 0 )
					{
						/* Sub-select all partitions and slices within this disk. */
						for( i = 0 ; i < count ; i++ )
						{
							if
							(
								   c[i].device_type   == c[focus].device_type
								&& c[i].device_host   == c[focus].device_host
								&& c[i].device_bus    == c[focus].device_bus
								&& c[i].device_target == c[focus].device_target
								&& c[i].device_lun    == c[focus].device_lun
								&& c[i].device_part   > 0
							)
							{
								c[i].select = DWIPE_SELECT_TRUE_PARENT;
							}

						} /* for */

					} /* if sub-select */

					else
					{
						/* ASSERT: ( c[focus].device_part > 0 ) */

						/* Super-deselect the disk that contains this device. */
						for( i = 0 ; i < count ; i++ )
						{
							if
							(
								   c[i].device_type   == c[focus].device_type
								&& c[i].device_host   == c[focus].device_host
								&& c[i].device_bus    == c[focus].device_bus
								&& c[i].device_target == c[focus].device_target
								&& c[i].device_lun    == c[focus].device_lun
								&& c[i].device_part   == 0
							)
							{
								c[i].select = DWIPE_SELECT_FALSE_CHILD;
							}
						}

					} /* else super-deselect */

					break;

				} /* if DWIPE_SELECT_FALSE */

				/* TODO: Explain to the user why they can't change this. */
				break;

			case 'm':
			case 'M':
			
				/*  Run the method dialog. */
				dwipe_gui_method();
				break;

			case 'p':
			case 'P':

				/* Run the PRNG dialog. */
				dwipe_gui_prng();
				break;

			case 'r':
			case 'R':

				/* Run the rounds dialog. */
				dwipe_gui_rounds();
				break;

			case 'v':
			case 'V':

				/* Run the option dialog. */
				dwipe_gui_verify();
				break;
			case 'f':
			case 'F':
				/* Toggle fingerprinting */
				dwipe_options.fingerprint = !dwipe_options.fingerprint;
				break;
			case 'c':
			case 'C':
				/* Show configuration menu */
				dwipe_gui_configuration();
				break;

		} /* keystroke switch */

		selected = 0;
		for( i = 0 ; i < count ; i++ )
		{
			if( c[i].select == DWIPE_SELECT_TRUE )
			{
				selected += 1;
			};
		}

	} while( keystroke != KEY_F(10) || selected == 0 );

	/* Clear the main window. */
	werase( main_window );

	/* Refresh the main window. */
	wrefresh( main_window );

	/* Clear the footer. */
	werase( footer_window );

	/* Refresh the footer window. */
	wrefresh( footer_window );

} /* dwipe_gui_select */



void dwipe_gui_options( void )
{
/**
 * Updates the options window.
 *
 * @modifies  options_window
 *
 */

	/* Erase the window. */
	werase( options_window );

	mvwprintw( options_window, DWIPE_GUI_OPTIONS_ENTROPY_Y, DWIPE_GUI_OPTIONS_ENTROPY_X, \
	  "Entropy: Linux Kernel (urandom)" );

	mvwprintw( options_window, DWIPE_GUI_OPTIONS_PRNG_Y,    DWIPE_GUI_OPTIONS_PRNG_X, \
	  "PRNG:    %s", dwipe_options.prng->label );

	mvwprintw( options_window, DWIPE_GUI_OPTIONS_METHOD_Y,  DWIPE_GUI_OPTIONS_METHOD_X, \
	  "Method:  %s", dwipe_method_label( dwipe_options.method) );

	mvwprintw( options_window, DWIPE_GUI_OPTIONS_VERIFY_Y,  DWIPE_GUI_OPTIONS_VERIFY_X,  "Verify:  " );

	switch( dwipe_options.verify )
	{
		case DWIPE_VERIFY_NONE:
			wprintw( options_window, "Off" );
			break;

		case DWIPE_VERIFY_LAST:
			wprintw( options_window, "Last Pass" );
			break;

		case DWIPE_VERIFY_ALL:
			wprintw( options_window, "All Passes" );
			break;

		default:
			wprintw( options_window, "Unknown %i", dwipe_options.verify );
			
	} /* switch verify */

	mvwprintw( options_window, DWIPE_GUI_OPTIONS_FINGERPRINT_Y,  DWIPE_GUI_OPTIONS_FINGERPRINT_X, \
	  "Fingerprinting:  %s", (dwipe_options.fingerprint ? "On" : "Off"));

	mvwprintw( options_window, DWIPE_GUI_OPTIONS_ROUNDS_Y,  DWIPE_GUI_OPTIONS_ROUNDS_X,  "Rounds:  " );
	wprintw( options_window, "%i", dwipe_options.rounds );


	/* Add a border. */
	box( options_window, 0, 0 );

	/* Add a title. */
	dwipe_gui_title( options_window, options_title, 0 );

	/* Refresh the window. */
	wrefresh( options_window );

} /* dwipe_gui_options */



void dwipe_gui_rounds( void )
{
/**
 * Allows the user to change the rounds option.
 *
 * @modifies  dwipe_options.rounds
 *
 */

	int keystroke;

	/* Create Window */
	WINDOW* win;

	win = newwin( DWIPE_GUI_ROUNDS_H, DWIPE_GUI_ROUNDS_W, DWIPE_GUI_ROUNDS_Y, DWIPE_GUI_ROUNDS_X );

	/* Set the initial focus. */
	int focus = dwipe_options.rounds;

	/* Max rounds allowed */
	int focus_max = 10000;

	if( has_colors() )
	{
		/* Set the background style. */
		wbkgdset( win, COLOR_PAIR(7) | ' ' );

		/* Apply the color change. */
		wattron( win, COLOR_PAIR(7) );
	}

	/* Clear the window. */
	werase( win );
	/* Add a border. */
	box(win, 0, 0);

	wrefresh(win);

	int tab1 = 5; // Starting column for menu
	int row = 4; // Starting row for menu


	bool breakLoop = false;
	bool noSave = false;

	curs_set(1);
	while (!breakLoop) {
		int help_row = 6; // Starting row for help

		werase(win);
		dwipe_gui_title(win, "Number of Rounds", 1);

		mvwprintw(win, help_row++, tab1, "This is the number of times to run the wipe method on each device.");
		help_row++;
		if (focus > 0) {
			/* Print the syslinux configuration hint. */
			mvwprintw(win, help_row++, tab1, "syslinux.cfg:  nuke=\"dwipe --rounds %d\"", focus);

			/* Print this line last so that the cursor is in the right place. */
			mvwprintw(win, row, tab1, "> %d", focus);
		} else {
			mvwprintw(win, help_row++, tab1, "The number of rounds must be a non-negative integer.");

			/* Print this line last so that the cursor is in the right place. */
			mvwprintw(win, row, tab1, "> ");
		}

		/* Refresh Window */
		wrefresh(win);

		keystroke = getch();
		switch (keystroke) {
			case 27: // Escape
				breakLoop = true;
				noSave = true;
			break;
			case KEY_DOWN:
			case 'k':
			case 'K':
				if (focus > 1) focus--;
			break;
			case KEY_UP:
			case 'j':
			case 'J':
				if (focus < focus_max) focus++;
			break;
			case KEY_ENTER:
			case ' ':
			case 10:
				if (focus > 0) {
					breakLoop = true;
				}
			break;
			case KEY_LEFT:
			case KEY_BACKSPACE:
			case 127:
				/* Right shift, base ten. */
				focus /= 10;
			break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case '0':
				/* Left shift, base ten. */
				focus *= 10;

				/* Add new input */
				focus += keystroke - '0';

				if (focus > focus_max) {
					focus = focus_max;
				}
			break;
		}
	}

	if (!noSave) {
		dwipe_options.rounds = focus;
	}

	werase(win);
	delwin(win);
	curs_set(0);
} /* dwipe_guid_rounds */



void dwipe_gui_prng( void )
{
/**
 * Allows the user to change the PRNG.
 *
 * @modifies  dwipe_options.prng
 *
 */

	extern dwipe_prng_t dwipe_twister;
	extern dwipe_prng_t dwipe_isaac;


	int keystroke;

	/* Create Window */
	WINDOW* win;

	win = newwin( DWIPE_GUI_PRNG_H, DWIPE_GUI_PRNG_W, DWIPE_GUI_PRNG_Y, DWIPE_GUI_PRNG_X );

	int selected_item = COLOR_PAIR(1);
	int normal_item = COLOR_PAIR(7);

	/* Set the initial focus. */
	int focus;
	if( dwipe_options.prng == &dwipe_twister ) { focus = 0; }
	if( dwipe_options.prng == &dwipe_isaac   ) { focus = 1; }

	if( has_colors() )
	{
		/* Set the background style. */
		wbkgdset( win, COLOR_PAIR(7) | ' ' );

		/* Apply the color change. */
		wattron( win, COLOR_PAIR(7) );
	}

	/* Clear the window. */
	werase( win );
	/* Add a border. */
	box(win, 0, 0);

	wrefresh(win);

	struct dwipe_menu_items items[] = {
			{1, dwipe_twister.label },
			{2, dwipe_isaac.label },
		};
	int count = sizeof(items) / sizeof(items[0]); // Number of items


	// Work out the longest line to pad others to match
	int i = 0;
	int longest = 0;
	for (i = 0; i < count; i++) {
		if (strlen(items[i].text) > longest) {
			longest = strlen(items[i].text);
		}
	}
	longest += 5; // Add a slight border

	int tab1 = 5; // Starting column for menu
	int row = 4; // Starting row for menu

	bool breakLoop = false;
	bool noSave = false;

	while (!breakLoop) {
		int help_row = 8; // Starting row for help

		werase(win);
		dwipe_gui_title(win, "Pseudo Random Number Generator", 1);

		int i = 0;
		for (i = 0; i < count; i++) {
			if (focus == i) wattron(win, selected_item);
			mvwprintw(win, row + i, tab1, "%d) %s%*s", items[i].number, items[i].text, (longest - strlen(items[i].text)), " ");
			if (focus == i) wattron(win, normal_item);
		}

		switch (focus) {
			case 0:
				mvwprintw(win, help_row++, tab1, "syslinux.cfg:  nuke=\"dwipe --prng twister\"" );
				help_row++;
				mvwprintw(win, help_row++, tab1, "The Mersenne Twister, by Makoto Matsumoto and Takuji Nishimura, is a");
				mvwprintw(win, help_row++, tab1, "generalized feedback shift register PRNG that is uniform and");
				mvwprintw(win, help_row++, tab1, "equidistributed in 623-dimensions with a proven period of 2^19937-1.");
				help_row++;
				mvwprintw(win, help_row++, tab1, "This implementation passes the Marsaglia Diehard test suite.");
			break;
			case 1:
				mvwprintw(win, help_row++, tab1, "syslinux.cfg:  nuke=\"dwipe --prng isaac\"");
				help_row++;
				mvwprintw(win, help_row++, tab1, "ISAAC, by Bob Jenkins, is a PRNG derived from RC4 with a minimum");
				mvwprintw(win, help_row++, tab1, "period of 2^40 and an expected period of 2^8295.  It is difficult to");
				mvwprintw(win, help_row++, tab1, "recover the initial PRNG state by cryptanalysis of the ISAAC stream.");
			break;
		}

		/* Refresh Window */
		wrefresh(win);

		keystroke = getch();
		switch (keystroke) {
			case KEY_BACKSPACE:
			case 127:
			case 27: // Escape
				breakLoop = true;
				noSave = true;
			break;
			case KEY_DOWN:
			case 'k':
			case 'K':
				if (focus < count - 1) focus++;
				else focus = 0;
			break;
			case KEY_UP:
			case 'j':
			case 'J':
				if (focus > 0) focus--;
				else focus = count - 1;
			break;
			case KEY_ENTER:
			case ' ':
			case 10: {
				int i = 0;
				for (i = 0; i < count; i++) {
					if (focus == i) {
						breakLoop = true;
					}
				}
			}
			break;
			case '1':
			case '2':
			case '3':
				focus = keystroke - '0' - 1;
				//breakLoop = true;
			break;
		}
	}

	if (!noSave) {
		if (focus == 0) { dwipe_options.prng = &dwipe_twister; }
		else if (focus == 1) { dwipe_options.prng = &dwipe_isaac; }
	}

	werase(win);
	delwin(win);
} /* dwipe_gui_prng */



void dwipe_gui_verify( void )
{
/**
 * Allows the user to change the verification option.
 *
 * @modifies  dwipe_options.verify
 *
 */

	int keystroke;

	/* Create Window */
	WINDOW* win;

	win = newwin( DWIPE_GUI_VERIFY_H, DWIPE_GUI_VERIFY_W, DWIPE_GUI_VERIFY_Y, DWIPE_GUI_VERIFY_X );

	int selected_item = COLOR_PAIR(1);
	int normal_item = COLOR_PAIR(7);

	/* Set the initial focus. */
	int focus = dwipe_options.verify;
	
	if( has_colors() )
	{
		/* Set the background style. */
		wbkgdset( win, COLOR_PAIR(7) | ' ' );

		/* Apply the color change. */
		wattron( win, COLOR_PAIR(7) );
	}

	/* Clear the window. */
	werase( win );
	/* Add a border. */
	box(win, 0, 0);

	wrefresh(win);


	struct dwipe_menu_items items[] = {
			{1, "Verification Off "},
			{2, "Verify Last Pass "},
			{3, "Verify All Passes"},
		};
	int count = sizeof(items) / sizeof(items[0]); // Number of items


	int tab1 = 5; // Starting column for menu
	int row = 4; // Starting row for menu

	
	bool breakLoop = false;
	bool noSave = false;

	while (!breakLoop) {
		int help_row = 10; // Starting row for help

		werase(win);
		dwipe_gui_title(win, "Verification Mode", 1);

		int i = 0;
		for (i = 0; i < count; i++) {
			if (focus == i) wattron(win, selected_item);
			mvwprintw(win, row + i, tab1, "%d) %s", items[i].number, items[i].text);
			if (focus == i) wattron(win, normal_item);
		}

		switch (focus) {
			case 0:
				mvwprintw(win, help_row++, tab1, "syslinux.cfg:  nuke=\"dwipe --verify off\"");
				help_row++;
				mvwprintw(win, help_row++, tab1, "Do not verify passes. The wipe will be a write-only operation.");
			break;
			case 1:
				mvwprintw(win, help_row++, tab1, "syslinux.cfg:  nuke=\"dwipe --verify last\"");
				help_row++;
				mvwprintw(win, help_row++, tab1, "Check whether the device is actually empty after the last pass fills the");
				mvwprintw(win, help_row++, tab1, "device with zeros.");
			break;
			case 2:
				mvwprintw(win, help_row++, tab1, "syslinux.cfg:  nuke=\"dwipe --verify all\"");
				help_row++;
				mvwprintw(win, help_row++, tab1, "After every pass, read back the pattern and check whether it is correct.");
				mvwprintw(win, help_row++, tab1, "");
				mvwprintw(win, help_row++, tab1, "This program writes the entire length of the device before it reads back");
				mvwprintw(win, help_row++, tab1, "for verification, even for random pattern passes, to better ensure that");
				mvwprintw(win, help_row++, tab1, "hardware caches are actually flushed.");
			break;
		}

		/* Refresh Window */
		wrefresh(win);

		keystroke = getch();
		switch (keystroke) {
			case KEY_BACKSPACE:
			case 127:
			case 27: // Escape
				breakLoop = true;
				noSave = true;
			break;
			case KEY_DOWN:
			case 'k':
			case 'K':
				if (focus < count - 1) focus++;
				else focus = 0;
			break;
			case KEY_UP:
			case 'j':
			case 'J':
				if (focus > 0) focus--;
				else focus = count - 1;
			break;
			case KEY_ENTER:
			case ' ':
			case 10: {
				int i = 0;
				for (i = 0; i < count; i++) {
					if (focus == i) {
						breakLoop = true;
					}
				}
			}
			break;
			case '1':
			case '2':
			case '3':
				focus = keystroke - '0' - 1;
				//breakLoop = true;
			break;
		}
	}

	if (!noSave) {
		dwipe_options.verify = focus;
	}

	werase(win);
	delwin(win);
} /* dwipe_gui_verify */



void dwipe_gui_configuration( void )
{
	/* Create the configuration menu */
	config_window = newwin( DWIPE_GUI_CONFIG_H, DWIPE_GUI_CONFIG_W, DWIPE_GUI_CONFIG_Y, DWIPE_GUI_CONFIG_X );

	if( has_colors() )
	{
		/* Set the background style. */
		wbkgdset( config_window, COLOR_PAIR(7) | ' ' );

		/* Apply the color change. */
		wattron( config_window, COLOR_PAIR(7) );
	}

	int selected_item = COLOR_PAIR(1);
	int normal_item = COLOR_PAIR(7);

	/* Clear the config window. */
	werase( config_window );
	/* Add a border. */
	box(config_window, 0, 0);


	struct dwipe_menu_items items[] = {
			{1, "PRNG                   "},
			{2, "Method                 "},
			{3, "Verify                 "},
			{4, "Rounds                 "},
			{5, "Toggle fingerprinting  "},
			{0, "Continue               "}
		};


	int keystroke;
	int tab1 = 5; // Starting column for menu
	int row = 4; // Starting row for menu
	int focus = -1; // Currently selected item
	int count = sizeof(items) / sizeof(items[0]); // Number of items

	/* Change footer to menu keys */
	werase(footer_window);
	dwipe_gui_title(footer_window, dwipe_buttons2, 0);
	wrefresh(footer_window);

	do {
		werase(config_window);
		dwipe_gui_title(config_window, "Settings", 1);

		int i = 0;
		for (i = 0; i < count; i++) {
			if (focus == i) wattron(config_window, selected_item); // Highlight this line
			mvwprintw(config_window, row + i, tab1, "%d) %s", items[i].number, items[i].text);
			if (focus == i) wattron(config_window, normal_item); // Reset colour
		}

		/* Refresh Window */
		wrefresh(config_window);

		keystroke = getch();
		switch (keystroke) {
			case KEY_DOWN:
			case 'k':
			case 'K':
				if (focus < count - 1) focus++;
				else focus = 0;
			break;
			case KEY_UP:
			case 'j':
			case 'J':
				if (focus > 0) focus--;
				else focus = count - 1;
			break;
			case KEY_ENTER:
			case ' ':
			case 10:
				if (focus > -1) {
					int i = 0;
					for (i = 0; i < count; i++) {
						if (focus == i) {
							keystroke = '0' + items[i].number;
						}
					}
				}
			break;
		}

		switch (keystroke) {
			case '1':
				dwipe_gui_prng();
			break;
			case '2':
				dwipe_gui_method();
			break;
			case '3':
				dwipe_gui_verify();
			break;
			case '4':
				dwipe_gui_rounds();
			break;
			case '5':
				dwipe_options.fingerprint = !dwipe_options.fingerprint;
			break;
		}

		refresh_all_windows();
		dwipe_gui_options();
	} while (keystroke != '0' && keystroke != KEY_BACKSPACE && keystroke != 127 && keystroke != 27);

	delwin(config_window);
} /* dwipe_gui_configuration */



void dwipe_gui_method( void )
{
/**
 * Allows the user to change the wipe method.
 *
 * @modifies  dwipe_options.method
 *
 */

	int keystroke;

	/* Create Window */
	WINDOW* win;

	win = newwin( DWIPE_GUI_METHOD_H, DWIPE_GUI_METHOD_W, DWIPE_GUI_METHOD_Y, DWIPE_GUI_METHOD_X );

	int selected_item = COLOR_PAIR(1);
	int normal_item = COLOR_PAIR(7);

	/* Set the initial focus. */
	int focus;
	if( dwipe_options.method == &dwipe_zero       ) { focus = 0; }
	if( dwipe_options.method == &dwipe_ops2       ) { focus = 1; }
	if( dwipe_options.method == &dwipe_dodshort   ) { focus = 2; }
	if( dwipe_options.method == &dwipe_dod522022m ) { focus = 3; }
	if( dwipe_options.method == &dwipe_gutmann    ) { focus = 4; }
	if( dwipe_options.method == &dwipe_random     ) { focus = 5; }

	if( has_colors() )
	{
		/* Set the background style. */
		wbkgdset( win, COLOR_PAIR(7) | ' ' );

		/* Apply the color change. */
		wattron( win, COLOR_PAIR(7) );
	}

	/* Clear the window. */
	werase( win );
	/* Add a border. */
	box(win, 0, 0);

	wrefresh(win);

		if( dwipe_options.method == &dwipe_zero       ) { focus = 0; }
	if( dwipe_options.method == &dwipe_ops2       ) { focus = 1; }
	if( dwipe_options.method == &dwipe_dodshort   ) { focus = 2; }
	if( dwipe_options.method == &dwipe_dod522022m ) { focus = 3; }
	if( dwipe_options.method == &dwipe_gutmann    ) { focus = 4; }
	if( dwipe_options.method == &dwipe_random     ) { focus = 5; }
	
	struct dwipe_menu_items items[] = {
			{1, dwipe_method_label(&dwipe_zero) },
			{2, dwipe_method_label(&dwipe_ops2) },
			{3, dwipe_method_label(&dwipe_dodshort) },
			{4, dwipe_method_label(&dwipe_dod522022m) },
			{5, dwipe_method_label(&dwipe_gutmann) },
			{6, dwipe_method_label(&dwipe_random) },
		};
	int count = sizeof(items) / sizeof(items[0]); // Number of items


	// Work out the longest line to pad others to match
	int i = 0;
	int longest = 0;
	for (i = 0; i < count; i++) {
		if (strlen(items[i].text) > longest) {
			longest = strlen(items[i].text);
		}
	}
	longest += 5; // Add a slight border

	int tab1 = 5; // Starting column for menu
	int row = 4; // Starting row for menu

	bool breakLoop = false;
	bool noSave = false;

	while (!breakLoop) {
		int help_row = 12; // Starting row for help

		werase(win);
		dwipe_gui_title(win, "Wipe Method", 1);

		int i = 0;
		for (i = 0; i < count; i++) {
			if (focus == i) wattron(win, selected_item);
			mvwprintw(win, row + i, tab1, "%d) %s%*s", items[i].number, items[i].text, (longest - strlen(items[i].text)), " ");
			if (focus == i) wattron(win, normal_item);
		}

		switch (focus) {
			case 0:
				mvwprintw(win, help_row++, tab1, "syslinux.cfg: nuke=\"dwipe --method zero\"");
				mvwprintw(win, help_row++, tab1, "Security Level: Low (1 pass)");
				help_row++;
				mvwprintw(win, help_row++, tab1, "This method fills the device with zeros. Note that the rounds option does");
				mvwprintw(win, help_row++, tab1, "not apply to this method. This method always runs one round.");
				help_row++;
				mvwprintw(win, help_row++, tab1, "Use this method to blank disks before internal redeployment, or before");
				mvwprintw(win, help_row++, tab1, "reinstalling Microsoft Windows to remove the data areas that the format");
				mvwprintw(win, help_row++, tab1, "utility preserves.");
			break;
			case 1:
				mvwprintw(win, help_row++, tab1, "syslinux.cfg: nuke=\"dwipe --method ops2\"" );
				mvwprintw(win, help_row++, tab1, "Security Level: Medium (8 passes)" );
				help_row++;
				mvwprintw(win, help_row++, tab1, "The Royal Canadian Mounted Police Technical Security Standard for            " );
				mvwprintw(win, help_row++, tab1, "Information Technology, Appendix OPS-II: Media Sanitization.                 " );
				help_row++;
				mvwprintw(win, help_row++, tab1, "This implementation, with regards to paragraph 2 section A of the standard,");
				mvwprintw(win, help_row++, tab1, "uses a pattern that is one random byte and that is changed each round.");
			break;
			case 2:
				mvwprintw(win, help_row++, tab1, "syslinux.cfg: nuke=\"dwipe --method dodshort\"");
				mvwprintw(win, help_row++, tab1, "Security Level: Medium (3 passes)");
				help_row++;
				mvwprintw(win, help_row++, tab1, "The American Department of Defense 5220.22-M short wipe.");
				mvwprintw(win, help_row++, tab1, "This method is composed of passes 1,2,7 from the standard wipe.");
			break;
			case 3:
				mvwprintw(win, help_row++, tab1, "syslinux.cfg: nuke=\"dwipe --method dod522022m\"");
				mvwprintw(win, help_row++, tab1, "Security Level: Medium (7 passes)");
				help_row++;
				mvwprintw(win, help_row++, tab1, "The American Department of Defense 5220.22-M standard wipe.");
				mvwprintw(win, help_row++, tab1, "This implementation uses the same algorithm as the Heidi Eraser product.");
			break;
			case 4:
				mvwprintw(win, help_row++, tab1, "syslinux.cfg: nuke=\"dwipe --method gutmann\"");
				mvwprintw(win, help_row++, tab1, "Security Level: High (35 passes)");
				help_row++;
				mvwprintw(win, help_row++, tab1, "This is the method described by Peter Gutmann in the paper entitled");
				mvwprintw(win, help_row++, tab1, "\"Secure Deletion of Data from Magnetic and Solid-State Memory\".");
			break;
			case 5:
				mvwprintw(win, help_row++, tab1, "syslinux.cfg: nuke=\"dwipe --method random\"" );
				mvwprintw(win, help_row++, tab1, "Security Level: Depends on Rounds" );
				help_row++;
				mvwprintw(win, help_row++, tab1, "This method fills the device with a stream from the PRNG. It is probably the");
				mvwprintw(win, help_row++, tab1, "best method to use on modern hard disk drives because encoding schemes vary.");
				help_row++;
				mvwprintw(win, help_row++, tab1, "This method has a medium security level with 4 rounds, and a high security");
				mvwprintw(win, help_row++, tab1, "level with 8 rounds.");
			break;
		}

		/* Refresh Window */
		wrefresh(win);

		keystroke = getch();
		switch (keystroke) {
			case KEY_BACKSPACE:
			case 127:
			case 27: // Escape
				breakLoop = true;
				noSave = true;
			break;
			case KEY_DOWN:
			case 'k':
			case 'K':
				if (focus < count - 1) focus++;
				else focus = 0;
			break;
			case KEY_UP:
			case 'j':
			case 'J':
				if (focus > 0) focus--;
				else focus = count - 1;
			break;
			case KEY_ENTER:
			case ' ':
			case 10: {
				int i = 0;
				for (i = 0; i < count; i++) {
					if (focus == i) {
						breakLoop = true;
					}
				}
			}
			break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
				focus = keystroke - '0' - 1;
				//breakLoop = true;
			break;
		}
	}

	if (!noSave) {
		switch (focus) {
			case 0:
				dwipe_options.method = &dwipe_zero;
			break;
			case 1:
				dwipe_options.method = &dwipe_ops2;
			break;
			case 2:
				dwipe_options.method = &dwipe_dodshort;
			break;
			case 3:
				dwipe_options.method = &dwipe_dod522022m;
			break;
			case 4:
				dwipe_options.method = &dwipe_gutmann;
			break;
			case 5:
				dwipe_options.method = &dwipe_random;
			break;
		}
	}

	werase(win);
	delwin(win);
} /* dwipe_gui_method */


void
dwipe_gui_load( void )
{
/**
 * Prints the system load average to the statistics window.
 *
 * @modifies stat_window      Prints the system load average to the statistics window.
 *
 */

	/* A file handle for the stat file. */
	FILE* dwipe_fp;

	/* The one, five, and fifteen minute load averages. */
	float load_01;
	float load_05;
	float load_15;


	/* Open the loadavg file. */
	dwipe_fp = fopen( DWIPE_KNOB_LOADAVG, "r" );

	/* Print the label. */
	mvwprintw( stats_window, DWIPE_GUI_STATS_LOAD_Y, DWIPE_GUI_STATS_LOAD_X, "Load Averages:" );

	if( dwipe_fp )
	{
		/* The load averages are the first three numbers in the file. */
		if( 3 == fscanf( dwipe_fp, "%f %f %f", &load_01, &load_05, &load_15 ) )
		{
			/* Print the load average. */
			mvwprintw( stats_window, DWIPE_GUI_STATS_LOAD_Y, DWIPE_GUI_STATS_TAB,
			  "%04.2f %04.2f %04.2f", load_01, load_05, load_15 );
		}

		else
		{
			/* Print an error. */
			mvwprintw( stats_window, DWIPE_GUI_STATS_LOAD_Y, DWIPE_GUI_STATS_TAB, "(fscanf error %i)", errno );
		}

		/* Close the loadavg file. */
		fclose( dwipe_fp );
	}

	else
	{
		mvwprintw( stats_window, DWIPE_GUI_STATS_LOAD_Y, DWIPE_GUI_STATS_TAB, "(fopen error %i)", errno );
	}

} /* dwipe_gui_load */



void dwipe_gui_status( int count, dwipe_context_t* c )
{
/**
 * Shows runtime statistics and overall progress.
 *
 * @parameter count           The number of contexts in the array.
 * @parameter c               An array of device contexts.
 *
 * @modifies  main_window     Prints information into the main window.
 * @modifies  c[].throughput  Updates the i/o throughput value. 
 *
 */

	/* Throughput print formats. */
	char* dwipe_tera = "%llu TB/s";
	char* dwipe_giga = "%llu GB/s";
	char* dwipe_mega = "%llu MB/s";
	char* dwipe_kilo = "%llu kB/s";
	char* dwipe_unit = "%llu B/s";

	/* The throughput format pointer. */
	char* dwipe_format;

	/* We count time from when this function is first called. */
	static time_t dwipe_time_start = 0;

	/* The current time. */
	time_t dwipe_time_now;

	/* The index of the element that is visible in the first slot. */
	static int offset;

	/* The number of elements that we can show in the window. */
	int slots;

	/* Window dimensions. */
	int wlines;
	int wcols;

	/* Generic loop variable. */
	int i;

	/* The current working line in the main window. */
	int yy;

	/* User input buffer. */
	int keystroke;

	/* The combined througput of all processes. */
	u64 dwipe_throughput = 0;

	/* The estimated runtime of the slowest device. */
	time_t dwipe_maxeta = 0;

	/* The combined number of errors of all processes. */
	u64 dwipe_errors = 0;

	/* Time values. */
	int dwipe_hh;
	int dwipe_mm;
	int dwipe_ss;

	/* The number of active wipe processes. */
	int dwipe_active = 0;


	if( dwipe_time_start == 0 )
	{
		/* This is the first time that we have been called. */
		dwipe_time_start = time( NULL ) -1;
	}

	/* Get the window dimensions. */
	getmaxyx( main_window, wlines, wcols );

	/* Less four lines for the box and padding. */
	slots = wlines - 4;

	/* Each element prints three lines. */
	slots /= 3;

	/* Get the current time. */
	dwipe_time_now = time( NULL );

	/* Erase the main window. */
	werase( main_window );

	/* Erase the stats window. */
	werase( stats_window );

	/* Initialize our working offset to the third line. */
	yy = 2;


	/* Try to get a keystroke. */
	keystroke = getch();

	switch( keystroke )
	{
			case KEY_DOWN:
			case 'j':
			case 'J':

				/* Scroll down. */
				offset += 1;

				if( count < slots )
				{
					offset = 0;
				}

				else if( offset + slots > count )
				{
					offset = count - slots;
				}

				break;

			case KEY_UP:
			case 'k':
			case 'K':

				/* Scroll up. */
				offset -= 1;

				if( offset < 0 )
				{
					offset = 0;
				}

				break;

			default:

				/* Do nothing. */
				break;

	} /* keystroke */


	/* Enumerate all contexts to compute statistics. */
	for( i = 0 ; i < count ; i++ )
	{
		/* Check whether the child process is still running the wipe. */
		if( c[i].pid > 0 )
		{
			/* Increment the child counter. */
			dwipe_active += 1;

			/* Maintain a rolling average of throughput. */
			dwipe_update_speedring( &c[i].speedring, c[i].round_done, dwipe_time_now );

			if( c[i].speedring.timestotal > 0 )
			{
				/* Update the current average throughput in bytes-per-second. */
				c[i].throughput = c[i].speedring.bytestotal / c[i].speedring.timestotal;

				/* Update the estimated remaining runtime. */
				c[i].eta = ( c[i].round_size - c[i].round_done ) / c[i].throughput;

				if( c[i].eta > dwipe_maxeta )
				{
					dwipe_maxeta = c[i].eta;
				}
			}

			/* Update the percentage value. */
			c[i].round_percent = (double) c[i].round_done / (double) c[i].round_size * 100;

			/* Accumulate combined throughput. */
			dwipe_throughput += c[i].throughput;

		} /* child running */

		/* Accumulate the error count. */
		dwipe_errors += c[i].pass_errors;
		dwipe_errors += c[i].verify_errors;

	} /* for statistics */


	/* Print information for the user. */
	for( i = offset ; i < offset + slots && i < count ; i++ )
	{
		/* Print the context label. */
		mvwprintw( main_window, yy++, 2, "%s", c[i].label );

		/* Check whether the child process is still running the wipe. */
		if( c[i].pid > 0 )
		{
			/* Print percentage and pass information. */
			mvwprintw( main_window, yy++, 4, "[%05.2f%%, round %i of %i, pass %i of %i] ", \
			  c[i].round_percent, c[i].round_working, c[i].round_count, c[i].pass_working, c[i].pass_count );

		} /* child running */

		else
		{
			if( c[i].result == 0 ) { mvwprintw( main_window, yy++, 4, "(success) " );                         }
			else if( c[i].signal ) { mvwprintw( main_window, yy++, 4, "(failure, signal %i) ", c[i].signal ); }
			else                   { mvwprintw( main_window, yy++, 4, "(failure, code %i) ", c[i].result );   }

		} /* child returned */

		if( c[i].verify_errors ) { wprintw( main_window, "[verify errors: %llu] ", c[i].verify_errors ); }
 		if( c[i].pass_errors   ) { wprintw( main_window, "[pass errors: %llu] ",   c[i].pass_errors   ); }


		switch( c[i].pass_type )
		{
			case DWIPE_PASS_FINAL_BLANK:
  				wprintw( main_window, "[blanking] " );
				break;

			case DWIPE_PASS_FINAL_OPS2:
  				wprintw( main_window, "[OPS-II final] " );
				break;

			case DWIPE_PASS_WRITE:
				wprintw( main_window, "[writing] " );
				break;

			case DWIPE_PASS_VERIFY:
				wprintw( main_window, "[verifying] " );
				break;

			case DWIPE_PASS_NONE:
				break;
		}

  		if( c[i].sync_status   ) { wprintw( main_window, "[syncing] "   ); }


		     if( c[i].throughput >= INT64_C( 1000000000000 ) )
			    { wprintw( main_window, "[%llu TB/s] ", c[i].throughput / INT64_C( 1099511627776 ) ); }
		else if( c[i].throughput >= INT64_C( 1000000000    ) )
		       { wprintw( main_window, "[%llu GB/s] ", c[i].throughput / INT64_C( 1073741824    ) ); }
		else if( c[i].throughput >= INT64_C( 100000       ) )
		       { wprintw( main_window, "[%llu MB/s] ", c[i].throughput / INT64_C( 1048576       ) ); }
		else if( c[i].throughput >= INT64_C( 1000          ) )
		       { wprintw( main_window, "[%llu kB/s] ", c[i].throughput / INT64_C( 1024          ) ); }
		else
		       { wprintw( main_window, "[%llu B/s] ",  c[i].throughput / INT64_C( 1             ) ); }

		/* Insert whitespace. */
		yy += 1;

	} /* for */

	if( offset > 0 )
	{
		mvwprintw( main_window, 1, wcols -8, " More " );
		waddch( main_window, ACS_UARROW );
	}

	if( count - offset > slots )
	{
		mvwprintw( main_window, wlines -2, wcols -8, " More " );
		waddch( main_window, ACS_DARROW );
	}


	/* Box the main window. */
	box( main_window, 0, 0 );

	/* Refresh the main window. */
	wrefresh( main_window );

	/* Update the load average field. */
	dwipe_gui_load();


	     if( dwipe_throughput >= INT64_C( 1000000000000 ) )
		    { dwipe_throughput /= INT64_C( 1099511627776 ); dwipe_format = dwipe_tera; }
	else if( dwipe_throughput >= INT64_C( 1000000000    ) )
	       { dwipe_throughput /= INT64_C( 1073741824       ); dwipe_format = dwipe_giga; }
	else if( dwipe_throughput >= INT64_C( 100000        ) )
	       { dwipe_throughput /= INT64_C( 1048576          ); dwipe_format = dwipe_mega; }
	else if( dwipe_throughput >= INT64_C( 1000          ) )
	       { dwipe_throughput /= INT64_C( 1024             ); dwipe_format = dwipe_kilo; }
	else
	       { dwipe_format = dwipe_unit; }

	/* Print the combined throughput. */
	mvwprintw( stats_window, DWIPE_GUI_STATS_THROUGHPUT_Y, DWIPE_GUI_STATS_THROUGHPUT_X, "Throughput:" );

	if( dwipe_throughput > 0 )
	{
		mvwprintw( stats_window, DWIPE_GUI_STATS_THROUGHPUT_Y, DWIPE_GUI_STATS_TAB, dwipe_format, dwipe_throughput );
	}

	/* Change the current time into a delta. */
	dwipe_time_now -= dwipe_time_start;

	/* Put the delta into HH:mm:ss form. */
	dwipe_hh = dwipe_time_now / 3600;
	dwipe_time_now %= 3600;
	dwipe_mm = dwipe_time_now / 60;
	dwipe_time_now %= 60;
	dwipe_ss = dwipe_time_now;

	/* Print the runtime. */
	mvwprintw( stats_window, DWIPE_GUI_STATS_RUNTIME_Y, 1, "Runtime:" );
	mvwprintw( stats_window, DWIPE_GUI_STATS_RUNTIME_Y, DWIPE_GUI_STATS_TAB, "%02i:%02i:%02i", dwipe_hh, dwipe_mm, dwipe_ss );

	mvwprintw( stats_window, DWIPE_GUI_STATS_ETA_Y, 1, "Remaining:" );

	if( dwipe_maxeta > 0 )
	{
		/* Do it again for the estimated runtime remaining. */
		dwipe_hh = dwipe_maxeta / 3600;
		dwipe_maxeta %= 3600;
		dwipe_mm = dwipe_maxeta / 60;
		dwipe_maxeta %= 60;
		dwipe_ss = dwipe_maxeta;
	
		/* Print the estimated runtime remaining. */
		mvwprintw( stats_window, DWIPE_GUI_STATS_ETA_Y, DWIPE_GUI_STATS_TAB, "%02i:%02i:%02i", dwipe_hh, dwipe_mm, dwipe_ss );
	}

	/* Print the error count. */
	mvwprintw( stats_window, DWIPE_GUI_STATS_ERRORS_Y, DWIPE_GUI_STATS_ERRORS_X, "Errors:" );
	mvwprintw( stats_window, DWIPE_GUI_STATS_ERRORS_Y, DWIPE_GUI_STATS_TAB, "%llu", dwipe_errors );

	/* Add a border. */
	box( stats_window, 0, 0 );

	/* Add a title. */
	mvwprintw( stats_window, 0, ( DWIPE_GUI_STATS_W - strlen( stats_title ) ) /2, "%s", stats_title );

	/* Refresh the stats window. */
	wrefresh( stats_window );

} /* dwipe_gui_status */


void dwipe_update_speedring( dwipe_speedring_t* speedring, u64 speedring_bytes, time_t speedring_now )
{
	if( speedring->timeslast == 0 )
	{
		/* Ignore the first sample and initialize. */
		speedring->timeslast = speedring_now;
		return;
	}

	if( speedring_now - speedring->timeslast < DWIPE_KNOB_SPEEDRING_GRANULARITY )
	{
		/* Avoid jitter caused by frequent updates. */
		return;
	}

	/* Subtract the oldest speed sample from the accumulator. */
	speedring->bytestotal -= speedring->bytes[ speedring->position ];
	speedring->timestotal -= speedring->times[ speedring->position ];

	/* Put the lastest bytes-per-second sample into the ring buffer. */
	speedring->bytes[ speedring->position ] = speedring_bytes - speedring->byteslast;
	speedring->times[ speedring->position ] = speedring_now   - speedring->timeslast;

	/* Add the newest speed sample to the accumulator. */
	speedring->bytestotal += speedring->bytes[ speedring->position ];
	speedring->timestotal += speedring->times[ speedring->position ];

	/* Remember the last sample. */
	speedring->byteslast = speedring_bytes;
	speedring->timeslast = speedring_now;

	if( ++speedring->position >= DWIPE_KNOB_SPEEDRING_SIZE )
	{
		speedring->position = 0;
	}
}

void refresh_all_windows() {
	touchwin(stdscr);
	refresh();

	touchwin(header_window);
	wrefresh(header_window);

	touchwin(options_window);
	wrefresh(options_window);

	touchwin(stats_window);
	wrefresh(stats_window);

	touchwin(main_window);
	wrefresh(main_window);

	touchwin(config_window);
	wrefresh(config_window);

	touchwin(footer_window);
	wrefresh(footer_window);
}

/* eof */
